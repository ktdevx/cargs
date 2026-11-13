# CLArgs の解析の使い方

検証済みのスキーマと argv を渡して解析し、結果から値を読み、破棄する方法をまとめる。プログラムの利用者がコマンドラインに書ける形（引数の書き方）と、引数がスキーマに合わないときに返る解析エラーも、ここにまとめる。

この文書が扱うのは、解析、結果の読み取り、解析エラー、結果の破棄である。スキーマの定義と検証は、[スキーマの使い方](schema.md)にある。ヘルプと usage 行の文面の生成は、[ヘルプの使い方](help.md)にある。解析は、何も出力せず、プロセスを終了させず、環境変数を読まない。

関数ごとの詳しい説明と前提条件は、公開ヘッダ `include/clargs/parse.h` のコメントにある。メモリの持ち方の決まりは、[所有権と寿命の規約](ownership.md)にある。

目次:

- [一連の例](#一連の例)
- [解析の入口](#解析の入口)
- [引数の書き方](#引数の書き方)
- [必須の検査と既定値](#必須の検査と既定値)
- [結果の読み取り](#結果の読み取り)
- [解析エラー](#解析エラー)
- [`--help` の扱い](#--help-の扱い)
- [ワイド文字の argv](#ワイド文字の-argv)
- [寿命の規則](#寿命の規則)
- [前提条件](#前提条件)

## 一連の例

`pack` という架空のプログラムで、スキーマの検証から、解析、`--help` の確認、解析エラーの表示、値の読み取り、破棄までを行う。次の形のコマンドラインを受け付ける。

```
pack [-h] [-v]... [--color | --no-color] create [-h] -o FILE [-l LEVEL] file...
pack [-h] [-v]... [--color | --no-color] list [-h] archive [pattern]
```

例は 1 つの翻訳単位で、取り込むのは `<clargs/clargs.h>` と標準ヘッダだけである。取り出して、そのままコンパイルできる。

```c
#include <clargs/clargs.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* The schema of pack, and the handles that the result is read with. The
 * root owns all of them. */
typedef struct pack_schema {
    clargs_command_t *root;
    clargs_command_t *create;
    clargs_command_t *list;
    clargs_option_t *verbose;     /* pack: -v, --verbose; a count */
    clargs_option_t *color;       /* pack: --color, --no-color */
    clargs_option_t *output;      /* create: -o, --output FILE; required */
    clargs_option_t *level;       /* create: -l, --level LEVEL; 6 if not given */
    clargs_positional_t *files;   /* create: file... */
    clargs_positional_t *archive; /* list: archive */
    clargs_positional_t *pattern; /* list: [pattern]; "*" if not given */
} pack_schema_t;

/* Keeps the result of a call in *error and tells whether the call failed. */
static bool failed(clargs_error_t *error, clargs_error_t result)
{
    *error = result;
    return result != CLARGS_ERROR_NONE;
}

/* Adds -h, --help to command. An option is only known where its command is
 * the current command, so every command gets its own. */
static clargs_error_t add_help(clargs_command_t *command)
{
    return clargs_command_add_flag(command, 'h', "help", NULL);
}

/* Defines the schema. When it fails, schema->root is NULL or a schema that
 * can be destroyed. */
static clargs_error_t define_schema(pack_schema_t *schema)
{
    clargs_error_t error;

    schema->root = NULL;

    if (failed(&error, clargs_command_create("pack", &schema->root)) ||
        failed(&error, add_help(schema->root)) ||
        failed(&error, clargs_command_add_count(schema->root, 'v', "verbose", &schema->verbose)) ||
        failed(&error, clargs_command_add_negatable(schema->root, '\0', "color", &schema->color)) ||
        failed(&error, clargs_option_set_initial(schema->color, true))) {
        return error;
    }

    if (failed(&error, clargs_command_add_subcommand(schema->root, "create", &schema->create)) ||
        failed(&error, add_help(schema->create)) ||
        failed(&error, clargs_command_add_value(schema->create, 'o', "output", clargs_type_string(),
                                                &schema->output)) ||
        failed(&error, clargs_option_set_required(schema->output, true)) ||
        failed(&error, clargs_command_add_value(schema->create, 'l', "level", clargs_type_int(),
                                                &schema->level)) ||
        failed(&error, clargs_option_set_default(schema->level, "6")) ||
        failed(&error, clargs_command_add_positional(schema->create, "file", clargs_type_string(),
                                                     CLARGS_ARITY_ONE_OR_MORE, &schema->files))) {
        return error;
    }

    if (failed(&error, clargs_command_add_subcommand(schema->root, "list", &schema->list)) ||
        failed(&error, add_help(schema->list)) ||
        failed(&error, clargs_command_add_positional(schema->list, "archive", clargs_type_string(),
                                                     CLARGS_ARITY_REQUIRED, &schema->archive)) ||
        failed(&error, clargs_command_add_positional(schema->list, "pattern", clargs_type_string(),
                                                     CLARGS_ARITY_OPTIONAL, &schema->pattern)) ||
        failed(&error, clargs_positional_set_default(schema->pattern, "*"))) {
        return error;
    }

    /* A schema must be validated before it is parsed with. */
    return clargs_command_validate(schema->root, NULL, NULL, NULL);
}

/* Tells whether --help or -h was given to one of the commands that the
 * command line selected. This also works on a result with a parse error:
 * it holds what was read before the error. */
static bool help_was_given(const clargs_result_t *result)
{
    size_t i;

    for (i = 0; i < clargs_result_command_count(result); ++i) {
        const clargs_command_t *command = clargs_result_command_at(result, i);
        const clargs_option_t *help = clargs_command_find_long(command, "help", 4, NULL);

        if (help != NULL && clargs_result_flag(result, help)) {
            return true;
        }
    }
    return false;
}

/* Prints the usage of the command that the command line ends in. */
static void print_usage(const pack_schema_t *schema, const clargs_command_t *command)
{
    if (command == schema->create) {
        printf("usage: pack create -o FILE [-l LEVEL] file...\n");
    } else if (command == schema->list) {
        printf("usage: pack list archive [pattern]\n");
    } else {
        printf("usage: pack [-v]... [--color | --no-color] <command> ...\n");
        printf("commands: create, list\n");
    }
}

/* The text of a source, for this example. Later versions may add sources,
 * so the switch has a default label. */
static const char *source_text(clargs_source_t source)
{
    switch (source) {
    case CLARGS_SOURCE_COMMAND_LINE:
        return "command line";
    case CLARGS_SOURCE_DEFAULT:
        return "default";
    default:
        return "none";
    }
}

/* Reads the values of a command line that matched the schema. */
static int run(const pack_schema_t *schema, const clargs_result_t *result)
{
    const clargs_command_t *command = clargs_result_command(result);

    if (command != schema->create && command != schema->list) {
        /* A command line that names no subcommand is not a parse error. */
        fprintf(stderr, "pack: a command is required\n");
        return EXIT_FAILURE;
    }

    /* The options of the root: a count and a negatable flag. */
    printf("verbose: %lu, color: %s\n", (unsigned long)clargs_result_count(result, schema->verbose),
           clargs_result_flag(result, schema->color) ? "on" : "off");

    if (command == schema->create) {
        const char *output = NULL;
        const char *file = NULL;
        int64_t level = 0;
        size_t i;

        /* --output is required and --level has a default value: after a
         * parse that succeeded, both have a value. */
        if (!clargs_result_option_string(result, schema->output, &output) ||
            !clargs_result_option_int(result, schema->level, &level)) {
            return EXIT_FAILURE;
        }
        printf("create %s, level %" PRId64 " (%s)\n", output, level,
               source_text(clargs_result_option_source(result, schema->level)));

        /* A positional that takes any number of arguments: its values in
         * the order of the command line. */
        for (i = 0; i < clargs_result_positional_count(result, schema->files); ++i) {
            if (clargs_result_positional_string(result, schema->files, i, &file)) {
                printf("  %s\n", file);
            }
        }
    } else {
        const char *archive = NULL;
        const char *pattern = NULL;

        if (!clargs_result_positional_string(result, schema->archive, 0, &archive) ||
            !clargs_result_positional_string(result, schema->pattern, 0, &pattern)) {
            return EXIT_FAILURE;
        }
        printf("list %s, pattern %s (%s)\n", archive, pattern,
               source_text(clargs_result_positional_source(result, schema->pattern)));
    }

    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
    pack_schema_t schema;
    clargs_result_t *result = NULL;
    clargs_error_t error;
    int status = EXIT_FAILURE;

    if (failed(&error, define_schema(&schema))) {
        fprintf(stderr, "pack: cannot define the command line: %s\n", clargs_error_message(error));
    } else {
        /* argc and argv go in as main() received them. */
        error = clargs_parse(schema.root, argc, argv, &result);

        if (result == NULL) {
            /* CLARGS_ERROR_OUT_OF_MEMORY: there is no result. */
            fprintf(stderr, "pack: %s\n", clargs_error_message(error));
        } else if (help_was_given(result)) {
            /* --help first: "pack create --help" leaves out what create
             * requires, and that must not hide the help. */
            print_usage(&schema, clargs_result_command(result));
            status = EXIT_SUCCESS;
        } else if (error == CLARGS_ERROR_INVALID_COMMAND_LINE) {
            /* The message is one line with no prefix and no line break. */
            fprintf(stderr, "pack: %s\n", clargs_result_error_message(result));
        } else {
            status = run(&schema, result);
        }

        if (status != EXIT_SUCCESS) {
            fprintf(stderr, "Try 'pack --help' for more information.\n");
        }
    }

    /* The result first, then the schema it was made from. A parse error
     * also leaves a result to destroy. Destroying NULL does nothing. */
    clargs_result_destroy(result);
    clargs_command_destroy(schema.root);

    return status;
}
```

実行の例（`$` の行が入力、次の行からが出力）:

```
$ pack -vv create -o out.tar a.txt b.txt
verbose: 2, color: on
create out.tar, level 6 (default)
  a.txt
  b.txt

$ pack --no-color create -l9 --output=out.tar -- -x.txt
verbose: 0, color: off
create out.tar, level 9 (command line)
  -x.txt

$ pack create a.txt
pack: missing required option '--output'
Try 'pack --help' for more information.

$ pack create --help
usage: pack create -o FILE [-l LEVEL] file...
```

例の要点:

- 解析に渡すのは、検証済みのスキーマのルートと、`main` が受け取った `argc` と `argv` である。加工もキャストも要らない。
- 戻り値は 3 通りである。`CLARGS_ERROR_NONE`（成功）と `CLARGS_ERROR_INVALID_COMMAND_LINE`（解析エラー）では、`result` に結果が書かれる。`CLARGS_ERROR_OUT_OF_MEMORY` では `NULL` が書かれる。
- 解析エラーを見る前に、`--help` が指定されたかを見る。解析エラーの結果からも、エラーまでに読んだ分は読める（[`--help` の扱い](#--help-の扱い)）。
- 例の `print_usage` は、解析の説明のために、usage の文面を自分で書いている。スキーマからヘルプと usage 行を生成する方法は、[ヘルプの使い方](help.md)にある。
- 解析エラーのメッセージは、`clargs_result_error_message` が返す 1 行である。プログラム名などの接頭辞と改行は、表示する側が付ける。
- 値は、スキーマを定義したときに受け取ったハンドル（`schema.output` など）を指定して読む。名前の文字列では指定しない。
- 破棄は、結果が先、スキーマが後である。解析エラーの結果も、利用者が破棄する。

## 解析の入口

```
clargs_error_t clargs_parse(const clargs_command_t *root, int argc, char *const *argv,
                            clargs_result_t **out);
clargs_error_t clargs_parse_wide(const clargs_command_t *root, int argc, wchar_t *const *argv,
                                 clargs_result_t **out);
```

`clargs_parse` は、`char` の argv を解析する。`clargs_parse_wide` は、ワイド文字の argv を UTF-8 に変換してから、同じ解析を行う（[ワイド文字の argv](#ワイド文字の-argv)）。

| 戻り値 | `*out` | 意味 |
|---|---|---|
| `CLARGS_ERROR_NONE` | 結果 | 成功。値の解決まで済んでいる |
| `CLARGS_ERROR_INVALID_COMMAND_LINE` | 結果 | 解析エラー。結果から、エラーの情報と、エラーまでに読んだ分を読める |
| `CLARGS_ERROR_OUT_OF_MEMORY` | `NULL` | 確保の失敗。何も残らない |

- `*out` が `NULL` でなければ、利用者が `clargs_result_destroy` で破棄する。**解析エラーのときも結果が渡る。** これは、「失敗したら出力先は `NULL`」という、ほかの関数の決まりの例外である。
- 値型の変換関数が `CLARGS_ERROR_OUT_OF_MEMORY` を返したときも、確保の失敗として扱う。
- `argv[0]` はプログラム名で、解析しない。`argc` が 0 または 1 なら、何も指定されなかったコマンドラインとして解析する。`argv[argc]` は読まない。
- `root` に渡すのは、検証済みのスキーマのルートである。検証を済ませていないスキーマと、サブコマンドを渡すのは、エラーコードにならない[前提条件](#前提条件)の違反である。
- 解析は、スキーマ、値型、argv の配列、argv の文字列を変更しない。呼び出しの間に持ち越す状態はなく、同じスキーマと同じ argv には、いつも同じ結果を返す。スキーマが変更されていない間は、複数のスレッドから同じスキーマで同時に解析できる。
- 要素は、先頭から順に 1 回ずつ読む。スキーマに合わない最初の要素で、解析をやめる。

## 引数の書き方

プログラムの利用者がコマンドラインに書ける形である。「要素」は argv の 1 つの文字列、「現在のコマンド」は、最初はルートで、サブコマンドの名前が現れるたびに、そのサブコマンドに替わる。

### 要素の分類

上の行から順に当てはめる。

| 要素の形 | 条件 | 扱い |
|---|---|---|
| `--` | 終端より前 | 終端。以後はすべて位置引数 |
| `--x…` | 終端より前 | 長いオプション。最初の `=` で名前と値に分ける |
| `-` | — | オプションではない要素 |
| `-N…`、`-.N…`（N は数字） | 現在のコマンドに数字の短い名前がない | オプションではない要素 |
| `-x…` | 上のどれでもない | 短いオプションの束 |
| それ以外（空の文字列を含む） | — | オプションではない要素 |
| すべて | 終端より後ろ | 位置引数（サブコマンドを持つコマンドでは、余分な位置引数のエラー） |

- オプションではない要素は、現在のコマンドがサブコマンドを持てばサブコマンドの名前、持たなければ位置引数の値として扱う。検証済みのスキーマでは、1 つのコマンドがサブコマンドと位置引数の両方を持つことはない。
- 終端の `--` は、値にならない。終端より後ろの要素は、`-v` も `--name` も 2 つ目の `--` も、形にかかわらず値である。終端は、サブコマンドに切り替わった後も有効である。
- 負の数の形（`-1`、`-.5` など）の要素は、現在のコマンドに、数字を短い名前とするオプションが 1 つもなければ、オプションではない要素である。1 つでもあれば、負の数の形の要素はすべて束として読む（`-.5` は、`.` が未知のオプションになる）。そういうコマンドで負の数を位置引数に渡すには、終端の後ろに書く。
- 値付きオプションの値として取り込まれた要素は、分類されない。`--name -x` の `-x` も、`--name --` の `--` も、値である。
- オプションの名前とサブコマンドの名前は、名前の全体を、バイト単位で比べる。大文字と小文字は別の名前で、名前の一部（前方一致）は一致しない。
- オプションは、現在のコマンドの定義だけから探す。親のコマンドと、サブコマンドのオプションは見ない（[サブコマンド](#サブコマンド)）。

### 値の取り方

長いオプションは、長い名前と否定名で照合する。短いオプションの束は、`-` の後ろの文字を、先頭から 1 文字ずつ、短い名前で照合する。

| 書き方 | オプションの種類 | 動作 |
|---|---|---|
| `--name=value` | 値付き | `=` の直後を値にする（空でもよい） |
| `--name` | 値付き | 次の要素を、内容にかかわらず値にする。なければ `MISSING_VALUE` |
| `--name=value`、`--no-name=value` | フラグ、否定、カウント | `UNEXPECTED_VALUE` |
| `-o` の後ろに文字が残る | 値付き | 残りの全体を値にする。束の解釈を終える |
| `-o` が束の最後 | 値付き | 次の要素を、内容にかかわらず値にする。なければ `MISSING_VALUE` |

値を取らないオプションの書き方:

| 書き方 | オプションの種類 | 動作 |
|---|---|---|
| `--name`、`-n` | フラグ | 指定された |
| `--name`、`-n` | カウント | 回数が 1 増える |
| `--name`、`-n` | 否定できるフラグ | オンになる |
| `--no-name` | 否定できるフラグ | オフになる |

- 長いオプションの名前は、最初の `=` の手前までである。`--name=a=b` の値は `a=b` である。
- `-abc` は `-a -b -c` と同じである。`-vvv` は、カウントを 3 回指定する。`-abovalue` は `-a -b -ovalue` と、`-abo value` は `-a -b -o value` と同じである。
- 束の中の値付きオプションの値は、残りの文字の全体である。`-o=value` の値は `=value` で、`=` は取り除かれない。
- 値を取らないオプションの後ろの文字は、どの文字も、束の 1 文字として照合する。`-a-b` の 2 つ目の `-` と、`-a=1` の `=` は、短い名前にならない文字なので、未知のオプションになる（メッセージは `unknown option '--'`、`unknown option '-='`）。
- 値は、加工せずに、書かれたとおりに値型へ渡る。前後の空白も取り除かない。値型が受け付ける入力は、[スキーマの使い方](schema.md#値型)にある。

### 複数回の指定

同じオプションは、短い名前と長い名前のどちらででも、何度でも指定できる。

| オプションの種類 | 複数回の指定の結果 |
|---|---|
| フラグ | 指定された、のままである |
| カウント | 指定された回数（`-v --verbose -vv` は 4 回） |
| 否定できるフラグ | 最後の指定（`--color --no-color` はオフ、逆順ならオン） |
| 値付き | 最後の値（`--out a --out b` は `b`） |

値付きオプションの値は、読んだ時点で、1 つずつ値型で変換する。後で置き換えられる値も変換するので、`--num x --num 5` は、`x` の変換の失敗の解析エラーになる。

### 位置引数の割り当て

サブコマンドを持たないコマンドでは、オプションではない要素を、現れた順に、位置引数を足した順に割り当てる。

| 位置引数の個数の種類 | 割り当て |
|---|---|
| `CLARGS_ARITY_REQUIRED`、`CLARGS_ARITY_OPTIONAL` | 値を 1 個割り当てる。次の要素は、次の位置引数に進む |
| `CLARGS_ARITY_ZERO_OR_MORE`、`CLARGS_ARITY_ONE_OR_MORE` | 値を割り当てる。以後の要素も、すべてこの位置引数の値になる |

- すべての位置引数に値を割り当て終えた後の要素と、位置引数を持たないコマンドでのオプションではない要素は、余分な位置引数の解析エラー（`UNEXPECTED_POSITIONAL`）になる。
- オプションは、位置引数の値の前、間、後ろのどこに書いてもよい。割り当ては変わらない。
- 単独の `-` と空の文字列は、位置引数の値になる。`-` で始まる値を渡すには、終端の `--` の後ろに書く。
- 値は、読んだ時点で、その位置引数の値型で変換する。

### サブコマンド

サブコマンドを持つコマンドでは、終端より前のオプションではない要素を、サブコマンドの名前として照合する。

- 一致したサブコマンドが選ばれ、現在のコマンドになる。その名前より後ろの要素は、そのサブコマンドのオプション、位置引数、サブコマンドで読む。入れ子の深さに制限はない。
- 現在のコマンドのサブコマンドのどれにも一致しない名前は、未知のサブコマンドの解析エラー（`UNKNOWN_SUBCOMMAND`）になる。
- **オプションは、それを定義したコマンドの名前の後ろ、そのサブコマンドの名前の前に書く。** `pack -v create …` の `-v` は `pack` のオプションである。`pack create -v …` の `-v` は `create` のオプションとして探すので、`create` に `-v` がなければ、未知のオプションになる。親のオプションは、サブコマンドに引き継がれない。
- 親と子のコマンドに同じ名前のオプションがあるときは、それぞれの位置で、それぞれのコマンドのものになる。
- サブコマンドの名前が現れずにコマンドラインが終わっても、解析エラーではない。`clargs_result_command` が、サブコマンドを持つコマンドを返す。サブコマンドを必須にしたいプログラムは、例の `run` のように、自分で確かめる。
- 終端より後ろの要素は、サブコマンドの名前にならない。サブコマンドを持つコマンドは位置引数を持たないので、余分な位置引数の解析エラーになる。
- 選ばれなかったサブコマンドのオプションと位置引数は、指定されなかったものとして読める（[読み取りの規則](#読み取りの規則)）。

## 必須の検査と既定値

最後の要素まで解析エラーなしに読み終えたら、選ばれたコマンド（ルートと、名前が現れたサブコマンド）について、必須の検査と既定値の適用を行う。選ばれなかったコマンドの定義要素は、必須でも検査せず、既定値も入れない。

1. 必須の検査。必須の値付きオプション（`clargs_option_set_required`）に値がない、または `CLARGS_ARITY_REQUIRED` か `CLARGS_ARITY_ONE_OR_MORE` の位置引数に値が 1 つもないと、必須の不足の解析エラー（`MISSING_OPTION`、`MISSING_POSITIONAL`）になる。不足が 2 つ以上あるときは、最初の 1 つを報告する。順序は、ルートに近いコマンドから、1 つのコマンドの中ではオプションが先で位置引数が後、それぞれ足した順である。
2. 既定値の適用。不足が 1 つもないときだけ行う。指定されなかった値付きオプションと `CLARGS_ARITY_OPTIONAL` の位置引数のうち、既定値を持つものに、既定値を値型で変換した値を入れる。出どころは `CLARGS_SOURCE_DEFAULT` になる。

- 既定値を持たない、指定されなかった値付きオプションと任意の位置引数は、値を持たない（出どころは `CLARGS_SOURCE_NONE`）。指定されなかった `CLARGS_ARITY_ZERO_OR_MORE` の位置引数は、値が 0 個である。
- 指定されなかったフラグは「指定されていない」、カウントは 0、否定できるフラグは初期の状態（`clargs_option_set_initial`）である。出どころは `CLARGS_SOURCE_DEFAULT` である。
- 既定値の変換が失敗すると、値の変換の失敗の解析エラー（`INVALID_VALUE`）になる。スキーマの検証が既定値を確かめているので、これが起こるのは、利用者定義の値型が、同じ入力に違う答えを返したときだけである。
- 値の出どころは、コマンドラインと既定値だけである。環境変数などは読まない。
- **解析エラーの結果には、既定値を適用しない。** 指定されなかった値付きオプションと位置引数は、既定値を持つものも、値を持たない。

## 結果の読み取り

結果（`clargs_result_t`）は、成功のときも解析エラーのときも、同じ関数で読む。読み取りの関数は、`const` の結果を取り、何も変更せず、確保せず、失敗しない。1 つの結果を、複数のスレッドから同時に読める。

| 読むもの | 関数 | 「ない」ときの値 |
|---|---|---|
| 解析の成否 | `clargs_result_error` | `CLARGS_ERROR_NONE` または `CLARGS_ERROR_INVALID_COMMAND_LINE` を返す |
| 選ばれたコマンドの並び | `clargs_result_command_count`、`clargs_result_command_at`、`clargs_result_command` | 並びは 1 つ以上である。番号 0 はルート。`clargs_result_command` は最後のコマンド |
| フラグ、否定できるフラグ | `clargs_result_flag` | `false`（否定できるフラグは初期の状態） |
| カウント | `clargs_result_count` | 0 |
| オプションの値の出どころ | `clargs_result_option_source` | `CLARGS_SOURCE_NONE` |
| 値付きオプションの値 | `clargs_result_option_string`、`_int`、`_float`、`_choice` | `false` を返し、`*out` を変えない |
| 値付きオプションの値の領域 | `clargs_result_option_value` | `NULL` |
| 位置引数の値の個数と出どころ | `clargs_result_positional_count`、`clargs_result_positional_source` | 0、`CLARGS_SOURCE_NONE` |
| 位置引数の値 | `clargs_result_positional_string`、`_int`、`_float`、`_choice` | `false` を返し、`*out` を変えない |
| 位置引数の値の領域 | `clargs_result_positional_value` | `NULL` |
| 解析エラーの情報 | `clargs_result_error_kind` など（[解析エラー](#解析エラー)） | `CLARGS_PARSE_ERROR_NONE`、`NULL`、`CLARGS_SOURCE_NONE`、0、`'\0'` |

- 定義要素は、スキーマのオプションまたは位置引数のハンドルそのもので指定する。追加の関数が書いたハンドルでも、スキーマの読み取りと検索の関数が返したハンドルでもよい。
- 型の付いた読み取りは、値型の種類ごとにある。値の型は、`_string` が `const char *`、`_int` が `int64_t`、`_float` が `double`、`_choice` が `size_t`（一致した選択肢の番号）である。
- `_value` は、どの種類の値型にも使える。利用者定義の値型の値は、これで読む。返るポインタは、値型が値を書いた領域を指す。領域にあるのは、値型の値の型のオブジェクト（`const char *`、`int64_t`、`double`、`size_t`、または利用者定義の値型の変換関数が書いたもの）である。
- 位置引数の値は、番号（0 から、コマンドラインに現れた順）を指定して読む。必須と任意の位置引数の個数は 0 または 1、可変個の位置引数の個数は、割り当てられた値の個数である。番号が個数以上のときは、値がないものとして `false` または `NULL` が返る。前提条件の違反ではない。
- 文字列の値は、結果が持つ引数の複製の中、またはスキーマの既定値を指す。ワイド文字の入口で解析した結果では、UTF-8 である。

### 読み取りの規則

定義要素の状態ごとに、読み取りが返すものである。

| 定義要素の状態 | フラグ | カウント | 否定できるフラグ | 値付きオプション、位置引数 | 出どころ |
|---|---|---|---|---|---|
| 並びのコマンドに属し、コマンドラインで指定された | `true` | 回数 | 最後の指定 | 値 | `COMMAND_LINE` |
| 並びのコマンドに属し、指定されず、既定値が適用された | — | — | — | 既定値 | `DEFAULT` |
| 並びのコマンドに属し、指定されなかった（上を除く） | `false` | 0 | 初期の状態 | 値なし | フラグ類は `DEFAULT`、値付きと位置引数は `NONE` |
| 並びに属さないコマンドのもの | `false` | 0 | 初期の状態 | 値なし | `NONE` |

- 「並び」は、選ばれたコマンドの並びである。出どころの列は、`CLARGS_SOURCE_` を省いて書いた。
- 解析エラーの結果では、既定値を適用していないので、2 行目は現れない。
- 選ばれなかったコマンドの定義要素を読むのは、失敗でも前提条件の違反でもない。4 行目のとおりに読める。
- 出どころの列挙 `clargs_source_t` には、後のバージョンで値を足すことがある。`switch` で分けるときは、`default` を書く。

## 解析エラー

引数がスキーマに合わないと、解析は `CLARGS_ERROR_INVALID_COMMAND_LINE` を返し、結果に解析エラーを 1 つ記録する。記録するのは、argv の先頭から見て最初に見つけた 1 つだけである。必須の不足は、最後の要素まで読み終えてから検査するので、コマンドラインの要素についてのどの解析エラーよりも後になる。

### 種類とメッセージ

種類は `clargs_result_error_kind` が返す `clargs_parse_error_kind_t` で、メッセージは `clargs_result_error_message` が返す。下の表では、種類の `CLARGS_PARSE_ERROR_` を省いて書いた。

| 種類 | 文面 | 例 |
|---|---|---|
| `UNKNOWN_OPTION` | `unknown option '<name>'` | `unknown option '--colour'`、`unknown option '-x'` |
| `UNKNOWN_SUBCOMMAND` | `unknown command '<value>'` | `unknown command 'comit'` |
| `MISSING_VALUE` | `option '<name>' requires a value` | `option '--output' requires a value` |
| `UNEXPECTED_VALUE` | `option '<name>' does not take a value` | `option '--no-color' does not take a value` |
| `INVALID_VALUE`（オプション） | `invalid value '<value>' for option '<name>' (expected <type>)` | `invalid value 'abc' for option '--jobs' (expected integer)` |
| `INVALID_VALUE`（位置引数） | `invalid value '<value>' for argument '<name>' (expected <type>)` | `invalid value 'x' for argument 'COUNT' (expected integer)` |
| `INVALID_VALUE`（既定値） | 上の 2 つの `invalid value` を `invalid default value` にしたもの | — |
| `MISSING_OPTION` | `missing required option '<name>'` | `missing required option '--output'` |
| `MISSING_POSITIONAL` | `missing required argument '<name>'` | `missing required argument 'FILE'` |
| `UNEXPECTED_POSITIONAL` | `unexpected argument '<value>'` | `unexpected argument 'extra'` |
| `INVALID_ENCODING` | `invalid character encoding in argument '<value>'` | — |

- メッセージは、英語の 1 行である。改行を含まず、プログラム名などの接頭辞も、句点も付かない。そのままプログラムの利用者に見せられる。埋め込む名前と値は、加工せずにそのまま入る。
- オプションの `<name>` は、接頭辞（`--` または `-`）と名前をつなげたもので、プログラムの利用者が書いた形である。否定名で書かれたら `--no-<name>`、束の中の 1 文字なら `-` とその文字になる。長いオプションに `=` で付いた値は含まない（`--bogus=1` は `unknown option '--bogus'`）。書かれていないオプション（必須の不足、既定値）は、長い名前があれば `--<長い名前>`、なければ `-<短い名前>` である。
- 既定値の文面は、たとえば `invalid default value '64x' for option '--block-size' (expected size)` となる。
- 位置引数の `<name>` は、スキーマの位置引数の名前である。`<type>` は、値型の名前（`clargs_type_name`）である。
- `<value>` は、未知のサブコマンドと余分な位置引数では、その要素である。値の変換の失敗では、値の文字列である。`INVALID_ENCODING` では、変換できない単位を U+FFFD に置き換えた、UTF-8 の要素である。
- 種類の列挙には、後のバージョンで値を足すことがある。`switch` で分けるときは、`default` を書く。
- 解析のエラーコード `CLARGS_ERROR_INVALID_COMMAND_LINE` の説明文（`clargs_error_message`）は、`"invalid command line"` である。種類ごとのメッセージは、結果から読む。値型の変換関数が返す `CLARGS_ERROR_INVALID_VALUE` は、解析からは返らない。

### 解析エラーの情報

メッセージのほかに、次の関数で、エラーの内容を読める。列の名前は、関数名の `clargs_result_error_` を省いたものである。

| 種類 | `argument`、`index`、`source` | `value` | `option` / `positional` | `short_name` |
|---|---|---|---|---|
| `UNKNOWN_OPTION` | その要素、`COMMAND_LINE` | `NULL` | `NULL` | 束の中なら、その文字 |
| `UNKNOWN_SUBCOMMAND` | その要素 | その要素 | `NULL` | `'\0'` |
| `MISSING_VALUE` | オプションを書いた要素 | `NULL` | そのオプション | 束の中なら、その文字 |
| `UNEXPECTED_VALUE` | その要素 | `=` の直後 | そのオプション | `'\0'` |
| `INVALID_VALUE`（コマンドライン） | 値を含む要素 | 値の文字列 | 定義要素 | 束の中の値付きオプションなら、その文字 |
| `INVALID_VALUE`（既定値） | `NULL`、0、`DEFAULT` | 既定値の文字列 | 定義要素 | `'\0'` |
| `MISSING_OPTION`、`MISSING_POSITIONAL` | `NULL`、0、`NONE` | `NULL` | 定義要素 | `'\0'` |
| `UNEXPECTED_POSITIONAL` | その要素 | その要素 | `NULL` | `'\0'` |
| `INVALID_ENCODING` | その要素（U+FFFD に置き換えた UTF-8） | その要素 | `NULL` | `'\0'` |

- 1 列目に `source` を書いていない行の `source` は、`CLARGS_SOURCE_COMMAND_LINE` である。
- `argument`（`clargs_result_error_argument`）は、問題の要素の全体である。束の中のエラーでは、束の全体（`-vx`）である。
- `index`（`clargs_result_error_index`）は、利用者が渡した argv での番号である（`argv[0]` が 0）。`source`（`clargs_result_error_source`）が `CLARGS_SOURCE_COMMAND_LINE` のときにだけ、意味を持つ。
- `command`（`clargs_result_error_command`）は、エラーを見つけたときの現在のコマンドである。必須の不足と既定値では、その定義要素が属するコマンドである。解析エラーの結果では、`NULL` にならない。
- `option`（`clargs_result_error_option`）と `positional`（`clargs_result_error_positional`）は、表の「定義要素」が値付きオプションなら `option`、位置引数なら `positional` に入り、もう一方は `NULL` である。
- `INVALID_VALUE` で値が次の要素から取り込まれたとき（`--jobs many`、`-j many`）、`argument` と `index` は、値の要素のものである。
- `short_name`（`clargs_result_error_short_name`）は、束の中の 1 文字が原因のときの、その文字である。単独の短いオプション（`-n x`）も、1 文字の束として読むので、その文字（`'n'`）が入る。長いオプションで書かれたときは `'\0'` である。
- 解析が成功した結果では、どの関数も「ない」ことを表す値（`CLARGS_PARSE_ERROR_NONE`、`NULL`、`CLARGS_SOURCE_NONE`、0、`'\0'`）を返す。
- 返る文字列は、結果を破棄するまで有効で、利用者は解放しない。コマンド、オプション、位置引数は、スキーマのものである。

### 解析エラーのときの結果

解析エラーの結果からは、エラーの情報のほかに、エラーを見つけるまでに読んだ分を、成功のときと同じ関数で読める。

- 選ばれたコマンドの並び、指定されたフラグ、カウント、否定できるフラグ、変換に成功した値が読める。出どころは、どれも `CLARGS_SOURCE_COMMAND_LINE` である。
- エラーの要素より後ろの要素は、読まれていない。
- 既定値は適用されていない。指定されなかった値付きオプションと位置引数は、値を持たない。指定されなかったフラグ、カウント、否定できるフラグは、「指定されていない」、0、初期の状態である。

### メッセージの表示

ライブラリは、何も出力しない。表示は、利用者のコードが行う。

```
fprintf(stderr, "pack: %s\n", clargs_result_error_message(result));
```

usage 行を添える、問題の要素の位置を示す、種類ごとに文面を変える、といったことは、[解析エラーの情報](#解析エラーの情報)を読んで、利用者のコードで行う。エラーを見つけたコマンドの usage 行を生成して添える例は、[ヘルプの使い方](help.md#--help-とエラーの表示)にある。メッセージの文面は英語だけである。

## `--help` の扱い

ライブラリは、`--help` を特別扱いしない。`--help` は、利用者がスキーマに定義するフラグである。指定されたかどうかを結果から読み、ヘルプを表示して終えるのは、利用者のコードである。ヘルプの文面は、スキーマから生成できる。解析は、ヘルプを生成も出力もしない。生成の関数と、`--help` を読み取ってヘルプを出す例は、[ヘルプの使い方](help.md#--help-とエラーの表示)にある。

**解析エラーを見る前に、`--help` が指定されたかを見る。** `pack create --help` は、`create` の必須のオプション `--output` を書いていないので、解析は必須の不足の解析エラーを返す。しかし、解析エラーの結果からも、エラーまでに読んだ分は読めるので、`--help` は「指定された」と読める。先にエラーを見ると、ヘルプを求めた利用者に、エラーのメッセージを見せることになる。

順序は、例の `main` のとおりである。

1. `*out` が `NULL` なら、確保の失敗である。結果はない。
2. `--help` が指定されていたら、ヘルプを表示して終える。どのコマンドのヘルプを表示するかは、`clargs_result_command`（最後に選ばれたコマンド）で決める。そのコマンドを `clargs_help_write` に渡すと、そのコマンドのヘルプが生成される。
3. 戻り値が `CLARGS_ERROR_INVALID_COMMAND_LINE` なら、解析エラーのメッセージを表示する。
4. どちらでもなければ、値を読んで、プログラムの処理を行う。

**サブコマンドにも `--help` を定義する。** オプションは、それを定義したコマンドが現在のコマンドである間だけ認識される。ルートにだけ `--help` を定義すると、`pack --help` と `pack --help create` は受け付けるが、`pack create --help` は、`create` に `--help` がないので、未知のオプションの解析エラーになる。`tool sub --help` の形を受け付けるには、例の `add_help` のように、すべてのコマンドに `--help` を定義する。

- 指定されたかどうかは、選ばれたコマンドの並びを順に見て、それぞれのコマンドの `--help` を `clargs_result_flag` で読めば分かる（例の `help_was_given`）。選ばれなかったコマンドの `--help` は、`false` と読める。
- `--help` より前の要素に解析エラーがあると、解析はそこで止まるので、`--help` は読まれない（`pack create --bogus --help` は、未知のオプションのエラーになる）。

## ワイド文字の argv

`clargs_parse_wide` は、`wmain` が受け取るような、ワイド文字の argv を解析する。Windows で、ASCII 以外の文字を含む引数を、コードページによる欠落なしに受け取るための入口である。

- **`clargs_parse_wide` は、すべての環境で提供する。** Windows だけでなく、Linux と macOS のライブラリにもある。
- ワイド文字列の読み方は、`wchar_t` の幅で決まる。ロケールには左右されない。

  | `wchar_t` の幅 | 環境の例 | 読み方 |
  |---|---|---|
  | 16 ビット | Windows | UTF-16。上位と下位のサロゲートの組を、1 つの文字として読む |
  | それより広い | Linux、macOS | UTF-32 |

- 各要素を UTF-8 に変換してから、`clargs_parse` と同じ解析を行う。同じ内容を UTF-8 で書いた `char` の argv を `clargs_parse` に渡したときと、同じ結果、または同じ解析エラーになる。
- 結果から読む文字列は、すべて UTF-8 の `char` の文字列である。文字列の値も、解析エラーの問題の要素、値、メッセージも、UTF-8 である。
- UTF-8 に変換できない並びを含む要素は、解析エラー `CLARGS_PARSE_ERROR_INVALID_ENCODING` になる。変換できない並びとは、UTF-16 では対になっていないサロゲート、UTF-32 では負の値、サロゲートの範囲（U+D800〜U+DFFF）の値、U+10FFFF を超える値である。
  - このエラーは、その要素を読む直前に見つける。値付きオプションの値として取り込まれる要素でも同じである。それより前の要素に別の解析エラーがあれば、そちらが返る。
  - 問題の要素と値は、変換できない単位を U+FFFD に置き換えた、UTF-8 の文字列である。
  - `argv[0]` は解析しないので、このエラーにならない。
- `clargs_parse` は、`char` の argv の要素を、文字コードを検査も変換もせずに、バイトの並びとしてそのまま扱う。UTF-8 として不正なバイトも、そのまま値になる。
- 戻り値、`*out`、所有権、前提条件は、`clargs_parse` と同じである。

`wmain` での例である。`greet` は、名前を 1 つ受け取る。

```c
#include <clargs/clargs.h>

#include <stdio.h>
#include <stdlib.h>

int wmain(int argc, wchar_t **argv)
{
    clargs_command_t *root = NULL;
    clargs_positional_t *name = NULL;
    clargs_result_t *result = NULL;
    const char *text = NULL;
    clargs_error_t error;
    int status = EXIT_FAILURE;

    error = clargs_command_create("greet", &root);
    if (error == CLARGS_ERROR_NONE) {
        error = clargs_command_add_positional(root, "name", clargs_type_string(), CLARGS_ARITY_REQUIRED,
                                              &name);
    }
    if (error == CLARGS_ERROR_NONE) {
        error = clargs_command_validate(root, NULL, NULL, NULL);
    }

    if (error != CLARGS_ERROR_NONE) {
        fprintf(stderr, "greet: cannot define the command line: %s\n", clargs_error_message(error));
    } else {
        /* The wide argv goes in as wmain() received it. */
        error = clargs_parse_wide(root, argc, argv, &result);

        if (result == NULL) {
            fprintf(stderr, "greet: %s\n", clargs_error_message(error));
        } else if (error == CLARGS_ERROR_INVALID_COMMAND_LINE) {
            /* The message is UTF-8, like every string of the result. */
            fprintf(stderr, "greet: %s\n", clargs_result_error_message(result));
        } else if (clargs_result_positional_string(result, name, 0, &text)) {
            /* text is UTF-8, whatever the width of wchar_t is. */
            printf("hello, %s\n", text);
            status = EXIT_SUCCESS;
        }
    }

    clargs_result_destroy(result);
    clargs_command_destroy(root);

    return status;
}
```

- `wmain` は、Windows のコンパイラが用意する入口である。ほかの環境では、`clargs_parse_wide` は使えるが、`wmain` という入口はない。
- 例は、UTF-8 の文字列をそのまま標準出力に書いている。コンソールが UTF-8 を表示するようにすること（Windows では、コンソールの出力コードページの設定など）は、プログラムの側で行う。ライブラリは、OS の API を呼ばない。

## 寿命の規則

| もの | 所有者 | 有効な期間 |
|---|---|---|
| 結果（`clargs_result_t`） | 解析を呼んだ利用者 | `clargs_result_destroy` で破棄するまで。解析エラーの結果も同じ |
| 結果から読んだ文字列と、値の領域 | 結果 | 結果を破棄するまで。利用者は、解放も変更もしない |
| 解析エラーの情報とメッセージ | 結果 | 結果を破棄するまで。利用者は、解放も変更もしない |
| 結果から読んだコマンド、オプション、位置引数 | スキーマ | スキーマのルートを破棄するまで |
| argv の配列と文字列 | 利用者 | 解析の呼び出しの間だけ、有効であればよい |
| スキーマと、それが使う値型 | 利用者 | その結果をすべて破棄し終えるまで、変更も破棄もしない |

- **結果は、スキーマより先に破棄する。** 結果は、スキーマと値型を借りる。結果の破棄は、保持している値を値型の後始末関数で後始末するので、そのときまで、スキーマも値型も有効でなければならない。破棄の順序は、結果、スキーマ、値型、値型が借りていた利用者のデータである（[スキーマの使い方](schema.md#値型を借りる契約と破棄の順序)）。
- **結果がある間は、スキーマを変更しない。** そのスキーマの結果が 1 つでも残っている間は、スキーマに定義要素を足さず、属性を設定しない。
- **argv は、解析の後に変更または解放してよい。** 結果は、argv のすべての文字列を複製して持つ。解析の呼び出しが戻った後は、argv の配列も文字列も参照しない。
- **結果から読んだ文字列と値は、結果の破棄まで有効である。** 文字列の値は、結果が持つ引数の複製の中、またはスキーマの既定値を指す。結果を破棄した後に使うなら、破棄の前に複製する。
- **解析エラーのときも結果が渡り、利用者が破棄する。** 確保の失敗（`CLARGS_ERROR_OUT_OF_MEMORY`）では、`*out` は `NULL` で、破棄するものはない。その解析のために確保したメモリと、変換した値は、すべて後始末されている。
- `clargs_result_destroy(NULL)` は、何もしない。例のように、`result` を `NULL` で初期化しておけば、どの経路でも同じ順序で破棄を書ける。
- 結果の破棄は、スキーマと値型を、変更も破棄もしない。同じスキーマで、何度でも解析できる。結果どうしは独立で、どの順序で破棄してもよい。
- メモリは、すべて、ライブラリに設定されたアロケータ（`clargs_set_allocator`）から確保する。結果が残っている間に、アロケータを設定し直してはならない（[所有権と寿命の規約](ownership.md#5-アロケータ)）。
- ライブラリは、スキーマより先に結果を破棄しない誤り、結果がある間のスキーマの変更を、検出しない。

## 前提条件

前提条件を破るのは、プログラムの誤りである。エラーコードは返らない。検査できるものは `assert` で検査され、Debug 構成では、破るとプロセスが中断する。Release 構成では検査されず、動作は定まらない（[所有権と寿命の規約](ownership.md#実行時の失敗とプログラムの誤り)）。関数ごとの前提条件は、公開ヘッダ `include/clargs/parse.h` のコメントにも `Preconditions:` として書いてある。

どの関数にも共通すること:

- ポインタの引数は、`NULL` でない。`NULL` でもよいのは、`clargs_result_destroy` の `result` と、`argc` が 0 のときの `argv` だけである。`assert` で検査する。
- 結果は、破棄されていない。その結果のスキーマと、スキーマが使う値型は、破棄も変更もされていない。検査しない。

関数ごとの前提条件:

| 関数 | `assert` で検査する前提条件 | 検査しない前提条件 |
|---|---|---|
| `clargs_parse`、`clargs_parse_wide` | `root` と `out` は `NULL` でない。`root` はルートコマンドである（サブコマンドでない）。`root` のスキーマは検証済みである（`clargs_command_is_validated`）。`argc` は 0 以上である。`argc` が 0 より大きければ、`argv` は `NULL` でなく、`argv[0]` から `argv[argc - 1]` は `NULL` でない | 結果を破棄するまで、スキーマと、それが使う値型を、変更も破棄もしない |
| `clargs_result_destroy` | なし | `result` は、`NULL` か、破棄されていない結果である。その結果のスキーマと値型は、破棄も変更もされていない |
| `clargs_result_error`、`clargs_result_command_count`、`clargs_result_command` | `result` は `NULL` でない | 共通のもの |
| `clargs_result_command_at` | `result` は `NULL` でない。`index` は `clargs_result_command_count` より小さい | 共通のもの |
| `clargs_result_flag` | `result` と `option` は `NULL` でない。`option` は、この結果のスキーマに属する。`option` は、フラグまたは否定できるフラグである | 共通のもの |
| `clargs_result_count` | `result` と `option` は `NULL` でない。`option` は、この結果のスキーマに属する。`option` は、カウントである | 共通のもの |
| `clargs_result_option_source` | `result` と `option` は `NULL` でない。`option` は、この結果のスキーマに属する（種類は問わない） | 共通のもの |
| `clargs_result_option_string`、`_int`、`_float`、`_choice` | `result`、`option`、`out` は `NULL` でない。`option` は、この結果のスキーマに属する。`option` は、値付きオプションである。値型の種類が、それぞれ `CLARGS_TYPE_STRING`、`CLARGS_TYPE_INT`、`CLARGS_TYPE_FLOAT`、`CLARGS_TYPE_CHOICE` である | 共通のもの |
| `clargs_result_option_value` | `result` と `option` は `NULL` でない。`option` は、この結果のスキーマに属する。`option` は、値付きオプションである | 共通のもの。返ったポインタは、値型の値の型で読む |
| `clargs_result_positional_count`、`clargs_result_positional_source` | `result` と `positional` は `NULL` でない。`positional` は、この結果のスキーマに属する | 共通のもの |
| `clargs_result_positional_string`、`_int`、`_float`、`_choice` | `result`、`positional`、`out` は `NULL` でない。`positional` は、この結果のスキーマに属する。値型の種類が、それぞれ `CLARGS_TYPE_STRING`、`CLARGS_TYPE_INT`、`CLARGS_TYPE_FLOAT`、`CLARGS_TYPE_CHOICE` である | 共通のもの |
| `clargs_result_positional_value` | `result` と `positional` は `NULL` でない。`positional` は、この結果のスキーマに属する | 共通のもの。返ったポインタは、値型の値の型で読む |
| `clargs_result_error_kind`、`_message`、`_argument`、`_value`、`_source`、`_index`、`_command`、`_option`、`_positional`、`_short_name` | `result` は `NULL` でない | 共通のもの |

- 「この結果のスキーマに属する」とは、その定義要素のコマンドから親をたどったルートが、解析に渡したルートである、ということである。別のスキーマのオプションや位置引数を渡すのは、前提条件の違反である。
- 前提条件の違反ではないもの: 選ばれなかったコマンドの定義要素を読むこと。値を持たない値付きオプションや位置引数の値を読むこと。位置引数の値を、個数以上の番号で読むこと。成功した結果から、解析エラーの情報を読むこと。どれも、「ない」ことを表す値が返る（[結果の読み取り](#結果の読み取り)）。
- 検証を済ませていないスキーマを解析に渡すのは、前提条件の違反である。スキーマに定義要素を足したり、属性を設定したりすると、スキーマは未検証に戻るので、解析の前に、もう一度検証する（[スキーマの使い方](schema.md#検証と失敗の調べ方)）。
- `_value` が返すポインタを、値型の値の型と違う型で読む誤りは、コンパイラもライブラリも検出しない。
