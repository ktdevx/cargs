# Gap Analysis: argv-parser

要件（`requirements.md`）と、現在のコード（build-foundation、core-runtime、command-schema の実装）との差を調べた。設計の判断材料であり、決定ではない。調べたのは、`include/clargs/`、`src/`、`tests/`、`cmake/`、`docs/`、`.github/workflows/ci.yml` である。外部のライブラリは使わないので、Web での調査はしていない。

## 要約

- argv を読むコード、結果、解析エラーの表現は、まだ 1 つもない。すべて新規に作る。
- command-schema の読み取りインターフェースは、解析に必要な照合（短い名前、長い名前と否定名、サブコマンド）をすでに持っている。`clargs_command_find_long()` は長さを受け取るので、`--name=value` をその場で照合できる。足りないのは「定義要素から結果の中の場所を引く手段」だけで、これは設計で方法を選ぶ。
- 文字列の値型の値は、入力の文字列へのポインタそのものである。結果が生きている間、入力の文字列（argv、ワイド文字から変換した UTF-8、スキーマの既定値）を有効に保つ仕組みが要る。argv を複製するか借りるかの判断（要件 13.7）は、ここで決まる。
- export の検査（`clargs.exports`）は、公開ヘッダの前処理の行を読み飛ばす。`#ifdef _WIN32` の中に `CLARGS_API` の宣言を書くと、Windows 以外の動的ライブラリで検査が失敗する。ワイド文字の入口を Windows だけにするか、全環境にするか（要件 12.6）は、この制約を踏まえて決める。
- 既存の型（不透明ハンドル、`create`/`destroy`、エラーコードの一覧、テスト用アロケータ）にそのまま乗れる。工数は L、リスクは Medium と見る。

## 1. 現状

### 使える資産

| 資産 | 場所 | 解析での使い道 |
|---|---|---|
| 短い名前の照合 | `clargs_command_find_short()` | 束の 1 文字ずつの照合（要件 4）。`'0'`〜`'9'` で呼べば、数字の短い名前の有無も分かる（要件 2.5） |
| 長い名前と否定名の照合 | `clargs_command_find_long(command, name, length, &negated)` | `=` の手前までの長さを渡して照合する（要件 3.1〜3.6）。否定名で一致したかどうかも返る |
| サブコマンドの照合 | `clargs_command_find_subcommand()`、`clargs_command_subcommand_count()` | サブコマンドの切り替え（要件 7） |
| 列挙 | `clargs_command_option_count/at()`、`clargs_command_positional_count/at()` | 必須の検査、既定値の適用を定義した順に行う（要件 9.4） |
| 属性の読み取り | `clargs_option_kind/type/is_required/default/initial()`、`clargs_positional_arity/type/default()` | 種類ごとの扱い、値の解決 |
| 値の変換と後始末 | `clargs_type_convert()`、`clargs_type_release()`、`clargs_type_value_size()`、`clargs_type_kind()` | 値の変換（要件 8）。組み込みと利用者定義を区別しない |
| 検証済みかどうか | `clargs_command_is_validated()` | 前提条件の `assert`（要件 1.7） |
| 親をたどる | `clargs_command_parent()` | ルートかどうかの `assert`、エラーの「現在のコマンド」 |
| メモリ | `clargs_malloc/calloc/realloc/free()`（`src/alloc_internal.h`）、`clargs_strdup()`（`src/str.h`）、`clargs_vec_t`（`src/vec.h`） | 結果の領域、可変個の位置引数の値の並び、文字列の複製 |
| テスト用アロケータ | `tests/support/test_allocator.h` | 確保の各回を順に失敗させるテスト（要件 13.9）。手順は `docs/conventions.md` の「確保の各回を順に失敗させる」にある |

### 従う規約

