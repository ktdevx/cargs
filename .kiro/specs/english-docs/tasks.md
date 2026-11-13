# Implementation Plan

どのタスクも、設計の「翻訳の規則」「用語の対応」「文書の形」「見出しの対応表」に従う。関数名、型名、マクロ名、エラーコード、CMake の変数とターゲット、コマンド、ライブラリが出力する文面、コードの本体は、1 文字も変えずに写す。訳すのは、本文、見出し、表、リンクの文字列、コードブロックの中のコメントと仮の値だけである。人向けの文書には、`.kiro`、スペック、スペックの名前、ステアリング、Kiro の手順を書かない。訳す途中で文書と実装（公開ヘッダのコメント、テスト、CMake の定義）の食い違いに気づいたら、実装に合わせて書き、タスクの記録に「どの記述を、何に合わせて直したか」を残す。実装は変えない。

ほかの文書の節へのリンクは、相手の翻訳を待たずに、見出しの対応表の英語のアンカーで書く。相手がまだ日本語の間、そのアンカーは検査で未解決と報告されるが、各文書のタスクでは、自分の文書の中の見出しと、自分の文書から出るリンクの書き方だけを確かめる。全体の解決は 5.2 で確かめる。

このスペックは release-automation に依存し、release-automation の実装の後に始める。そのため、作業の開始時点で CHANGELOG.md、`docs/releasing.md`、`docs/versioning.md`（英語）、`tools/release/*`、`tools/ci/changes.cmake`、`tests/release/*` と `tests/CMakeLists.txt` でのその登録、`.github/workflows/` の `ci.yml`、`release-pr.yml`、`release-publish.yml`、「バージョンについての注意」をプレリリースの内容で更新した（日本語のままの）`docs/conventions.md` が既にある。どのタスクも、CHANGELOG.md、`docs/releasing.md`、`docs/versioning.md`、`.github/`、`tools/`、`tests/release/` を編集しない。それらに見つけた問題は、release-automation への指摘としてタスクの記録に書く。

「英語化の前のコミット」（`<base>`）は、このスペックの最初の実装のタスク（1.1）の直前のコミットを指す。release-automation のファイルは `<base>` に既にある。原文を読むとき（特に 3.2 で ownership.md から移す記述）は、そのコミットの版（`git show <base>:<path>`）を使う。

- [x] 1. 土台: 検査スクリプト
- [x] 1.1 言語、AI 向けの作業領域への言及、リンク、アンカーを調べる検査を作る
  - 設計の DocsChecker の `check` の契約どおりに、スペックのディレクトリに検査スクリプトを作る。Python 3 の標準ライブラリだけを使い、ファイルを変更しない
  - 対象の集合は、リポジトリで管理しているファイルから `.kiro/` と `.claude/` の下と CLAUDE.md を除いたもの、人向けの文書はそのうち README.md、CONTRIBUTING.md、`docs/` の下の Markdown とし、決め方を 1 か所に置く
  - `japanese`、`kiro`、`spec-term`、`missing-file`、`missing-anchor`、`pending` の 6 種を、`<path>:<line>: <kind>: <detail>` の形で報告し、件数の要約を出す。`pending` 以外が 1 件以上なら終了コード 1、なければ 0 を返す。`--files` で報告を指定のファイルに限れる
  - アンカーは GitHub の規則（小文字化、英数字・空白・`-`・`_` 以外の除去、空白を `-`、重複に `-1`、`-2`）で、コードブロックの外の見出しから作る。`CHANGELOG.md`、`docs/releasing.md`、`docs/versioning.md` を指すリンクで、指す先がないものは `pending` にする（release-automation が先に実装されるので通常は出ない、安全網である）
  - `spec-term` の `\bspecs?\b` は語の単位で照合し、`specification` などに一致させない。人向けの文書には release-automation の `docs/versioning.md` と `docs/releasing.md` も含め、release-automation のパスの一覧（CHANGELOG.md、この 2 文書、`tools/`、`tests/release/`、`.github/workflows/`）の中の報告は、要約で release-automation への指摘として分けて数える
  - 完了の目安: 英語化の前の作業ツリー（release-automation の実装の後）で実行すると、日本語の行を README.md と `docs/` の 5 ファイルについてだけ報告し（計画の時点の件数は README 66、conventions 247、help 278、ownership 174、parse 256、schema 254 行。conventions は release-automation が「バージョンについての注意」に足した行の分だけ多くなる）、release-automation のファイルからは日本語も `pending` も報告せず、`specification` を `spec-term` として報告しない。`kiro` を `docs/ownership.md` の 1 行だけ報告し、`tests/public/command_test.c` の罫線を報告しない。日本語の見出しへの今のアンカー付きリンクを、`missing-anchor` として報告しない（今の文書の中では解決している）
  - _Requirements: 1.2, 2.1, 2.2, 9.1, 9.2, 9.5, 10.1, 10.3, 10.4, 11.1, 11.2, 11.4, 11.5, 11.6, 11.7_

