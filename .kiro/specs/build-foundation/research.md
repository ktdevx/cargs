# Research & Design Decisions

## Summary

- **Feature**: `build-foundation`
- **Discovery Scope**: New Feature（グリーンフィールド。リポジトリにソースもビルド設定もない）
- **Key Findings**:
  - Naos v0.4.0 はテストの実行ファイルをビルド直後（POST_BUILD）に `--naos-list` 付きで起動してテストを列挙する。POST_BUILD は追加した順に走るので、Windows で DLL を実行ファイルの隣へコピーする処理は、列挙の登録より前に追加する必要がある。`naos_add_executable()` では順序を制御できないため、`add_executable()` と `naos_discover_tests()` を使う。
  - Naos はサブプロジェクトとして取り込むと常に静的ライブラリになり、自己テストもインストール規則も既定で無効になる。警告フラグを親から隔離する仕組みはないので、CLine 側は警告をターゲット単位で付ける。
  - Visual Studio のようなマルチコンフィグのジェネレータでは、`ctest --test-dir build` に `-C` がないとテストが実行されない。要件 1.1（追加の引数なしで 3 コマンドが通る）を満たすには、構成が指定されないときの既定を CLine 側で与える必要がある。

## Research Log

### Naos の取り込みとテスト登録

- **Context**: テスト基盤（要件 8）の契約を決めるため、Naos の CMake インターフェースを確認した。
- **Sources Consulted**: `https://github.com/vabelorg/naos` を `v0.4.0` と `main`（`09b1421`）でクローンして読んだ。`CMakeLists.txt`、`src/CMakeLists.txt`、`cmake/Naos.cmake`、`cmake/NaosDiscover.cmake`、`include/naos/session.h`、`docs/reference/cmake.md`、`CHANGELOG.md`、`.github/workflows/ci.yml`。ビルドと実行はしていない。
- **Findings**:
  - 最新のタグは `v0.4.0`（CHANGELOG の日付は 2026-11-03）。MIT、C99、CMake 3.21 以上。
  - 取り込みは `FetchContent_Declare(Naos GIT_REPOSITORY https://github.com/vabelorg/naos.git GIT_TAG v0.4.0)`。リンクするターゲットは `Naos::naos` だけ。
  - ライブラリは `main()` を持たない。テスト側が `NAOS_MAIN(NAOS_ADD_SUITE(...))` を書く。
  - 登録用の関数は 2 つ。`naos_add_executable(<target> SOURCES ... [LIBRARIES ...] [PREFIX ...] [LABELS ...] [PROPERTIES ...])` と、既存の実行ファイル向けの `naos_discover_tests(<target> [PREFIX ...] [LABELS ...] [PROPERTIES ...])`。後者はターゲットを作ったのと同じディレクトリで 1 回だけ呼べる。
  - 列挙は POST_BUILD で `--naos-list` を実行する方式。列挙に失敗するとビルドが失敗する。各テストは `<prefix><suite>.<test>` という名前で CTest に載る。
  - サブプロジェクトでは `NAOS_SELFTEST` と `NAOS_INSTALL` が既定で OFF。ライブラリは常に STATIC。`CMAKE_RUNTIME_OUTPUT_DIRECTORY` には触らない。
  - アサーションを 1 つも実行しないテストは失敗する。テストのないスイートは終了コード 2 になる。
  - `NAOS_SUITE` / `NAOS_MAIN` の最後の要素の後にカンマを書けない。`NAOS_TEST(...) { }` などの後に `;` を付けると `-Wpedantic` が警告する。
  - v0.4.0 に残る既知の問題が 2 つある（`main` では修正済み）。`NAOS_CHECK_NOT_NULL` / `NAOS_REQUIRE_NOT_NULL` を未書き込みのメモリに使うと GCC が `-Wmaybe-uninitialized` を出す（#39）。`*_NULL` / `*_NOT_NULL` に関数ポインタを渡すと `-Wpedantic` が警告する（#40）。どちらもテストコード側で出る。
  - Naos の CI が通している警告は `-Wall -Wextra -Wpedantic -Werror` と `/W4 /WX`。これより厳しいフラグ（`-Wconversion` など）での動作は確認されていない。
  - 親プロジェクトのトップレベルで `enable_testing()` を呼ばないと、CTest はテストを見つけない。
