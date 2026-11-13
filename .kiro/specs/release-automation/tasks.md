# Implementation Plan

コードとテストのタスクは、どれも `tests/CMakeLists.txt` のリリースのテストの登録に 1 行足すので、番号の順に行う（`(P)` を付けていない）。`(P)` を付けたのは、互いに別のファイルだけを書く文書のタスク（5.1〜5.3）だけである。どのタスクも、人が読むもの（ワークフロー、スクリプト、コメント、エラーメッセージ、CHANGELOG、文書、PR の本文、Release のノート）を英語で書き、`.kiro/`、スペック、Kiro に触れない。README.md、CONTRIBUTING.md、CODE_OF_CONDUCT.md、SECURITY.md、`.github/ISSUE_TEMPLATE/`、`.github/pull_request_template.md` は変えない。リリースのスクリプトはネットワーク、`git`、`gh` を呼ばない。

- [x] 1. 土台: プレリリースを表せるバージョンと、リリースのテストの仕組み
- [x] 1.1 バージョンヘッダにプレリリースの識別子を足し、読み取りで検査する
  - バージョンヘッダに、プレリリースの識別子を持つ 4 つ目のマクロを、設計の VersionHeader の書式（1 行、文字列リテラル、ハイフンなし、正式版は空）で足す。冒頭のコメントに 4 つ目のマクロの書式を足す。数値のマクロと `clargs_version()` は変えない
  - バージョンの読み取りに、任意の 3 つ目の出力（識別子）を足す。識別子のマクロは引数の数にかかわらず常に検査し、欠けている、重複している、文字列リテラルでない、SemVer のプレリリースの識別子として正しくない場合を、ほかのマクロの問題と一緒に、ヘッダのパスとマクロ名を示す 1 つのエラーにまとめる
  - 公開 API のバージョンのテストに、識別子のマクロが文字列リテラルとして使えることの確認を足す
  - リリースのテストの補助（確認の関数、失敗の集計、確認が 1 つも走らなければ失敗、フィクスチャを作業ディレクトリに写す関数）を作り、テストの登録に `release` のラベルと `release.<name>` の名前で登録する仕組みを足す。最初のテストとして、設計の Testing Strategy の `release.version_read` の内容（正しい識別子、各不正な形、2 引数の呼び出しが今と同じ値を返すこと）を足す
  - 完了の目安: 静的と動的の両方の構成で、既存のテストと `release.version_read` が通る。識別子のマクロを壊したヘッダの写しで構成すると、パスとマクロ名を含むエラーで構成が止まる
  - _Requirements: 2.1, 2.2, 2.6, 2.7, 9.2, 9.3_

- [x] 1.2 完全なバージョンを pkg-config と CMake のパッケージに渡し、consumer check で比べる
  - トップレベルの構成で識別子を読み、完全なバージョン（識別子があれば `<数字>-<識別子>`）を作る。`project()` に渡すのは数字の部分のまま
  - pkg-config のファイルの `Version` を完全なバージョンにし、CMake のパッケージの設定ファイルで、完全なバージョンと識別子の 2 つの変数（設計の PackageVersion の名前）を設定する。互換判定のファイルと SOVERSION は数字の部分のまま
  - consumer check の find_package と pkg_config の経路で、パッケージのバージョンの正規表現にプレリリースを許し、識別子をプログラムへ渡し、ヘッダの識別子と文字列で比べる。違えば両方を表示して失敗する。手順のスクリプトと CI のステップは変えない
  - 完了の目安: 正式版のヘッダで、静的と動的の Debug と Release の consumer check の 4 つの経路が今と同じく通る。識別子を一時的に `rc.1` にした作業ツリーの写しで構成とインストールをすると、`clargs.pc` の `Version` と完全なバージョンの変数が `0.1.0-rc.1`、`CLArgs_VERSION` が `0.1.0` になり、その写しで consumer check が通る（確認後、写しは捨てる）
  - _Requirements: 2.3, 2.4, 2.5, 2.8_

