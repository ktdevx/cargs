# Research & Design Decisions

## Summary

- **Feature**: `core-runtime`
- **Discovery Scope**: Extension（build-foundation の土台の上に、最初の機能モジュールを足す）。軽い調査を行った。対象は、既存のコードと規約、および C ライブラリで広く使われているアロケータとエラーコードの形である。
- **Key Findings**:
  - アロケータは、レビューでの指示により、ライブラリ全体に 1 つ設定する形にした。関数は標準の `malloc`、`realloc`、`calloc`、`free` と同じ形で、利用者のデータは持たない。`roadmap.md` と各スペックの `brief.md` も、この決定に合わせて直した。
  - 既存の export の確認は公開ヘッダを読んで期待する名前を得るので、公開関数を足しても build-foundation のファイルを直す必要がない。内部ユーティリティが export されないことも、同じ確認が保証する。
  - テスト用アロケータは公開ヘッダだけに依存させられる。そうすると、テストの `SOURCES` に 1 ファイル足すだけで、公開・内部のどちらのテストからも使え、`cmake/ClargsTesting.cmake` を変更せずに済む。

## Research Log

### 既存のコードと規約

- **Context**: 新しいヘッダ、ソース、テストをどこに置き、どう登録するか。
- **Sources Consulted**: `docs/conventions.md`、`src/CMakeLists.txt`、`tests/CMakeLists.txt`、`cmake/ClargsTesting.cmake`、`cmake/ClargsCheckExports.cmake`、`include/clargs/*.h`。
- **Findings**:
  - 公開と内部は接頭辞で区別しない。公開かどうかは `CLARGS_API` で決まる。
  - 内部関数のテストは `clargs_internal` にリンクし、動的の構成でも実行できる。
  - `CLARGS_API` を変数に付けると export の確認が失敗する。公開の定数（既定のアロケータなど）を変数として公開する形は取れない。
  - テストは `-Wall -Wextra -Wpedantic` でコンパイルされ、既定で警告がエラーになる。`-Wswitch` は `-Wall` に含まれる。
  - Naos は関数ポインタを `NAOS_CHECK_NULL` 系に渡すと警告する。アロケータのテストでは比較を書く。
- **Implications**: 設計は build-foundation のファイルを変更しない。変更するのは、追記が前提の `clargs.h`、`src/CMakeLists.txt`、`tests/CMakeLists.txt`、`docs/conventions.md` だけである。

### アロケータの形

- **Context**: 関数ポインタのシグネチャと、構造体の拡張方法を決める。
- **Sources Consulted**: 既存の C ライブラリの形についての一般的な知識（Lua の `lua_Alloc`、zlib の `zalloc`/`zfree`/`opaque`、SQLite の `sqlite3_mem_methods`、cJSON の `cJSON_Hooks`）。今回、各ライブラリの文書を取得して確認してはいない。
- **Findings**:
  - コンテキストつきの関数ポインタを利用者が渡す形は、広く使われている。
  - 解放や再確保に古い大きさを渡す形（Lua）は、アリーナに有利だが、ライブラリ側がすべてのブロックの大きさを覚える必要がある。
  - グローバルなフック（cJSON）は、スレッドや複数のインスタンスで干渉する。`brief.md` が退けている。
- **Implications**: 下の Design Decisions を参照。

### 確保関数の直接呼び出しの検査方法

- **Context**: 3.6 の自動の検査をどう実現するか。
- **Findings**:
  - ソースを走査する方法は、3 つの OS で同じように動き、既存の `ClargsCheckExports.cmake` と同じ `cmake -P` の方式で書ける。
  - オブジェクトファイルの未定義シンボルを調べる方法は、マクロ経由の呼び出しも検出できるが、道具と出力の形式が OS ごとに違う。
- **Implications**: ソースの走査を選ぶ。マクロで名前を組み立てる書き方は規約で禁じる。

## Architecture Pattern Evaluation

| Option | Description | Strengths | Risks / Limitations | Notes |
|--------|-------------|-----------|---------------------|-------|
| 層になった小さなモジュール | `error` ← `alloc` ← `str`、`vec` | 依存が一方向。各ファイルの責務が 1 つ | ファイルが小さく数が多い | 採用 |
| 1 つの `runtime.c` にまとめる | 全部を 1 ファイルに | ファイルが少ない | 「`alloc.c` だけが確保関数を呼べる」という検査の単位がぼやける | 不採用 |
| ヘッダだけのマクロによる配列 | 型つきの配列をマクロで生成 | 型安全 | マクロが大きく、警告の扱いが難しい。デバッグしにくい | 不採用 |

## Design Decisions

### Decision: アロケータはライブラリ全体に 1 つ設定する

- **Context**: 設計のレビューで「アロケーターはグローバル領域に設定する」「ユーザーデータは不要」という指示があった。最初の案は、関数ポインタとコンテキストの構造体を、オブジェクトの生成時に渡す形だった（`brief.md` の Approach）。
- **Alternatives Considered**:
  1. オブジェクトごとに渡す。グローバル状態を持たない。
  2. ライブラリ全体に 1 つ設定する。
