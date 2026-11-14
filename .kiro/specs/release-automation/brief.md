# Brief: release-automation

## Problem

CLine を GitHub でリリースしたいが、リリースの仕組みが何もない。タグ、GitHub Release、CHANGELOG がなく、バージョンを上げる手順も決まっていない。プレリリースを出す方法もない。利用者は `main` を取り込むしかなく、どの版で何が変わったかを知る手段がない。また、CI はすべてのブランチへの push と PR の両方で動くので、PR のブランチでは同じテストが 2 回走る。文書だけの変更でも 3 OS × 静的/動的のビルドがすべて走る。

## Current State

- バージョンの唯一の出どころは `include/cline/version.h` の `CLINE_VERSION_MAJOR` / `MINOR` / `PATCH`（今は 0.1.0）である。`cmake/ClineVersion.cmake` がそれを文字列として読み、`project(CLine VERSION ...)` に渡す。
- `find_package` の互換判定は、メジャーが 0 の間は「同じマイナー」、1 以上では「同じメジャー」である（`docs/conventions.md` の「バージョンについての注意」）。
- `cline_version()` と `CLINE_VERSION_NUMBER` は `0x00MMmmpp` に詰めた数値で、プレリリースを表せない。
- pkg-config のファイルは `cmake/cline.pc.in` から作られる。
- `.github/workflows/ci.yml`: `push`（全ブランチ）と `pull_request` で動く。`quickstart`（3 OS）と `build`（4 環境 × 静的/動的、Debug と Release、consumer check）の 2 つのジョブ。権限は `contents: read`。
- タグ、GitHub Release、CHANGELOG.md、リリース用のワークフローはない。

## Desired Outcome

- メンテナーが手動起動のワークフローにバージョン（例 `0.2.0`、`0.2.0-rc.1`）を渡すと、次の内容のリリース PR が開く。
  - `version.h` をそのバージョンに書き換える。
  - CHANGELOG の `Unreleased` を、そのバージョンと日付の節に移す。
- リリース PR では CI が走り、合格してからマージできる。
- リリース PR をマージすると、次のものが自動でできる。
  - マージコミットに付いた注釈付きタグ `v<version>`。
  - CHANGELOG の該当節をノートにした GitHub Release。プレリリースの識別子があれば Pre-release になる。
- 不正なバージョン（SemVer でない、今より小さい、既にあるタグ、`Unreleased` が空など）は、PR を作る前に止まる。
- CHANGELOG.md が Keep a Changelog 1.1.0 の形式で存在し、最初の版の記録と比較リンクを持つ。
- CI は PR、main への push、手動起動で動き、二重に走らない。文書だけの変更では重いビルドを飛ばすが、required check は必ず結果を返す。
- メンテナー向けのリリース手順書（英語）が、ワークフローの使い方、プレリリース、失敗したときの戻し方、必要なリポジトリの設定を説明する。
- SemVer の運用規則（1.0 より前の互換の意味、何を破壊的変更とみなすか、プレリリースの意味）が、利用者と開発者の読む場所に英語で書かれている。

## Approach

自前の GitHub Actions で組む（roadmap の Approach Decision のとおり）。

1. **バージョンの表現**: `version.h` に、プレリリースの識別子を表すマクロ（例: 文字列の `CLINE_VERSION_PRERELEASE`。正式版では空）を足す。`cmake/ClineVersion.cmake` は、数字の部分を `project(VERSION)` に、全体の文字列を pkg-config と CMake のパッケージに渡す。`cline_version()` の数値の意味と ABI は変えない。必要なら、全体の文字列を返す関数を足すかどうかを設計で決める。
2. **CHANGELOG.md**: Keep a Changelog 1.1.0 の形式。`Unreleased`、各版の節、末尾の比較リンク。0.1.0 を最初の版として記録する。
3. **リリース PR の作成**（`workflow_dispatch`）: 入力のバージョンを検証し、`release/v<version>` ブランチで `version.h` と CHANGELOG を書き換えて PR を開く。`GITHUB_TOKEN` で作った PR では `pull_request` の CI が起動しないので、CI を `workflow_dispatch` で起動するか、GitHub App のトークンを使う。どちらにするかは設計で決める。
4. **タグと Release**: リリース PR のマージを検出し（ブランチ名、ラベル、`version.h` の変更などのどれで判定するかは設計で決める）、タグを付け、CHANGELOG の節から作ったノートで Release を作る。この 2 つは同じワークフローで行う。
5. **CI の起動**: `pull_request`、`push`（main）、`workflow_dispatch`。文書だけの変更を判定するジョブを置き、重いジョブを条件付きで飛ばす。required check にする集約ジョブを 1 つ置く。今あるジョブの中身（環境、形、consumer check）は変えない。
6. **手順書**: `docs/releasing.md`（仮）。メンテナーが行う設定（Actions に PR の作成を許す、required check、ブランチの保護、トークン）を含む。

