# Requirements Document

## Introduction

ArgX（ライブラリ名 `argx`）は、C99 で書くコマンドライン引数パーサーライブラリである。このスペックは、ライブラリの機能を書き始める前に必要な土台を定める。対象は、ビルド、静的/動的ライブラリの切り替え、配布、テスト基盤、CI、そして後続のスペックが従う命名規約とディレクトリ構成である。

利用者は 2 種類いる。

- **ライブラリ開発者**: ArgX 自体を開発する人。3 つの OS で静的・動的の両方をビルドしてテストできる土台を必要とする。
- **ライブラリ利用者**: ArgX を自分のプロジェクトに取り込む人。手間の少ない取り込み方法を必要とする。

現在のリポジトリにはスペック駆動開発の設定だけがあり、ソースコード、ビルド設定、CI はない。背景と判断の経緯は同じディレクトリの `brief.md` と、`.kiro/steering/roadmap.md` を参照。

## Boundary Context

- **In scope**:
  - 公開名（`roadmap.md` の Naming）のビルドと配布への反映、命名規約、ディレクトリ構成
  - ライブラリのビルド（C99、警告の扱い、静的/動的の切り替え、公開シンボルの制御、Debug ビルドの名前）
  - バージョン情報（ヘッダ、実行時に取得する関数、配布物への反映）
  - 取り込み方法（`FetchContent`、`add_subdirectory`、`find_package`、pkg-config）とインストール
  - テスト基盤（Naos の取り込み、CTest への登録、内部関数のテスト方法、Windows で動的ライブラリをテストできること）
  - CI（Linux の GCC と Clang、macOS の Clang、Windows の MSVC。それぞれ静的と動的）
  - リポジトリの骨組み（`.gitignore`、`.editorconfig`、ライセンス、README）
- **Out of scope**:
  - ライブラリの機能そのもの（アロケータ、エラーモデル、スキーマ、パーサー、ヘルプ生成）と、その公開 API
  - 個々の機能のテスト
  - リリースの自動化（タグ付け、リリースノート、パッケージレジストリへの登録）
  - CMake 以外のビルドシステム向けのビルド定義
  - API リファレンスの生成
- **Adjacent expectations**:
  - 後続のスペック（core-runtime、command-schema、argv-parser、help-formatter）は、ここで決めた命名規約、ディレクトリ構成、公開関数の印の付け方、テストの登録方法に従う。ビルド設定や命名を独自に決めない。
  - このスペックが置く公開関数は、バージョンを返す関数だけである。それ以外の公開関数は後続のスペックが足す。
  - Naos はテストのためだけに使う外部プロジェクトで、リリースタグに固定して取り込む。Naos 自体の挙動はこのスペックの対象外である。

## Requirements

### Requirement 1: 標準手順でのビルドとテスト

**Objective:** As a ライブラリ開発者, I want リポジトリを取得した直後に決まった手順でビルドとテストを実行できること, so that どの OS でも同じ手順で開発を始められる

#### Acceptance Criteria

1. When ライブラリ開発者がリポジトリのルートで `cmake -S . -B build`、`cmake --build build`、`ctest --test-dir build` を順に実行したとき, the ArgX ビルド shall Linux・macOS・Windows のそれぞれで、追加の引数や事前の手作業なしにライブラリのビルドとテストの実行を成功させる。
2. The ArgX ビルド shall CMake 3.21 以上を要求する。
3. If CMake のバージョンが 3.21 より古い環境で構成が実行されたら, the ArgX ビルド shall 必要なバージョンを示して構成を失敗させる。
4. The ArgX ビルド shall ライブラリのソースを C99 として、コンパイラ独自の拡張を無効にしてコンパイルする。
5. The argx ライブラリ shall 実行時に C 標準ライブラリ以外へ依存しない。
6. The ArgX ビルド shall 対応するコンパイラ（GCC、Clang、MSVC）のそれぞれで、ライブラリのソースに対して警告を有効にする。

### Requirement 2: 静的ライブラリと動的ライブラリの切り替え

**Objective:** As a ライブラリ開発者またはライブラリ利用者, I want 同じソースから静的ライブラリと動的ライブラリのどちらもビルドできること, so that 用途に合った形でライブラリを使える

#### Acceptance Criteria

1. When `BUILD_SHARED_LIBS` を指定せず、または偽にして構成したとき, the ArgX ビルド shall 静的ライブラリを生成する。
2. When `BUILD_SHARED_LIBS` を真にして構成したとき, the ArgX ビルド shall 動的ライブラリを生成する。
3. The ArgX ビルド shall 静的ライブラリと動的ライブラリを、ソースを変更せずに同じソースから生成する。
4. Where 動的ライブラリをビルドする場合, the ArgX ビルド shall ライブラリのバージョンに対応する ABI バージョン（SOVERSION）を成果物に付ける。
5. Where Debug 構成でビルドする場合, the ArgX ビルド shall 成果物のファイル名を Release 構成の成果物と区別できる名前にする。

### Requirement 3: 公開シンボルの制御

