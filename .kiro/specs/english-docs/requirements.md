# Requirements Document

## Project Description (Input)
### Problem

CLArgs を OSS として公開したいが、人が読む文書がすべて日本語である（README.md 約 310 行、`docs/` 5 ファイル 約 3,300 行）。日本語を読まない第三者は、ライブラリを使うことも開発に参加することもできない。また、`docs/ownership.md` が `.kiro/steering/roadmap.md` を参照しているなど、人は見ない前提の `.kiro/` を当てにした記述が混ざっている。

### Current State

- README.md: 概要、完全なサンプルプログラム、ビルドとテスト、CMake オプション、4 つの取り込み方法（FetchContent、add_subdirectory、find_package、pkg-config）、開発への参加、ライセンス。すべて日本語。FetchContent の例は `GIT_TAG main` である。
- `docs/`:
  - `conventions.md`（命名規約、ディレクトリ構成、公開関数・ヘッダ・ソース・テストを足す手順、実装の規則、バージョンについての注意）。開発者向け。
  - `schema.md`、`parse.md`、`help.md`、`ownership.md`。利用者向けの API の説明。
- ソースのコメント（`include/`、`src/`、`tests/`、`cmake/`、`.github/`）は、点検した範囲ではすでに英語である（`tests/public/command_test.c` の罫線文字は日本語ではない）。
- CONTRIBUTING.md はない。README の「開発に参加する」は `docs/conventions.md` を指すだけである。
- 文書のアンカーは日本語の見出しから作られている（例 `docs/conventions.md#バージョンについての注意`）。

### Desired Outcome

- README、`docs/`、CONTRIBUTING.md が英語だけで書かれ、日本語も `.kiro/` も知らない人が、次のことをできる。
  - CLArgs の目的と特徴を知り、最初のプログラムを動かす。
  - 4 つの方法のどれかでプロジェクトに取り込む。
  - スキーマの定義、解析、ヘルプの生成、所有権と寿命の規約を、API の説明から理解する。
  - リポジトリをビルドしてテストし、規約に沿った変更を PR として出す。
- 利用者向けの文書と開発者向けの文書が分かれ、README から迷わずたどれる。
- 人向けの文書に、`.kiro/`、スペック、Kiro の手順への言及がない。
- 文書の中のリンクとアンカーが、英語の見出しに合わせて切れていない。
- ソースのコメントに日本語が残っていないことを確かめてある。

### Approach

既存の文書を英語に書き直しつつ、利用者向けと開発者向けに構成を整える。内容の正しさは今の実装と文書が保証しているので、新しい API の説明は書き起こさず、翻訳と再編に集中する。

- README: 英語化し、OSS の入口として整える。概要、特徴、最小の例、取り込み方法（FetchContent の例はリリースタグ `v<SemVer>` を使う）、文書へのリンク、コントリビューションへの案内、ライセンス。CI やライセンスなどのバッジを付ける。
- 利用者向けの文書（schema、parse、help、ownership）: 英語化する。置き場所と索引（例 `docs/README.md`）は設計で決める。
- 開発者向けの文書: `conventions.md` を英語化し、CONTRIBUTING.md（開発環境、ビルドとテスト、変更の出し方、PR の流れ、CHANGELOG の書き方への案内、行動規範への案内）から参照する。CONTRIBUTING をどこまで自己完結させるかは設計で決める。
- 最後に、日本語の文字と `.kiro` の文字列が人向けのファイルに残っていないことを機械的に確かめる。

### Scope

- **In**:
  - README.md の英語化と再構成
  - `docs/*.md` の英語化と、利用者向け・開発者向けへの再編（ファイル名の変更や索引の追加を含む）
  - CONTRIBUTING.md の新規作成
  - 人向けの文書から `.kiro/` やスペックの手順への言及を除くこと
  - 文書の中のリンクとアンカーの更新
  - `include/`、`src/`、`tests/`、`cmake/`、`.github/` のコメントと文字列の言語の点検（日本語があれば英語にする）
