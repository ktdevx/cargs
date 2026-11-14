# Research & Design Decisions

## Summary
- **Feature**: `release-automation`
- **Discovery Scope**: Extension（build-foundation のバージョンの仕組みと CI を拡張し、リリースのワークフローを新設する。GitHub Actions の制約の確認を含むため、light discovery に外部の確認を足した）
- **Key Findings**:
  - バージョンの読み取り（`cmake/ClineVersion.cmake`）は既に `cmake -P` からも呼べる作りで、ヘッダの正規の書式の検査を持つ。リリースの処理を CMake のスクリプトで書けば、この読み取りをそのまま再利用でき、テストを CTest に登録するだけで今ある CI のジョブの中で走る（新しいジョブが要らない）。
  - consumer check（`tests/consumer/CMakeLists.txt`）は、パッケージが知らせるバージョンを `^[0-9]+\.[0-9]+\.[0-9]+$` で検査している。pkg-config の `Version` にプレリリースを含めると、プレリリースのリリース PR の CI でこの検査が落ちる。consumer check の版の比較を、プレリリースの識別子も比べる形に広げる必要がある。
  - `GITHUB_TOKEN` で作った PR では `pull_request` の CI が起動しないが、`workflow_dispatch` は `GITHUB_TOKEN` から起動できる。dispatch した実行のチェックは、ブランチの先頭のコミット（＝リリース PR の head）に付く。これが required check を満たすことは公式文書で明言されていないので、通し確認で確かめ、だめな場合の戻し方（PR を閉じて開き直す、CI を手で起動する）を手順書に書く。

## Research Log

### 既存のバージョンの仕組み
- **Context**: プレリリースをどこに持つか。
- **Sources Consulted**: `include/cline/version.h`、`cmake/ClineVersion.cmake`、`CMakeLists.txt`、`src/CMakeLists.txt`、`cmake/ClineInstall.cmake`、`cmake/cline.pc.in`、`cmake/CLineConfig.cmake.in`、`tests/consumer/*`、`tests/public/version_test.c`
- **Findings**:
  - `cline_read_version(<header> <out-var>)` は 3 つのマクロを 1 行ずつ読み、コメントを除いてから正規表現で照合する。引数が 2 つでないと失敗する。
  - `project(VERSION)`、SOVERSION（`src/CMakeLists.txt`）、`write_basic_package_version_file`（`SameMinorVersion`/`SameMajorVersion`）、pkg-config の `Version`（`@PROJECT_VERSION@`）は、すべて `PROJECT_VERSION` から来る。
  - `CLineConfig.cmake.in` は `configure_package_config_file` で作られ、`@VAR@` の置換が使える。
  - consumer check の `main.c` は、パッケージのバージョンを `CONSUMER_PACKAGE_VERSION_MAJOR/MINOR/PATCH` で受け取り、ヘッダのマクロと比べる。
- **Implications**: 数字の部分（`PROJECT_VERSION`）はそのまま残し、完全な文字列を別の変数で pkg-config と CMake のパッケージに渡す。互換判定と SOVERSION は変えない。consumer check にプレリリースの比較を足す。

### GitHub Actions の起動の制約
- **Context**: リリース PR の CI と、マージ後の公開をどう起動するか。
- **Sources Consulted**: GitHub Docs「Triggering a workflow」（`GITHUB_TOKEN` によるイベントは新しい実行を起動しない。例外は `workflow_dispatch` と `repository_dispatch`）、GitHub Changelog 2022-09（`GITHUB_TOKEN` で `workflow_dispatch` を起動できる）、peter-evans/create-pull-request の `docs/concepts-guidelines.md`（`GITHUB_TOKEN` の PR では CI が走らない。回避策として PR を閉じて開き直す、PAT、GitHub App）
- **Findings**:
  - `gh workflow run ci.yml --ref release/v<V>` は `actions: write` で起動でき、実行はそのブランチの先頭のコミットに付く。
  - リリース PR を人がマージしたときの `pull_request`（`closed`）イベントは、人の操作なので公開のワークフローを起動する。
  - `GITHUB_TOKEN` で push したタグは別のワークフローを起動しない。タグと Release は同じジョブで作る。
  - ワークフローの `paths-ignore` で飛ばすと required check が Pending のまま残る。
  - 一部の情報源は、`GITHUB_TOKEN` で作った PR の `pull_request` の実行が承認待ちになる場合があると報告している。いずれにせよ、`pull_request` の CI には頼らない。