- **Implications**:
  - テストの実行ファイルは `add_executable()` で作り、DLL のコピーを追加してから `naos_discover_tests()` を呼ぶ。
  - テストコードに付ける警告は Naos の CI が通している範囲にとどめる。ライブラリ本体にはそれより厳しい警告を付ける。
  - 警告フラグを `add_compile_options()` や `CMAKE_C_FLAGS` で全体に付けない。Naos のソースにも適用されるため。

### Windows の DLL とテストの列挙

- **Context**: 要件 8.6。動的ライブラリをテストするとき、列挙のための起動で DLL が見つからないとビルドが失敗する。
- **Sources Consulted**: CMake のドキュメント（`TARGET_RUNTIME_DLLS`、3.21 で追加）、Naos の `docs`。
- **Findings**:
  - `$<TARGET_RUNTIME_DLLS:tgt>` は、ターゲットが実行時に必要とする DLL のパスを返す。
  - 出力先を 1 つにまとめる方法（`CMAKE_RUNTIME_OUTPUT_DIRECTORY`）でも解決できるが、全体設定なので、CLine がほかのプロジェクトに組み込まれたときに親へ影響する。
- **Implications**: テストの実行ファイルごとに、DLL を実行ファイルの隣へコピーする POST_BUILD を付ける。Windows で動的ライブラリをビルドするときだけ付ける。

### マルチコンフィグでの `ctest`

- **Context**: 要件 1.1。Windows の既定のジェネレータは Visual Studio（マルチコンフィグ）で、`cmake --build build` は Debug をビルドするが、`ctest --test-dir build` は `-C` がないとテストを「利用できない」と扱う。
- **Sources Consulted**: CMake のドキュメント（`TEST_INCLUDE_FILES`、`ctest -C`）。
- **Findings**:
  - CTest が読むテスト定義ファイルは、`-C` の値を変数 `CTEST_CONFIGURATION_TYPE` として受け取る。
  - ディレクトリプロパティ `TEST_INCLUDE_FILES` に登録したスクリプトは、テスト定義の前に読み込まれる。
- **Implications**: `CTEST_CONFIGURATION_TYPE` が空のときに `Debug` を入れるスクリプトを、トップレベルかつマルチコンフィグのときだけ登録する。この方法は文書からの推定で、実機で確かめていない。実装の最初に Windows で確かめ、CI に「3 コマンドだけを実行するジョブ」を置いて守る。

### export マクロの既定

- **Context**: 要件 3.3 と 3.4。静的・動的のどちらの利用者も、追加の定義なしで使える必要がある。
- **Findings**:
  - Windows では、関数は `__declspec(dllimport)` がなくても import ライブラリ経由でリンクできる。データ（変数）は `dllimport` がないとリンクできない。
  - 静的ライブラリの利用者に `dllimport` が付くと、リンクが失敗する。
  - CMake の `GenerateExportHeader` はヘッダをビルドディレクトリに生成する。ソースツリーをそのまま使う利用者（ほかのビルドシステムへの持ち込み）にはヘッダが足りなくなる。
- **Implications**: マクロなしの状態を「静的」とし、動的のときだけ `CLINE_SHARED` を定義する。定義は CMake のターゲットと `cline.pc` が利用者へ伝える。公開するのは関数だけとし、変数を export しない規約にすれば、定義が伝わらない経路でもリンクは通る。ヘッダは手書きでソースツリーに置く。

### 配布物の再配置

- **Context**: 要件 7.5。
- **Findings**:
  - `configure_package_config_file()` と `install(EXPORT)` が生成するファイルは、インストール先からの相対パスで動く。
  - pkg-config と pkgconf はどちらも `${pcfiledir}`（`.pc` のあるディレクトリ）を展開する。`prefix` を `${pcfiledir}` からの相対で書けば、移動後も動く。
- **Implications**: `cline.pc` の `prefix` は `${pcfiledir}` からの相対パスにする。`CMAKE_INSTALL_LIBDIR` が絶対パスで指定された場合は相対にできないので、絶対パスを書く。

## Architecture Pattern Evaluation