- **Selected Approach**: 2。`clargs_set_allocator()` で設定し、`src/alloc.c` の `static` 変数に持つ。`NULL` で既定に戻す。
- **Rationale**: 指示による。関数が標準の 4 つと同じ形になり、利用者は既存の確保ライブラリの関数をそのまま設定できる。後続のスペックの公開関数と内部関数から、アロケータの引数がなくなる。
- **Trade-offs**:
  - 変更可能なグローバル状態が 1 つできる。設定は同期しないので、オブジェクトがなく、ほかのスレッドが呼んでいないときに行う、という利用者側の決まりが要る。ライブラリは違反を検出しない。
  - 利用者のデータがないので、状態を持つアロケータは、利用者が自分のグローバルな状態を使う。テスト用アロケータも `static` の状態になる。
  - 静的ライブラリを複数のモジュールにリンクすると、モジュールごとに別の設定になる。
- **Follow-up**: `roadmap.md`（Overview、Approach Decision、Constraints）と、core-runtime、command-schema、argv-parser、help-formatter の `brief.md` を、この決定に合わせて直した。要件 1、2、3、7.6、9.1、9.6 も直した。

### Decision: アロケータの構造体の配置を固定する

- **Context**: 10.1。
- **Alternatives Considered**:
  1. 先頭に `size_t struct_size` を置き、ライブラリが大きさを見て版を判断する。
  2. 配置を固定し、拡張は新しい型と新しい設定関数で行う。
- **Selected Approach**: 2。`clargs_allocator_t` は 4 つのフィールドのまま変えない。
- **Rationale**: 1 は、利用者が大きさの設定を忘れると不正な引数になり、使い方が難しくなる。2 は最も単純で、既存のバイナリが確実に動き続ける。
- **Trade-offs**: 後から機能を足すときに型が増える。

### Decision: 大きさ 0、`NULL` の解放、`NULL` の再確保を利用者のアロケータへ渡さない

- **Context**: `malloc(0)` と `realloc(p, 0)` の挙動は処理系によって違う。
- **Selected Approach**: 内部のメモリ関数が、0 バイトを 1 バイトに切り上げ、`NULL` の解放を呼び出さず、伸長の最初は確保、以後は再確保と使い分ける。
- **Rationale**: 利用者のアロケータが扱う場合が減り、`NULL` の戻り値が必ず失敗を意味する。既定のアロケータと利用者のアロケータで結果が揃う（2.3）。
- **Trade-offs**: 0 バイトの要求で 1 バイトを使う。
- **Follow-up**: テスト用アロケータが、契約に反する呼び出しを `violations` として数える。

### Decision: フィールドは `malloc_fn`、`realloc_fn`、`calloc_fn`、`free_fn`、内部関数は `clargs_malloc` など

- **Context**: 設計のレビューでの指示。フィールドをこの 4 つにする。引数の名前は `ptr` にする。内部関数は、既知の関数との対が分かる名前にする。
- **Selected Approach**: 指示のとおり。関数ポインタの型は `clargs_<名前>_fn_t`。内部関数は `clargs_malloc`、`clargs_calloc`、`clargs_realloc`、`clargs_free` で、引数も標準の関数と同じにする。
- **Rationale**: 標準の関数と 1 対 1 で対応し、読む人が意味を知っている。`_fn` が付くので、`free` などを関数形式のマクロにする環境でも、フィールドの呼び出しは書き換えられない。
- **Trade-offs**: `clargs_realloc` は積の桁あふれを確かめないので、積で大きさを求める呼び出し側（`vec`）が自分で確かめる。

### Decision: 型の名前は `_t` で終える

- **Context**: 設計のレビューでの指示。`docs/conventions.md` は語尾を決めておらず、既存のコードに型はまだない。
- **Selected Approach**: `typedef` の名前は、公開・内部・テスト支援のどれも `_t` で終える。タグには付けない。この規則を `docs/conventions.md` に足す。
- **Trade-offs**: POSIX は `_t` で終わる名前を予約している。`clargs_` の接頭辞があるので、実際に衝突する見込みは小さい。
- **Follow-up**: 後続のスペックの型（スキーマ、結果など）も同じ規則に従う。

### Decision: 公開ヘッダのないモジュールの内部ヘッダには `_internal` を付けない

- **Context**: 設計のレビューでの指示。既存の規約は、内部ヘッダをすべて `src/<名前>_internal.h` としていた。
- **Selected Approach**: 同じ名前の公開ヘッダがあるモジュールだけ `_internal` を付ける（`alloc_internal.h`、既存の `version_internal.h`）。ないモジュールは `src/<名前>.h` とする（`str.h`、`vec.h`）。`docs/conventions.md` の規約を直す。
- **Trade-offs**: `src/` のヘッダが非公開かどうかは、名前ではなく置き場所で判断する。

