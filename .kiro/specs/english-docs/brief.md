# Brief: english-docs

## Problem

CLine を OSS として公開したいが、人が読む文書がすべて日本語である（README.md 約 310 行、`docs/` 5 ファイル 約 3,300 行）。日本語を読まない第三者は、ライブラリを使うことも開発に参加することもできない。また、`docs/ownership.md` が `.kiro/steering/roadmap.md` を参照しているなど、人は見ない前提の `.kiro/` を当てにした記述が混ざっている。

## Current State

- README.md: 概要、完全なサンプルプログラム、ビルドとテスト、CMake オプション、4 つの取り込み方法（FetchContent、add_subdirectory、find_package、pkg-config）、開発への参加、ライセンス。すべて日本語。FetchContent の例は `GIT_TAG main` である。
- `docs/`:
  - `conventions.md`（命名規約、ディレクトリ構成、公開関数・ヘッダ・ソース・テストを足す手順、実装の規則、バージョンについての注意）。開発者向け。
  - `schema.md`、`parse.md`、`help.md`、`ownership.md`。利用者向けの API の説明。
- ソースのコメント（`include/`、`src/`、`tests/`、`cmake/`、`.github/`）は、点検した範囲ではすでに英語である（`tests/public/command_test.c` の罫線文字は日本語ではない）。
- CONTRIBUTING.md はない。README の「開発に参加する」は `docs/conventions.md` を指すだけである。
- 文書のアンカーは日本語の見出しから作られている（例 `docs/conventions.md#バージョンについての注意`）。

## Desired Outcome

- README、`docs/`、CONTRIBUTING.md が英語だけで書かれ、日本語も `.kiro/` も知らない人が、次のことをできる。
  - CLine の目的と特徴を知り、最初のプログラムを動かす。
  - 4 つの方法のどれかでプロジェクトに取り込む。
  - スキーマの定義、解析、ヘルプの生成、所有権と寿命の規約を、API の説明から理解する。
  - リポジトリをビルドしてテストし、規約に沿った変更を PR として出す。
- 利用者向けの文書と開発者向けの文書が分かれ、README から迷わずたどれる。
- 人向けの文書に、`.kiro/`、スペック、Kiro の手順への言及がない。
- 文書の中のリンクとアンカーが、英語の見出しに合わせて切れていない。
- ソースのコメントに日本語が残っていないことを確かめてある。

## Approach

既存の文書を英語に書き直しつつ、利用者向けと開発者向けに構成を整える。内容の正しさは今の実装と文書が保証しているので、新しい API の説明は書き起こさず、翻訳と再編に集中する。

- README: 英語化し、OSS の入口として整える。概要、特徴、最小の例、取り込み方法（FetchContent の例はリリースタグ `v<SemVer>` を使う）、文書へのリンク、コントリビューションへの案内、ライセンス。CI やライセンスなどのバッジを付ける。
- 利用者向けの文書（schema、parse、help、ownership）: 英語化する。置き場所と索引（例 `docs/README.md`）は設計で決める。
- 開発者向けの文書: `conventions.md` を英語化し、CONTRIBUTING.md（開発環境、ビルドとテスト、変更の出し方、PR の流れ、CHANGELOG の書き方への案内、行動規範への案内）から参照する。CONTRIBUTING をどこまで自己完結させるかは設計で決める。
- 最後に、日本語の文字と `.kiro` の文字列が人向けのファイルに残っていないことを機械的に確かめる。

## Scope

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

## Boundary Candidates

- 利用者向けの文書（README、API の説明）
- 開発者向けの文書（CONTRIBUTING、規約と手順）
- 言語の点検（文書とソースのコメントに日本語と `.kiro` が残っていないことの機械的な確認）

## Out of Boundary

- CHANGELOG.md と、リリース・バージョンの規則の中身。english-docs はそれらへリンクするだけである。
- `docs/conventions.md` の「バージョンについての注意」の内容の更新（プレリリースの扱いは release-automation が決める）。english-docs はその節を英語にするだけで、内容は変えない。
- ワークフローの中身と CI の挙動。

## Upstream / Downstream

- **Upstream**: Phase 1 の全スペック（文書が説明する API と、build-foundation が決めたビルド・取り込みの方法）。release-automation（roadmap の「実装の順序」により先に実装する。english-docs の実装の時点で、CHANGELOG.md、`docs/releasing.md`、`docs/versioning.md`、`tools/release/`、`tools/ci/changes.cmake`、`tests/release/`、`release-pr.yml`、`release-publish.yml`、更新された `ci.yml`、日本語のまま更新された `docs/conventions.md` の「バージョンについての注意」が既にある。english-docs はそれらへリンクし、ディレクトリ構成に載せ、その節を英語にするが、release-automation のファイルは変えない）。community-health-files（直接実装済み。CONTRIBUTING から CoC と SECURITY へリンクする）。
- **Downstream**: 最初のリリース v0.1.0 の README。

## Existing Spec Touchpoints

- **Extends**: なし（Phase 1 のスペックが作った `docs/` を書き直すが、スペックの要件は変えない）。
- **Adjacent**: release-automation（`docs/conventions.md` のバージョンの節、README の取り込み例のタグ、リリース手順書のファイル名）、build-foundation（README の取り込み方法と CMake オプションの記述は、build-foundation の挙動に合わせる）。

## Constraints

- 英語のみ。日本語の文書を別に残さない。
- 人向けの文書は `.kiro/`、スペック、Kiro の手順に触れない。
- README の取り込み例のタグは `v<SemVer>` の形式（roadmap の共有の決定）。
- README のサンプルプログラムは、今の API でコンパイルでき、説明どおりに動くこと。
- 翻訳で説明の意味を変えない。今の文書と実装が食い違っていれば、実装を正とする。
- アンカーは GitHub の見出しからの自動生成に合わせる。
