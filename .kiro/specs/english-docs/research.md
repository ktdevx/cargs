# Research & Design Decisions

## Summary

- **Feature**: `english-docs`
- **Discovery Scope**: Extension（既存の文書の翻訳と再編。ライブラリのコードは変えない）。light discovery を実施した。
- **Key Findings**:
  - 日本語の文字を含む人向けのファイルは、README.md（66 行）と `docs/` の 5 ファイル（conventions 247 行、help 278 行、ownership 174 行、parse 256 行、schema 254 行）だけである。`include/`、`src/`、`tests/`、`cmake/`、`.github/`、ルートの CMake とコミュニティのファイルには、日本語の文字がない。ASCII 外の文字は `tests/public/command_test.c` の罫線だけである。
  - `.kiro` を含む人向けのファイルは `docs/ownership.md` だけで（「スキーマ、argv、結果への適用」の冒頭）、スペックの名前と「このスペック」「設計で決めた」という言い回しも `docs/ownership.md` だけにある（4 か所）。
  - ソースのコメントが文書を名前で指している箇所が 2 つある（`tests/public/parse_failure_test.c` と `tests/public/schema_failure_test.c` が、`docs/conventions.md` の「確保の各回を順に失敗させる」を英語で "the failure of every allocation in turn" と呼んでいる）。英語の見出しをこの言い回しに合わせれば、コメントは変えずに済む。

## Research Log

### 人向けのファイルの言語と `.kiro` の現状

- **Context**: 要件 1、2、10、11 の対象の大きさを知るため。
- **Sources Consulted**: `git ls-files` で、`.kiro/`、`.claude/`、CLAUDE.md を除いたすべてのファイルを、ひらがな・カタカナ・漢字・CJK の記号・全角形（U+3000–303F、U+3040–30FF、U+4E00–9FFF、U+FF00–FFEF）と、`kiro`（大文字小文字を区別しない）で走査した。
- **Findings**:
  - 日本語: 上の 6 ファイルだけ。コードブロックの中にも日本語がある（README 2 ブロック、conventions 5、help 1、ownership 7）。多くは C のコメントと、`<インストール先>` のような仮の値。
  - `.kiro`: `docs/ownership.md` 301 行目の 1 か所。
  - スペックへの言及: `docs/ownership.md` の 8、312、328、330–332 行目と、目次の 24 行目。
- **Implications**: ソースの点検（要件 10）は「見つからないことの確認」で済む。修正の作業は文書に集中する。

### 文書の中のリンクとアンカー

- **Context**: 要件 9。日本語の見出しから作られたアンカーは、英語化で全部変わる。
- **Sources Consulted**: README と `docs/*.md` の `](...)` を抽出した。GitHub の見出しアンカーの生成規則（小文字化、英数字・空白・ハイフン・下線以外の記号を除く、空白をハイフンにする、同じアンカーには `-1`、`-2` を付ける）。
- **Findings**:
  - 文書間のアンカー付きリンクは、schema → help（`#制限`、`#usage-行`、`#文面の規則`、`#構成`）、schema → parse（`#前提条件`、`#寿命の規則`）、schema/parse → ownership（`#実行時の失敗とプログラムの誤り`、`#5-アロケータ`）、parse → help（`#--help-とエラーの表示`）、parse → schema（`#検証と失敗の調べ方`、`#値型`、`#値型を借りる契約と破棄の順序`）、ownership → schema/parse/help、README → conventions（`#バージョンについての注意`）。
  - 文書ごとの翻訳を並行して進めると、相手の見出しの英訳が決まっていないとリンクを書けない。
  - バッククォートを含む見出し（`` `--help` の扱い ``）は、バッククォートが除かれ、`--help` のハイフンは残る（`#handling---help` のような形）。
- **Implications**: 設計で、相互参照される見出しを含む全見出しの英訳を固定する（design.md の「見出しの対応表」）。各翻訳はそれに従い、検査スクリプトが GitHub と同じ規則でアンカーを確かめる。