- 公開ヘッダは `include/clargs/<名前>.h` に置き、`clargs.h` に 1 行足す。ソースは `src/CMakeLists.txt` の一覧に 1 行、テストは `tests/CMakeLists.txt` に `clargs_add_test()` を 1 行足す。
- 生成は `clargs_error_t clargs_<型>_create(…, clargs_<型>_t **out)` の形、破棄は `void clargs_<型>_destroy()` で、`NULL` は何もしない。失敗したら `*out` は `NULL`。
- エラーコードは `CLARGS_ERROR_LIST` の末尾に足す。一覧は 1 つだけで、エラーコードには位置や対象を持たせない（`docs/conventions.md` の「エラーコードを足す」）。
- プログラムの誤りは `assert` の前提条件。変更可能な `static` 変数を足さない。標準の確保関数を直接呼ばない（`clargs.allocations` が検査する）。
- 動的ライブラリが export するのは、公開ヘッダで `CLARGS_API` を付けた関数とちょうど一致する（`clargs.exports`）。
- 公開ヘッダで `<stdbool.h>`、`<stddef.h>` は使われている。`<stdint.h>` と `<wchar.h>` を公開ヘッダで使うのは、このスペックが初めてになる。
- CI は、Linux（GCC、Clang）、macOS、Windows（MSVC）の、静的と動的の両方で、ビルドとテストを実行する。

## 2. 要件と資産の対応

| 要件 | 既存の資産 | 差 |
|---|---|---|
| 1 解析の入口 | `is_validated()`、`parent()` | **Missing**: 入口の関数。argv の要素が `NULL` の場合の扱い（前提条件にするか）は設計で決める |
| 2 要素の解釈 | なし | **Missing**: 字句の分類。スキーマを知らない層として書ける。負の数の規則（2.5）だけは、現在のコマンドに数字の短い名前があるかを照合の層から渡す必要がある |
| 3 長いオプション | `find_long()` | **Missing**: 照合の処理。照合そのものは既存で足りる |
| 4 短いオプションと束ね | `find_short()` | **Missing**: 束の走査 |
| 5 複数回の指定 | `clargs_type_release()` | **Missing**: 置き換えられた値の後始末の管理（8.7） |
| 6 位置引数と終端 | `positional_at()`、`positional_arity()` | **Missing**: 割り当て。検証済みのスキーマでは可変個が最後に 1 つだけなので、左から順に割り当てれば足りる |
| 7 サブコマンド | `find_subcommand()`、`subcommand_count()` | **Missing**: 切り替えと、選ばれたコマンドの並びの記録 |
| 8 値の変換 | `clargs_type_convert/release()` | **Constraint**: 値の領域は「どんな型のオブジェクトにも合う境界」に置く（`type.h`）。**Constraint**: 文字列の値は入力へのポインタなので、入力を値より長く保つ（下の 3.1） |
| 9 既定値と必須 | `option_default()`、`positional_default()`、`is_required()`、`initial()` | **Missing**: 値の解決の段。既定値の変換は検証で一度確かめられているが、解析のたびにもう一度変換する。利用者定義の値型やメモリ確保の失敗で、ここでも失敗しうる |
| 10 結果の参照 | なし | **Missing**: 結果オブジェクトと参照 API のすべて。**Unknown**: 定義要素から結果の中の場所を引く方法（下の 3.2） |
| 11 解析エラー | `CLARGS_ERROR_LIST` | **Missing**: エラーの種類、位置、メッセージ。**Unknown**: エラーコードの切り方、メッセージをいつ組み立てるか（下の 3.3、3.4） |
| 12 ワイド文字 | なし | **Missing**: UTF-16 から UTF-8 への変換と入口。**Constraint**: export の検査（下の 3.5） |
| 13 メモリと寿命 | アロケータ、テスト用アロケータ | **Missing**: 失敗時の後始末と、そのテスト。`docs/ownership.md` の「結果は argv の文字列を複製するか」の行を、決定で埋める |
| 14 互換性と独立性 | `clargs.h`、命名規約、CI | **Missing**: 使い方の文書（`docs/schema.md` と並ぶもの）、README の更新 |

