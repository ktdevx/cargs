# Requirements Document

## Introduction

CLArgs（ライブラリ名 `clargs`）を GitHub で OSS としてリリースできるようにする。今のリポジトリには、タグ、GitHub Release、CHANGELOG、バージョンを上げる手順、プレリリースを出す方法のどれもない。利用者は `main` を取り込むしかなく、どの版で何が変わったかを知る手段がない。また、CI はすべてのブランチへの push と PR の両方で動くので PR のブランチでは同じテストが 2 回走り、文書だけの変更でも 3 OS × 静的/動的のビルドがすべて走る。

このスペックは、リリースを半自動にする。メンテナーが手動起動のワークフローにバージョンを渡すとリリース PR ができ、それをマージするとタグと GitHub Release ができる。プレリリースも同じ流れで出す。バージョンは Semantic Versioning 2.0.0、変更履歴は Keep a Changelog 1.1.0 の形式とする。あわせて、CI の起動タイミングを見直す。

関わる人は 3 種類いる。

- **メンテナー**: リリースを出し、リポジトリの設定を管理する人。
- **コントリビューター**: PR で変更を出す人。利用者に見える変更を CHANGELOG に記録する。
- **ライブラリ利用者**: CLArgs を取り込む人。リリースタグ、Release のノート、CHANGELOG、バージョンの規則を読む。

背景と判断の経緯は同じディレクトリの `brief.md` と、`.kiro/steering/roadmap.md` の「Phase 2: OSS としての公開」を参照。

## Boundary Context

- **In scope**:
  - SemVer の運用規則（1.0 より前の互換の意味、破壊的変更の範囲、プレリリースの意味）の英語の文書
  - プレリリースの識別子の表現（バージョンヘッダ、ビルド、CMake のパッケージ、pkg-config）
  - CHANGELOG.md の作成と、`Unreleased` に記録する規則
  - リリース PR を作るワークフローと、マージ後にタグと GitHub Release を作るワークフロー
  - CI の起動タイミング（起動の契機、二重実行の解消、文書だけの変更での省略、必ず結果を返す集約チェック）
  - メンテナー向けのリリース手順書（英語）
  - `docs/conventions.md` の「バージョンについての注意」の内容の更新
  - リリースの仕組みの検証の方法（スクリプトの自動テストと、試験用のバージョンでの通し確認の手順）
- **Out of scope**:
  - CI のジョブの追加（Sanitizer、clang-format、静的解析、リンク検査など）と、今あるジョブの中身（環境、静的/動的の形、Debug/Release、consumer check の手順）の変更
  - ビルド済みバイナリの添付、パッケージ管理への登録
  - README と CONTRIBUTING の本文、`docs/` の英語化と再編（english-docs が持つ）
  - CODE_OF_CONDUCT.md、SECURITY.md、Issue/PR テンプレート（既に実装済み）
  - 実際の v0.1.0 のリリースの操作（仕組みができたあとにメンテナーが行う）
  - コミットメッセージの規約（Conventional Commits など）
  - リポジトリの設定（ブランチの保護、required check、Actions の PR 作成の許可）をワークフローから変えること
- **Adjacent expectations**:
  - 実装の順序は release-automation が先、english-docs があと（english-docs は release-automation に依存する）。english-docs は、このスペックが作るファイル（`tools/`、`tests/release/`、CHANGELOG.md、ワークフロー）が既にある前提で、`docs/conventions.md` の英語化とディレクトリ構成の節を書く。
  - english-docs は、README と CONTRIBUTING から、このスペックが置く CHANGELOG、リリース手順書、バージョンの規則の文書へリンクする。それらの中身は書かない。`docs/conventions.md` の「バージョンについての注意」は、このスペックが内容を更新し、english-docs が英語にする。
  - 既存の PR テンプレートは「利用者に見える変更は CHANGELOG の `Unreleased` に項目を足す」ことを求めている。このスペックの記録の規則はこれと食い違わない。
  - 既存の SECURITY.md は「最新の 0.x のリリースだけを支援し、プレリリースは支援しない」としている。このスペックのバージョンの規則はこれと食い違わない。
  - build-foundation が決めた「`include/clargs/version.h` がバージョンの唯一の出どころ」「`find_package` の互換判定（メジャーが 0 の間は同じマイナー、1 以上では同じメジャー）」「`clargs_version()` と `CLARGS_VERSION_NUMBER` の意味」は変えない。