- [x] 2. コア: リリースの処理のモジュールと変更の判定
- [x] 2.1 SemVer の解析と優先順位の比較を作る
  - 設計の SemverModule の契約どおり、`<major>.<minor>.<patch>[-<pre-release>]` だけを受け付け、ビルドメタデータ、先頭の 0、空の識別子、0〜255 を超える数字を理由つきで拒む解析を作る
  - SemVer 2.0.0 の優先順位の比較を作る。数字だけの識別子は桁数と文字列で比べ、数値の桁あふれを起こさない
  - 設計の Testing Strategy の `release.semver` の内容（正しい書式と各誤った書式、仕様の例の順序、長い数字の識別子）をテストとして足す
  - 完了の目安: `release.semver` が通り、仕様の例の 8 つの版が期待どおりの順に並ぶ
  - _Requirements: 5.1, 5.2, 5.3, 5.4, 9.1, 9.2_

- [x] 2.2 CHANGELOG の解析、`Unreleased` の移動、ノートの取り出しを作る
  - 設計の Data Models の「CHANGELOG の受け付ける形」に従う解析を作る。版の一覧（ファイルの順）、`Unreleased` の項目の数、無効なときの理由を返す
  - `Unreleased` の本文を新しい版の節に移し、`Unreleased` を空にし、比較リンクを更新する処理を作る。最初の版はタグのページ、2 つ目以降は直下の版との比較、`Unreleased` は新しい版からの比較にする。改行の形を保つ
  - 版の節の本文をノートとして取り出し、正式版の直下に同じ数字の部分のプレリリースが続く場合は、設計の文面でそれらへの参照を足す処理を作る
  - 設計の Testing Strategy の `release.changelog` の内容をテストとして足す
  - 完了の目安: `release.changelog` が通り、最初の版と 2 つ目の版の移動の結果が、期待するファイルの全文と一致する
  - _Requirements: 3.2, 3.3, 4.3, 5.7, 6.3, 6.5, 9.2_

- [x] 2.3 バージョンヘッダの値を書き換える処理を作る
  - 4 つのマクロの行の値だけを置き換え、ほかのバイトを保つ処理を作る。行がちょうど 1 回見つからなければ理由つきで失敗する
  - 設計の Testing Strategy の `release.version_header` の内容をテストとして足す。書き換えた結果を、1.1 のバージョンの読み取りで読み直して確かめる
  - 完了の目安: `release.version_header` が通り、正式版とプレリリースへの書き換えで、4 つの値の行以外の差分がない
  - _Requirements: 4.2, 9.2_

- [x] 2.4 文書だけの変更を判定するスクリプトを作る
  - 設計の ChangesScript の規則（文書のパターン、一覧のファイルがない・空・判定できないときは文書だけでないとする）で判定し、結果を標準出力と出力ファイルに書くスクリプトを作る
  - `.md` を文書とみなすのは、リポジトリの根の直下、`docs/` の下、`.github/` の下だけにする。`tests/` の下の `.md`（`tests/release/fixtures/` の CHANGELOG のフィクスチャなど）は文書とみなさない
  - 設計の Testing Strategy の `release.changes` の内容（`tests/` の下の `.md` だけの一覧の場合を含む）をテストとして足す
  - 完了の目安: `release.changes` が通り、`.github/workflows/` のファイルを含む一覧と `tests/release/fixtures/` の `.md` だけの一覧は `docs_only=false`、根と `docs/` と `.github/` の Markdown と `docs/` だけの一覧は `docs_only=true` になる
  - _Requirements: 7.3, 7.4, 7.10, 9.1, 9.2_

- [ ] 3. 入口のスクリプト
- [x] 3.1 リリース PR の前の検証と書き換えのスクリプトを作る
  - 設計の PrepareScript の入力、検証の順序、出力のとおりに作る。すべての検証は書く前に行い、2 つのファイルの新しい中身を作ってから書き、ヘッダを読み直して確かめる。検証だけの指定では何も書かない
  - タグの一覧のうち、`v` に続けて正しい SemVer になるものだけを既存のリリースとして扱う。ヘッダの版と同じ版は、そのタグがないときだけ受け付ける
  - 設計の Testing Strategy の `release.prepare` の内容をテストとして足す。各失敗で終了コードが 0 でなく、理由が出力に含まれ、写したファイルが変わらないことを確かめる
  - 完了の目安: `release.prepare` が通る。最初のリリース（ヘッダ 0.1.0、タグなし、`0.1.0`）と通常のプレリリースが成功し、変わるのはヘッダと CHANGELOG だけで、出力ファイルに `version`、`tag`、`prerelease` が書かれる
  - _Requirements: 4.2, 4.3, 4.6, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7, 9.1, 9.2, 9.4_