**Objective:** As a ライブラリ利用者, I want 動的ライブラリが公開関数だけを見せること, so that 内部の変更で自分のプログラムが壊れず、静的・動的のどちらでも同じヘッダで使える

#### Acceptance Criteria

1. Where 動的ライブラリをビルドする場合, the argx ライブラリ shall 公開関数として印を付けた関数だけを export する。
2. Where 動的ライブラリをビルドする場合, the argx ライブラリ shall 印を付けていない内部の関数と変数を、Linux・macOS・Windows のいずれでも export しない。
3. When 利用者のプログラムが動的ライブラリの公開関数を呼び出すとき, the argx ライブラリ shall Linux・macOS・Windows のいずれでも、利用者側の追加の定義なしにリンクと実行を成功させる。
4. When 利用者のプログラムが静的ライブラリの公開関数を呼び出すとき, the argx ライブラリ shall 動的ライブラリ向けの属性を利用者のコードに適用せず、利用者側の追加の定義なしにリンクを成功させる。
5. The argx ライブラリ shall 公開関数に印を付ける方法を 1 つだけ提供し、後続のスペックが同じ方法で公開関数を足せるようにする。

### Requirement 4: 公開名と命名規約

**Objective:** As a ライブラリ開発者, I want 公開名、命名規約、ディレクトリ構成が最初に決まっていること, so that 後続のスペックがそれぞれ独自に決めずに済み、既存の別ライブラリと名前が衝突しない

#### Acceptance Criteria

1. The ArgX ビルド shall プロジェクト名を `ArgX`、ライブラリの成果物の名前を `argx` とする。
2. The argx ライブラリ shall 公開ヘッダを `<argx/argx.h>` として取り込めるようにし、すべての公開ヘッダを `argx/` の下に置く。
3. The argx ライブラリ shall 公開する関数と型のすべてに接頭辞 `argx_` を、公開するマクロのすべてに接頭辞 `ARGX_` を付ける。
4. The ArgX リポジトリ shall 成果物、ヘッダ、識別子、パッケージのいずれの公開名にも `cargs` を使わない。
5. The ArgX リポジトリ shall 公開する識別子と内部の識別子の命名規約、ファイルの命名規約、ディレクトリ構成（公開ヘッダ、ソース、テスト、ビルド補助ファイルの置き場所）を、リポジトリ内の文書に記載する。
6. The ArgX リポジトリ shall 後続のスペックが新しい公開ヘッダ、ソースファイル、テストを足すときの手順を、リポジトリ内の文書に記載する。

### Requirement 5: バージョン情報

**Objective:** As a ライブラリ利用者, I want コンパイル時と実行時の両方でライブラリのバージョンを知ること, so that 使っているヘッダとリンクしているライブラリが合っているかを確かめられる

#### Acceptance Criteria

1. The argx ライブラリ shall バージョン（メジャー、マイナー、パッチ）を、公開ヘッダからコンパイル時に参照できる形で提供する。
2. When 利用者のプログラムがバージョンを返す関数を呼び出したとき, the argx ライブラリ shall ビルドに使われた公開ヘッダのバージョンと一致する値を返す。
3. The ArgX ビルド shall ビルド、`find_package` 用のパッケージ情報、pkg-config の情報、動的ライブラリの ABI バージョンのすべてで、公開ヘッダに書かれたバージョンを唯一の出どころとして使う。
4. If 公開ヘッダからバージョンを読み取れなかったら, the ArgX ビルド shall 原因を示して構成を失敗させる。
5. The argx ライブラリ shall このスペックの範囲では、バージョンを返す関数以外の公開関数を提供しない。

### Requirement 6: ソースからの取り込み

**Objective:** As a ライブラリ利用者, I want ArgX のソースを自分のビルドに直接組み込めること, so that 事前のインストールなしで使い始められ、自分のビルドが余計な影響を受けない

#### Acceptance Criteria

1. When 利用者のプロジェクトが `FetchContent` または `add_subdirectory` で ArgX を組み込んだとき, the ArgX ビルド shall ターゲット `ArgX::argx` を提供する。
2. When 利用者のターゲットが `ArgX::argx` にリンクしたとき, the ArgX ビルド shall 利用者側の追加の設定なしに、公開ヘッダの取り込みとリンクに必要な設定を利用者のターゲットへ伝える。
3. While ArgX がほかのプロジェクトに組み込まれている間, the ArgX ビルド shall 既定では、テストのビルド、Naos の取得、インストール規則の追加、警告のエラー化を行わない。
4. While ArgX がトップレベルのプロジェクトとしてビルドされている間, the ArgX ビルド shall 既定で、テストのビルド、インストール規則、警告のエラー化を有効にする。
5. The ArgX ビルド shall テストのビルド、インストール規則、警告のエラー化のそれぞれについて、既定の動作を上書きするオプションを接頭辞 `ARGX_` 付きの名前で提供する。
6. The ArgX ビルド shall 警告の設定を ArgX 自身のソースだけに適用し、ArgX にリンクする利用者のターゲットへ伝えない。

### Requirement 7: インストールとパッケージ

**Objective:** As a ライブラリ利用者, I want インストール済みの ArgX を標準的な方法で見つけて使えること, so that CMake でもそれ以外のビルドシステムでも取り込める