## Requirements

### Requirement 1: バージョンの運用規則の文書

**Objective:** As a ライブラリ利用者またはコントリビューター, I want CLArgs がバージョン番号をどう上げるかを英語の文書で読めること, so that 版を上げたときに何が壊れうるかを判断できる

#### Acceptance Criteria

1. The バージョン方針の文書 shall CLArgs が Semantic Versioning 2.0.0 に従い、タグを `v<バージョン>`（例 `v0.2.0`、`v0.2.0-rc.1`）の形で付けることを英語で説明する。
2. The バージョン方針の文書 shall メジャーが 0 の間は、マイナーが上がると互換が切れうること、パッチの更新は互換を保つこと、メジャーが 1 以上になったら互換が切れうるのはメジャーの更新だけであることを説明する。
3. The バージョン方針の文書 shall 破壊的変更とみなすものとして、公開 API（関数、型、マクロ）の削除や意味の変更、ソースの互換の喪失、動的ライブラリの ABI の互換の喪失、文書に書かれた挙動の変更を列挙する。
4. The バージョン方針の文書 shall プレリリース（例 `-rc.1`）が正式版より前の試験用の版であり、互換と支援を約束しないことを説明する。
5. The バージョン方針の文書 shall 既存の SECURITY.md の支援の方針（最新の 0.x のリリースだけを支援し、プレリリースは支援しない）と食い違わない。
6. The バージョン方針の文書 shall 利用者向けの文書と開発者向けの文書の両方からリンクでたどれる場所に置かれる。

### Requirement 2: プレリリースを表せるバージョンの表現

**Objective:** As a メンテナー, I want バージョンの唯一の出どころであるヘッダにプレリリースの識別子も持てること, so that 正式版とプレリリースを同じ仕組みで出せる

#### Acceptance Criteria

1. The バージョンヘッダ（`include/clargs/version.h`） shall メジャー、マイナー、パッチに加えて、プレリリースの識別子を持つ。正式版では識別子は空である。
2. The CLArgs ビルド shall バージョンヘッダだけからバージョン（数字の部分とプレリリースの識別子）を読み、ほかのファイルに手で書かれたバージョンを使わない。
3. When プレリリースの識別子が空でないバージョンで構成したとき, the CLArgs ビルド shall pkg-config のファイルのバージョンと、CMake のパッケージが知らせる完全なバージョンの文字列に、プレリリースの識別子を含める（例 `0.2.0-rc.1`）。
4. When プレリリースの識別子が空のバージョンで構成したとき, the CLArgs ビルド shall pkg-config のファイルと CMake のパッケージに、今と同じ `<major>.<minor>.<patch>` の形のバージョンを示す。
5. The CLArgs ビルド shall プレリリースの有無にかかわらず、`find_package` の互換判定と、動的ライブラリの ABI バージョン（SOVERSION）を数字の部分だけで決め、今の判定の規則を変えない。
6. The clargs ライブラリ shall `clargs_version()` と `CLARGS_VERSION_NUMBER` の値の意味（`0x00MMmmpp` に詰めた数字の部分）を変えない。
7. If バージョンヘッダのプレリリースの識別子が欠けている、重複している、または SemVer 2.0.0 のプレリリースの識別子として正しくない場合, then the CLArgs ビルド shall ヘッダのパスと問題のある箇所を示して構成を失敗させる。
8. While バージョンヘッダが正式版を表している間, the consumer check shall 今と同じ取り込み方法のそれぞれで合格する。

### Requirement 3: CHANGELOG と記録の規則

**Objective:** As a ライブラリ利用者, I want 版ごとの変更を 1 つのファイルで読めること, so that 取り込む版を選び、更新の影響を判断できる

#### Acceptance Criteria