## Scope

- **In**:
  - SemVer の運用規則と、プレリリースの表現（`version.h`、`ClineVersion.cmake`、CMake のパッケージのバージョン、pkg-config の `Version`）
  - CHANGELOG.md の作成と、`Unreleased` に記録する規則
  - リリース PR を作るワークフロー、マージ後にタグと GitHub Release を作るワークフロー
  - `ci.yml` の起動タイミングの見直し（トリガー、二重実行の解消、文書だけの変更での省略、required check にする集約ジョブ）
  - リリース手順書（英語）
  - `docs/conventions.md` の「バージョンについての注意」の内容の更新（プレリリースの扱い）
  - ワークフローの検証（スクリプトの単体テストや、試験用のバージョンでの通し確認の方法）
- **Out**:
  - CI のジョブの追加（Sanitizer、clang-format、静的解析、リンク検査など）
  - ビルド済みバイナリの添付、パッケージ管理への登録
  - README と CONTRIBUTING の本文（english-docs が持ち、リリース手順書と CHANGELOG へリンクする）
  - CODE_OF_CONDUCT.md、SECURITY.md、テンプレート（直接実装）
  - 実際の v0.1.0 のリリースの操作
  - Conventional Commits などのコミットメッセージの規約

## Boundary Candidates

- バージョンの表現（ヘッダ、CMake、pkg-config）
- 変更履歴（CHANGELOG.md と記録の規則）
- リリースのワークフロー（PR の作成、タグと Release）
- CI の起動の制御（トリガーと省略の判定、集約ジョブ）

## Out of Boundary

- 人向けの入口の文書（README、CONTRIBUTING）の本文。
- CI のジョブの中身（どの環境・形でビルドしてテストするか）。build-foundation が決めたものをそのまま使う。
- ライブラリの公開 API の挙動。バージョンの文字列を返す関数を足す場合も、既存の関数の意味は変えない。

## Upstream / Downstream

- **Upstream**: build-foundation（`version.h` を唯一の出どころとする仕組み、`ClineVersion.cmake`、パッケージの互換判定、`ci.yml`、consumer check）。
- **Downstream**: english-docs（README の取り込み例のタグ、CONTRIBUTING からのリンク）、community-health-files（PR テンプレートの CHANGELOG の項目、SECURITY.md の対応バージョンの方針）。最初のリリース v0.1.0。

## Existing Spec Touchpoints

- **Extends**: なし。build-foundation のバージョンの仕組みと CI に手を入れるが、build-foundation の要件（`version.h` が唯一の出どころであること、CI のジョブの構成）は守る。
- **Adjacent**: english-docs（`docs/conventions.md` は english-docs が英語にし、release-automation がバージョンの節の内容を更新する。両スペックの作業が重ならないよう、どちらが先かを実装の順で合わせる）。

## Constraints

- タグは `v<SemVer>`。SemVer 2.0.0 に従う。
- バージョンの唯一の出どころは `include/cline/version.h` のまま。ほかのファイルにバージョンの数字を手で書かない（CHANGELOG の見出しは除く）。
- CMake の `project(VERSION)` は数字しか受け付けない。プレリリースは別に持つ。
- `cline_version()` と `CLINE_VERSION_NUMBER` の意味を変えない（ABI と既存の利用者を壊さない）。
- `GITHUB_TOKEN` による PR や push は `pull_request` や `push` のワークフローを起動しない。タグ付けと Release は同じワークフローで行う。
- `paths-ignore` でワークフローごと飛ばさない（required check が Pending のまま残るため）。
- 追加のシークレットはできるだけ要さない。要る場合は理由と最小の権限を手順書に書く。ワークフローの権限は、ジョブごとに必要な最小にする。
- ワークフロー、スクリプト、手順書、CHANGELOG、Release のノートはすべて英語。
- リポジトリの設定（Actions の PR 作成の許可、ブランチの保護、required check）は、ワークフローからは変えない。手順書でメンテナーに案内する。