- [x] 1.2 文書の C の例を取り出す機能を足し、英語化の前の例で手順を確かめる
  - 検査スクリプトに、設計の DocsChecker の `extract` の契約どおりの機能を足す。人向けの文書の C のコードブロックのうち `main` または `wmain` を定義するものを、出力先のディレクトリに 1 つずつ書き出し、入口の種類と元の位置を一覧のファイルに書く
  - スクラッチのディレクトリで、テストを無効にしてリポジトリをビルドしてインストールし、取り出した `main` の例を `-std=c99 -Wall -Wextra -Wpedantic -Werror` でコンパイルしてリンクする手順を作る。`wmain` の例はコンパイルだけを行う。ownership のアロケータの例は、利用者の 4 つの関数を定義した小さなファイルと一緒にリンクする。手順はタスクの記録に、繰り返せるコマンドとして残す
  - README の例を README の実行の例のコマンドラインで実行し、出力と終了の仕方を README と比べる。schema、parse、help の例も、それぞれの文書が示す出力と比べる
  - 完了の目安: 英語化の前の文書から 7 つの例（README 1、schema 1、parse 2 のうち `wmain` 1、help 2、ownership 1）が取り出され、すべてコンパイルでき、`main` の例が文書の示す出力と終了コードのとおりに動く。違いがあれば、その例と違いがタスクの記録にある。release-automation の文書から取り出された例があれば、7 つに数えず、失敗しても直さずに release-automation への指摘として記録する
  - _Requirements: 3.4, 5.4, 11.5_

- [x] 2. 利用者向けの API の説明を英語にする
- [x] 2.1 (P) スキーマの文書を英語にする
  - スキーマの文書を、節、例、表、一覧の順序と数を保って英語にする。見出しは対応表の schema の行に、題は `Schemas` にする
  - 冒頭に、扱うこと、扱わないこと、関係する文書（parse、help、ownership）へのリンク、詳しい説明が公開ヘッダのコメントにあることを置き、`Contents:` の目次を置く
  - 一連の例のコードは変えず、コメントだけを訳す。誤りの一覧の表のエラーコードと説明は、公開ヘッダと一致させる
  - 完了の目安: この文書について検査を実行すると、`japanese`、`kiro`、`spec-term` が 0 件で、この文書の中の節へのアンカーがすべて解決する。ほかの文書へのリンクのアンカーは、すべて対応表の英語のアンカーである。英語化の前の版と比べて、コードブロックの差分がコメントだけである
  - _Boundary: SchemaGuide_
  - _Requirements: 1.1, 1.4, 1.5, 2.2, 5.1, 5.2, 5.3, 5.5, 6.4, 6.5, 9.1, 9.2_

- [x] 2.2 (P) 解析の文書を英語にする
  - 解析の文書を、節、例、表、一覧の順序と数を保って英語にする。見出しは対応表の parse の行に、題は `Parsing` にする
  - 引数の書き方の一覧、解析エラーの種類とメッセージの表は、メッセージの文面とエラーの種類の名前を実装と 1 文字も違えずに写す
  - 2 つの例（`main` と `wmain`）のコードは変えず、コメントだけを訳す
  - 完了の目安: この文書について検査を実行すると、`japanese`、`kiro`、`spec-term` が 0 件で、この文書の中の節へのアンカーがすべて解決する。ほかの文書へのリンクのアンカーは、すべて対応表の英語のアンカーである。英語化の前の版と比べて、コードブロックの差分がコメントだけである
  - _Boundary: ParseGuide_
  - _Requirements: 1.1, 1.4, 1.5, 2.2, 5.1, 5.2, 5.3, 5.5, 6.4, 6.5, 9.1, 9.2_

