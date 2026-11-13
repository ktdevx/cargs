# Roadmap

## Overview

C99 で書かれたコマンドライン引数パーサーライブラリを新規に作る。C のアプリケーション開発者が、短い名前・長い名前、`--no-<name>` 否定、`-vvv` のような回数カウント、値付きオプション、位置引数、サブコマンドを持つコマンドラインを、少ない記述で安全に解析できるようにする。静的ライブラリと動的ライブラリの両方として配布でき、Windows・macOS・Linux で同じ挙動をする。

公開 API は「ビルダー＋不透明ハンドル」とする。利用者は関数呼び出しでコマンドとオプションを定義し（スキーマ）、解析は読み取り専用のスキーマに対して行い、結果は結果オブジェクトから取り出す。構造体の中身を公開しないので、動的ライブラリでも ABI を壊さずに機能を足せる。メモリ確保はすべて差し替え可能なアロケータを通す。アロケータは、ライブラリ全体に 1 つ設定する。

## Approach Decision

- **Chosen**: ビルダー＋不透明ハンドル。定義（スキーマ）・解析（パーサー）・結果・表示（ヘルプ）を別の境界に分け、依存を一方向にする。
- **Why**:
  - 動的ライブラリとして配布するため、公開構造体を ABI にしない設計が後からの拡張に最も強い。
  - アロケータは、ライブラリ全体に 1 つ設定する。標準の `malloc`、`realloc`、`calloc`、`free` と同じ形の関数を設定でき、公開関数はアロケータを引数に取らない。
  - スキーマを読み取り専用で参照する形にすると、パーサーとヘルプ生成が互いを知らずに済み、境界がはっきりする。
- **Rejected alternatives**:
  - 宣言的テーブル（構造体配列＋利用者変数へのバインド）: 記述は最短だが、公開構造体が ABI になり、フィールド追加が破壊的変更になる。定義・解析・結果の境界も曖昧になる。
  - イベント駆動コア＋上位層の 2 層構成: 最も柔軟だが、設計・実装・テストの量が最大になり、初期リリースには過剰。将来、低水準イテレータが必要になれば argv-parser の拡張として検討する。

## Scope

- **In**:
  - CMake によるビルド、静的ライブラリと動的ライブラリの両対応、インストールとパッケージ（`find_package`、pkg-config）
  - Windows・macOS・Linux での CI によるビルドとテスト
  - 差し替え可能なアロケータ、エラーモデル
  - コマンド、サブコマンド、オプション（短い名前・長い名前、フラグ、`--no-<name>` 否定、カウント、値付き）、位置引数の定義
  - 型付き値変換（文字列、整数、浮動小数、選択肢）と、利用者定義の型を足す拡張点
  - argv の解析（`-abc` の束ね、`-vvv`、`-o value` / `-ovalue`、`--name value` / `--name=value`、`--` 終端、サブコマンドの切り替え）と結果の参照
  - Windows のワイド文字 argv（`wmain` の `wchar_t`）の受け付け
  - ヘルプと usage 文の自動生成
  - Naos によるテスト
- **Out**:
  - 環境変数や設定ファイルからの値の補完（初版の対象外。後から足すときの境界は Boundary Strategy の Future boundaries に記録）
  - シェル補完スクリプトの生成
  - 長いオプション名の前方一致による省略（GNU 流の abbreviation）
  - レスポンスファイル（`@file`）
  - メッセージの多言語化（メッセージは英語のみ）
  - C++ ラッパー、他言語バインディング
  - ライブラリによるプロセス終了（`exit`）や、利用者が指定していない出力先への書き込み（Debug 構成で、プログラムの誤りを `assert` が検出したときの中断を除く）

## Constraints