- [ ] 3.2 マージ後の照合とノートのスクリプトを作る
  - 設計の PublishScript の入力、照合（ヘッダの完全なバージョン、CHANGELOG の最新の版）、出力（ノートのファイル、`prerelease`、`title`）のとおりに作る。ノートが空なら失敗する
  - 設計の Testing Strategy の `release.publish` の内容をテストとして足す
  - 完了の目安: `release.publish` が通り、プレリリースは `prerelease=true`、先行するプレリリースのある正式版のノートに参照の文が入り、ヘッダか CHANGELOG が一致しない写しでは失敗する
  - _Requirements: 6.3, 6.4, 6.5, 6.8, 9.1, 9.2_

- [ ] 4. ワークフロー
- [ ] 4.1 CI の起動の契機、変更の判定、集約チェックを組み込む
  - 起動の契機を PR、`main` への push、手動起動にし、concurrency を設計のとおり（PR だけ古い実行を取り消す）にする。権限は読み取りのまま
  - 設計の CIWorkflow の `changes` ジョブ（イベントごとの一覧の作り方、失敗や手動起動では一覧を作らない）を足し、2.4 のスクリプトで `docs_only` を出力する
  - `quickstart` と `build` には `needs` と `if` だけを足す。名前、matrix、steps は 1 文字も変えない
  - `CI result` の集約ジョブを、設計の判定の規則で足し、結果と理由を実行の要約に書く。冒頭のコメントを新しい起動の契機に合わせて英語で書き直す
  - 完了の目安: ワークフローの YAML が構文として読め、`git diff` で既存の 2 つのジョブの差分が `needs` と `if` の行だけである。集約ジョブの判定を、`docs_only` と各ジョブの結果の組（成功、失敗、取り消し、文書だけでの省略、文書だけでないのに省略）について、手元で同じ bash を環境変数を変えて走らせて確かめる
  - _Requirements: 4.5, 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7, 7.8, 7.9, 7.10, 10.5_

- [ ] 4.2 リリース PR のワークフローを作る
  - 設計の ReleasePRWorkflow の起動、concurrency、権限、ステップ（`main` の確認、ブランチの有無、タグの一覧、3.1 のスクリプト、ブランチとコミット、ほかの変更がないことの確認、PR の作成、CI の dispatch、実行の要約）のとおりに作る
  - 入力は `env:` で渡し、`run:` に式で直接埋め込まない。PR の本文は設計の雛形どおり英語で書き、リリース手順書へリンクする
  - 完了の目安: ワークフローの YAML が構文として読め、`run:` の中に `inputs` や `github.event` の式が直接現れない。ジョブの権限が `contents: write`、`pull-requests: write`、`actions: write` だけで、最上位は空である。3.1 のスクリプトを呼ぶ行の引数が、そのスクリプトの入力の表と一致する
  - _Requirements: 4.1, 4.4, 4.5, 4.6, 4.7, 5.6, 5.8, 10.1, 10.5, 10.6_

- [ ] 4.3 タグと Release を作る公開のワークフローを作る
  - 設計の PublishWorkflow の起動（マージで閉じた PR と、やり直しの手動起動）、ジョブの条件、concurrency、権限、ステップ（バージョンとコミットの決定、checkout、3.2 のスクリプト、タグの有無と指すコミットの確認、注釈付きタグ、Release の有無の確認と作成、実行の要約）のとおりに作る
  - プレリリースでは Pre-release にし Latest にしない。資産のファイルを付けない。`pull_request_target` を使わない
  - 完了の目安: ワークフローの YAML が構文として読め、ジョブの条件にマージ済み・同じリポジトリ・`release/v` の 3 つが入り、タグと Release のそれぞれに「あれば確かめて飛ばす」分岐がある。`run:` の中にイベントの式が直接現れない
  - _Requirements: 6.1, 6.2, 6.4, 6.6, 6.7, 6.9, 6.10, 10.1, 10.5, 10.6_

- [ ] 5. CHANGELOG と英語の文書
- [ ] 5.1 (P) CHANGELOG を作る
  - 設計の ChangelogFile の冒頭（形式の宣言、バージョン方針の文書へのリンク、記録の規則）、`Unreleased` の 0.1.0 の内容、末尾のリンクのとおりに作る。0.1.0 の項目は、今の README と `docs/` が説明する機能から確かめて書く
  - 記録の規則の文言を、PR テンプレートの CHANGELOG の項目と同じ意味にする
  - 単独の語 "spec"、"specs" と "steering" を使わない（SemVer は "Semantic Versioning" または "specification" と書く）
  - 完了の目安: リポジトリの写しで、3.1 のスクリプトを検証だけの指定、バージョン `0.1.0`、空のタグの一覧で走らせると成功する
  - _Boundary: ChangelogFile_
  - _Depends: 3.1_
  - _Requirements: 1.6, 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 10.7_