- **Out**:
  - API や挙動の変更（文書の誤りを見つけたら、文書を実装に合わせる。実装は変えない）
  - CHANGELOG.md、リリース手順書、バージョンの規則の中身（release-automation が持つ）
  - CODE_OF_CONDUCT.md、SECURITY.md、Issue/PR テンプレート（直接実装の community-health-files）
  - API リファレンスのサイト生成
  - `.kiro/` の中の文書と CLAUDE.md（日本語のままでよい）

### Boundary Candidates

- 利用者向けの文書（README、API の説明）
- 開発者向けの文書（CONTRIBUTING、規約と手順）
- 言語の点検（文書とソースのコメントに日本語と `.kiro` が残っていないことの機械的な確認）

### Out of Boundary

- CHANGELOG.md と、リリース・バージョンの規則の中身。english-docs はそれらへリンクするだけである。
- `docs/conventions.md` の「バージョンについての注意」の内容の更新（プレリリースの扱いは release-automation が決める）。english-docs はその節を英語にするだけで、内容は変えない。
- ワークフローの中身と CI の挙動。

### Upstream / Downstream

- **Upstream**: Phase 1 の全スペック（文書が説明する API と、build-foundation が決めたビルド・取り込みの方法）。
- **Downstream**: community-health-files（CONTRIBUTING から CoC と SECURITY へリンクする）、release-automation（README と CONTRIBUTING からリリース手順書と CHANGELOG へリンクする）。最初のリリース v0.1.0 の README。

### Existing Spec Touchpoints

- **Extends**: なし（Phase 1 のスペックが作った `docs/` を書き直すが、スペックの要件は変えない）。
- **Adjacent**: release-automation（`docs/conventions.md` のバージョンの節、README の取り込み例のタグ、リリース手順書のファイル名）、build-foundation（README の取り込み方法と CMake オプションの記述は、build-foundation の挙動に合わせる）。

### Constraints

- 英語のみ。日本語の文書を別に残さない。
- 人向けの文書は `.kiro/`、スペック、Kiro の手順に触れない。
- README の取り込み例のタグは `v<SemVer>` の形式（roadmap の共有の決定）。
- README のサンプルプログラムは、今の API でコンパイルでき、説明どおりに動くこと。
- 翻訳で説明の意味を変えない。今の文書と実装が食い違っていれば、実装を正とする。
- アンカーは GitHub の見出しからの自動生成に合わせる。

## Introduction

このスペックは、CLArgs（ライブラリ名 `clargs`）を OSS として公開するために、人が読む文書を英語だけにし、利用者向けと開発者向けに整理する作業を定める。対象は、README.md、`docs/` の文書、新しく作る CONTRIBUTING.md、それらの中のリンクとアンカー、ソースのコメントと文字列の言語の点検である。このスペックは、ライブラリの API と挙動を変えず、新しい API の説明を書き起こさない。今ある説明を英語に直し、置き場所と導線を整える。

読む人は 3 種類いる。

- **ライブラリ利用者**: CLArgs を自分の C のプログラムに取り込む開発者。日本語を読まず、`.kiro/` も知らない。
- **コントリビューター**: CLArgs のリポジトリをビルドしてテストし、変更を PR として出す人。日本語を読まず、`.kiro/` も知らない。
- **メンテナー**: PR をレビューし、リリースを出す人。リリースの手順そのものは release-automation の文書を読む。

この文書では、次の用語を使う。