1. The CHANGELOG（リポジトリのルートの `CHANGELOG.md`） shall Keep a Changelog 1.1.0 の形式で、英語で書かれ、先頭に `Unreleased` の節を持つ。
2. The CHANGELOG shall 各版の節を `[<バージョン>] - <YYYY-MM-DD>` の見出しで新しい順に並べ、変更を Keep a Changelog の分類（Added、Changed、Deprecated、Removed、Fixed、Security）で書く。
3. The CHANGELOG shall 末尾に、`Unreleased` と各版の比較リンク（前の版のタグとの比較。最初の版はそのタグのページ）を持つ。
4. The CHANGELOG shall 最初のリリース（0.1.0）に載せる内容を、作成の時点で `Unreleased` の節に記録している。
5. The CHANGELOG shall 冒頭で、Keep a Changelog と Semantic Versioning に従うことと、バージョン方針の文書へのリンクを示す。
6. The 記録の規則 shall 「利用者に見える変更は、その変更の PR で、CHANGELOG の `Unreleased` の該当する分類に項目を足す」と定め、既存の PR テンプレートの CHANGELOG の項目と同じ内容にする。

### Requirement 4: リリース PR の作成

**Objective:** As a メンテナー, I want バージョンを渡すだけでリリース PR ができること, so that 手作業の書き換えの誤りなくリリースを準備できる

#### Acceptance Criteria

1. When メンテナーが `main` を対象にリリース PR のワークフローを手動で起動し、正しいバージョン（例 `0.2.0`、`0.2.0-rc.1`）を渡したとき, the リリース PR ワークフロー shall `release/v<バージョン>` のブランチを作り、`main` への PR を開く。
2. When リリース PR ワークフローがリリース PR を作るとき, the リリース PR ワークフロー shall そのブランチのバージョンヘッダを渡されたバージョン（数字の部分とプレリリースの識別子）に書き換える。
3. When リリース PR ワークフローがリリース PR を作るとき, the リリース PR ワークフロー shall CHANGELOG の `Unreleased` の内容を、渡されたバージョンと起動した日（UTC）の節に移し、`Unreleased` を空にし、末尾の比較リンクを更新する。
4. The リリース PR ワークフロー shall リリース PR の本文に、リリースするバージョン、Pre-release になるかどうか、マージしたときに起きること（タグと GitHub Release の作成）、マージ前に確かめること を英語で書く。
5. When リリース PR を開いたとき, the リリース PR ワークフロー shall そのリリース PR の先頭のコミットで CI を起動し、メンテナーの手作業なしに CI の結果がリリース PR に示されるようにする。
6. The リリース PR ワークフロー shall バージョンヘッダと CHANGELOG 以外のファイルを変えない。
7. The リリース PR ワークフロー shall 同時に 2 つ以上走らない。

### Requirement 5: リリース PR を作る前の検証

**Objective:** As a メンテナー, I want 誤ったバージョンでの起動が何も作らずに止まること, so that 不正なタグや壊れた PR を後から片付けずに済む

#### Acceptance Criteria

1. If 渡されたバージョンが SemVer 2.0.0 の `<major>.<minor>.<patch>[-<pre-release>]` の形でない（ビルドメタデータ `+...` を含む場合を含む）, then the リリース PR ワークフロー shall ブランチも PR も作らずに失敗し、理由を示す。
2. If 渡されたバージョンのメジャー、マイナー、パッチのどれかがバージョンヘッダで表せる範囲（0〜255）を超える, then the リリース PR ワークフロー shall ブランチも PR も作らずに失敗し、理由を示す。
3. If 渡されたバージョンが、既にあるリリースタグのどれかのバージョン以下である（SemVer の優先順位で比べる）, then the リリース PR ワークフロー shall ブランチも PR も作らずに失敗し、理由を示す。
4. If 渡されたバージョンが `main` のバージョンヘッダのバージョンより小さい, then the リリース PR ワークフロー shall ブランチも PR も作らずに失敗し、理由を示す。
5. Where `main` のバージョンヘッダのバージョンにまだタグがない場合（最初のリリースなど）, the リリース PR ワークフロー shall そのバージョンと同じバージョンでのリリースを受け付ける。
6. If `v<バージョン>` のタグ、または `release/v<バージョン>` のブランチが既にある, then the リリース PR ワークフロー shall ブランチも PR も作らずに失敗し、理由を示す。
7. If CHANGELOG の `Unreleased` に変更の項目が 1 つもない, または CHANGELOG が期待する形でない, then the リリース PR ワークフロー shall ブランチも PR も作らずに失敗し、理由を示す。
8. If リリース PR ワークフローが `main` 以外のブランチを対象に起動された, then the リリース PR ワークフロー shall ブランチも PR も作らずに失敗し、理由を示す。