- [ ] 5.2 (P) バージョン方針の文書を書く
  - 設計の VersioningDoc の節の構成どおりに、英語で書く。支援の方針は SECURITY.md へのリンクで示し、内容を繰り返さない。プレリリースと `find_package`、pkg-config の比較の注意を含める。単独の語 "spec"、"specs" と "steering" を使わない（SemVer の仕様は "specification" と書く）
  - 完了の目安: 文書が要件 1 の各項目を節として持ち、SECURITY.md の「最新の 0.x だけを支援し、プレリリースは支援しない」と食い違う記述がない
  - _Boundary: VersioningDoc_
  - _Depends: 1.2_
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 10.7_

- [ ] 5.3 (P) リリース手順書を書く
  - 設計の ReleasingDoc の 8 つの節を英語で書く。ワークフローの名前、入力、権限、required check の名前、スクリプトの引数は、4.1〜4.3 と 3.1 の実装と一致させる
  - One-time repository setup に、Settings > Code security > Private vulnerability reporting を有効にすること（SECURITY.md と Issue テンプレートの `config.yml` が前提にしている）を足す
  - Rehearsing の節（最初の本番のリリースの前の注意）に、最初のリリースの前に README.md の FetchContent の例がこれから作るタグを指していることを確かめ（英文はスペック名に触れず、README の今の内容に依らない形で書く）、違うならリリース PR とは別の PR で README を直すこと（リリース PR は `version.h` と `CHANGELOG.md` だけを変える）を書く
  - 単独の語 "spec"、"specs" と "steering" を使わない
  - 完了の目安: 手順書が要件 8 の各項目（8.7 の非公開の脆弱性報告と 8.8 の README のタグの確認を含む）を節の中に持ち、書かれたワークフローの名前・ジョブの権限・`CI result` の名前・スクリプトの引数が実装のファイルと一致する
  - _Boundary: ReleasingDoc_
  - _Depends: 4.1, 4.2, 4.3_
  - _Requirements: 3.6, 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7, 8.8, 10.7_

- [ ] 6. 統合と最終の確認
- [ ] 6.1 開発者向けの規約のバージョンの節を更新する
  - `docs/conventions.md` の「バージョンについての注意」の節だけに、設計の ConventionsVersionNotes の内容を足す。english-docs はこのスペックのあとに実装するので、ファイルはまだ日本語であり、日本語で書く。節の外と節の見出しは変えない（英語化後の見出し `Version notes` は README からのリンク先として固定されている）
  - 既存の最初の項目（唯一の出どころを 3 つのマクロとする文）は、節が食い違わないように `CLARGS_VERSION_PRERELEASE` を含む 4 つのマクロを挙げる形に改める（数字のマクロの書式の説明は残す）
  - バージョン方針の文書とリリース手順書へは、ファイルへのリンクで案内する
  - 完了の目安: `git diff <base>`（6.2 の定義）の差分がその節の中だけにあり、既存の 3 つの項目が（最初の項目は 4 つのマクロを挙げる形で）残り、2 つの文書へのリンク先のファイルが存在する
  - _Depends: 5.2, 5.3_
  - _Requirements: 1.6, 10.4_

- [ ] 6.2 全体を確かめる
  - 静的と動的、Debug と Release の構成で、すべてのテスト（`release` のラベルを含む）と consumer check が通ることを確かめる
  - 差分の基準 `<base>` は、このスペックの最初の実装タスク（1.1）のコミットの直前のコミットとする（実装の開始時に記録する）。差分による確認はすべて `git diff <base>` で行い、あとの作業の影響を受けないようにする
  - 新しいファイルと変更したファイルに、日本語の文字と `.kiro`、`Kiro` の文字列がないこと（`docs/conventions.md` はまだ日本語なので言語の確認から除く）、README.md と CONTRIBUTING.md に差分がないことを機械的に確かめる
  - `CHANGELOG.md`、`docs/releasing.md`、`docs/versioning.md` に、大文字と小文字を区別しない `\bspecs?\b` と `steering` の一致がないことを機械的に確かめる（"specification" は一致しない）。同じ 3 つのファイルに、スペック名（build-foundation、core-runtime、command-schema、argv-parser、help-formatter、english-docs、release-automation）が現れないことも確かめる
  - 3 つのワークフローの YAML が構文として読め、すべてのジョブに明示的な権限があり、`ci.yml` が読み取りの権限だけであることを確かめる
  - GitHub の上での通し確認（フォークでの試験用のプレリリース、文書だけの PR、やり直しの dispatch）は、リリース手順書の Rehearsing の節のとおりにメンテナーが行う項目として残す
  - 完了の目安: 上の確認の結果（テストの件数と合否、検索の結果が空であること）を記録でき、すべて合格している
  - _Requirements: 2.8, 9.3, 10.1, 10.2, 10.3, 10.5, 10.6, 10.7_

