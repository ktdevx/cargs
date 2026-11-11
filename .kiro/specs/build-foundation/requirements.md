# Requirements Document

## Project Description (Input)

詳細は同じディレクトリの `brief.md` を参照。以下はその要約。

### 誰の問題か

- ライブラリ（CLArgs / `clargs`）を作る開発者。機能を書き始める前に、3 つの OS で静的・動的の両方をビルドしてテストできる土台が要る。土台がないと、後続のスペックがそれぞれビルド設定や命名を決めてしまい、Windows の DLL の export 漏れや、OS ごとの挙動の違いに後から気づくことになる。
- ライブラリの利用者。自分のプロジェクトへ簡単に取り込める配布形態が要る。

### 現在の状況

リポジトリにはスペック駆動開発の設定（`.claude/`、`.kiro/`、`CLAUDE.md`）だけがあり、ソースコード、ビルド設定、CI はない。公開名は決まっている（`roadmap.md` の Naming。プロジェクト名 CLArgs、ライブラリ名 clargs）。

### 何を変えるか

- `cmake -S . -B build && cmake --build build && ctest --test-dir build` が Linux・macOS・Windows で通る。
- 同じソースから静的ライブラリと動的ライブラリをビルドできる。動的ライブラリは公開関数だけを export する。
- 利用者は `FetchContent`、`add_subdirectory`、`find_package`、pkg-config のどれでも取り込める。
- Naos で書いたテストが CTest に登録され、CI が 3 つの OS × 静的/動的の組み合わせで実行する。
- 公開名が `roadmap.md` の Naming のとおりにビルドと配布へ反映され、命名規約、ディレクトリ構成、公開ヘッダの置き方が決まっていて、後続のスペックはそれに従うだけでよい。

### 範囲

- **含む**: 公開名の反映と命名規約、ディレクトリ構成、CMake プロジェクトとライブラリターゲット（C99、警告設定）、静的/動的の切り替え（export マクロ、可視性、SOVERSION、Debug ビルドの名前）、バージョンのヘッダとバージョンを返す関数、インストール規則と `find_package` 用 Config・pkg-config の `.pc`、Naos の取り込みと CTest への登録、Windows での DLL の置き場所、内部関数をテストするためのリンク方法、CI（Linux の GCC と Clang、macOS の Clang、Windows の MSVC。静的と動的）、`.gitignore`・`.editorconfig`・ライセンス・README の骨組み。
- **含まない**: アロケータ、エラーモデル、パーサーなどライブラリの機能そのもの、リリースの自動化、CMake 以外のビルドシステムのビルド定義、API リファレンスの生成。

### 制約

- C99。MSVC でもビルドできる範囲に限る。
- CMake 3.21 以上。
- 実行時の依存は C 標準ライブラリのみ。Naos はテスト時だけで、利用者には要求しない。
- Naos はリリースタグに固定する（調査時点の最新は v0.4.0）。組み込んだ Naos は常に静的ライブラリになる。
- Naos はテストの実行ファイルをビルド直後に一度起動してテストを列挙するため、Windows で動的ライブラリをテストするときは DLL を実行ファイルと同じ場所に置く。
- 公開名は `roadmap.md` の Naming に従う。リポジトリ名の `cargs` は既存の `likle/cargs` と衝突するので公開名に使わない。

## Requirements
<!-- Will be generated in /kiro-spec-requirements phase -->