- **人向けの文書**: README.md、`docs/` の下の Markdown の文書、CONTRIBUTING.md。
- **人向けのファイル**: リポジトリで管理しているファイルのうち、`.kiro/` の下のもの、`.claude/` の下のもの、CLAUDE.md を除いたすべて。人向けの文書、ソース、テスト、CMake のファイル、`.github/` の下のファイル、CODE_OF_CONDUCT.md、SECURITY.md などを含む。`.kiro/`、`.claude/`、CLAUDE.md は AI 向けの作業領域であり、日本語のままでよい。
- **日本語の文字**: ひらがな、カタカナ、漢字（CJK 統合漢字）、CJK の記号と句読点（`、`、`。`、`「」` など）、全角の英数字と記号。罫線の文字（`├──` など）や `→` のような、日本語でない ASCII 外の文字は含まない。
- **AI 向けの作業領域への言及**: `.kiro` という文字列、スペック（requirements、design、tasks などの仕様書や、build-foundation などのスペックの名前）、ステアリング、Kiro の手順についての記述。
- **利用者向けの API の説明**: 今の `docs/schema.md`、`docs/parse.md`、`docs/help.md`、`docs/ownership.md` が説明している内容（スキーマの定義と検証、解析と結果、ヘルプの生成、所有権と寿命の規約）。
- **開発者向けの規約**: 今の `docs/conventions.md` が説明している内容（命名規約、ディレクトリ構成、手順、実装の規則、テストの注意、確かめ方、バージョンについての注意）。
- **release-automation の文書**: release-automation が作る CHANGELOG.md、リリース手順書（`docs/releasing.md`）、バージョン方針の文書（`docs/versioning.md`）。

背景と判断の経緯は、同じディレクトリの `brief.md` と、`.kiro/steering/roadmap.md` の「Phase 2: OSS としての公開」を参照。

## Boundary Context

- **In scope**:
  - README.md の英語化と、OSS の入口としての再構成（概要、特徴、バッジ、最小の例、ビルドとテスト、CMake のオプション、4 つの取り込み方法、文書への案内、コントリビューションへの案内、ライセンス）。
  - `docs/` の文書の英語化と、利用者向け・開発者向けへの再編（ファイル名の変更、索引の追加、文書間での節の移動を含む）。
  - CONTRIBUTING.md の新規作成（リポジトリのルート）。
  - 人向けの文書から、AI 向けの作業領域への言及を除くこと。
  - 人向けの文書の中のリンクとアンカーを、英語の見出しに合わせて直すこと。ソースのコメントが文書の節を指していれば、それも切れないようにすること。
  - 人向けのファイルの、日本語の文字と `.kiro` の文字列の点検。ソースのコメントと文字列に日本語があれば英語にすること。
- **Out of scope**:
  - ライブラリの API と挙動の変更。文書と実装が食い違っていれば、文書を実装に合わせる。
  - CHANGELOG.md、リリース手順書、バージョン方針の文書、SemVer とプレリリースの規則の中身（release-automation が持つ）。
  - `docs/conventions.md` の「バージョンについての注意」の内容の更新（release-automation が持つ）。このスペックは、その節を意味を変えずに英語にするだけである。
  - CODE_OF_CONDUCT.md、SECURITY.md、Issue と PR のテンプレート（直接実装済み）。このスペックはそれらを変更しない。
  - ワークフローの中身と CI の挙動、CI へのジョブの追加（リンク検査などを含む）。
  - API リファレンスのサイト生成。
  - `.kiro/`、`.claude/`、CLAUDE.md の中の文書。
- **Adjacent expectations**:
  - release-automation は、CHANGELOG.md をリポジトリのルートに、リリース手順書（`docs/releasing.md`）とバージョン方針の文書（`docs/versioning.md`）を `docs/` の下に置く。ファイル名は両スペックの設計で合わせたものである。README と CONTRIBUTING と `docs/` の索引は、それらへリンクするだけである。
  - リリースのタグは `v<SemVer>` の形式である（例 `v0.1.0`、`v0.2.0-rc.1`）。
  - PR テンプレートは、ルートの CONTRIBUTING.md と CHANGELOG.md を `https://github.com/ktdevx/cargs/blob/main/...` の URL で指している。
  - CI のワークフローのファイル（`.github/workflows/ci.yml`）は、README の 3 つのコマンド（`cmake -S . -B build`、`cmake --build build`、`ctest --test-dir build`）をそのまま実行している。
  - 「利用者に見える変更は PR で CHANGELOG の `Unreleased` に書く」という規則は release-automation が決める。CONTRIBUTING はそれを案内する。

## Requirements

### Requirement 1: 英語だけの人向けの文書