- [x] 2.3 (P) ヘルプの文書を英語にする
  - ヘルプの文書を、節、例、表、一覧の順序と数を保って英語にする。見出しは対応表の help の行に、題は `Help and Usage` にする。冒頭の用語の表は、用語の対応の英語に合わせる
  - 文面の規則の節に載る生成された文面（usage 行、見出し、注記、折り返しの例）は、ライブラリが出力するものなので、1 文字も変えない
  - 2 つの例のコードは変えず、コメントだけを訳す
  - 完了の目安: この文書について検査を実行すると、`japanese`、`kiro`、`spec-term` が 0 件で、この文書の中の節へのアンカーがすべて解決する。ほかの文書へのリンクのアンカーは、すべて対応表の英語のアンカーである。英語化の前の版と比べて、コードブロックの差分がコメントだけである
  - _Boundary: HelpGuide_
  - _Requirements: 1.1, 1.4, 1.5, 2.2, 5.1, 5.2, 5.3, 5.5, 6.4, 6.5, 9.1, 9.2_

- [x] 2.4 (P) 所有権と寿命の文書を、利用者向けの英語の文書にする
  - 所有権と寿命の文書を、題 `Ownership and Lifetime` と、対応表の ownership の行の見出しで英語にする。規則 1〜8、規約の一覧、実行時の失敗とプログラムの誤りの表、スキーマ・argv・結果とヘルプへの適用の表は、すべて残す
  - 設計の「ownership.md から除く記述と、書き換える記述」の表のとおりに、開発者だけに向けた記述（規則 6 の作らない形の表、規則 7 の開発者の側の例とその後の 3 項目、規則 8 の開発者向けの文、内部ユーティリティの表と 2 つの違いとグローバル状態の段落）を除き、`.kiro` とスペックへの言及を書き換える。除いた記述は、この文書の中にも、ほかの場所にも書かない（移すのは 3.2 である）
  - 最後の節は `Allocator and error functions` とし、`clargs_set_allocator` と `clargs_error_message` の表だけを残す。冒頭で、ライブラリのコードが規則をどう実装するかは開発者向けの規約の `#applying-the-ownership-rules` にあると 1 文で示す
  - アロケータの例のコードは変えず、コメントだけを訳す
  - 完了の目安: この文書について検査を実行すると、`japanese`、`kiro`、`spec-term` が 0 件で、この文書の中の節へのアンカーがすべて解決する。文書に内部ヘッダの名前（`alloc_internal.h`、`str.h`、`vec.h`）と内部ユーティリティの表が残っていない。規則 1〜8 の見出しと、2 つの適用の表が揃っている
  - _Boundary: OwnershipGuide_
  - _Requirements: 1.1, 1.4, 1.5, 2.2, 2.3, 2.4, 5.1, 5.2, 5.3, 5.5, 6.1, 6.4, 6.5, 8.2, 9.1, 9.2_

- [x] 3. 開発者向けの文書
- [x] 3.1 (P) 開発者向けの規約を英語にする
  - 開発者向けの規約の文書を、題 `Development Conventions` と、対応表の conventions の行の見出しで、節の順序と数を保って英語にする。命名規約の表、手順の表、`clargs_add_test()` の規則、実装の規則、テストを書くときの注意、確かめ方を、すべて訳す
  - C、CMake、シェルの例は、コメントと仮の値の表記（`<名前>` → `<name>`、`<動的ライブラリのパス>` → `<path-to-shared-library>` など）だけを訳し、コードを変えない
  - 確保の各回を順に失敗させる節の見出しを `The failure of every allocation in turn` にし、テストのコメント 2 か所の言い回しと一致させる
  - `Version notes` の節は、release-automation が日本語のまま更新した後の内容を、項目の数と順序を保ち、足さず減らさずに訳す。既存の 3 項目に加え、プレリリースの内容（`CLARGS_VERSION_PRERELEASE` の書式、バージョンをリリースのワークフローで上げること、パッケージでの扱い、`docs/versioning.md` と `docs/releasing.md` へのリンク）も忠実に訳す。見出しは `Version notes`、アンカーは `#version-notes` から変えない
  - 完了の目安: この文書について検査を実行すると、`japanese`、`kiro`、`spec-term` が 0 件で、この文書の中の節へのアンカーがすべて解決する。`#the-failure-of-every-allocation-in-turn` と `#version-notes` のアンカーが存在する。英語化の前の版と比べて、コードブロックの差分がコメントと仮の値だけで、`Version notes` の項目の数が同じである
  - _Boundary: DevConventions_
  - _Requirements: 1.1, 1.4, 1.5, 2.2, 6.4, 6.5, 8.1, 8.4, 8.5, 8.6, 8.8, 9.1, 9.2, 9.4, 9.7_