- **Implications**: 追加のシークレットなしで、リリース PR の CI を `workflow_dispatch` で起動する。チェックが PR に示されない場合の戻し方を手順書に書き、通し確認の項目にする。

### 文書だけの変更の判定
- **Context**: required check を Pending にせずに重いジョブを飛ばす。
- **Findings**: 判定ジョブ → 条件付きの重いジョブ → `if: always()` の集約ジョブ、の 3 段が定石。集約ジョブは `needs.<job>.result` を見て、`skipped` を「文書だけのときだけ」成功とみなす。サードパーティの paths-filter のアクションも使えるが、判定の規則が数行なので自前のスクリプトにし、CTest でテストする。
- **Implications**: 判定の規則（パターン）を 1 つのスクリプトに置き、判定できないときは「文書だけでない」とする。

### pkg-config のバージョン比較
- **Context**: pkg-config の `Version` に `-rc.1` を含めたときの比較。
- **Findings**: pkg-config（freedesktop）と pkgconf は rpm 流の比較で、`0.2.0-rc.1` を `0.2.0` より新しいとみなす（SemVer では古い）。
- **Implications**: `--atleast-version` でプレリリースを除外できないことを、バージョン方針の文書の注意に書く。プレリリースは試験用で支援しないので、実害は小さい。

## Architecture Pattern Evaluation

| Option | Description | Strengths | Risks / Limitations | Notes |
|--------|-------------|-----------|---------------------|-------|
| CMake スクリプト（`cmake -P`）＋ `gh` CLI | 検証と書き換えを CMake スクリプトに、GitHub の操作をワークフローの `gh`/`git` に分ける | `ClineVersion.cmake` を再利用。テストが CTest で 3 OS の既存ジョブの中で走る。開発者は CMake だけで手元で試せる | CMake の文字列処理は冗長 | 採用 |
| Python スクリプト | 同じ分け方を Python で書く | 文字列処理が楽 | テストを走らせるには CI のジョブの中身を変えるか新しいジョブが要る（どちらも範囲外）。バージョンの読み取りを二重に持つ | 不採用 |
| release-please などの既製ツール | Conventional Commits から CHANGELOG を作る | 実装が少ない | roadmap で不採用と決定済み（Keep a Changelog の手書きと合わない） | 不採用 |
| GitHub App のトークンで PR を作る | App のトークンなら `pull_request` の CI が起動する | CI が通常の PR と同じに走る | シークレットと App の登録が要る | 不採用（手順書に代替として触れない。追加のシークレットを要さない方針） |

## Design Decisions

### Decision: リリース PR の CI は `workflow_dispatch` で起動する
- **Context**: 4.5。`GITHUB_TOKEN` の PR では `pull_request` の CI が起動しない。
- **Alternatives Considered**:
  1. GitHub App のトークン — シークレットと App の管理が増える。
  2. PAT — 個人に紐づき、権限が広い。
  3. `gh workflow run ci.yml --ref release/v<V>` — `actions: write` だけで済む。
- **Selected Approach**: 3。CI に `workflow_dispatch` を足し（7.1 で必要）、リリース PR のワークフローが PR を開いた直後に起動する。
- **Rationale**: 追加のシークレットが要らない。チェックは同じ名前（集約チェック `CI result`）で head のコミットに付く。
- **Trade-offs**: dispatch の実行は PR の「マージ後の姿」（merge ref）ではなくブランチの先頭を試す。リリース PR は `main` から作ったばかりなので差はない。メンテナーがリリース PR のブランチに push すれば、人の push なので `pull_request` の CI が通常どおり走る。
- **Follow-up**: dispatch のチェックが required check を満たすことを通し確認で確かめる。満たさない場合、手順書の戻し方（PR を閉じて開き直す）で運用する。