**Objective:** As a 日本語を読まないライブラリ利用者またはコントリビューター, I want 人向けの文書がすべて英語で書かれていること, so that 翻訳の手段なしに CLArgs を使い、開発に参加できる

#### Acceptance Criteria

1. The 人向けの文書 shall 本文、見出し、表、リンクの文字列、コードブロックの中のコメントと文字列を含めて、英語だけで書かれている。
2. The 人向けの文書 shall 日本語の文字を 1 文字も含まない。
3. The リポジトリ shall 人向けの文書の日本語版を、別のファイルとしても、同じファイルの中の節としても持たない。
4. The 人向けの文書 shall 今の日本語の文書が説明している内容を、意味を変えずに、欠けることなく説明する。ただし、要件 2 で除く記述は除く。要件 8.2 で開発者向けの規約へ移す記述は、移した先で説明する。
5. If 今の文書の説明と、今の実装（公開ヘッダのコメント、ソース、テスト、ビルドの定義）が食い違っていれば, then the 人向けの文書 shall 実装に合わせた説明を載せ、実装は変更されない。

### Requirement 2: AI 向けの作業領域への言及の除去

**Objective:** As a `.kiro/` を知らないコントリビューター, I want 人向けの文書が `.kiro/` やスペックの手順を前提にしないこと, so that 文書だけを読んで開発に参加できる

#### Acceptance Criteria

1. The 人向けのファイル shall `.kiro` という文字列を、大文字と小文字を区別せずに数えて、1 つも含まない。
2. The 人向けの文書 shall スペック、スペックの名前（build-foundation、core-runtime、command-schema、argv-parser、help-formatter、english-docs、release-automation）、ステアリング、Kiro の手順に触れない。
3. When 今の文書の記述が AI 向けの作業領域を参照して規則や判断の根拠を示していれば, the 人向けの文書 shall その参照を除き、規則そのものを、参照なしで読める形で述べる。
4. The 人向けの文書 shall コントリビューターに、AI 向けの作業領域にあるファイルの作成、更新、参照を求めない。

### Requirement 3: OSS の入口としての README

**Objective:** As a 初めて CLArgs を知ったライブラリ利用者, I want README だけで CLArgs の目的と特徴を知り、最初のプログラムを動かせること, so that 使うかどうかを短い時間で判断し、すぐに試せる

#### Acceptance Criteria

1. The README shall 先頭に、プロジェクト名 CLArgs、1 段落の概要、特徴の一覧を載せる。特徴の一覧は、今の README が挙げている特徴（C99 と依存の少なさ、対応する OS とコンパイラ、静的と動的、4 つの取り込み方法、定義できる要素と値型、ヘルプの生成、出力も終了もしないこと）をすべて含む。
2. The README shall 先頭の近くに、CI の結果のバッジとライセンスのバッジを載せる。
3. The README shall 完全なサンプルプログラム（スキーマの定義と検証、解析、`--help` の扱い、値の読み取り、破棄）と、その実行例を載せる。
4. When README のサンプルプログラムを取り出し、今の CLArgs にリンクしてビルドしたとき, the サンプルプログラム shall 警告をエラーにした構成でもコンパイルでき、README に載せたコマンドラインを与えると、README に載せた出力と終了の仕方をする。
5. The README shall リポジトリをビルドしてテストする手順として、`cmake -S . -B build`、`cmake --build build`、`ctest --test-dir build` の 3 つのコマンドを、この順に、引数を変えずに載せる。
6. The README shall テストを有効にした構成が Naos を取得するためにネットワークを要すること、ネットワークなしで構成する方法、テストを無効にする方法、複数の構成を持つジェネレータでの構成の指定を説明する。
7. The README shall CMake のオプション（`CLARGS_BUILD_TESTS`、`CLARGS_INSTALL`、`CLARGS_WARNINGS_AS_ERRORS`）と、直接ビルドするときと組み込まれたときの既定値、`BUILD_SHARED_LIBS` による静的と動的の選択を説明する。
8. The README shall 文書の一覧（利用者向けの API の説明、開発者向けの文書）へのリンク、コントリビューションへの案内（CONTRIBUTING.md へのリンク）、ライセンス（MIT と LICENSE へのリンク）を載せる。