## 3. 設計で決める点

### 3.1 入力の文字列の寿命

文字列の値型は、値として入力の文字列へのポインタを書く（`type.h`: "The value is the pointer to the input string itself, not a copy"）。利用者定義の値型も、入力を参照する値を書いてよい。したがって、結果は、変換に渡したすべての文字列を、値を後始末するまで有効に保つ必要がある。入力の出どころは 3 つある。

- `char` の argv: 借りるなら、利用者が結果の破棄まで argv を保つ。`main` の argv はプログラムの終了まで有効なので、ふつうは問題にならない。複製するなら、結果が所有する。`docs/ownership.md` の既定は複製である。
- ワイド文字の argv: 変換した UTF-8 は、どちらにしても結果が所有するしかない。
- 既定値: スキーマが持つ文字列を借りる。スキーマは、結果がある間は変更も破棄もされない（要件 13.6）ので、借りられる。

`-ovalue` や `--name=value` の値は、要素の途中から始まる。借りる場合も複製する場合も、要素の途中を指すポインタを値型に渡せる。

### 3.2 定義要素から結果の中の場所を引く方法

結果への問い合わせは、オプションまたは位置引数のハンドルで指定する（要件 10.3）。公開の読み取りインターフェースには、定義要素がコマンドの中で何番目かを返す関数がない。選択肢は 3 つある。

- 公開の列挙（`option_at()`）でポインタを比べて探す。スキーマに手を入れないが、問い合わせのたびに線形に探す。
- command-schema に、番号を返す読み取り関数を足す。brief.md の方針（足りない読み取り関数は command-schema に足す）に合う。
- `src/schema_internal.h` の構造体を直接読む。brief.md と要件の Adjacent expectations が禁じている。

選ばれたコマンドの並びに属さない定義要素の問い合わせ（要件 10.9）は、`clargs_option_command()` と並びを比べれば判定できる。

### 3.3 エラーコードの切り方

規約は「一覧は 1 つ」「エラーコードには位置や対象を持たせない」としている。選択肢は 2 つある。

- 解析エラーを 1 つのコード（たとえば「解析エラー」）で返し、種類は結果から読む別の列挙にする。エラーコードの一覧が増えにくい。
- 種類ごとにエラーコードを足す（未知のオプション、値の不足、…）。戻り値だけで種類が分かる。値の変換の失敗には、既存の `CLARGS_ERROR_INVALID_VALUE` を使える。

どちらでも、位置、問題の要素、関係する定義要素は、結果から読む（要件 10.10、11.4）。

### 3.4 メッセージの組み立て

結果の読み取りはメモリを確保しない（要件 10.13）。メッセージ（要件 11.8）の持ち方には、次の選択肢がある。

- 解析エラーを見つけた時点で組み立てて、結果に持たせる。確保が失敗したら、解析の全体をメモリ確保の失敗にする。
- 利用者が用意したバッファに書く関数にする（`snprintf` と同じ形）。確保が要らないが、利用者のコードが 1 手間増える。

問題の要素は長さに上限がないので、固定長のバッファに切り詰めて持つ方法は、要件 11.9（問題の名前や値を含める）との兼ね合いを確かめる必要がある。

### 3.5 ワイド文字の入口を提供する環境

`cmake/ClargsCheckExports.cmake` は、公開ヘッダから前処理の行を取り除いてから、`CLARGS_API` の付いた宣言を集める。`#ifdef _WIN32` で囲んだ宣言も、すべての環境で「export されるはず」と数えられる。選択肢は 3 つある。