### Decision: エラーの型は `clargs_error_t`、成功は `CLARGS_ERROR_NONE`、ヘッダは `error.h`

- **Context**: 設計のレビューでの指示。最初の案は `clargs_status`、`CLARGS_OK`、`status.h` だった。
- **Selected Approach**: 指示のとおりにする。関数は `clargs_error_message()`、ソースは `src/error.c`、テストは `tests/public/error_test.c` とする。要件の文書の用語も「エラーコード」に揃えた。

### Decision: エラーコードは 1 つの列挙に集め、スペックごとに番号の範囲を割り当てる

- **Context**: 6.1〜6.3。`roadmap.md` は、各スペックがコードをどう足すかをここで決めるとしている。
- **Alternatives Considered**:
  1. スペックごとに別の列挙型。
  2. 1 つの列挙、値は詰めて連番。
  3. 1 つの列挙、スペックごとに 100 刻みの範囲。
- **Selected Approach**: 3。
- **Rationale**: 1 は、関数ごとに戻り値の型が変わり、4.2 の「同じ形」に反する。2 は、並行して進むスペック（argv-parser と help-formatter）が同じ番号を取り合う。3 は、値を見ればどの層の失敗か分かり、並行作業でぶつからない。
- **Trade-offs**: 1 つのスペックが 99 個を超えるコードを持てない。現実的には足りる。
- **Follow-up**: 説明文の網羅は、`default` のない `switch` とコンパイラの警告で確かめる。MSVC は既定では警告しないので、CI の GCC と Clang の構成に頼る。

### Decision: テスト用アロケータはソースとして取り込む

- **Context**: 9.7。後続のスペックが追加のビルド設定なしで使えること。
- **Alternatives Considered**:
  1. `clargs_test_support` という静的ライブラリを作り、`clargs_add_test()` がすべてのテストにリンクする。
  2. 使うテストが `SOURCES` に `support/test_allocator.c` を足す。
- **Selected Approach**: 2。
- **Rationale**: 1 は build-foundation の `cmake/ClargsTesting.cmake` の変更が要る。2 は、既に書く登録の 1 行にファイル名を足すだけで、上流のファイルに触れない。
- **Trade-offs**: 使うテストごとに同じソースがコンパイルされる。小さいので問題にならない。
- **Follow-up**: テスト用アロケータは公開ヘッダだけに依存させ、動的の構成の公開 API のテストでも使えるようにする。

### Decision: 文字列は既定で複製する

- **Context**: 7.3 と、`roadmap.md` の「所有権と寿命」の問い。
- **Selected Approach**: 呼び出しの後も使う文字列は複製する。借りるのは例外で、関数のコメントに期間を書く。argv の文字列をどちらにするかは、argv-parser の設計がこの枠の中で決める。
- **Rationale**: 複製が既定なら、利用者は寿命を考えずに済み、誤用が減る。argv は量が多く、プログラムの終了まで有効なのがふつうなので、借りる余地を残す。
- **Trade-offs**: スキーマの定義で確保の回数が増える。
- **Follow-up**: argv-parser の設計で、借りるか複製するかを決めて記録する。

## Synthesis Outcomes

- **Generalization**: 文字列の複製も配列の伸長も「設定されたアロケータを通した確保と、失敗時に元を保つこと」の変種である。両方を内部のメモリ関数の上に載せ、大きさ 0 の扱いを `alloc` に集めた。
- **Build vs. Adopt**: 実行時の依存は C 標準ライブラリだけという制約があるので、外部のライブラリは採用しない。検査は、既存の `cmake -P` の方式を踏襲する。
- **Simplification**: 次のものを設計から外した。設定されているアロケータを取得する公開関数、長さ指定の文字列複製、配列の削除や挿入、テスト支援のためのビルドターゲット、構造体の大きさのフィールド、オブジェクトごとのアロケータとコンテキスト。

## Risks & Mitigations

- オブジェクトが存在する間、またはほかのスレッドが動いている間に、利用者がアロケータを設定し直す — ライブラリは検出しない。`docs/ownership.md` に設定してよい時点を書く。
- 後続のスペックが内部のメモリ関数を通さずに標準の確保関数を呼ぶ — 検査 `clargs.allocations` と、失敗注入のテストで検出する。
- MSVC だけで開発すると、エラーコードの `case` の不足に気づかない — CI の GCC と Clang の構成が、警告をエラーとして検出する。
- 変更可能なグローバル状態が設定されたアロケータだけであることを自動で確かめていない — レビューの項目とし、規約に書く。
- ソースの走査は、マクロで組み立てた名前を検出しない — 規約で禁じる。

## References

- `.kiro/steering/roadmap.md` — 制約、命名、Shared seams。
- `docs/conventions.md` — 命名規約、ファイルの置き方、テストの登録方法。
- `.kiro/specs/core-runtime/brief.md` — アプローチとスコープ。