- [x] 3.2 所有権の規則を実装に当てはめる節を開発者向けの規約に足し、ディレクトリ構成を更新する
  - 実装の規則の中、メモリの確保の節の後に `Applying the ownership rules` の節を足し、英語化の前のコミットの所有権の文書から、設計の表で「conventions へ移す」とした記述を、意味を変えずに英語にして置く。小見出しは対応表の新規の 3 つ（作らない形、失敗の返し方、内部ユーティリティの従い方）とする
  - 規則 8 の開発者向けの文は、既存の `Global state` の節に統合する。既に同じ内容がある文は足さず、同じ見出しを 2 つ作らない
  - 移した例（仮の名前の `clargs_example_create` と `clargs_example_destroy`）のコードは変えず、コメントだけを訳す。所有権の規則そのものは、所有権の文書の規則の節（`#6-whoever-allocates-frees` など）へリンクして参照する
  - ディレクトリ構成の節（このスペックが内容まで持つ）を、作業の時点の `git ls-files` と照らし、条件を付けずにすべてを載せるように更新する。`docs/` の一覧に索引の `README.md`、`versioning.md`、`releasing.md` を足し、各ファイルの説明を利用者向けか開発者向けかが分かる英語にする。CHANGELOG.md、`tools/release/`、`tools/ci/`、`tests/release/`、`.github/workflows/` の `ci.yml`、`release-pr.yml`、`release-publish.yml` を各 1 行の役割とともに載せ、`tests/CMakeLists.txt` が release のテストを `release.<name>` の名前と `release` のラベルで登録していることを書く。冒頭の導入で、コントリビューターの入口がルートの CONTRIBUTING.md であることに触れる
  - 完了の目安: 開発者向けの規約に `#applying-the-ownership-rules` とその 3 つの小見出しのアンカーがあり、設計の「ownership.md から除く記述と、書き換える記述」の表で conventions へ移すとした記述のすべてが、この文書のどこかに英語で 1 回ずつある。`#global-state` は 1 つだけである。ディレクトリ構成の節が、上に挙げた release-automation のファイルとディレクトリをすべて載せ、それらへの相対リンクが解決する。この文書について検査を実行すると、`japanese`、`kiro`、`spec-term`、`missing-file`、`pending` が 0 件である
  - _Depends: 3.1_
  - _Requirements: 1.4, 6.1, 8.2, 8.3, 8.7, 9.1, 9.2, 9.7_

- [x] 3.3 (P) CONTRIBUTING.md を作る
  - リポジトリのルートに CONTRIBUTING.md を作り、設計の「CONTRIBUTING の構成」の 11 の部分を、その順に書く
  - 必要なもの（CMake 3.21 以上、C99 のコンパイラ、Naos の取得とネットワーク）と対応する環境、README の 3 つのコマンド、静的と動的の両方の確認のコマンド（開発者向けの規約の確かめ方と同じコマンド）を載せ、詳しい確かめ方は `docs/conventions.md#verifying-a-change` へ案内する
  - 変更の出し方（Issue での相談、フォークとブランチ、テストの追加、ローカルでの確認、PR テンプレートの記入、CI の合格、レビュー）を、PR テンプレートのチェック項目と矛盾しないように書く。CI のジョブやチェックの名前は書かない
  - 開発者向けの規約、所有権の文書、CODE_OF_CONDUCT.md、SECURITY.md、CHANGELOG.md、`docs/releasing.md`、`docs/versioning.md`、LICENSE へリンクする。CHANGELOG の記録の規則とリリースの手順の中身は書かず、リンクで示すだけにする。文書とコメントは英語で書くという決まりを書く
  - 完了の目安: CONTRIBUTING.md について検査を実行すると、`japanese`、`kiro`、`spec-term`、`missing-file`、`pending` が 0 件で（CHANGELOG.md、`docs/releasing.md`、`docs/versioning.md` は既にあるので解決する）、開発者向けの規約へのアンカーはすべて対応表の英語のアンカーである。PR テンプレートの CONTRIBUTING.md の URL が指すパスにファイルがある
  - _Boundary: Contributing_
  - _Requirements: 1.1, 2.2, 2.4, 6.4, 6.5, 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7, 7.8, 7.9, 7.10, 9.1, 9.2, 9.5, 9.7_