- すべての環境で提供する。`wchar_t` は C99 の型なので、宣言はどこでも書ける。Windows 以外では `wchar_t` が 32 ビット（UTF-32）なので、変換を 2 通り持つ。検査のスクリプトは直さなくてよく、Linux の CI でも変換をテストできる。
- Windows だけで提供し、検査のスクリプトに条件付きの宣言を扱う仕組みを足す。build-foundation のファイルを直すことになる。
- 変換の関数を全環境に置き、`wmain` 向けの入口だけを薄くする。上の 2 つの中間である。

UTF-16 から UTF-8 への変換は、C 標準ライブラリにはない（`wcstombs` はロケールに依存する）。Windows の API（`WideCharToMultiByte`）を使うと「C 標準ライブラリ以外に依存しない」（要件 14.5）から外れるので、変換は自前で書くことになる。量は小さい。対になっていないサロゲートは解析エラーにする（要件 12.4）。

### 3.6 値の領域

`clargs_type_convert()` に渡す領域は、`clargs_type_value_size()` バイト以上で、どんな型にも合う境界に置く。値 1 つごとに `clargs_malloc()` で確保すれば境界は満たせる。まとめて確保する場合は、境界の計算を自前でする（C99 には `max_align_t` がない）。可変個の位置引数は値の個数が実行時に決まるので、`clargs_vec_t` に値へのポインタを入れる形が既存の型に合う。

### 3.7 そのほか

- カウントの回数の型と、上限を超えたときの扱い。
- 解析が失敗したときに `*out` に何を書くか（解析エラーでは結果、メモリ確保の失敗では `NULL`）。要件 13.3 が文書への記載を求めている。
- 値の出どころ、解析エラーの種類、位置の出どころの種類の 3 つの列挙。どれも、後から値を足せる形にする（`clargs_type_kind_t` のコメントが先例である）。
- Windows のテスト。ワイド文字の入口のテストは、`wchar_t` の配列を直接渡せば `wmain` なしで書ける。

## 4. 実装の進め方の選択肢

### Option A: 既存のソースを広げる

`command.c` や `type.c` に解析の処理を足す。

- ✅ ファイルが増えない。スキーマの内部を直接読める。
- ❌ 「スキーマを読み取りインターフェースだけで読む」という境界を破る。
- ❌ command-schema のソースが、定義と解析の 2 つの責務を持つ。`roadmap.md` の一方向の依存に反する。

採る理由は見当たらない。

### Option B: 新しいモジュールとして作る

公開ヘッダを足し（たとえば `parse.h`）、ソースを層ごとに分ける（字句、照合、値の解決、結果、エラーとメッセージ、ワイド文字の変換）。既存のファイルで直すのは、追記が前提のもの（`clargs.h`、`error.h` の一覧、`src/CMakeLists.txt`、`tests/CMakeLists.txt`、`docs/`、README）だけにする。

- ✅ brief.md の Boundary Candidates と 1 対 1 に対応する。字句の層とワイド文字の変換は、スキーマなしで内部関数としてテストできる。
- ✅ 環境変数の対応を後から足す場所（値の解決の層）が、初めから分かれている。
- ❌ 3.2 で公開の列挙だけを使うと、問い合わせが線形の探索になる。

### Option C: B に、command-schema への小さな追加を組み合わせる

B と同じ構成で、3.2 のために command-schema に読み取り関数を 1 つか 2 つ足す（定義要素の番号を返すもの）。

- ✅ 問い合わせが定数時間になり、内部構造にも触れない。
- ❌ 承認済みの command-schema のスペック（要件 9、設計、`docs/schema.md`、テスト）に追記が要る。
- ❌ 公開の関数が増える。

## 5. 工数とリスク

- **工数: L（1〜2 週間）**。新しい公開 API がまとまった量あり（入口 2 つ、結果の参照、エラーの参照）、書き方の規則の組み合わせが多い。確保の各回を失敗させるテストと、3 つの OS での確認も要る。
- **リスク: Medium**。使う技術は既知で、既存の型に乗れる。注意が要るのは、入力の文字列と値の寿命（3.1）、置き換えと失敗のときの後始末の漏れ（要件 8.7）、ワイド文字の入口と export の検査（3.5）の 3 つである。