### Requirement 6: マージ後のタグと GitHub Release

**Objective:** As a メンテナー, I want リリース PR をマージするだけでタグと Release ができること, so that 公開の手順を忘れたり誤ったりしない

#### Acceptance Criteria

1. When リリース PR が `main` にマージされたとき, the 公開ワークフロー shall マージで `main` にできたコミットに、注釈付きタグ `v<バージョン>` を付ける。
2. When 公開ワークフローがタグを付けたとき, the 公開ワークフロー shall 同じ実行の中で、そのタグの GitHub Release を作る。
3. The 公開ワークフロー shall Release のノートを、マージされたコミットの CHANGELOG のそのバージョンの節から英語で作る。
4. Where バージョンにプレリリースの識別子がある場合, the 公開ワークフロー shall Release を Pre-release にし、最新のリリース（Latest）にしない。
5. Where プレリリースの識別子がないバージョンの直前に、同じ `<major>.<minor>.<patch>` のプレリリースが出ている場合, the 公開ワークフロー shall ノートにそれらのプレリリースへの参照を含める。
6. The 公開ワークフロー shall Release にソース以外のファイル（ビルド済みのバイナリなど）を付けない。
7. If マージされた PR がこのリポジトリの `release/v<バージョン>` のブランチからのものでない、またはマージされずに閉じられた, then the 公開ワークフロー shall タグも Release も作らない。
8. If マージされたコミットのバージョンヘッダまたは CHANGELOG が、ブランチ名のバージョンと一致しない, then the 公開ワークフロー shall タグも Release も作らずに失敗し、理由を示す。
9. If 同じ名前のタグが別のコミットに既にある, then the 公開ワークフロー shall タグを動かさず、Release も作らずに失敗し、理由を示す。
10. When 途中で失敗した公開をメンテナーがやり直したとき, the 公開ワークフロー shall 既にできているタグや Release を重複させずに、足りないものだけを作る。

### Requirement 7: CI の起動タイミング

**Objective:** As a コントリビューターまたはメンテナー, I want CI が必要なときに 1 回だけ走り、必ず結果を返すこと, so that 待ち時間と無駄な実行を減らしつつ、マージの判断ができる

#### Acceptance Criteria

1. The CI ワークフロー shall PR の作成と更新、`main` への push、手動起動のときに動き、それ以外のブランチへの push では動かない。
2. When 同じ PR に新しいコミットが push されたとき, the CI ワークフロー shall その PR の古い実行を取り消す。
3. When PR または `main` への push の変更が文書だけ（Markdown のファイル、`docs/` の下、LICENSE、Issue テンプレートなど、ビルドとテストに影響しないファイル）であるとき, the CI ワークフロー shall 3 OS のビルドとテストのジョブを飛ばす。
4. If 変更されたファイルを判定できない、または判定に失敗した, then the CI ワークフロー shall 文書だけの変更とみなさず、すべてのジョブを走らせる。
5. When CI ワークフローが手動で起動されたとき, the CI ワークフロー shall 変更の内容にかかわらず、すべてのジョブを走らせる。
6. The CI ワークフロー shall 名前が固定された 1 つの集約チェックを、ジョブが飛ばされたときも含めて毎回、成功か失敗で終わらせる。
7. If 走らせたジョブのどれかが失敗した、取り消された、または飛ばすべきでないのに飛ばされた, then the CI ワークフロー shall 集約チェックを失敗させる。
8. The CI ワークフロー shall 今あるジョブの中身（対象の環境、静的/動的、Debug/Release、consumer check）とジョブの名前を変えない。
9. The CI ワークフロー shall 今と同じく、リポジトリの読み取り以外の権限を持たない。
10. When 変更されたファイルに `tests/` の下の Markdown のファイル（テストのフィクスチャなど）が含まれるとき, the CI ワークフロー shall それを文書とみなさず、3 OS のビルドとテストのジョブを走らせる。

### Requirement 8: リリース手順書