### 用語

- **Context**: 要件 5.3。文書の用語が公開ヘッダのコメントと食い違うと、利用者が混乱する。
- **Sources Consulted**: `include/clargs/*.h` のコメントの英語の用語を集計した。
- **Findings**: ヘッダは `positional`（150 回）、`value type`（71）、`preconditions`（69）、`arity`、`parse error`、`help options`、`usage line`、`write function`、`heading`、`annotation`、`description column`、`indent`、`width`、`negatable`、`choice` を使っている。
- **Implications**: 文書の英語の用語は、公開ヘッダのコメントの用語に合わせる（design.md の「用語の対応」）。

### README と CI、PR テンプレートの結び付き

- **Context**: 要件 3.5、7.1、Adjacent expectations。
- **Sources Consulted**: `.github/workflows/ci.yml`、`.github/pull_request_template.md`、`.github/ISSUE_TEMPLATE/config.yml`、SECURITY.md、CODE_OF_CONDUCT.md。
- **Findings**:
  - `ci.yml` の `quickstart` ジョブは「README の 3 つのコマンド」をそのまま実行するとコメントに書いている。
  - PR テンプレートは `https://github.com/ktdevx/cargs/blob/main/CONTRIBUTING.md` と `.../CHANGELOG.md` を指す。CHANGELOG はまだない（release-automation が作る）。
  - PR テンプレートのチェック項目は、ローカルでの 3 つのコマンド、テスト、文書の更新、CHANGELOG の `Unreleased`、英語の文書とコメント。
- **Implications**: README の 3 つのコマンドは変えない。CONTRIBUTING の PR の説明は、PR テンプレートの項目と一致させる。CHANGELOG とリリース手順書へのリンクは、実装の時点で未作成でありうる（要件 9.5）。

### release-automation との継ぎ目

- **Context**: Phase 2 の 2 つのスペックは並行して作られる。
- **Sources Consulted**: `.kiro/steering/roadmap.md` の Phase 2、`.kiro/specs/release-automation/brief.md`。
- **Findings**:
  - release-automation は CHANGELOG.md、リリース手順書（仮 `docs/releasing.md`）、SemVer とプレリリースの規則、`docs/conventions.md` の「バージョンについての注意」の内容を持つ。
  - release-automation の設計は、まだない（requirements の段階）。
  - 同じ `docs/conventions.md` を両スペックが触るので、ファイルのパスと節の見出しが、両者の約束になる。
  - （設計の後に確認）release-automation の設計は、`docs/releasing.md`（リリース手順書）と、新しい `docs/versioning.md`（バージョン方針、英語）を作り、`docs/conventions.md` の「バージョンについての注意」を自分の最後のタスクで、その時点のファイルの言語で編集する。README と CONTRIBUTING は変えない。required check の名前は `CI result`。
- **Implications**: english-docs は `docs/conventions.md` のパスを変えず、節の英語の見出しを `Version notes`（アンカー `#version-notes`）に固定する。リリース手順書 `docs/releasing.md` とバージョン方針 `docs/versioning.md` へリンクし、パスが変わったら再確認の対象にする。CONTRIBUTING は required check の名前を書かず、その名前の変更に左右されないようにする。

### バッジ

- **Context**: 要件 3.2。
- **Sources Consulted**: GitHub Actions のワークフローのステータスバッジの URL の形（`https://github.com/<owner>/<repo>/actions/workflows/<file>/badge.svg`）、shields.io の静的バッジ（`https://img.shields.io/badge/<label>-<message>-<color>`）。
- **Findings**: CI のバッジはワークフローのファイル名に結び付く。ライセンスのバッジは、静的バッジなら GitHub の API に依存しない。
- **Implications**: CI のバッジは `ci.yml` と `main` ブランチを指す。ライセンスのバッジは静的な `License: MIT` とし、LICENSE へリンクする。

## Architecture Pattern Evaluation