## 6. 設計への申し送り

- **進め方**: Option B を基本にし、3.2 で番号を返す関数が要ると判断したときだけ Option C にする。オプションの数はふつう数十までなので、線形の探索で足りるかどうかを先に見積もる。
- **先に決めること**: 3.1（argv を借りるか複製するか）、3.3（エラーコードの切り方）、3.5（ワイド文字の入口を提供する環境）。公開 API の形がこの 3 つで決まる。
- **Research Needed**:
  - `clargs.exports` が条件付きの宣言をどう数えるかを、実際に動的ライブラリで確かめる（3.5）。
  - Windows 以外の `wchar_t`（32 ビット）で、サロゲートの範囲の値や `0x10FFFF` を超える値をどう扱うか。
  - メッセージの文面の型（引用符の付け方、束の中の 1 文字の示し方）。help-formatter の文面と揃えるかどうか。
  - 利用者定義の値型が、既定値の変換を解析のたびに受けることを、`docs/schema.md` の記述（変換関数は副作用を持たない）と合わせて文書にどう書くか。

---

# Research & Design Decisions

## Summary

- **Feature**: `argv-parser`
- **Discovery Scope**: Extension（command-schema の上に、解析のモジュールを足す）。調査の対象は、上のギャップ分析と、既存のコードと規約（`include/clargs/`、`src/`、`cmake/ClargsCheckExports.cmake`、`cmake/ClargsTargetDefaults.cmake`、`docs/conventions.md`、`docs/ownership.md`）、および command-schema の `design.md` である。外部のライブラリを採用しないので、Web での調査は行っていない。
- **Key Findings**:
  - `cmake/ClargsCheckExports.cmake` は、前処理の行を正規表現で取り除いてから `CLARGS_API` の宣言を集める（108 行目）。条件付きの宣言は、すべての環境で export が求められる。動的ライブラリでの実行では確かめていない。スクリプトを読んだ結果である。
  - 文字列の値型の値は入力へのポインタなので、結果は、変換に渡した文字列を自分で持つか、借りる期間を利用者に課すかのどちらかになる。ワイド文字の入口では、変換した UTF-8 を結果が持つしかない。
  - C では、`char **` から `const char *const *` へ暗黙に変換できない。`main` の argv をキャストなしで渡すには、引数の型を `char *const *` にする。
  - 警告の設定（`-Wall -Wextra -Wpedantic` ほか、MSVC は `/W4`）に `-Wwrite-strings` はない。テストは `char *argv[] = { "tool", "-v" };` の形で argv を書ける。
  - カウントの回数は argv の総バイト数を超えないので、`size_t` で桁あふれが起こらない。
- **Consulted skills**: `kiro-spec-design` の `design-principles.md`、`design-discovery-light.md`、`design-synthesis.md`、`design-review-gate.md`。ほかに、この作業に当たるスキルはなかった。

## Research Log

### export の検査と、条件付きの宣言

- **Context**: ワイド文字の入口を Windows だけにできるか（要件 12.6）。
- **Sources Consulted**: `cmake/ClargsCheckExports.cmake`、`docs/conventions.md` の「公開関数を足す」。
- **Findings**: 前処理の行（`#` で始まる行）はすべて取り除かれる。`#ifdef _WIN32` の中の宣言も、期待する名前に入る。Linux と macOS の動的ライブラリがその関数を export しなければ、`clargs.exports` が失敗する。
- **Implications**: 入口をすべての環境で提供する。検査のスクリプト（build-foundation のファイル）は直さない。

### `wchar_t` の幅