- [ ] 4. 入口の文書
- [x] 4.1 (P) README を OSS の入口として英語で書き直す
  - 設計の「README の構成」の 11 の部分を、その順に書く。CI のバッジとライセンスのバッジは設計の URL で置く
  - 特徴の一覧は今の 7 項目をすべて訳す。サンプルプログラムと実行の例は、コードと出力を変えずに写し、例の後の注意を訳す
  - ビルドとテストの 3 つのコマンドを、この順に、引数を変えずに載せる。Naos とネットワーク、ネットワークなしの構成、テストの無効化、複数の構成を持つジェネレータ、CMake のオプションの表と注意、`BUILD_SHARED_LIBS` を訳す
  - 4 つの取り込み方法と、組み込んだ CLArgs のテストの実行を訳す。CMake の例は `GIT_TAG` の行だけを `v0.1.0` に変え、ほかのコードは変えない。FetchContent の例の後に、最新のリリースのタグへの置き換え、タグが `v<SemVer>` の形であること（例 `v0.2.0`。プレリリースは `v0.2.0-rc.1` など。Releases and changes の節と同じ書き方）、Releases のページ、タグかコミットのハッシュへの固定を案内する。find_package のバージョンの注意は、利用者向けの互換の規則として `docs/versioning.md` へリンクし、開発者向けの補足として `docs/conventions.md#version-notes` へのリンクも置く
  - 文書の一覧（利用者向けの 4 文書、索引、CONTRIBUTING.md、開発者向けの規約）、Releases and changes（CHANGELOG.md、Releases のページ、タグの形、バージョン方針の文書へのリンクだけ）、Contributing（CONTRIBUTING.md、CODE_OF_CONDUCT.md、SECURITY.md）、License を置く
  - 完了の目安: README について検査を実行すると、`japanese`、`kiro`、`spec-term`、`missing-file`、`pending` が 0 件で（CHANGELOG.md と `docs/versioning.md` へのリンクが解決する）、README の中の節へのアンカーがすべて解決する。FetchContent の後の案内と Releases and changes の節のタグの書き方が一致する。README から、利用者向けの 4 文書、索引、CONTRIBUTING.md、開発者向けの規約へ、それぞれ 1 回のリンクで移れる。英語化の前の版と比べて、コードブロックの差分が `GIT_TAG` の行とコメントと仮の値だけである
  - _Boundary: ReadmeEntry_
  - _Requirements: 1.1, 1.4, 1.5, 2.2, 3.1, 3.2, 3.3, 3.5, 3.6, 3.7, 3.8, 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 4.7, 6.3, 6.4, 9.1, 9.2, 9.5, 9.6, 9.7_

- [ ] 4.2 (P) 文書の索引を作る
  - `docs/` に索引の文書を作り、設計の「索引の構成」のとおり、利用者向け（schema、parse、help、ownership、バージョン方針の文書と、読む順）、コントリビューター向け（CONTRIBUTING.md、開発者向けの規約）、メンテナー向け（リリース手順書、CHANGELOG.md）の 3 つの群に、各文書の読者と内容の 1 行の説明を付けて並べる
  - 完了の目安: 索引について検査を実行すると、`japanese`、`kiro`、`spec-term`、`missing-file`、`pending` が 0 件で（リリース手順書、バージョン方針の文書、CHANGELOG.md へのリンクが解決する）、`docs/` のすべての文書と CONTRIBUTING.md が索引から 1 回のリンクで開ける
  - _Boundary: DocsIndex_
  - _Requirements: 1.1, 2.2, 6.2, 6.3, 6.4, 9.1, 9.5, 9.7_