| Option | Description | Strengths | Risks / Limitations | Notes |
|--------|-------------|-----------|---------------------|-------|
| A. ファイル名を保つ | `docs/schema.md` などの名前を変えず、索引 `docs/README.md` で利用者向けと開発者向けを分ける | ソースのコメント、release-automation、外部からのリンクが切れない。差分が翻訳だけになり、レビューしやすい | ディレクトリで読者が分からない（索引で補う） | 採用 |
| B. `docs/guide/` と `docs/dev/` に分ける | 利用者向けと開発者向けをディレクトリで分ける | ディレクトリだけで読者が分かる | `docs/conventions.md` のパスが変わり、release-automation とテストのコメントに波及する。差分が大きい | 不採用 |
| C. CONTRIBUTING に規約をすべて入れる | `docs/conventions.md` をなくす | 開発者の入口が 1 つ | CONTRIBUTING が 700 行を超え、入口として読めない。release-automation の節の置き場所が変わる | 不採用 |

## Design Decisions

### Decision: 文書のファイル名を変えず、索引で分ける

- **Context**: 要件 6（構成と導線）と、release-automation との継ぎ目、ソースのコメント（要件 9.3、9.4）。
- **Alternatives Considered**:
  1. ファイル名を保ち、索引を足す
  2. ディレクトリで分ける
- **Selected Approach**: `docs/schema.md`、`parse.md`、`help.md`、`ownership.md` を利用者向け、`docs/conventions.md` と CONTRIBUTING.md を開発者向けとし、`docs/README.md` の索引で 2 つの群に分ける。
- **Rationale**: 読者の分離は文書の単位で満たせる（要件 6.1）。パスを変えないことで、`docs/conventions.md` を共有する release-automation との衝突を避ける。
- **Trade-offs**: パスだけでは読者が分からない。索引と各文書の冒頭で補う。
- **Follow-up**: release-automation の設計が `docs/conventions.md` のパスや節の見出しを変えないことを確かめる。

### Decision: `docs/ownership.md` の開発者向けの記述を `docs/conventions.md` へ移す

- **Context**: 要件 6.1、8.2。今の ownership.md は、利用者の規則と、ライブラリのソースを書く人の決まり（内部関数、開発者の側の例）を混ぜている。
- **Alternatives Considered**:
  1. 移す
  2. ownership.md の中で「For library developers」の節に寄せる
- **Selected Approach**: 開発者だけに向けた記述（下の design.md の一覧）を `docs/conventions.md` の新しい節 `Applying the ownership rules` へ移す。規則そのもの（1〜8）は ownership.md に残し、conventions.md から参照する。
- **Rationale**: 利用者向けの文書に内部ヘッダ（`alloc_internal.h`）の例が載らなくなる。規則の正本は 1 か所に残る。
- **Trade-offs**: 開発者は 2 つの文書を読む。conventions.md の冒頭が、先に ownership.md を読むよう案内しているので、導線は今と同じである。

### Decision: CONTRIBUTING は入口に徹し、詳しい規約は conventions.md に任せる

- **Context**: brief の「CONTRIBUTING をどこまで自己完結させるかは設計で決める」。
- **Selected Approach**: CONTRIBUTING.md は、必要なもの、ビルドとテスト（静的と動的）、変更の出し方、PR の流れ、CHANGELOG の案内、英語の決まり、行動規範と脆弱性の報告、リリース手順書への案内を持つ。命名、手順、実装の規則、テストの注意、確かめ方の詳細は conventions.md へリンクする。
- **Rationale**: 重複を避け、規約の正本を 1 つにする。CONTRIBUTING は 1 画面〜数画面で読める長さに保つ。

### Decision: 見出しの英訳を設計で固定する

- **Context**: 並行して翻訳する文書どうしのアンカー（要件 9.2）と、テストのコメントが指す節（要件 9.4）。
- **Selected Approach**: design.md に全文書の見出しの対応表を置く。翻訳はこれに従う。変えるときは表を直し、それを指すリンクも同じ変更で直す。