- **Context**: すべての環境で入口を提供するなら、Windows 以外の `wchar_t` をどう読むか。
- **Sources Consulted**: C99 の `<stdint.h>`（`WCHAR_MAX`）、対応環境（Linux の GCC と Clang、macOS の Clang、Windows の MSVC）。
- **Findings**: MSVC では 16 ビットの符号なしで、内容は UTF-16 である。Linux と macOS では 32 ビットの符号付きで、内容は UTF-32 である。`WCHAR_MAX` は前処理で比べられる。
- **Implications**: `WCHAR_MAX <= 0xFFFF` で読み方を分ける。32 ビットの環境では、負の値、サロゲートの範囲、U+10FFFF を超える値を、変換できない単位とする。テストも同じ条件で入力を分ける。

### `main` の argv の型

- **Context**: 要件 1.2（加工せずにそのまま渡せる）。
- **Findings**: `char **` は `char *const *` に暗黙に変換できる。`const char *const *` には、C ではできない（C++ ではできる）。command-schema の `clargs_type_create_choice()` は `const char *const *` を取るが、あちらは利用者が自分で作る配列である。
- **Implications**: `clargs_parse()` は `char *const *argv` を取る。文字列を書き換えないことは、型ではなくコメントで約束する。

## Architecture Pattern Evaluation

| Option | Description | Strengths | Risks / Limitations | Notes |
|--------|-------------|-----------|---------------------|-------|
| 段に分けたパイプライン | 入力の適応 → 字句 → 照合 → 値の解決。全段が 1 つの結果に書く | brief.md の境界と一致。字句と変換を単独でテストできる。環境変数を足す場所が決まっている | 内部関数の数が増える | 採用 |
| 1 つの関数にまとめる | 走査、変換、解決を 1 か所に書く | ファイルが少ない | 値の解決を後から分けると、利用者に見えない範囲でも作り直しになる。`roadmap.md` の「初版で守ること」に反する | 不採用 |
| イベント駆動のコア | 要素ごとにイベントを出し、上の層が結果を作る | 低水準のイテレータを公開できる | `roadmap.md` が、初版には過剰として退けている | 不採用 |

## Design Decisions

### Decision: 結果が argv を複製する

- **Context**: 要件 13.7。`docs/ownership.md` は、既定を複製とし、借りるなら例外として期間を書くとしている。
- **Alternatives Considered**:
  1. 借りる — 確保が減る。利用者は、結果の破棄まで argv を保つ。
  2. 複製する — 結果が自分で持つ。
- **Selected Approach**: 複製する。配列とすべての文字列を、1 回の確保にまとめる。
- **Rationale**: ワイド文字の入口では、変換した UTF-8 を結果が持つしかない。`char` の入口だけ借りると、寿命の規則が入口ごとに違ってしまう。複製なら、エラーの `argument` も値の文字列も、結果の寿命だけで説明できる。
- **Trade-offs**: 確保が 1 回と、argv の総バイト数ぶんのメモリが増える。
- **Follow-up**: 大きさの合計の桁あふれを検査する。

### Decision: 解析エラーのエラーコードは 1 つ

- **Context**: 要件 11.11。規約は、エラーコードに位置や対象を持たせず、一覧を 1 つにしている。
- **Alternatives Considered**:
  1. 種類ごとにエラーコードを足す（9 つ）。
  2. エラーコードは 1 つにし、種類は結果から読む列挙にする。
- **Selected Approach**: `CLARGS_ERROR_INVALID_COMMAND_LINE` の 1 つ。種類は `clargs_parse_error_kind_t`。
- **Rationale**: 利用者は、どのみち結果からメッセージを読む。戻り値は、結果があるか（成功、解析エラー）、ないか（確保の失敗）を分ければ足りる。種類を足しても、ライブラリ全体のエラーコードの一覧が増えない。
- **Trade-offs**: 種類を知るのに、関数の呼び出しが 1 回増える。

### Decision: 解析エラーのときも結果を渡す