### Requirement 4: 取り込み方法

**Objective:** As a ライブラリ利用者, I want 4 つの方法のどれかで CLArgs を自分のプロジェクトに取り込む手順を英語で読めること, so that 自分のビルドに合う方法を選び、そのまま書き写して使える

#### Acceptance Criteria

1. The README shall FetchContent、add_subdirectory、find_package、pkg-config の 4 つの取り込み方法を、それぞれ、利用側の `CMakeLists.txt` の全体またはコマンドの例とともに説明する。
2. The README shall FetchContent の例の `GIT_TAG` に、ブランチ名ではなく、`v<SemVer>` の形式のリリースタグ（例 `v0.1.0`）を書く。
3. The README shall FetchContent の例の近くで、最新のリリースのタグに置き換えること、再現できるビルドのためにタグまたはコミットのハッシュに固定することを案内し、リリースの一覧への導線を示す。
4. The README shall 今の README が取り込み方法について説明している注意（手元のチェックアウトの使い方、プロジェクトの外のソースを指すときの 2 つ目の引数、インストールの手順、標準でない場所へのインストール、インストール先の移動、`find_package` のバージョン指定とメジャー 0 の間の互換判定、`PKG_CONFIG_PATH` と実行時のライブラリの探索、組み込んだ CLArgs のテストの実行と `enable_testing()`）を、意味を変えずに説明する。
5. The README の取り込み方法の説明 shall 今のビルドの定義（CMake のパッケージ名 `CLArgs`、ターゲット `CLArgs::clargs`、pkg-config のモジュール名 `clargs`、動的ライブラリのときの `CLARGS_SHARED`）と一致する。

### Requirement 5: 利用者向けの API の説明

**Objective:** As a ライブラリ利用者, I want スキーマの定義、解析、ヘルプの生成、所有権と寿命の規約を英語の文書で理解できること, so that 公開ヘッダを読み込む前に、正しい使い方と守るべき規則を知れる

#### Acceptance Criteria

1. The 利用者向けの API の説明 shall スキーマの定義と検証、解析と結果、ヘルプと usage 行の生成、所有権と寿命の規約のそれぞれを、1 つの文書として英語で説明する。
2. The 利用者向けの API の説明 shall 今の文書が持つ一連の例、表、規則の一覧、前提条件の一覧、誤りとエラーの一覧を、すべて英語で持つ。
3. The 利用者向けの API の説明 shall 関数名、型名、マクロ名、エラーコード、オプションの書き方、ライブラリが出力する文面（ヘルプの文面、エラーメッセージ）を、実装と 1 文字も違えずに載せる。
4. When 利用者向けの API の説明に載せた C の例のうち、完全なプログラムとして示したものを取り出し、今の CLArgs にリンクしてビルドしたとき, the 例 shall コンパイルでき、文書が述べる出力と終了コードのとおりに動く。
5. When 利用者向けの API の説明に、完全なプログラムでない C の断片を載せるとき, the 断片 shall 今の公開ヘッダの宣言と一致する名前と引数を使う。

### Requirement 6: 文書の構成と導線

**Objective:** As a ライブラリ利用者またはコントリビューター, I want 利用者向けの文書と開発者向けの文書が分かれ、README から迷わずたどれること, so that 自分に必要な文書だけを読める

#### Acceptance Criteria

1. The 人向けの文書 shall 利用者向けの文書（API の説明）と、開発者向けの文書（コントリビューションの案内と、開発者向けの規約）を、文書の単位で分ける。1 つの文書に、利用者が読む規則と、ライブラリのソースを書く人だけが読む規則を混ぜない。
2. The `docs/` shall 利用者向けの文書と開発者向けの文書を、それぞれの読者と内容の 1 行の説明とともに一覧にした索引の文書を持つ。
3. When 読者が README から始めたとき, the 人向けの文書 shall どの利用者向けの文書にも、どの開発者向けの文書にも、2 回以内のリンクの移動でたどり着けるようにする。
4. The 各々の人向けの文書 shall 冒頭で、その文書が扱うことと扱わないこと、関係する文書へのリンクを示す。
5. Where 文書が複数の節を持つとき, the 文書 shall 冒頭に、節へのリンクの目次を持つ。