### Decision: リリースの処理は CMake スクリプトで書き、CTest でテストする
- **Context**: 9.1〜9.4、7.8、Out of scope（CI のジョブの追加）。
- **Selected Approach**: `tools/release/` にモジュール（SemVer、CHANGELOG、ヘッダの書き換え）と入口のスクリプト（`prepare.cmake`、`publish.cmake`）、`tools/ci/changes.cmake` を置く。テストは `tests/release/` の `cmake -P` スクリプトで、`tests/CMakeLists.txt` に `release` ラベルで登録する。
- **Rationale**: 既存のジョブの `ctest` がそのまま拾う。Windows・macOS・Linux のすべてで走る。
- **Trade-offs**: テストを有効にした構成（Naos の取得を伴う）でしか CTest に載らない。スクリプトは直接 `cmake -P` でも走らせられるので、手元の確認には支障がない。

### Decision: プレリリースは文字列のマクロ 1 つで持ち、公開関数は足さない
- **Context**: 2.1〜2.6。brief は「完全な文字列を返す関数を足すか」を設計に委ねた。
- **Alternatives Considered**:
  1. `CLINE_VERSION_PRERELEASE "rc.1"`（ハイフンなし）だけを足す。
  2. 1 に加えて `cline_version_string()` を足す。
- **Selected Approach**: 1。
- **Rationale**: どの要件も実行時の完全な文字列を求めていない。関数を足すと、公開 API と export の検査（`cline.exports`）の対象が増え、文字列を作るための可変の状態か CMake からの定義の注入が要る。プレリリースは支援しない試験用の版なので、実行時に見分ける必要は薄い。必要になれば後から非破壊で足せる。
- **Trade-offs**: 実行時にリンクしたライブラリがプレリリースかどうかは分からない。

### Decision: 完全なバージョンは CMake のパッケージでは別の変数で知らせる
- **Context**: 2.3〜2.5。`write_basic_package_version_file` に `0.2.0-rc.1` を渡すと、互換判定の正規表現と比較の挙動が CMake の版に依存して不安定になる。
- **Selected Approach**: `CLine_VERSION`（互換判定）は数字のまま。`CLineConfig.cmake` で `CLine_VERSION_PRERELEASE` と `CLine_VERSION_FULL` を設定する。pkg-config の `Version` は完全な文字列にする。
- **Trade-offs**: `find_package(CLine 0.2)` は `0.2.0-rc.1` も見つける。バージョン方針の文書に注意として書く。

### Decision: マージの検出はブランチ名・同じリポジトリ・マージ済みの 3 つで行い、ヘッダと CHANGELOG で裏付ける
- **Context**: 6.1、6.7、6.8。
- **Alternatives Considered**: ラベル（ラベルの事前作成が要る）、`version.h` の変更の検出（通常の PR の誤検出の恐れ）。
- **Selected Approach**: `pull_request` の `closed` で、`merged == true`、head のリポジトリが同じ、head のブランチが `release/v` で始まる、の 3 つを満たすときだけ動く。さらに、マージで `main` にできたコミットのヘッダと CHANGELOG がブランチ名のバージョンと一致することを確かめる。
- **Rationale**: 設定が要らず、フォークからの同名のブランチを除ける。

### Decision: 公開のやり直しは、同じワークフローの手動起動でも行える
- **Context**: 6.10、8.4。失敗した実行の「再実行」は、古いコミットのワークフローの定義で走る。ワークフローの修正後にやり直す手段が要る。
- **Selected Approach**: 公開のワークフローに `workflow_dispatch`（入力 `version`）を足す。マージ済みの `release/v<V>` の PR を API で探し、そのマージのコミットに対して同じ手順を行う。タグと Release は「既にあれば確かめて飛ばす」ので、何度走らせても重複しない。