| Option | Description | Strengths | Risks / Limitations | Notes |
|--------|-------------|-----------|---------------------|-------|
| ターゲット中心の CMake（採用） | 設定をすべてターゲットのプロパティとして持ち、機能ごとに `cmake/` のモジュールへ分ける | 利用者や Naos へ設定が漏れない。組み込み時に静か | モジュールが数個に分かれる | brief の Approach と一致 |
| 全体設定の CMake | `CMAKE_C_FLAGS` や `add_compile_options()` で一括設定 | 記述が短い | 警告が Naos と利用者へ漏れる。要件 6.6 に反する | 不採用 |
| 静的と動的を同時にビルド | 1 回の構成で両方のターゲットを作る | 1 回で両方を配布できる | ターゲット名と配布名が 2 系統になる。`BUILD_SHARED_LIBS` の慣習から外れる | 不採用。要件 2.1 / 2.2 は切り替え方式 |

## Design Decisions

### Decision: export マクロは手書きで、既定を「静的」にする

- **Context**: 要件 3.1〜3.5。
- **Alternatives Considered**:
  1. `GenerateExportHeader` — CMake が生成する。静的のとき `CLINE_STATIC_DEFINE` が要る。
  2. 既定を `dllimport` にして、静的のとき `CLINE_STATIC` を定義する。
  3. 既定を属性なしにして、動的のとき `CLINE_SHARED` を定義する。
- **Selected Approach**: 3。`include/cline/export.h` に手書きする。
- **Rationale**: 定義が利用者へ伝わらない場合でも、静的はそのままリンクでき、動的も関数だけならリンクできる。壊れる組み合わせがない。ヘッダがソースツリーにそろう。
- **Trade-offs**: 公開する変数を持てない。ロードマップの方針（不透明ハンドルと関数）と一致するので制約にならない。
- **Follow-up**: Windows で、`CLINE_SHARED` なしの利用者が DLL にリンクできることを利用側の確認で確かめる。

### Decision: 内部関数のテストは、同じソースから作る静的なテスト用ライブラリにリンクする

- **Context**: 要件 8.5。動的ライブラリは内部関数を export しない。
- **Alternatives Considered**:
  1. OBJECT ライブラリを本体とテストで共有する — コンパイルは 1 回。ただし動的のときのオブジェクトは `dllexport` 付きで、テスト側の宣言と食い違う。
  2. テストのときだけ全シンボルを export する — 本番と違うものをテストすることになる。
  3. 動的のときだけ、同じソース一覧から静的なテスト用ライブラリをもう 1 つ作る。
- **Selected Approach**: 3。ターゲット名は `cline_internal`。静的のときは `cline` 自体を指す。
- **Rationale**: 本体の成果物に手を加えない。テスト側のリンク先がいつも同じ名前になる。
- **Trade-offs**: 動的かつテスト有効のとき、ソースを 2 回コンパイルする。ライブラリが小さいので許容する。
- **Follow-up**: なし。

### Decision: テストは 1 ファイル 1 実行ファイルにする

- **Context**: 要件 8.3。下流の 4 スペックが並行してテストを足す。
- **Alternatives Considered**:
  1. 種類ごとに 1 つの実行ファイル — `main` を持つファイルに全スイートを列挙する。
  2. テストのファイルごとに実行ファイルを作る — 各ファイルが `NAOS_MAIN` を持つ。
- **Selected Approach**: 2。`cline_add_test()` を 1 回呼ぶと 1 つの実行ファイルになる。
- **Rationale**: スイートを列挙する共有ファイルがなくなり、並行作業でぶつからない。足す手順が「ファイルを置いて 1 行書く」だけになる。
- **Trade-offs**: リンクと列挙の回数が増える。
- **Follow-up**: テストが増えてビルド時間が問題になったら、1 つの `cline_add_test()` に複数のソースを渡してまとめる。

### Decision: バージョンの互換性は、1.0 より前はマイナー一致、以後はメジャー一致

- **Context**: 要件 7.3 と 2.4。
- **Alternatives Considered**:
  1. 常に `SameMajorVersion` — 0.x の間、破壊的変更を表せない。
  2. 常に `ExactVersion` — パッチ更新でも利用者の指定を変える必要がある。
  3. メジャーが 0 の間は `SameMinorVersion`、1 以上は `SameMajorVersion`。