- [ ] 5. 統合と検証
- [ ] 5.1 ソース、テスト、ビルド、ワークフローのファイルの言語を点検する
  - 検査を、`include/`、`src/`、`tests/`、`cmake/`、`.github/`、`tools/` の下のファイルと、ルートの CMakeLists.txt に限って実行する
  - release-automation のファイル（`tools/` の下、`tests/release/` の下、`.github/workflows/` の下）に日本語の文字や `.kiro` が見つかっても直さず、ファイル名と行番号を release-automation への指摘としてタスクの記録に書く
  - 日本語の文字が見つかれば、そのコメントと文字列だけを意味を変えずに英語にし、静的と動的の両方の構成でビルドしてテストを通す。コードとテストの期待値は変えない。見つからなければ、何も変えない
  - 完了の目安: それらのファイルについて、`japanese` と `kiro` が 0 件であることを示す検査の出力がタスクの記録にある。罫線の文字のある `tests/public/command_test.c` は報告されない
  - _Requirements: 10.1, 10.2, 10.3, 10.4, 2.1_

- [ ] 5.2 人向けのファイル全体の検査を通す
  - 2〜4 の成果がそろった作業ツリーで、検査を対象の制限なしに実行する
  - 文書をまたぐアンカーの未解決があれば、見出しの対応表と照らして、リンクか見出しを直す。対応表を変えたときは、その見出しを指すすべてのリンクを同じ変更で直す
  - README から、どの利用者向けの文書にも開発者向けの文書にも、2 回以内のリンクの移動でたどれることを、リンクを順にたどって確かめる
  - 検査は release-automation の `docs/versioning.md` と `docs/releasing.md` も走査する。それらと CHANGELOG.md、`tools/`、`tests/release/`、`.github/workflows/` についての報告は、このスペックの境界の外なので直さず、ファイル名、行番号、種類を release-automation への指摘としてタスクの記録に書く。`spec-term` が `specification` のような語に一致していれば、検査の誤りとして検査スクリプトを直す
  - 完了の目安: 検査が、このスペックが変更するファイルについて 0 件を報告し、`pending` が 0 件である（release-automation の文書へのリンクはすべて存在するファイルとして解決する）。release-automation のファイルについての報告がなければ終了コード 0 を返し、あればその一覧が指摘としてタスクの記録にある。`pending` が出た場合は、release-automation の File Structure Plan とパスを照らし、こちらの誤りならリンクを直し、ファイルが本当にないなら release-automation に報告する
  - _Depends: 2.1, 2.2, 2.3, 2.4, 3.2, 3.3, 4.1, 4.2_
  - _Requirements: 1.1, 1.2, 2.1, 2.2, 6.3, 9.1, 9.2, 9.5, 9.7, 11.1, 11.2, 11.3, 11.4, 11.6, 11.7_

- [ ] 5.3 英語にした文書の例をビルドして動かす
  - 1.2 の手順を、英語にした後の文書に対して繰り返す。取り出した `main` の例を警告をエラーにしてコンパイルしてリンクし、`wmain` の例はコンパイルだけを行う
  - README の例を、README の実行の例の 4 つのコマンドラインで実行し、出力と終了の仕方を README と比べる。schema の例が `the schema of pack is valid` を出して 0 で終わること、parse と help の例が文書の示す出力のとおりであること、ownership の例が 0 で終わることを確かめる
  - 違いがあれば、翻訳の写し間違いなら文書を直す。1.2 の時点で既にあった違いなら、実装に合わせて文書を直し、記録に残す
  - 完了の目安: 7 つの例がすべてコンパイルでき、`main` の例がすべて文書の示す出力と終了コードのとおりに動く
  - _Depends: 1.2, 5.2_
  - _Requirements: 3.4, 5.3, 5.4_

