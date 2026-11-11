# Research & Design Decisions

## Summary

- **Feature**: `core-runtime`
- **Discovery Scope**: Extension（build-foundation の土台の上に、最初の機能モジュールを足す）。軽い調査を行った。対象は、既存のコードと規約、および C ライブラリで広く使われているアロケータとエラーコードの形である。
- **Key Findings**:
  - このスペックの範囲には、アロケータを受け取る公開関数がない。公開するのは型 2 つと `clargs_status_message()` だけで、アロケータの挙動は内部 API の水準でテストする。
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
| 層になった小さなモジュール | `status` ← `alloc` ← `str`、`vec` | 依存が一方向。各ファイルの責務が 1 つ | ファイルが小さく数が多い | 採用 |
| 1 つの `runtime.c` にまとめる | 全部を 1 ファイルに | ファイルが少ない | 「`alloc.c` だけが確保関数を呼べる」という検査の単位がぼやける | 不採用 |
| ヘッダだけのマクロによる配列 | 型つきの配列をマクロで生成 | 型安全 | マクロが大きく、警告の扱いが難しい。デバッグしにくい | 不採用 |

## Design Decisions

### Decision: アロケータの構造体の配置を固定する

- **Context**: 10.1 と `brief.md` の制約（構造体の将来の拡張方法を設計で決める）。
- **Alternatives Considered**:
  1. 先頭に `size_t struct_size` を置き、ライブラリが大きさを見て版を判断する。
  2. 不透明なハンドルにして、生成関数で作る。
  3. 配置を固定し、拡張は新しい型と新しい関数で行う。
- **Selected Approach**: 3。`clargs_allocator` は 4 つのフィールドのまま変えない。
- **Rationale**: 1 は、利用者が大きさの設定を忘れると不正な引数になり、使い方が難しくなる。2 は、アロケータを作るためにメモリ確保が要るという循環になる。3 は最も単純で、既存のバイナリが確実に動き続ける。
- **Trade-offs**: 後から機能を足すときに型が増える。
- **Follow-up**: なし。

### Decision: 関数ポインタに大きさを渡さない

- **Context**: 解放と再確保に、古い大きさを渡すかどうか。
- **Alternatives Considered**:
  1. `malloc` 系と同じ形に、コンテキストを足すだけ。
  2. 解放と再確保に古い大きさを渡す。
- **Selected Approach**: 1。
- **Rationale**: 2 は、ライブラリのすべてのコードが各ブロックの大きさを覚えて渡す必要があり、後続のスペックの負担と誤りの余地が増える。要件は大きさを求めていない。
- **Trade-offs**: 大きさを必要とするアリーナは、利用者がブロックの前に自分で記録する。
- **Follow-up**: なし。

### Decision: 大きさ 0、`NULL` の解放、`NULL` の再確保を利用者のアロケータへ渡さない

- **Context**: `malloc(0)` と `realloc(p, 0)` の挙動は処理系によって違う。
- **Selected Approach**: `clargs_mem_*` が、0 バイトを 1 バイトに切り上げ、`NULL` の解放を呼び出さず、伸長の最初は確保、以後は再確保と使い分ける。
- **Rationale**: 利用者のアロケータが扱う場合が減り、`NULL` の戻り値が必ず失敗を意味する。既定のアロケータと利用者のアロケータで結果が揃う（2.3）。
- **Trade-offs**: 0 バイトの要求で 1 バイトを使う。
- **Follow-up**: テスト用アロケータが、契約に反する呼び出しを `violations` として数える。

### Decision: フィールド名を `allocate`、`reallocate`、`deallocate` にする

- **Context**: `free` や `realloc` という名前のフィールドは、これらを関数形式のマクロにする環境で、呼び出しが書き換えられる。
- **Selected Approach**: 標準の関数と同じ名前を避ける。
- **Rationale**: 利用者の環境を選ばない。確保関数の検査も、識別子の一致だけで済む。
- **Follow-up**: なし。

### Decision: 状態コードは 1 つの列挙に集め、スペックごとに番号の範囲を割り当てる

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

- **Generalization**: 文字列の複製も配列の伸長も「アロケータを通した確保と、失敗時に元を保つこと」の変種である。両方を `clargs_mem_*` の上に載せ、桁あふれと大きさ 0 の扱いを `alloc` に集めた。
- **Build vs. Adopt**: 実行時の依存は C 標準ライブラリだけという制約があるので、外部のライブラリは採用しない。検査は、既存の `cmake -P` の方式を踏襲する。
- **Simplification**: 次のものを設計から外した。既定のアロケータを返す公開関数、長さ指定の文字列複製、配列の削除や挿入、テスト支援のためのビルドターゲット、構造体の大きさのフィールド。

## Risks & Mitigations

- 後続のスペックが `clargs_mem_*` を通さずに利用者の関数ポインタを直接呼ぶ — 規約に書き、テスト用アロケータの `violations` と失敗注入のテストで検出する。
- MSVC だけで開発すると、状態コードの `case` の不足に気づかない — CI の GCC と Clang の構成が、警告をエラーとして検出する。
- 変更可能なグローバル状態がないことを自動で確かめていない — レビューの項目とし、規約に書く。
- ソースの走査は、マクロで組み立てた名前を検出しない — 規約で禁じる。

## References

- `.kiro/steering/roadmap.md` — 制約、命名、Shared seams。
- `docs/conventions.md` — 命名規約、ファイルの置き方、テストの登録方法。
- `.kiro/specs/core-runtime/brief.md` — アプローチとスコープ。