## Implementation Notes

- 1.1: リリースのテストの補助は `tests/release/ReleaseTestHelpers.cmake`（`clargs_test_expect_equal`、`clargs_test_expect_contains`、`clargs_test_expect_not_contains`、`clargs_test_copy_fixture`、`clargs_test_run_script`、`clargs_test_finish`）。登録は `tests/CMakeLists.txt` の `clargs_add_release_test(<name>)`（`release.<name>`、ラベル `release`、作業ディレクトリは開始時に空にされる）。テストは `cmake -D CLARGS_SOURCE_ROOT=... -D CLARGS_TEST_WORK_DIR=... -P` で動く。
- 1.1: `clargs_read_version(<header> <out> [<prerelease-out>])` は 2 引数でも識別子のマクロを検査する。トップレベルの `CMakeLists.txt` はまだ 2 引数で呼ぶ（1.2 で 3 引数にする）。
- 2.1: SemVer は `tools/release/ClargsSemver.cmake` の `clargs_semver_parse(<version> <prefix>)`（`_VALID`、`_ERROR`、`_MAJOR`、`_MINOR`、`_PATCH`、`_PRERELEASE`、`_CORE`）と `clargs_semver_compare(<a> <b> <out>)`（-1/0/1）。`CMAKE_MATCH_<n>` は前の一致の値が残ることがあるので注意する。FATAL_ERROR の文はCMake が折り返すので、子プロセスの出力を照合するときは行をつないでから比べる。
- 2.2: CHANGELOG は `tools/release/ClargsChangelog.cmake` の `clargs_changelog_parse`、`clargs_changelog_promote(<text> <version> <date> <repo-url> <out>)`、`clargs_changelog_notes(<text> <version> <repo-url> <out>)`。notes の結果は末尾に改行がないので、3.2 でファイルに書くときに足す。promote は V が最新の版より大きくなければ止まる。フィクスチャと実際の CHANGELOG では Semantic Versioning へのリンクを `https://semver.org/` にし、`spec` の語を避ける。
- 2.3: CMake 3.28 の `file(READ)` は CRLF を LF にする（`file(READ ... HEX)` のバイトには CR がある）。改行の形を保つ必要があるファイル（CHANGELOG.md、version.h）は、`tools/release/ClargsVersionHeader.cmake` の HEX と `string(ASCII)` による読み方（`_clargs_version_header_read_bytes`）を使うか共通化して読む。ヘッダは `clargs_version_header_write(<path> <major> <minor> <patch> <prerelease>)` が書き込みと `clargs_read_version` による読み直しまで行う（書く前の検査には `clargs_version_header_render`）。
- 2.4: `tools/ci/changes.cmake` は `-D CLARGS_CHANGED_FILES=<一覧のファイル>`、`-D CLARGS_CHANGES_OUTPUT=<出力ファイル>`（`$GITHUB_OUTPUT` をそのまま渡せる。`docs_only=<v>` を追記）。標準出力は message(STATUS) なので行頭に `-- ` が付く。終了コードは常に 0。
- 3.1: `tools/release/prepare.cmake` の入力は `CLARGS_RELEASE_VERSION`、`CLARGS_RELEASE_DATE`、`CLARGS_RELEASE_REPO_URL`、`CLARGS_SOURCE_DIR`、`CLARGS_RELEASE_TAGS_FILE`、`CLARGS_RELEASE_OUTPUT`（任意）、`CLARGS_RELEASE_CHECK_ONLY`（任意）。出力は `version=`、`tag=`、`prerelease=`。バイト単位の読み込みは `_clargs_prepare_read_bytes` に写しがある（`ClargsVersionHeader.cmake` の private 関数と重複。3.2 で共通化を検討）。