### Decision: 検査は使い捨てのスクリプトで行い、リポジトリに足さない

- **Context**: 要件 11。CI のジョブの追加（リンク検査など）は Phase 2 の対象外である。
- **Alternatives Considered**:
  1. リポジトリに `scripts/` を足し、CI で回す
  2. スペックのディレクトリに検査スクリプトを置き、実装と検証で実行する
- **Selected Approach**: `.kiro/specs/english-docs/check_docs.py`（Python 3 の標準ライブラリだけ）を置き、タスクと最終確認で実行する。
- **Rationale**: 人向けのファイルを増やさず、CI の範囲を広げない。繰り返し実行できる記録になる（要件 11.5）。
- **Trade-offs**: 将来の変更で日本語が混じっても CI は気づかない。CI への追加は別の機会とする。

### Synthesis

- **Generalization**: 「日本語の検出」「`.kiro` の検出」「相対リンクの検査」「アンカーの検査」は、どれも「人向けのファイルの集合に対する行単位またはリンク単位の検査」である。1 つのスクリプトに、対象の集合の決め方を 1 か所で持たせる。
- **Build vs. Adopt**: リンク検査には lychee や markdown-link-check があるが、ネットワークや Node の導入を要し、日本語と `.kiro` の検出はできない。標準ライブラリの Python で 150 行程度の検査を書くほうが小さい。アンカーの生成規則は GitHub の規則をそのまま実装する。
- **Simplification**: 例のコンパイルの確認は、恒久的なテストにせず、検証のタスクの中で取り出してビルドする手順にとどめる（CI のジョブの追加は対象外）。索引は `docs/README.md` 1 つにし、各文書の冒頭の案内と重複しない短い一覧にする。

## Risks & Mitigations

- 翻訳で意味が変わる — 見出しの対応表と用語の対応で表現を揃え、各文書の翻訳のタスクで、節ごとに元の日本語と照らすレビューを行う。関数名、エラーコード、出力の文面は書き換えずに写し、検証で例をビルドして実行する。
- release-automation と `docs/conventions.md` が衝突する — パスと `Version notes` の見出しを固定し、その節は意味を変えずに訳すだけにする。どちらが先に入っても、後の側が今の内容を基に作業する。
- CHANGELOG.md とリリース手順書が未作成のままリンクする — 検査スクリプトに「release-automation が作るパス」の一覧を持たせ、未作成なら失敗ではなく保留として報告する（要件 9.5）。
- README の FetchContent の例のタグ `v0.1.0` は、最初のリリースまで存在しない — Releases の一覧へ案内し、最新のタグに置き換えるよう書く。最初のリリースで解決する。
- 文書と実装の食い違いを翻訳中に見つける — 実装（公開ヘッダのコメント、テスト）を正として文書を直し、実装の記録に残す。実装は変えない。

## References

- [GitHub Docs: Basic writing and formatting syntax — Section links](https://docs.github.com/en/get-started/writing-on-github/getting-started-with-writing-and-formatting-on-github/basic-writing-and-formatting-syntax#section-links) — 見出しからのアンカーの生成規則
- [GitHub Docs: Adding a workflow status badge](https://docs.github.com/en/actions/monitoring-and-troubleshooting-workflows/monitoring-workflows/adding-a-workflow-status-badge) — CI のバッジの URL の形
- [Shields.io static badges](https://shields.io/badges/static-badge) — ライセンスのバッジ
- [GitHub Docs: Setting guidelines for repository contributors](https://docs.github.com/en/communities/setting-up-your-project-for-healthy-contributions/setting-guidelines-for-repository-contributors) — CONTRIBUTING.md をルートに置くと PR と Issue の画面から案内される
- `.kiro/steering/roadmap.md`（Phase 2）、`.kiro/specs/release-automation/brief.md` — 継ぎ目の決定