**Objective:** As a メンテナー, I want リリースの手順と必要な設定が英語の手順書にまとまっていること, so that 迷わずにリリースを出し、失敗から戻せる

#### Acceptance Criteria

1. The リリース手順書 shall 正式版とプレリリースのそれぞれについて、リリース PR のワークフローの起動からタグと Release ができるまでの手順を英語で説明する。
2. The リリース手順書 shall メンテナーが手で行うリポジトリの設定（Actions に PR の作成を許すこと、`main` のブランチの保護、required check にする集約チェックの名前、タグの保護を使う場合の注意）を説明する。
3. The リリース手順書 shall ワークフローが使う権限と、追加のシークレットを要さないこと（要る場合はその理由と最小の権限）を説明する。
4. The リリース手順書 shall 失敗したときの戻し方（PR を作る前に止まった場合、リリース PR を取り下げる場合、タグや Release の作成が途中で失敗した場合、誤って出したリリースを取り消す場合）を説明する。
5. The リリース手順書 shall リリース PR で CI の結果が示されないときに、メンテナーが CI を起動し直す方法を説明する。
6. The リリース手順書 shall 試験用のバージョンで、本物のリポジトリにタグや Release を残さずに、仕組み全体を通して確かめる方法を説明する。
7. The リリース手順書 shall メンテナーが手で行うリポジトリの設定として、非公開の脆弱性報告（Private vulnerability reporting）を有効にすることを説明する。既存の SECURITY.md と Issue テンプレートの案内がこの機能を前提にしているためである。
8. The リリース手順書 shall 最初のリリースの前に、README の取り込みの例が指すタグ（`GIT_TAG v0.1.0` など）が最初に出す版と一致することを確かめ、一致しない場合は README をリリース PR とは別の PR で直すことを説明する。リリース PR はバージョンヘッダと CHANGELOG だけを変える。

### Requirement 9: リリースの仕組みの検証

**Objective:** As a メンテナー, I want バージョンの検証や CHANGELOG の書き換えの規則が自動テストで確かめられていること, so that ワークフローを壊したことに本番のリリースの前に気づける

#### Acceptance Criteria

1. The リリースの仕組み shall バージョンの検証、バージョンヘッダの書き換え、CHANGELOG の節の移動とリンクの更新、ノートの取り出し、文書だけの変更の判定を、GitHub の外（開発者の手元）でも実行できる形で持つ。
2. The リリースの仕組みのテスト shall 正しい入力と、Requirement 5 の各失敗の場合を含む入力について、上の各処理の結果を確かめる。
3. The リリースの仕組みのテスト shall 新しい CI のジョブを足さずに、今ある CI のテストの実行の中で走る。
4. When 開発者が手元でリリースの処理を試すとき, the リリースの仕組み shall 作業ツリーの写しなど、指定した場所のファイルだけを書き換え、ネットワークにも GitHub にも触れない。

### Requirement 10: 文書の言語と隣接する作業との境界

**Objective:** As a メンテナー, I want 人が読む成果物が英語で、並行する文書の作業と重ならないこと, so that 第三者が読め、変更が衝突しない

#### Acceptance Criteria

1. The release-automation の成果物 shall ワークフロー、スクリプト、それらのコメント、CHANGELOG、リリース手順書、バージョン方針の文書、リリース PR の本文、Release のノートを英語だけで書く。
2. The release-automation の成果物 shall 人が読むファイルとワークフローの中で、`.kiro/`、スペック、Kiro の手順に触れない。
3. The release-automation の作業 shall README.md と CONTRIBUTING.md を変更しない。
4. When `docs/conventions.md` の「バージョンについての注意」を更新するとき, the release-automation の作業 shall その節だけを変え、そのときのファイルの言語で書き、プレリリースの表し方とパッケージでのバージョンの扱いを説明に加える。
5. The release-automation の成果物 shall ワークフローの各ジョブに、そのジョブに必要な最小の権限だけを与える。
6. The release-automation の成果物 shall リポジトリの設定を変える操作をワークフローの中で行わない。
7. The release-automation の成果物 shall CHANGELOG、リリース手順書、バージョン方針の文書の中で、開発の過程の文書を指す単独の語 "spec"、"specs" と、語 "steering" を使わない。SemVer の仕様を指すときは "specification" と書く。