- **Context**: 要件 10.10。既存の規約は、失敗したら `*out` を `NULL` にする。
- **Selected Approach**: 解析エラーでは `*out` に結果を渡し、確保の失敗では `NULL` にする。
- **Rationale**: エラーの情報とメッセージの置き場所が、結果の 1 つで済む。別の型（エラーオブジェクト）と、その破棄が要らない。
- **Trade-offs**: 規約の例外になる。`docs/ownership.md` と関数のコメントに書く。利用者は、解析エラーのときも結果を破棄する。

### Decision: 定義要素から枠を、線形の探索で引く

- **Context**: ギャップ分析の 3.2。
- **Alternatives Considered**:
  1. 公開の列挙でポインタを比べる。
  2. command-schema に、番号を返す読み取り関数を足す。
- **Selected Approach**: 1。
- **Rationale**: 承認済みの command-schema を変更せずに済む。オプションは数十個までの想定である。
- **Follow-up**: 足りなくなったら 2 に替える。公開の関数の形は変わらない。

### Decision: 値の出どころと、エラーの位置の出どころを、同じ列挙にする（一般化）

- **Context**: 要件 9.9 と 11.5 は、どちらも「後から種類を足せる出どころ」を求めている。
- **Selected Approach**: `clargs_source_t` の 1 つにし、`NONE`、`DEFAULT`、`COMMAND_LINE` を持たせる。`NONE` は、値がないこと、位置がないことを表す。
- **Rationale**: 環境変数を足すときに、値の出どころにも、エラーの位置にも、同じ値が 1 つ増える。値の有無の問い合わせ（10.5）も、この列挙で答えられる。

### Decision: 値の変換と保持を、結果の 1 か所に置く（単純化）

- **Context**: コマンドラインの値と既定値は、どちらも「文字列を値型で変換して、枠に入れる」処理である。
- **Selected Approach**: 内部関数 `clargs_result_record_option_value()` と `clargs_result_record_positional_value()` を、照合と値の解決の両方が使う。違いは、出どころの引数だけである。
- **Rationale**: 置き換え、後始末、変換の結果の対応づけが 1 か所になる。既定値の変換の失敗も、出どころが `DEFAULT` の `INVALID_VALUE` として、同じ経路で表せる。

### Decision: 自前で作るもの（Build vs. Adopt）

- UTF-8 への変換: C 標準ライブラリの変換はロケールに左右され、OS の API は依存の制約に反する。変換は数十行で書ける。自前で書く。
- メッセージの組み立て: `snprintf` は、長さが `int` を超える場合の扱いと、ロケールの影響を考える必要がある。埋め込むのは文字列だけなので、長さを数えて写す処理を自前で書く。
- 低水準のイテレータ、候補の提案: 要件にない。作らない。

## Risks & Mitigations

- 値を次の要素から取り込むときの添字の進め忘れ、終端の後ろでの分類 — 取り込まれた `--` と `-x` をテストに必ず含める。
- 置き換え、解析エラー、確保の失敗での後始末の漏れ、または二重の後始末 — 利用者定義の値型で変換と後始末の回数を数えるテストと、確保の各回を失敗させるテストで確かめる。
- `clargs.exports` が、条件付きでない新しい宣言で想定どおり通るか — 作業 4（最初に `parse.h` を足す作業）で、動的の構成で確かめる。
- 設計書が 1000 行に近い — 公開の関数の数（解析 2、読み取りと破棄 30）によるもので、責務は 1 つである。分割はしない。作業は Implementation Order の 10 段に分ける。
- メッセージの文面を後から変えると、利用者のテストが壊れうる — 文面を `docs/parse.md` に載せ、変更を Revalidation Triggers に入れた。

## References

- `.kiro/steering/roadmap.md` — 境界、Future boundaries、制約
- `docs/ownership.md`、`docs/conventions.md` — 所有権と寿命の規約、手順
- `.kiro/specs/command-schema/design.md` — 読み取りの関数と値型の契約
- `include/clargs/schema.h`、`include/clargs/type.h` — 上流の公開関数