### Requirement 7: CONTRIBUTING.md

**Objective:** As a 初めてのコントリビューター, I want リポジトリのルートの CONTRIBUTING.md から、環境の準備、ビルドとテスト、規約、変更の出し方を知れること, so that 規約に沿った PR を、ほかの人に聞かずに出せる

#### Acceptance Criteria

1. The リポジトリ shall CONTRIBUTING.md をルートに置き、PR テンプレートの `https://github.com/ktdevx/cargs/blob/main/CONTRIBUTING.md` が、それを指すようにする。
2. The CONTRIBUTING.md shall 開発に必要なもの（CMake 3.21 以上、C99 のコンパイラ、テストのための Naos の取得とネットワーク）と、対応する環境（Linux の GCC と Clang、macOS の Clang、Windows の MSVC）を説明する。
3. The CONTRIBUTING.md shall 変更を確かめる手順として、静的と動的の両方の構成でのビルドとテストのコマンドを載せ、詳しい確かめ方（確保関数の検査、export の確認、Release 構成での確認）への導線を示す。
4. The CONTRIBUTING.md shall 変更の出し方（Issue での相談、ブランチと PR、PR テンプレートの記入、CI の合格、レビュー）を説明する。
5. The CONTRIBUTING.md shall 開発者向けの規約の文書へのリンクを持ち、変更する前にそれを読むよう案内する。
6. The CONTRIBUTING.md shall 利用者に見える変更を CHANGELOG.md の `Unreleased` に記録するよう案内し、CHANGELOG.md へリンクする。記録の書き方の規則そのものは、release-automation の文書に任せ、CONTRIBUTING.md の中で別に定めない。
7. The CONTRIBUTING.md shall 文書とコードのコメントを英語で書くという決まりを示す。
8. The CONTRIBUTING.md shall CODE_OF_CONDUCT.md へリンクし、参加者がそれに従うことを示す。
9. The CONTRIBUTING.md shall 脆弱性を公開の Issue で報告せず、SECURITY.md の手順に従うよう案内し、SECURITY.md へリンクする。
10. The CONTRIBUTING.md shall メンテナー向けに、リリースの手順は release-automation のリリース手順書にあることを示し、それへリンクする。リリースの手順の中身は書かない。

### Requirement 8: 開発者向けの規約

**Objective:** As a コントリビューター, I want 命名規約、ディレクトリ構成、手順、実装の規則、テストの注意を英語で読めること, so that 独自に名前や置き場所を決めずに、既存のコードと揃った変更を書ける

#### Acceptance Criteria

1. The 開発者向けの規約 shall 今の `docs/conventions.md` の内容（命名規約、ディレクトリ構成、手順、実装の規則、テストを書くときの注意、確かめ方、バージョンについての注意）を、すべて英語で説明する。
2. The 開発者向けの規約 shall 今の `docs/ownership.md` のうち、ライブラリのソースを書く人だけに向けた記述（内部ユーティリティの従い方、開発者の側の書き方の例、開発者が作ってはならない関数の形、`src/` に変更可能な静的変数を足さないこと）を持つ。利用者向けの所有権と寿命の規約の文書は、それらを持たない。
3. The 開発者向けの規約 shall ディレクトリ構成の説明の中の `docs/` の一覧を、再編後のファイルと一致させる。
4. The 開発者向けの規約の「バージョンについての注意」にあたる節 shall 英語にする時点でその節が持つ内容を、意味を変えず、足さず、減らさずに英語で説明する。
5. If 英語にする時点で、release-automation がバージョンについての注意の内容を更新していれば, then the 開発者向けの規約 shall 更新後の内容を英語にする。
6. The 開発者向けの規約の C、CMake、シェルのコード例 shall 英語にする前と同じ意味のコードであり、コメントと仮の値の表記だけを英語にする。