### Decision: 最初の版は `Unreleased` に記録しておき、最初のリリースもワークフローで出す
- **Context**: 3.4、5.5。brief「0.1.0 を最初の版として記録する」と roadmap「v0.1.0 のリリースの操作はメンテナーが行う」。
- **Selected Approach**: CHANGELOG の作成時、0.1.0 に載せる内容（最初の公開で利用できる機能）を `Unreleased` に書く。比較リンクは、タグがまだないので `[Unreleased]: <repo>/commits/main` とする。最初のリリースで、ワークフローが `[0.1.0]: <repo>/releases/tag/v0.1.0` と `[Unreleased]: <repo>/compare/v0.1.0...HEAD` に書き換える。ヘッダは既に 0.1.0 なので、5.5 により同じバージョンを受け付ける。

### Generalization
- 「バージョンを比べる」処理（5.3、5.4、6.5 の直前のプレリリースの判定）は、SemVer の優先順位の比較 1 つに一般化した（`cline_semver_compare`）。
- 「CHANGELOG の節を読む」処理（5.7 の空の判定、4.3 の移動、6.3 のノート、6.8 の照合）は、節の一覧を作る 1 つの解析（`cline_changelog_parse`）に一般化した。

### Build vs. Adopt
- SemVer の比較と Keep a Changelog の解析は、CMake で使える既製のものがないので自作する。範囲は要件が求める部分（2.0.0 の書式と優先順位、1.1.0 の見出しとリンク）に限る。
- GitHub の操作は、ランナーに入っている `gh` CLI と `git` を使う。サードパーティのアクションは `actions/checkout`（既存）だけにする。

### Simplification
- 公開関数 `cline_version_string()` は足さない（上記）。
- リリース PR の本文の生成は、ワークフローの中のヒアドキュメントで足りる。専用のスクリプトにしない。
- 集約ジョブの判定は数行の bash で足りる。スクリプトにしない（判定の入力はジョブの結果だけで、通し確認で確かめる）。

## Risks & Mitigations
- dispatch した CI のチェックが required check を満たさない — 通し確認の項目にし、手順書に「PR を閉じて開き直す」「Actions のタブから CI を手で起動する」を書く。
- 並行する english-docs と `docs/conventions.md` の編集が重なる — このスペックは「バージョンについての注意」の節だけを、そのときのファイルの言語で変える。タスクの最後に置き、english-docs がまだ英語化していなければ先にマージする。
- english-docs が `docs/` を再編し、`docs/releasing.md`、`docs/versioning.md` の場所が変わる — 2 つのファイルの場所はこのスペックが持つ。english-docs はリンクだけを張る。場所を変えるなら Revalidation Trigger とする。
- リリースの日付が、PR を作った日とマージした日でずれる — 日付は PR を作った日（UTC）とし、手順書で「マージが遅れたら PR の CHANGELOG の日付を直してよい」と案内する。
- ワークフローの入力をシェルに直接埋め込むと、コマンドの注入になる — 入力は `env:` で渡し、スクリプトが SemVer の書式を検査するまで、ほかの用途に使わない。
- pkg-config がプレリリースを正式版より新しいとみなす — バージョン方針の文書に書く。

## References
- [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html) — 書式と優先順位の規則
- [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/) — CHANGELOG の形式
- [GitHub Docs: Triggering a workflow](https://docs.github.com/en/actions/how-tos/write-workflows/choose-when-workflows-run/trigger-a-workflow) — `GITHUB_TOKEN` のイベントと `workflow_dispatch` の例外
- [GitHub Changelog: workflows label](https://github.blog/changelog/label/workflows) — `GITHUB_TOKEN` による `workflow_dispatch` の起動
- [peter-evans/create-pull-request: concepts and guidelines](https://github.com/peter-evans/create-pull-request/blob/main/docs/concepts-guidelines.md) — `GITHUB_TOKEN` の PR で CI が走らないことと回避策