- **言語**: C99。コンパイラ拡張に依存しない。MSVC でもビルドできる範囲に限る（可変長配列、`<complex.h>` などは使わない）。
- **ビルド**: CMake 3.21 以上（Naos の要件。`PROJECT_IS_TOP_LEVEL` と `TARGET_RUNTIME_DLLS` も使える）。
- **対応環境**: Linux（GCC、Clang）、macOS（Clang）、Windows（MSVC）。静的・動的の両方をテストする。
- **テスト**: [Naos](https://github.com/vabelorg/naos) を使う。
  - `FetchContent` でリリースタグに固定して取り込む（調査時点の最新は v0.4.0、MIT、C99、CMake 3.21 以上）。1.0 より前なので、マイナーバージョンが上がると互換が切れることがある。
  - テストを有効にしたときだけ取り込む。ライブラリの利用者に Naos をダウンロードさせない。
  - 組み込んだ Naos は常に静的ライブラリになる。
  - アサーションを 1 つも実行しないテストは失敗する。
  - テストの実行ファイルはビルド直後に列挙のため一度起動される。動的ライブラリをテストするとき、Windows では DLL が実行ファイルと同じ場所にある必要がある。
- **メモリ**: ライブラリ内のメモリ確保はすべて差し替え可能なアロケータを通す。標準の `malloc` を直接呼ばない。アロケータは `clargs_set_allocator()` でライブラリ全体に 1 つ設定する。利用者のデータ（コンテキスト）は持たない。設定は、ライブラリのオブジェクトが 1 つもなく、ほかのスレッドがライブラリを呼んでいないときに行う。
- **エラー**: 実行時に起こりうる失敗は、戻り値のエラーコードで返す。呼び出し側のプログラムの誤り（必須の引数の `NULL` など）は、エラーコードにせず、関数の前提条件として標準の `assert` で検査する（Debug 構成では中断し、Release 構成では検査しない）。
- **状態**: 変更可能なグローバル状態は、設定されたアロケータだけとする。ほかには持たない。
- **依存**: 実行時の依存は C 標準ライブラリのみ。
- **文書の言語**: スペック文書は日本語で書く。

## Naming (decided)

プロジェクト名は **CLArgs**、ライブラリ名は **clargs** とする。リポジトリ名は `cargs` のままだが、既存の C ライブラリ [likle/cargs](https://github.com/likle/cargs)（`libcargs`、`cargs.h`）と衝突するため、公開名には使わない。

| 対象 | 名前 |
|---|---|
| プロジェクト名（文書、CMake の `project()`） | `CLArgs` |
| ライブラリ名 | `clargs`（`libclargs.a`、`libclargs.so`、`clargs.dll` など） |
| 公開ヘッダ | `<clargs/clargs.h>` |
| 関数と型の接頭辞 | `clargs_` |
| マクロの接頭辞 | `CLARGS_` |
| CMake のパッケージとターゲット | `find_package(CLArgs)`、`CLArgs::clargs` |
| pkg-config | `clargs.pc` |

- 同名の小規模なプロジェクトがある（NuGet の C++ ライブラリ `libclargs`、接頭辞 `clargs_` を使う `santerijps/clargs` など）。主要なパッケージ管理での広い配布は確認していないが、同じ環境に入れると名前がぶつかりうる。
- ヘッダは必ず `clargs/` の下に置き、公開する識別子にはすべて接頭辞を付ける。

## Boundary Strategy

- **Why this split**: 責務を「土台 → 実行時の基盤 → 定義 → 解析 / 表示」の順に積み、依存を一方向にした。各スペックは下の層の公開された契約だけを使い、上の層を知らない。argv-parser と help-formatter はどちらも command-schema だけに依存するので、並行して進められる。
  - build-foundation: ビルド、配布、テスト基盤、CI。ライブラリの機能は持たない。
  - core-runtime: アロケータとエラーモデル。コマンドラインの知識は持たない。
  - command-schema: 「何を受け付けるか」の定義と、その検証。argv は見ない。
  - argv-parser: 「いつ・どう当てはめるか」。スキーマを読むだけで、書き換えない。文面の整形はしない。
  - help-formatter: スキーマから文面を作る。解析はしない。
- **Shared seams to watch**:
  - **公開名と命名規約**: 公開名は上の Naming のとおり。細かな命名規約は build-foundation が決め、全スペックが従う。公開ヘッダの置き方、export マクロの付け方も同じ。
  - **所有権と寿命**: 誰が確保し誰が解放するか。特に、スキーマが文字列をコピーするか借りるか、結果が argv の文字列を借りるかコピーするか、結果がスキーマより長生きしてよいか。core-runtime が規約を決め、command-schema と argv-parser が従う。
  - **スキーマの読み取りインターフェース**: argv-parser と help-formatter は、command-schema が用意する読み取り用のインターフェースだけを使う。互いの内部構造に触れない。スキーマは解析に使う前に検証を済ませる。
  - **値型のインターフェース**: 値型の定義と変換関数は command-schema が持ち、argv-parser はそのインターフェースを通して変換を呼ぶだけにする。組み込みの型と利用者定義の型を同じ経路で扱う。
  - **エラーコード**: 仕組みは core-runtime が持つ。各スペックが自分のコードをどう足すかを core-runtime の設計で決める。
  - **ヘルプの起動**: `--help` を見つけるのは argv-parser（ふつうのフラグとして報告）、文面を作るのは help-formatter、両者をつなぐのは利用者のコード。どちらも相手を呼ばない。
  - **テスト支援**: 失敗を注入できるアロケータなど、共通のテスト補助は core-runtime が用意し、下流のスペックが再利用する。
  - **内部関数のテスト**: 動的ライブラリでは公開関数しか export しないので、内部関数のテストは静的な形でリンクする。公開 API のテストは静的・動的の両方で実行する。この方法は build-foundation が決める。
- **Future boundaries（初版の対象外）**:
  - **環境変数からの値の補完**: 初版では実装しないが、足すときの境界をここに決めておく。新しいスペックは作らず、既存の 3 スペックの拡張とする。
    - command-schema: オプションの属性として「対応する環境変数名」を持つだけ。環境は読まない。
    - argv-parser: 解析を「argv との照合」と「値の解決」の 2 段に分け、解決の段で argv → 環境 → 既定値の順に値を決める。環境は、利用者が差し替えられる参照関数を通して読む（既定は `getenv`）。照合の段は環境を知らない。パーサーの中で `getenv` を直接呼ばない。
    - help-formatter: 環境変数名の表記を足すだけ。
    - 設定ファイルなど別の供給元は、ライブラリが持たず、利用者が同じ参照関数の形で持ち込む。
  - **初版で守ること**: 上の拡張を非破壊で足せるよう、argv-parser は次を守る。参照関数の差し込み口そのものは初版では公開しない（使う機能がないうちに形を決めない）。
    - 「値の解決」の段を、照合の段から分けておく（既定値の適用と必須の検査のために初版でも必要）。
    - 値の出どころは真偽値ではなく列挙型で返す（初版は「コマンドライン」と「既定値」の 2 値）。
    - 解析エラーの位置情報は、argv の位置だけに固定せず、出どころの種類を持てる形にする。

## Phase 2: OSS としての公開（2026-11-13 追加）

### Overview

Phase 1 で実装を終えた CLArgs を、GitHub で OSS として公開し、リリースできる状態にする。第三者が、日本語も `.kiro/` も知らずに、ライブラリを使い、開発に参加できることが目標である。

人が読む文書（README、`docs/`、CONTRIBUTING など）とソースのコメントはすべて英語にする。`.kiro/` は AI 向けの作業領域であり、人は見ない前提とする。人向けの文書は `.kiro/` やスペック駆動の手順に触れない（`.kiro/` の中の文書は日本語のままでよい）。

リリースは半自動にする。メンテナーが手動起動のワークフローにバージョンを渡すとリリース PR ができ、それをマージするとタグと GitHub Release ができる。プレリリース（`-rc.1` など）も同じ流れで出す。バージョンは Semantic Versioning 2.0.0、変更履歴は Keep a Changelog 1.1.0 の形式とする。

### Approach Decision

- **Chosen**: 自前の GitHub Actions による「リリース PR の自動作成 → マージでタグと Release」。
  - 手動起動（`workflow_dispatch`）のワークフローが、渡されたバージョンで `include/clargs/version.h` を書き換え、CHANGELOG の `Unreleased` を新しいバージョンの節に移し、リリース PR を開く。
  - リリース PR のマージを検出したワークフローが、マージコミットに注釈付きタグ `v<version>` を付け、CHANGELOG の該当節から作ったノートで GitHub Release を作る。バージョンにプレリリースの識別子があれば Pre-release にする。
  - Release に載せるのはソースだけである（GitHub が自動で付けるアーカイブとノート）。ビルド済みのバイナリは付けない。
- **Why**:
  - CHANGELOG を人が書く Keep a Changelog の形のまま使える。コントリビューターにコミットメッセージの規約を強いない。
  - バージョンの唯一の出どころである `version.h` を、ワークフローがそのまま扱える。
  - 外部のリリースツールに依存しない。
- **Rejected alternatives**:
  - release-please: Conventional Commits から CHANGELOG を作るので、Keep a Changelog の手書きの形と合わない。プレリリースと `version.h` の更新に追加の設定が要る。
  - PR は手動で作り、タグの push で Release を作る方式: 自動化が少なく、依頼の「PR を作り、マージ後にタグを打つ」半自動に届かない。
  - ビルド済みバイナリの添付: 3 OS × 静的/動的のパッケージの作成と検証が増える。C ライブラリでは FetchContent や find_package で取り込むのがふつうなので、初版では付けない。

### Scope

- **In**:
  - README と `docs/` の英語化と再編（利用者向けと開発者向け）、CONTRIBUTING.md、人向け文書からの `.kiro/` への言及の除去、ソースのコメントの言語の点検
  - SemVer の運用規則（1.0 より前の互換、プレリリースの表し方）、`version.h`・CMake・pkg-config でのプレリリースの扱い
  - CHANGELOG.md（Keep a Changelog）
  - リリース PR を作るワークフロー、マージ後にタグと GitHub Release を作るワークフロー、メンテナー向けのリリース手順書
  - CI の起動タイミングの見直し（PR、main への push、手動起動。文書だけの変更では重いビルドを飛ばす）
  - CODE_OF_CONDUCT.md、SECURITY.md、Issue と PR のテンプレート（直接実装）
- **Out**:
  - リポジトリ名の変更（`cargs` のまま。公開名は Naming のとおり `CLArgs` / `clargs`）
  - vcpkg、Conan などのパッケージ管理への登録
  - API リファレンスのサイト生成（Doxygen、GitHub Pages など）
  - CI のジョブの追加（Sanitizer、clang-format、静的解析、リンク検査など）。今回見直すのは起動タイミングだけ
  - ビルド済みバイナリの配布
  - 実際の最初のリリース（v0.1.0）を出す操作。仕組みができたあとにメンテナーが行う

### Constraints

- **文書の言語**: 人が読むもの（README、`docs/`、CONTRIBUTING、CHANGELOG、CoC、SECURITY、テンプレート、ワークフローとソースのコメント、Release のノート）は英語のみ。`.kiro/` の中と CLAUDE.md は例外。
- **`.kiro/` を前提にしない**: 人向けの文書は `.kiro/`、スペック、Kiro の手順に触れない。コントリビューターはそれらを知らずに開発できる。
- **タグの形式**: `v<SemVer>`（例 `v0.2.0`、`v0.2.0-rc.1`）。両スペックが共有する決定である。
- **バージョンの出どころ**: これまでどおり `include/clargs/version.h` だけ。プレリリースの識別子も同じヘッダに置く（形は release-automation の設計で決める）。CMake の `project(VERSION)` は数字しか受け付けない。
- **GitHub Actions の制約**:
  - `GITHUB_TOKEN` で作った PR や push は、`pull_request` や `push` のワークフローを起動しない。そのため、リリース PR では CI が自動では走らない。`workflow_dispatch` は例外として起動できるので、それを使うか、GitHub App のトークンを使う。どちらにするかは設計で決める。
  - 同じ理由で、`GITHUB_TOKEN` で push したタグは別のワークフローを起動しない。タグ付けと Release の作成は同じワークフローで行う。
  - ワークフローごと `paths-ignore` で飛ばすと、required check が Pending のまま残り、PR がマージできなくなる。文書だけの変更を飛ばすときは、ジョブの側で判定し、チェックは必ず結果を返す。
  - リポジトリの設定（Actions に PR の作成を許す、非公開の脆弱性報告を有効にする、ブランチの保護など）は、メンテナーが手で行う。手順書に書く。
- **秘密情報**: できるだけ追加のシークレットを要さない構成にする。要るときは、その理由と最小の権限を手順書に書く。

### Boundary Strategy

- **Why this split**: 「人が読む文書」と「バージョン・リリース・CI の仕組み」は、触るファイルも検証の方法も違う。
- **実装の順序**: release-automation を先に実装し、english-docs はそのあとに行う。理由: 両スペックが `docs/conventions.md` を編集し（release-automation は「Version notes」の節の内容、english-docs は全体の英語化と「Directory layout」の節）、english-docs の再編と最終確認が release-automation の作るファイル（`CHANGELOG.md`、`docs/releasing.md`、`docs/versioning.md`、`tools/`、`tests/release/`、ワークフロー）の存在を前提にすると、リンクとディレクトリ構成の記述を一度で正しく書けるため。CoC、SECURITY、テンプレートは定型の文書で、境界の設計が要らないので、直接実装に回す。
  - english-docs: README、`docs/`、CONTRIBUTING.md の中身と構成。ソースのコメントの言語の点検。
  - release-automation: `version.h` のプレリリースの扱い、CMake と pkg-config への反映、CHANGELOG.md、リリースと CI のワークフロー、リリース手順書。
- **Shared seams to watch**:
  - **リリース手順書の置き場所とリンク**: 手順書（例 `docs/releasing.md`）と CHANGELOG.md の中身は release-automation が持つ。english-docs は README と CONTRIBUTING からそれらへリンクするだけで、中身を書かない。ファイル名は両スペックの設計で合わせる。
  - **バージョンについての記述**: `docs/conventions.md` の「バージョンについての注意」は、プレリリースの扱いが変わると古くなる。英語化は english-docs、内容の更新は release-automation が持つ。
  - **README の取り込み例**: FetchContent の `GIT_TAG` は、`main` ではなくリリースタグ（`v<SemVer>`）の例にする。タグの形式は上の Constraints のとおり。
  - **CI の required check の名前**: release-automation が決める。CONTRIBUTING で CI に触れる場合は、その名前に合わせる。
  - **変更の記録の規則**: 「利用者に見える変更は PR で CHANGELOG の `Unreleased` に書く」という規則は release-automation が決め、CONTRIBUTING と PR テンプレートがそれを案内する。

## Existing Spec Updates

- なし。build-foundation の CI は、ジョブの中身（3 OS × 静的/動的、consumer check）を変えない。起動タイミングの見直しは release-automation が持つ。

## Direct Implementation Candidates

- [x] community-health-files -- CODE_OF_CONDUCT.md（Contributor Covenant。連絡先は vabelorg@gmail.com）、SECURITY.md（GitHub の非公開の脆弱性報告で受け付ける。対応するバージョンの方針を書く）、Issue テンプレート（バグ報告、機能要望、`config.yml` で脆弱性は SECURITY へ案内）、PR テンプレート。定型の文書で境界の設計が要らないため、スペックにしない。PR テンプレートの CHANGELOG の項目は、release-automation の規則が決まってから書く。すべて英語。

## Specs (dependency order)

- [x] build-foundation -- CMake プロジェクト、静的/動的ビルド、export マクロ、インストールとパッケージ、Naos の導入、3 OS の CI。Dependencies: none
- [x] core-runtime -- 差し替え可能なアロケータ、エラーモデル、所有権の規約、テスト用の失敗注入アロケータ。Dependencies: build-foundation
- [x] command-schema -- コマンド、サブコマンド、オプション、位置引数、値型の定義 API と定義の検証。Dependencies: core-runtime
- [x] argv-parser -- argv の解析エンジン、ワイド文字 argv の受け付け、結果オブジェクトと参照 API、構造化された解析エラー。Dependencies: command-schema
- [x] help-formatter -- スキーマからヘルプと usage 文を生成する。Dependencies: command-schema
- [ ] release-automation -- SemVer の運用とプレリリースの扱い、CHANGELOG.md（Keep a Changelog）、リリース PR の作成とマージ後のタグ・GitHub Release のワークフロー、CI の起動タイミングの見直し、リリース手順書。Dependencies: none
- [ ] english-docs -- README と docs/ の英語化と、利用者向け・開発者向けの再編、CONTRIBUTING.md、人向け文書からの .kiro/ への言及の除去、コメントの言語の点検。Dependencies: release-automation