### Requirement 9: リンクとアンカー

**Objective:** As a 文書を読む人, I want 文書の中のリンクとアンカーが切れていないこと, so that 目次や相互参照から目的の節に確実に移れる

#### Acceptance Criteria

1. The 人向けの文書の、リポジトリの中のファイルを指す相対リンク shall すべて、存在するファイルを指す。ただし、要件 9.5 の場合を除く。
2. The 人向けの文書の、節を指すアンカー shall すべて、指す先の文書の英語の見出しから GitHub が自動で作るアンカーと一致する。
3. If 文書のファイル名を変えたとき, then the 人向けのファイル shall 古いファイル名を指すリンクとコメント（ソースとテストのコメントを含む）を、1 つも残さない。
4. When ソースまたはテストのコメントが、文書の節を名前で指しているとき, the 指された文書 shall その名前で見つかる英語の見出しを持つ。
5. If release-automation の文書（CHANGELOG.md、リリース手順書、バージョン方針の文書）が、人向けの文書を英語にする時点でまだリポジトリにないとき, then the 人向けの文書 shall 両スペックで合わせたパスへのリンクを持ち、そのリンクがまだ解決しないことを、実装の結果として報告する。
6. The README から開発者向けの規約の「バージョンについての注意」にあたる節へのリンク shall 英語にした見出しのアンカーを指す。

### Requirement 10: ソースのコメントと文字列の言語

**Objective:** As a コントリビューター, I want ソース、テスト、ビルドとワークフローのファイルのコメントと文字列が英語であること, so that コードを読むのに日本語を要しない

#### Acceptance Criteria

1. The `include/`、`src/`、`tests/`、`cmake/`、`.github/` の下のファイルと、ルートの CMakeLists.txt shall コメントと文字列に、日本語の文字を含まない。
2. If 点検でそれらのファイルに日本語の文字が見つかれば, then the 該当するコメントと文字列 shall 意味を変えずに英語にされ、コードの挙動とテストの期待値は変わらない。
3. The 点検 shall 日本語でない ASCII 外の文字（罫線の文字など）を、日本語の文字として扱わない。

### Requirement 11: 機械的な確認

**Objective:** As a このスペックの完了を判定するレビュアー, I want 人向けのファイルに日本語の文字と `.kiro` の文字列が残っていないことと、リンクが切れていないことを機械的に確かめられること, so that 目視に頼らず、完了を判定できる

#### Acceptance Criteria

1. When 確認を実行したとき, the 確認 shall リポジトリで管理しているすべての人向けのファイルを対象に、日本語の文字を含む行と、`.kiro` の文字列（大文字と小文字を区別しない）を含む行を、ファイル名と行番号とともに一覧にする。
2. The 確認 shall `.kiro/`、`.claude/`、CLAUDE.md を対象から除き、それ以外のファイルを除かない。
3. When 英語化を終えた状態で確認を実行したとき, the 確認 shall 該当する行を 1 つも報告しない。
4. The 確認 shall 人向けの文書の、相対リンクのファイルと節のアンカーが解決するかを調べ、解決しないものを一覧にする。
5. The 確認の手順 shall このスペックの検証の記録として残され、同じ手順を繰り返し実行して同じ結果を得られる。

### Requirement 12: 変更の範囲

**Objective:** As a メンテナー, I want 文書の英語化がライブラリの挙動とほかの成果物を変えないこと, so that 文書の変更だけを安心してレビューし、マージできる

#### Acceptance Criteria

1. The このスペックの変更 shall 公開ヘッダの宣言、ソースのコード、テストのコード、ビルドの定義、ワークフローの定義を変えない。変えてよいのは、要件 10 によるコメントと文字列だけである。
2. The このスペックの変更 shall CHANGELOG.md、リリース手順書、バージョン方針の文書、CODE_OF_CONDUCT.md、SECURITY.md、Issue と PR のテンプレートの中身を変えない。
3. When このスペックの変更を適用した後に README の 3 つのコマンドを実行したとき, the リポジトリ shall 今までどおり、構成、ビルド、テストに成功する。