- **Selected Approach**: 3。SOVERSION も同じ区切りにする（0.x の間は `0.<minor>`、1 以上は `<major>`）。
- **Rationale**: 1.0 より前はマイナーで互換が切れるという一般的な慣習に合う。パッケージの互換判定と ABI バージョンが同じ規則になる。
- **Trade-offs**: 0.x の間、マイナーを上げるたびに利用者の再リンクが要る。
- **Follow-up**: なし。

### Decision: バージョンを返す関数は、数値を 1 つ返す

- **Context**: 要件 5.2 と 5.5。公開関数は 1 つだけにする。
- **Alternatives Considered**:
  1. 文字列を返す — 表示には便利だが、比較しにくい。
  2. 3 つの出力引数で返す — 呼び出しが長い。
  3. `(major << 16) | (minor << 8) | patch` を返す。
- **Selected Approach**: 3。同じ式のマクロ `CLINE_VERSION_NUMBER` をヘッダに置く。
- **Rationale**: 「ヘッダとライブラリが合っているか」を `cline_version() == CLINE_VERSION_NUMBER` の 1 行で確かめられる。
- **Trade-offs**: 各要素が 0〜255 に限られる。
- **Follow-up**: 文字列が必要になったら、後続のスペックではなくこのスペックの改訂として足す。

### Decision: 利用側の確認は、1 つの小さなプロジェクトを 4 つのモードで動かす

- **Context**: 要件 9.4 と 7.5。
- **Selected Approach**: `tests/consumer/` に利用側のプロジェクトを 1 つ置き、変数で取り込み方法を切り替える。駆動は `cmake -P` のスクリプトにして、OS ごとのシェルの違いを避ける。インストール先を別の場所へ移してから取り込む。
- **Rationale**: 4 つの方法で同じ `main.c` を使える。CI のワークフローに OS 別の手順を書かずに済む。
- **Trade-offs**: pkg-config の確認は Linux と macOS だけにする。Windows の MSVC では pkg-config を使う経路が一般的でなく、ランナーに入っている保証もないため、`cline.pc` が配置されることだけを確かめる。

## Synthesis Outcomes

- **Generalization**: 「ターゲットに C99、警告、可視性を付ける」処理は、本体、テスト用ライブラリ、テストの実行ファイルで共通なので、1 つの関数にまとめる。警告の強さだけを引数で変える。
- **Build vs. Adopt**: パッケージ情報の生成は CMake 標準の `CMakePackageConfigHelpers` と `GNUInstallDirs` を使う。export マクロだけは `GenerateExportHeader` を採らず手書きにした（上の Decision を参照）。
- **Simplification**: 静的と動的の同時ビルド、コンポーネント分割したインストール、CMake プリセット、コード整形の設定は入れない。要件にない。

## Risks & Mitigations

- マルチコンフィグでの `ctest` の既定構成が推定どおりに動かない — 実装の最初に Windows で確かめる。動かなければ要件 1.1 の Windows での手順を見直す必要があるので、設計へ戻す。
- Naos v0.4.0 の既知の警告（#39、#40）でテストのビルドが止まる — 該当する使い方を規約で避ける。修正版がリリースされたらタグを上げる。
- Naos は 1.0 より前で、マイナーが上がると互換が切れる — タグを 1 か所で固定し、上げるときは全テストを通してから変える。
- テストを有効にすると、構成のときにネットワークが要る — README に `FETCHCONTENT_SOURCE_DIR_NAOS` と `CLINE_BUILD_TESTS=OFF` を書く。
- DLL を自分自身の場所へコピーする場合（親が出力先をまとめているとき）の挙動 — 実装時に確かめる。

## References

- [Naos](https://github.com/vabelorg/naos) — テストフレームワーク。`docs/reference/cmake.md` に登録用関数の仕様がある。
- [CMake: TARGET_RUNTIME_DLLS](https://cmake.org/cmake/help/latest/manual/cmake-generator-expressions.7.html) — DLL のパスを得る生成式。
- [CMake: CMakePackageConfigHelpers](https://cmake.org/cmake/help/latest/module/CMakePackageConfigHelpers.html) — Config とバージョンファイルの生成。
- [CMake: PROJECT_IS_TOP_LEVEL](https://cmake.org/cmake/help/latest/variable/PROJECT_IS_TOP_LEVEL.html) — トップレベルの判定。
