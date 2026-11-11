# Brief: build-foundation

## Problem

ライブラリを作る開発者には、機能を書き始める前に、3 つの OS で静的・動的の両方をビルドしてテストできる土台が要る。土台がないと、後続のスペックがそれぞれビルド設定や命名を決めてしまい、Windows の DLL の export 漏れや、OS ごとの挙動の違いに後から気づくことになる。

ライブラリの利用者には、自分のプロジェクトへ簡単に取り込める配布形態が要る。

## Current State

リポジトリにはスペック駆動開発の設定（`.claude/`、`.kiro/`、`CLAUDE.md`）だけがあり、ソースコード、ビルド設定、CI はない。公開名も決まっていない（`roadmap.md` の Open Decisions）。

## Desired Outcome

- `cmake -S . -B build && cmake --build build && ctest --test-dir build` が Linux・macOS・Windows で通る。
- 同じソースから静的ライブラリと動的ライブラリをビルドできる。動的ライブラリは公開関数だけを export する。
- 利用者は `FetchContent`、`add_subdirectory`、`find_package`、pkg-config のどれでも取り込める。
- Naos で書いたテストが CTest に登録され、CI が 3 つの OS × 静的/動的の組み合わせで実行する。
- 公開名、命名規約、ディレクトリ構成、公開ヘッダの置き方が決まっていて、後続のスペックはそれに従うだけでよい。

## Approach

CMake 3.21 以上の、ターゲット中心の構成にする。

- ライブラリの種類は `BUILD_SHARED_LIBS` で切り替える。動的ライブラリは可視性を既定で隠し、export マクロを付けた関数だけを公開する。静的ライブラリの利用者に DLL 用の属性が付かないよう、マクロは利用形態に応じて切り替える。
- バージョンはヘッダに持ち、CMake とパッケージがそこから読む。動的ライブラリには SOVERSION を付ける。
- トップレベルでビルドしたときだけ、テスト、インストール規則、警告のエラー化を有効にする。ほかのプロジェクトに組み込まれたときは静かにする。
- Naos はテストを有効にしたときだけ `FetchContent` でリリースタグに固定して取り込む。
- CI は GitHub Actions で、OS・コンパイラ・静的/動的の組み合わせを回す。
- 土台が動くことを確かめるため、バージョンを返す関数とそのテストを最初の公開関数として置く。

## Scope

- **In**:
  - 公開名の確定（ライブラリ名、ヘッダのディレクトリ、関数・型・マクロの接頭辞、CMake のパッケージ名とターゲット名）
  - ディレクトリ構成（公開ヘッダ、ソース、テスト、CMake モジュール）
  - CMake プロジェクト、ライブラリターゲット、C99 の指定、警告の設定
  - 静的/動的の切り替え、export マクロ、可視性、SOVERSION、Debug ビルドの名前
  - バージョンのヘッダと、バージョンを返す関数
  - インストール規則、`find_package` 用の Config とバージョンファイル、pkg-config の `.pc`
  - Naos の取り込み、テストの実行ファイルの作り方、CTest への登録
  - 動的ライブラリをテストするときの DLL の置き場所（Windows）
  - 内部関数をテストするためのリンク方法
  - CI（Linux の GCC と Clang、macOS の Clang、Windows の MSVC。静的と動的）
  - `.gitignore`、`.editorconfig`、ライセンス、README の骨組み
- **Out**:
  - アロケータ、エラーモデル、パーサーなど、ライブラリの機能そのもの
  - リリースの自動化（タグ付け、リリースノート、パッケージレジストリへの登録）
  - CMake 以外のビルドシステムのビルド定義（Meson、Bazel など）
  - API リファレンスの生成（Doxygen など）

## Boundary Candidates

- ビルド定義（ライブラリターゲット、コンパイル設定）
- 公開面の規約（export マクロ、可視性、バージョン、命名）
- 配布（インストール、`find_package`、pkg-config）
- テスト基盤（Naos の取り込み、テストの登録、内部関数のテスト方法）
- CI

## Out of Boundary

- ライブラリの機能の公開 API を決めること（各機能スペックが決める）
- エラーコードやアロケータの型を定義すること（core-runtime）
- 個々の機能のテストを書くこと（各機能スペック）

## Upstream / Downstream

- **Upstream**: なし。外部には Naos（テスト時のみ）、CMake、各 OS のコンパイラ。
- **Downstream**: core-runtime、command-schema、argv-parser、help-formatter のすべて。ここで決めた命名、ディレクトリ構成、テストの登録方法に従う。

## Existing Spec Touchpoints

- **Extends**: なし（最初のスペック）
- **Adjacent**: core-runtime。バージョンを返す関数以外の公開関数をここで足さない。

## Constraints

- C99。MSVC でもビルドできる範囲に限る。
- CMake 3.21 以上。
- 実行時の依存は C 標準ライブラリのみ。Naos はテスト時だけで、利用者には要求しない。
- Naos はリリースタグに固定する（調査時点の最新は v0.4.0）。組み込んだ Naos は常に静的ライブラリになる。
- Naos はテストの実行ファイルをビルド直後に一度起動してテストを列挙する。動的ライブラリをテストするとき、Windows では DLL が実行ファイルと同じ場所にないとビルドが失敗する。
- 公開名は既存の `likle/cargs` と衝突しないものにする。