#### Acceptance Criteria

1. When ライブラリ開発者またはライブラリ利用者がインストールを実行したとき, the ArgX ビルド shall ライブラリ、公開ヘッダ、`find_package` 用のパッケージ情報、pkg-config 用の `argx.pc` を、指定されたインストール先へ配置する。
2. When インストール済みの ArgX に対して利用者のプロジェクトが `find_package(ArgX)` を実行したとき, the ArgX パッケージ shall ターゲット `ArgX::argx` を提供し、利用者のターゲットがそれにリンクするだけでビルドできるようにする。
3. When 利用者のプロジェクトが `find_package(ArgX <version>)` でバージョンを指定したとき, the ArgX パッケージ shall インストール済みのバージョンが要求と互換である場合にだけ、見つかったと報告する。
4. When 利用者が pkg-config で `argx` を問い合わせたとき, the ArgX パッケージ shall インストール済みのライブラリを使ってコンパイルとリンクができるフラグと、ライブラリのバージョンを返す。
5. The ArgX パッケージ shall インストール先を別の場所へ移しても、`find_package` と pkg-config による取り込みが動くようにする。
6. The ArgX パッケージ shall インストール物に、テスト、Naos、内部ヘッダを含めない。
7. The ArgX パッケージ shall 静的ライブラリとしてインストールした場合も動的ライブラリとしてインストールした場合も、同じ取り込み手順（`find_package(ArgX)` と `ArgX::argx`、または pkg-config の `argx`）で使えるようにする。

### Requirement 8: テスト基盤

**Objective:** As a ライブラリ開発者, I want テストを書いて置くだけで実行対象になる基盤, so that 後続のスペックがテストの仕組みを作り直さずに済む

#### Acceptance Criteria

1. Where テストが有効な場合, the ArgX ビルド shall Naos を、決まったリリースタグに固定して自動的に取得する。
2. Where テストが無効な場合, the ArgX ビルド shall Naos を取得せず、テストの実行ファイルをビルドしない。
3. When ライブラリ開発者が決められた手順でテストのソースファイルを足したとき, the ArgX ビルド shall そのファイルに書かれた Naos のテストを CTest に登録し、`ctest` で実行できるようにする。
4. The ArgX ビルド shall 公開 API のテストを、構成で選ばれた形（静的または動的）のライブラリに対して実行する。
5. Where 動的ライブラリをビルドする場合, the ArgX ビルド shall 内部関数（export されない関数）のテストも、ビルドして実行できるようにする。
6. Where Windows で動的ライブラリをビルドする場合, the ArgX ビルド shall ライブラリ開発者が `PATH` の変更やファイルのコピーを手作業で行わなくても、テストのビルドと実行を成功させる。
7. The ArgX リポジトリ shall バージョンを返す関数のテストを含み、そのテストで土台（ビルド、公開関数の export、テストの登録）が動くことを確かめる。
8. If テストが 1 つでも失敗したら, the ArgX ビルド shall `ctest` を 0 以外の終了コードで終わらせる。

### Requirement 9: CI

**Objective:** As a ライブラリ開発者, I want 変更のたびに対応環境すべてでビルドとテストが自動で実行されること, so that OS やライブラリの形に固有の問題へすぐ気づける

#### Acceptance Criteria

1. When 変更がリポジトリへプッシュされたとき、またはプルリクエストが作成・更新されたとき, the CI shall ビルドとテストを自動で実行する。
2. The CI shall Linux の GCC、Linux の Clang、macOS の Clang、Windows の MSVC のそれぞれについて、静的ライブラリと動的ライブラリの両方をビルドしてテストする。
3. The CI shall 警告をエラーとして扱ってビルドする。
4. The CI shall インストールした ArgX を、リポジトリの外に置いた別のプロジェクトから `find_package` と pkg-config で取り込めること、および `add_subdirectory` と `FetchContent` で組み込めることを確かめる。
5. Where 動的ライブラリをビルドする場合, the CI shall 動的ライブラリが export するシンボルが公開関数だけであることを確かめる。
6. If いずれかの組み合わせでビルド、テスト、または確認が失敗したら, the CI shall 全体の結果を失敗として報告し、失敗した組み合わせを特定できるようにする。

### Requirement 10: リポジトリの骨組み

**Objective:** As a ライブラリ開発者またはライブラリ利用者, I want リポジトリに基本的な文書と設定がそろっていること, so that 利用条件と使い方がすぐ分かり、開発者の間で書式がそろう

#### Acceptance Criteria

1. The ArgX リポジトリ shall ライセンスの全文を記した文書を、リポジトリのルートに含む。
2. The ArgX リポジトリ shall README に、プロジェクトの概要、ビルドとテストの手順、4 つの取り込み方法（`FetchContent`、`add_subdirectory`、`find_package`、pkg-config）の例を記載する。
3. The ArgX リポジトリ shall ビルドの生成物を、バージョン管理の対象から除外する。
4. The ArgX リポジトリ shall 文字コード、改行、インデントの設定を、エディタが自動で適用できる形で提供する。