- [ ] 5.4 リポジトリのビルドと、変更の範囲を確かめる
  - README の 3 つのコマンド（`cmake -S . -B build`、`cmake --build build`、`ctest --test-dir build`）を新しいビルドディレクトリで実行する。構成のときに Naos を GitHub から取得するので、ネットワークが要る。取得できない環境なら、そのことと、`FETCHCONTENT_SOURCE_DIR_NAOS` で手元のコピーを渡した場合の結果を記録する
  - `<base>`（このスペックの最初の実装のタスクの直前のコミット。release-automation のファイルは既にある）との差分のファイルの一覧を取り、README.md、CONTRIBUTING.md、`docs/README.md`、`docs/schema.md`、`docs/parse.md`、`docs/help.md`、`docs/ownership.md`、`docs/conventions.md`（5.1 で直したものがあればそのファイル）だけであることを確かめる。CHANGELOG.md、`docs/releasing.md`、`docs/versioning.md`、CODE_OF_CONDUCT.md、SECURITY.md、`.github/` の下、`tools/` の下、`include/`、`src/`、`tests/`（`tests/release/` と `tests/CMakeLists.txt` を含む）、`cmake/`、CMakeLists.txt が含まれないことを確かめる。`docs/conventions.md` の差分が、英語化、ディレクトリ構成の節の更新、3.2 で移した記述だけであることを確かめる
  - `docs/` の既存の 5 ファイルの名前が変わっていないこと、日本語版のファイルや節が足されていないことを確かめる
  - 完了の目安: 3 つのコマンドが成功し、差分のファイルの一覧が上の範囲に収まり、その一覧とコマンドの結果がタスクの記録にある
  - _Requirements: 1.3, 9.3, 12.1, 12.2, 12.3, 12.4_

## Implementation Notes
- 1.1: 検査は `python3 .kiro/specs/english-docs/check_docs.py check [--files ...]`。`--files` は `git ls-files` にないパスを終了コード 2 で拒む。新しく作る文書（CONTRIBUTING.md、docs/README.md）は、検査の前に `git add -N <path>` で対象に入れる。
- 1.1: release-automation に分類するパスは要件 10.4 の一覧（設計の一覧より広い。`cmake/`、ルートの CMakeLists.txt、`tests/CMakeLists.txt`、`tests/consumer/`、`tests/public/version_test.c`、`include/clargs/version.h` を含む）。`pending` の対象は 3 文書のまま。アンカーの断片は大文字小文字を区別する。
- 1.1: 英語化の前の件数は japanese README 66、conventions 254（release-automation が 7 行増やした）、help 278、ownership 174、parse 256、schema 254。kiro は ownership.md:301 の 1 件、spec-term は ownership.md の 5 件。
- 1.2: 例の手順は `bash .kiro/specs/english-docs/build_examples.sh <repo-root> <outdir>`（テストを無効にしてビルドとインストール、例の取り出し、`-std=c99 -Wall -Wextra -Wpedantic -Werror` でのコンパイル、`wmain` はコンパイルだけ、ownership は利用者の 4 関数と一緒にリンク、直後の出力のブロックとの比較）。例の集合 7 つと README の 4 つのコマンドラインが欠けると失敗する。英語化の前の文書で 7 例、28 の確認がすべて通り、違いはなかった。テストは `python3 .kiro/specs/english-docs/test_check_docs.py`（`unittest discover` は `__pycache__` を残すので使わない）。
- 1.2: 翻訳のときは、例のコードブロックの info（```` ```c ````）と、出力のブロック（info なし、例との間に見出しを置かない）、README の `$ ` の行を変えない。手順はこの構造に頼っている。
- 2.3: `docs/help.md` の Limits の「幅はバイトで数える」例は、日本語の説明を使っていた。要件 1.2 のため、説明をギリシャ語（UTF-8 で 1 文字 2 バイト）に替え、出力を実際のライブラリで作り直した（同じ点を示す）。生成された文面を手で変えたのではない。
- 2.4: ownership.md から除いた記述（3.2 で conventions に移す。原文は `git show ff2b83c:docs/ownership.md`）: 5–8 行（読者の一覧の開発者の項）、214–219（規則 6 の作らない形の表）、233–288（規則 7 の開発者の側の例と 3 項目）、297（規則 8 の開発者向けの文）、341–354（内部ユーティリティの表、2 つの違い、グローバル状態の段落）。設計の表にない 79–83「開発者が守ること」、136、143 は、利用者向けの保証として書き換えて残した（conventions に既に同じ規則がある）。
- 3.1: conventions の見出しが英語になったので、README.md:263 の `#バージョンについての注意` へのリンクは 4.1 で `#version-notes` に直すまで切れている。
- 3.2: conventions の 3 件の missing-file（`../CONTRIBUTING.md` ×2、`docs/README.md`）は 3.3 と 4.2 で解決する。5.2 で 0 件を確かめる。
