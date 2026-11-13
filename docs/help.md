# CLArgs のヘルプの使い方

検証済みのスキーマのコマンドを 1 つ渡して、そのコマンドのヘルプ、または usage 行だけを生成する方法をまとめる。生成される文面の規則、整形の設定、出力先、失敗したときに出力先に残るものも、ここにまとめる。

この文書が扱うのは、ヘルプと usage 行の生成、設定、出力先である。スキーマの定義と検証は[スキーマの使い方](schema.md)に、argv の解析と解析エラーは[解析の使い方](parse.md)にある。生成は、スキーマを読むだけで、解析の結果を必要としない。ライブラリは、文面を自分ではどこにも出力せず、プロセスを終了させず、環境変数も端末の幅も読まない。`--help` のようなオプションを、スキーマに足すこともない。

関数ごとの詳しい説明と前提条件は、公開ヘッダ `include/clargs/help.h` のコメントにある。メモリの持ち方の決まりは、[所有権と寿命の規約](ownership.md)にある。

この文書で使う言葉:

| 言葉 | 意味 |
|---|---|
| ヘルプ | 1 つのコマンドを説明する文面の全体。usage 行、コマンドの説明文、一覧からなる |
| usage 行 | ヘルプの先頭の、`Usage: ` で始まる部分。折り返されると、複数の行になる |
| 一覧 | `Commands:`、`Arguments:`、`Options:` のそれぞれ |
| 項目 | 一覧に載る 1 つのサブコマンド、位置引数、オプション |
| 見出し | 項目の左側に出る、名前の表記（`-o, --output <FILE>` など） |
| 説明 | 項目の右側に出る文章。説明文と注記からなる |
| 注記 | 説明文の後ろに出る、`[default: 6]` のような、定義から作られる語句 |
| 桁 | 行の先頭から数えたバイトの数。行の先頭は 0 桁である。1 バイトを 1 桁と数える |
| 説明の列 | 項目の説明が始まる桁 |
| 設定 | 表示幅、字下げ、説明の列の上限を持つオブジェクト（`clargs_help_options_t`） |
| 書き込み関数 | 生成された文面を受け取る、利用者の関数（`clargs_help_write_fn_t`） |

目次:

- [最小の例](#最小の例)
- [`--help` とエラーの表示](#--help-とエラーの表示)
- [生成の関数](#生成の関数)
- [文面の規則](#文面の規則)
- [設定](#設定)
- [出力先](#出力先)
- [制限](#制限)
- [失敗したとき](#失敗したとき)
- [寿命の規則](#寿命の規則)
- [前提条件](#前提条件)

## 最小の例

`fetch` という架空のプログラムのヘルプを標準出力に出し、続けて usage 行だけを出す。例は 1 つの翻訳単位で、取り込むのは `<clargs/clargs.h>` と標準ヘッダだけである。取り出して、そのままコンパイルできる。

```c
#include <clargs/clargs.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* Keeps the result of a call in *error and tells whether the call failed. */
static bool failed(clargs_error_t *error, clargs_error_t result)
{
    *error = result;
    return result != CLARGS_ERROR_NONE;
}

/* The write function: writes one part of the text to the stream that
 * user_data points to. The part is not terminated, so it is written with
 * its length. */
static bool write_to_stream(void *user_data, const char *text, size_t length)
{
    return fwrite(text, 1, length, (FILE *)user_data) == length;
}

int main(void)
{
    clargs_command_t *root = NULL;
    clargs_option_t *option = NULL;
    clargs_positional_t *positional = NULL;
    clargs_error_t error;
    int status = EXIT_FAILURE;

    /* The schema. The help is generated from it, so it must be validated. */
    if (failed(&error, clargs_command_create("fetch", &root)) ||
        failed(&error, clargs_command_set_description(root, "Download a file.")) ||
        failed(&error, clargs_command_add_flag(root, 'q', "quiet", &option)) ||
        failed(&error, clargs_option_set_description(option, "Print nothing.")) ||
        failed(&error,
               clargs_command_add_value(root, 'o', "output", clargs_type_string(), &option)) ||
        failed(&error, clargs_option_set_description(option, "The file to write.")) ||
        failed(&error, clargs_option_set_value_name(option, "FILE")) ||
        failed(&error, clargs_option_set_required(option, true)) ||
        failed(&error, clargs_command_add_value(root, 'r', "retries", clargs_type_int(), &option)) ||
        failed(&error, clargs_option_set_description(option, "How often to try again.")) ||
        failed(&error, clargs_option_set_default(option, "3")) ||
        failed(&error, clargs_command_add_positional(root, "url", clargs_type_string(),
                                                     CLARGS_ARITY_REQUIRED, &positional)) ||
        failed(&error, clargs_positional_set_description(positional, "The address to read.")) ||
        failed(&error, clargs_command_validate(root, NULL, NULL, NULL))) {
        fprintf(stderr, "fetch: cannot define the command line: %s\n", clargs_error_message(error));
    } else {
        /* The help of the root, with the default settings (NULL). */
        error = clargs_help_write(root, NULL, write_to_stream, stdout);

        if (error == CLARGS_ERROR_NONE) {
            /* The usage line alone: the first line of the help. */
            printf("----\n");
            error = clargs_help_write_usage(root, NULL, write_to_stream, stdout);
        }

        if (error != CLARGS_ERROR_NONE) {
            fprintf(stderr, "fetch: cannot print the help: %s\n", clargs_error_message(error));
        } else {
            status = EXIT_SUCCESS;
        }
    }

    clargs_command_destroy(root);

    return status;
}
```

出力:

```
Usage: fetch [OPTIONS] --output <FILE> <url>

Download a file.

Arguments:
  <url>                    The address to read.

Options:
  -q, --quiet              Print nothing.
  -o, --output <FILE>      The file to write. [required]
  -r, --retries <integer>  How often to try again. [default: 3]
----
Usage: fetch [OPTIONS] --output <FILE> <url>
```

例の要点:

- 生成に渡すのは、検証済みのスキーマのコマンド、設定、書き込み関数、書き込み関数に渡すデータの 4 つである。設定を `NULL` にすると、既定の値で生成する。
- ライブラリは、文面を書き込み関数に渡すだけである。どこに書くかは、書き込み関数が決める。例の `write_to_stream` は、`fwrite()` で `FILE *` に書く（[標準出力に出す](#標準出力に出す)）。
- `clargs_help_write` がヘルプを、`clargs_help_write_usage` が usage 行だけを生成する。`----` の行は、例のプログラムが書いている。
- usage 行だけの文面は、同じコマンドと設定のヘルプの usage 行と、バイト単位で同じである。
- 文面は、スキーマの定義から作られる。`<integer>` は、値の表示名を設定していない `--retries` の、値型の名前である。`[required]` と `[default: 3]` は、必須と既定値の定義から作られた注記である。
- 生成は、スキーマも設定も変更しない。生成の関数が返すもののうち、利用者が解放するものはない。

## `--help` とエラーの表示

ライブラリは、`--help` を特別扱いしない。`--help` は、利用者がスキーマに定義するフラグである。指定されたかどうかを解析の結果から読み、ヘルプを生成して出力し、プログラムを終えるのは、利用者のコードである。`--help` の読み方と、解析エラーより先に確かめる理由は、[解析の使い方](parse.md#--help-の扱い)にある。

次の例は、`pack` という架空のプログラムで、`--help` が指定されたらヘルプを標準出力に出し、解析エラーのときはメッセージに usage 行を添えて標準エラー出力に出す。これも、そのままコンパイルできる。

```c
#include <clargs/clargs.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* The schema of pack. The root owns all of it. */
typedef struct pack_schema {
    clargs_command_t *root;
    clargs_command_t *create;
    clargs_command_t *list;
} pack_schema_t;

/* Keeps the result of a call in *error and tells whether the call failed. */
static bool failed(clargs_error_t *error, clargs_error_t result)
{
    *error = result;
    return result != CLARGS_ERROR_NONE;
}

/* Adds -h, --help to command. The library adds no such option by itself,
 * and an option is only known where its command is the current command, so
 * every command gets its own. */
static clargs_error_t add_help(clargs_command_t *command)
{
    clargs_option_t *option = NULL;
    clargs_error_t error;

    if (failed(&error, clargs_command_add_flag(command, 'h', "help", &option))) {
        return error;
    }
    return clargs_option_set_description(option, "Print this help.");
}

/* Defines the schema. When it fails, schema->root is NULL or a schema that
 * can be destroyed. */
static clargs_error_t define_schema(pack_schema_t *schema)
{
    clargs_option_t *option = NULL;
    clargs_positional_t *positional = NULL;
    clargs_error_t error;

    schema->root = NULL;

    if (failed(&error, clargs_command_create("pack", &schema->root)) ||
        failed(&error, clargs_command_set_description(schema->root,
                                                      "Pack files into an archive.")) ||
        failed(&error, add_help(schema->root)) ||
        failed(&error, clargs_command_add_count(schema->root, 'v', "verbose", &option)) ||
        failed(&error, clargs_option_set_description(option, "Print more; can be repeated.")) ||
        failed(&error, clargs_command_add_negatable(schema->root, '\0', "color", &option)) ||
        failed(&error, clargs_option_set_description(option, "Color the output.")) ||
        failed(&error, clargs_option_set_initial(option, true))) {
        return error;
    }

    if (failed(&error, clargs_command_add_subcommand(schema->root, "create", &schema->create)) ||
        failed(&error, clargs_command_set_description(schema->create, "Create an archive.")) ||
        failed(&error, add_help(schema->create)) ||
        failed(&error, clargs_command_add_value(schema->create, 'o', "output", clargs_type_string(),
                                                &option)) ||
        failed(&error, clargs_option_set_description(option, "The archive to write.")) ||
        failed(&error, clargs_option_set_value_name(option, "FILE")) ||
        failed(&error, clargs_option_set_required(option, true)) ||
        failed(&error,
               clargs_command_add_value(schema->create, 'l', "level", clargs_type_int(), &option)) ||
        failed(&error, clargs_option_set_description(option, "The compression level.")) ||
        failed(&error, clargs_option_set_value_name(option, "LEVEL")) ||
        failed(&error, clargs_option_set_default(option, "6")) ||
        failed(&error, clargs_command_add_positional(schema->create, "file", clargs_type_string(),
                                                     CLARGS_ARITY_ONE_OR_MORE, &positional)) ||
        failed(&error, clargs_positional_set_description(positional, "The files to pack."))) {
        return error;
    }

    if (failed(&error, clargs_command_add_subcommand(schema->root, "list", &schema->list)) ||
        failed(&error,
               clargs_command_set_description(schema->list, "List the files of an archive.")) ||
        failed(&error, add_help(schema->list)) ||
        failed(&error, clargs_command_add_positional(schema->list, "archive", clargs_type_string(),
                                                     CLARGS_ARITY_REQUIRED, &positional)) ||
        failed(&error, clargs_positional_set_description(positional, "The archive to read.")) ||
        failed(&error, clargs_command_add_positional(schema->list, "pattern", clargs_type_string(),
                                                     CLARGS_ARITY_OPTIONAL, &positional)) ||
        failed(&error, clargs_positional_set_description(positional, "The files to list.")) ||
        failed(&error, clargs_positional_set_default(positional, "*"))) {
        return error;
    }

    /* A schema must be validated before it is parsed with, and before a
     * help is generated from it. */
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

/* The write function: writes one part of the text to the stream that
 * user_data points to. */
static bool write_to_stream(void *user_data, const char *text, size_t length)
{
    return fwrite(text, 1, length, (FILE *)user_data) == length;
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
        error = clargs_parse(schema.root, argc, argv, &result);

        if (result == NULL) {
            /* CLARGS_ERROR_OUT_OF_MEMORY: there is no result. */
            fprintf(stderr, "pack: %s\n", clargs_error_message(error));
        } else if (help_was_given(result)) {
            /* --help first: "pack create --help" leaves out what create
             * requires, and that must not hide the help. The help is the one
             * of the command that the command line ends in. Ending the
             * program is up to this code: the library never exits. */
            if (clargs_help_write(clargs_result_command(result), NULL, write_to_stream, stdout) ==
                CLARGS_ERROR_NONE) {
                status = EXIT_SUCCESS;
            }
        } else if (error == CLARGS_ERROR_INVALID_COMMAND_LINE) {
            /* The message, then the usage line of the command in which the
             * error was found, both to the standard error stream. */
            fprintf(stderr, "pack: %s\n", clargs_result_error_message(result));
            clargs_help_write_usage(clargs_result_error_command(result), NULL, write_to_stream,
                                    stderr);
            fprintf(stderr, "Try 'pack --help' for more information.\n");
        } else {
            /* The values are read here; see docs/parse.md. */
            printf("pack: nothing to show in this example\n");
            status = EXIT_SUCCESS;
        }
    }

    /* The result first, then the schema it was made from. */
    clargs_result_destroy(result);
    clargs_command_destroy(schema.root);

    return status;
}
```

実行の例（`$` の行が入力、次の行からが出力）:

```
$ pack --help
Usage: pack [OPTIONS] [COMMAND]

Pack files into an archive.

Commands:
  create            Create an archive.
  list              List the files of an archive.

Options:
  -h, --help        Print this help.
  -v, --verbose...  Print more; can be repeated.
      --[no-]color  Color the output. [default: on]

$ pack create --help
Usage: pack create [OPTIONS] --output <FILE> <file>...

Create an archive.

Arguments:
  <file>...            The files to pack.

Options:
  -h, --help           Print this help.
  -o, --output <FILE>  The archive to write. [required]
  -l, --level <LEVEL>  The compression level. [default: 6]

$ pack create a.txt
pack: missing required option '--output'
Usage: pack create [OPTIONS] --output <FILE> <file>...
Try 'pack --help' for more information.

$ pack list a b c
pack: unexpected argument 'c'
Usage: pack list [OPTIONS] <archive> [pattern]
Try 'pack --help' for more information.
```

例の要点:

- `--help` は、すべてのコマンドに、利用者が定義する（`add_help`）。ライブラリが足すことはない。ヘルプの `Options:` には、スキーマにあるオプションだけが、ほかのオプションと同じ形で載る。
- ヘルプを出すコマンドは、`clargs_result_command`（コマンドラインが最後に選んだコマンド）である。`pack create --help` は `create` のヘルプになる。
- 解析エラーに添える usage 行のコマンドは、`clargs_result_error_command`（エラーを見つけたときの現在のコマンド）である。解析エラーの結果では、`NULL` にならない。
- usage 行の文面は、改行で終わる。メッセージと並べるときに、改行を足さない。
- ライブラリは、プロセスを終了させない。終了コードを決めて `main` から戻るのは、利用者のコードである。
- 出力先は、書き込み関数に渡すデータで変えられる。例は、ヘルプを `stdout` に、usage 行を `stderr` に書いている。

## 生成の関数

```
clargs_error_t clargs_help_write(const clargs_command_t *command,
                                 const clargs_help_options_t *options,
                                 clargs_help_write_fn_t write_fn, void *user_data);
clargs_error_t clargs_help_write_usage(const clargs_command_t *command,
                                       const clargs_help_options_t *options,
                                       clargs_help_write_fn_t write_fn, void *user_data);
clargs_error_t clargs_help_format(const clargs_command_t *command,
                                  const clargs_help_options_t *options,
                                  char *buffer, size_t size, size_t *length);
clargs_error_t clargs_help_format_usage(const clargs_command_t *command,
                                        const clargs_help_options_t *options,
                                        char *buffer, size_t size, size_t *length);
```

| 関数 | 生成するもの | 出力先 |
|---|---|---|
| `clargs_help_write` | ヘルプ | 書き込み関数 |
| `clargs_help_write_usage` | usage 行 | 書き込み関数 |
| `clargs_help_format` | ヘルプ | 利用者のバッファ |
| `clargs_help_format_usage` | usage 行 | 利用者のバッファ |

- `command` は、ルートコマンドでも、どの段のサブコマンドでもよい。そのコマンドのスキーマは、検証済みでなければならない（[前提条件](#前提条件)）。
- `options` は設定である。`NULL` を渡すと、すべての項目が既定の値になる（[設定](#設定)）。
- 同じコマンドと設定からは、何度生成しても、どの環境でも、バイト単位で同じ文面になる。文面は、ロケール、環境変数、端末に左右されない。
- バッファに生成する関数の文面は、書き込み関数に生成する関数の文面と同じである（[バッファに生成する](#バッファに生成する)）。
- 戻り値と、失敗したときに残るものは、[失敗したとき](#失敗したとき)にある。

## 文面の規則

文面は、スキーマの定義と設定から、次の規則だけで決まる。利用者は、生成された文面を、期待する文字列とバイト単位で比べるテストを書ける。

この節の規則は、名前（ルートコマンドの名前、位置引数の名前、値の表示名、利用者定義の値型の名前）、既定値、選択肢が、空白で始まらず、空白で終わらず、改行を含まないスキーマについて書いている。説明文は、[説明文の書き方についての注意](#説明文の書き方についての注意)にある書き方（改行や、空白だけの行で始める書き方）を避けているものとする。そうでないスキーマでは、この節の文のいくつかが成り立たない。主な違いは、[制限](#制限)と、その注意にある。

どの文面にも共通すること:

- ライブラリが足す行の区切りは `\n` だけで、`\r` を足さない。文面は `\n` で終わる。
- ライブラリは、行末に空白を足さない。スキーマの文字列が持つ `\r` と、名前が持つ行末の空白は、そのまま出力される。ルートコマンドの名前が改行で始まるときは、`Usage: ` の空白が行末に残る（[制限](#制限)）。
- 固定の語句（`Usage:`、`Commands:`、`[OPTIONS]`、`[default: …]` など）は英語である。
- スキーマの文字列（名前、説明文、値の表示名、既定値、選択肢）は、文字を変換せずに出力する。大文字と小文字も変えない。名前（値型の名前を含む）と値の表示名は、バイトのまま出力する。説明文と注記（既定値と選択肢を含む）は、[折り返し](#折り返し)の規則に従うので、行末の空白と、折り返した位置の空白は出力されない。説明文の末尾の改行は、注記が続くときを除いて、出力されない（[注記](#注記)）。コマンドの説明文と、オプションと位置引数の説明文の末尾にある空白と改行の並び（最後の、空白でも改行でもないバイトの後ろ）では、空白を無視し、改行だけを残す（`"foo\n  "` は `"foo\n"` と、`"foo\n \n"` は `"foo\n\n"` と同じ文面になる）。

### 構成

ヘルプは、次の部分を、この順に並べたものである。

| 順 | 部分 | 出る条件 | 内容 |
|---|---|---|---|
| 1 | usage 行 | 常に | [usage 行](#usage-行)の規則のとおり |
| 2 | コマンドの説明文 | 説明文が設定されている | 行の先頭から書く。末尾の改行は出力しない。表示幅で折り返す（[折り返し](#折り返し)） |
| 3 | `Commands:` | サブコマンドが 1 つ以上ある | 直接のサブコマンドを、1 つずつ。見出しは名前、説明は説明文の最初の行（最初の改行の手前まで） |
| 4 | `Arguments:` | 位置引数が 1 つ以上ある | 位置引数を、1 つずつ。説明は、説明文の全体と注記 |
| 5 | `Options:` | オプションが 1 つ以上ある | オプションを、1 つずつ。説明は、説明文の全体と注記 |

- 出る条件を満たさない部分は、名前の行ごと出力しない。
- 部分と部分の間に、空の行を 1 つ置く。文面の先頭と末尾には、空の行を置かない。改行や空白だけの行で始まる説明文は例外である（[説明文の書き方についての注意](#説明文の書き方についての注意)）。
- 一覧は、名前だけの行（`Commands:` など）で始まり、項目が、定義した順に続く。
- 一覧に載るのは、対象のコマンドに定義されたものだけである。親のコマンドのオプション、サブコマンドのオプションと位置引数、サブコマンドのサブコマンドは載らない。スキーマにないオプションが足されることもない。
- 1 つのコマンドは、位置引数とサブコマンドの両方を持てない（[スキーマの使い方](schema.md#コマンド)）。だから、1 つのヘルプに出る一覧は、「`Commands:` と `Options:`」か「`Arguments:` と `Options:`」の、多くて 2 つである。
- 空白と改行だけの説明文は、設定されていないものとして扱う（[説明文の書き方についての注意](#説明文の書き方についての注意)）。

`pack` のヘルプ（[`--help` とエラーの表示](#--help-とエラーの表示)の例の出力）は、usage 行、説明文、`Commands:`、`Options:` からなる。`pack create` のヘルプは、usage 行、説明文、`Arguments:`、`Options:` からなる。

説明文も、オプションも、位置引数も、サブコマンドも持たないコマンド `tool` のヘルプは、usage 行だけである。

```
Usage: tool
```

サブコマンドの説明は、説明文の最初の行だけである。次は、説明文が 2 行ある `create` と、説明文のない `check` を持つコマンドのヘルプである。

```
Usage: tool [COMMAND]

Commands:
  create  Create an archive.
  check
```

`create` 自身のヘルプには、説明文の全体が出る。

```
Usage: tool create

Create an archive.
The files are packed in the order of the command line.
```

### usage 行

usage 行は、次の項目を、この順に並べたものである。

| 順 | 項目 | 出る条件 | 表記 |
|---|---|---|---|
| 1 | コマンドの経路 | 常に | `Usage: ` に続けて、ルートから対象のコマンドまでの名前を、空白 1 つで区切って書く |
| 2 | `[OPTIONS]` | 必須でないオプションが 1 つ以上ある | `[OPTIONS]`。フラグ、否定できるフラグ、カウント、必須でない値付きオプションが、必須でないオプションである |
| 3 | 必須のオプション | 必須の値付きオプションごとに 1 つ。定義した順 | 長い名前があれば `--long <値の表示名>`、なければ `-s <値の表示名>` |
| 4 | 位置引数 | 位置引数ごとに 1 つ。定義した順 | 下の表の 4 通り |
| 5 | `[COMMAND]` | サブコマンドが 1 つ以上ある | `[COMMAND]` |

位置引数の表記は、個数の種類で決まる。`Arguments:` の一覧の見出しも、同じ表記である。

| 個数の種類 | 表記 |
|---|---|
| `CLARGS_ARITY_REQUIRED` | `<name>` |
| `CLARGS_ARITY_OPTIONAL` | `[name]` |
| `CLARGS_ARITY_ZERO_OR_MORE` | `[name]...` |
| `CLARGS_ARITY_ONE_OR_MORE` | `<name>...` |

値の表示名が設定されていないときに出る名前は、[見出し](#見出し)にある。

必須のオプション `--output`（値の表示名は `FILE`）と `-j`（整数。長い名前も値の表示名もない）、位置引数 `source`（必須）、`target`（任意）、`extra`（0 個以上）を持つコマンドの usage 行:

```
Usage: tool --output <FILE> -j <integer> <source> [target] [extra]...
```

フラグ `-q` と、位置引数 `file`（1 個以上）を持つコマンドの usage 行:

```
Usage: tool [OPTIONS] <file>...
```

`tool` のサブコマンド `remote` の、そのまたサブコマンド `add` の usage 行と、`remote` の usage 行:

```
Usage: tool remote add [OPTIONS] <name>
Usage: tool remote [COMMAND]
```

折り返しの規則:

- 2 つ目以降の項目は、前の項目との間に、空白を 1 つ置く。
- ただし、表示幅が 0 でなく、「現在の桁 + 1 + 項目の幅」が表示幅を超えるときは、空白の代わりに、改行と空白 7 桁を置く。7 桁は `Usage: ` の幅で、続きの行は、コマンドの経路の先頭と同じ桁から始まる。
- 項目は、途中で分けない。値の表示名に空白があっても、そこでは分けない。
- 表示幅より広い項目は、その項目だけの行になり、表示幅を超えて出力される。
- コマンドの経路は、表示幅を超えても、`Usage: ` の直後に続けて出力する。経路の中では折り返さない。
- 表示幅が 0 なら、折り返さない。
- 項目の幅は、その項目のバイト数である。

`pack create` の usage 行は、既定の表示幅（80）では 1 行である。表示幅を 40 にすると、次のようになる。`[OPTIONS]` の後ろに `--output <FILE>` を置くと 44 桁になるので、そこで折り返す。

```
Usage: pack create [OPTIONS]
       --output <FILE> <file>...
```

表示幅を 20 にした、値の表示名が `FILE OR DIRECTORY` の必須のオプションを持つコマンドの usage 行である。2 行目の項目は 28 桁あり、表示幅を超えるが、分けずに出力される。

```
Usage: tool
       --output <FILE OR DIRECTORY>
       <source>
```

### 見出し

サブコマンドの見出しは、名前である。位置引数の見出しは、[usage 行](#usage-行)の位置引数の表記と同じである。オプションの見出しは、名前の部分と、種類ごとの続きからなる。

名前の部分:

| オプションが持つ名前 | 表記 |
|---|---|
| 短い名前と長い名前 | `-s, --long` |
| 短い名前だけ | `-s` |
| 長い名前だけ | 空白 4 桁と `--long`。長い名前の桁が、短い名前を持つオプションと揃う |

種類ごとの続き:

| 種類 | 表記 | 例 |
|---|---|---|
| フラグ | 名前の部分だけ | `-h, --help` |
| 否定できるフラグ | 長い名前を `--[no-]long` と書く | `--[no-]color` |
| カウント | 名前の部分の直後に `...` | `-v, --verbose...` |
| 値付きオプション | 名前の部分の後ろに、空白 1 つ、`<`、値の表示名、`>` | `-o, --output <FILE>` |

値の表示名（`clargs_option_set_value_name`）が設定されていない値付きオプションでは、値の表示名の代わりに、値型の名前が出る。組み込みの値型の名前は、`string`、`integer`、`float` である。選択肢の値型の名前は `choice` で、利用者定義の値型の名前は、値型を生成したときに渡した名前である。usage 行の必須のオプションでも、同じである。

すべての表記を並べたヘルプである。

```
Usage: tool [OPTIONS]

Options:
  -h, --help           Both names
  -q                   A short name only
      --dry-run        A long name only
      --[no-]color     A negatable flag
  -c, --[no-]check     A negatable flag with a short name
  -v, --verbose...     A count
  -d...                A count with a short name only
  -o, --output <FILE>  A value with a display name
  -j <integer>         A value without one: the name of the type
      --ratio <float>  A float
      --name <string>  A string
      --mode <choice>  A choice [choices: fast, small]
```

- `--help` をフラグとして定義すると、ほかのフラグと同じ形で載る。
- 見出しの幅は、見出しのバイト数である。

### 注記

注記は、オプションと位置引数の説明文の後ろに、次の順に出る。サブコマンドには、注記がない。

| 順 | 注記 | 出る条件 |
|---|---|---|
| 1 | `[choices: a, b, c]` | 値付きオプションまたは位置引数で、値型が選択肢である。選択肢を、定義した順に、`, ` で区切って並べる |
| 2 | `[default: 値]` | 値付きオプションまたは位置引数で、既定値が設定されている。値は、設定した文字列のままである。ただし、注記も折り返されるので、値の中の行末の空白と、折り返した位置の空白は出力されない |
| 2 | `[default: on]` | 否定できるフラグで、初期の状態がオンである。オフのときは、注記がない |
| 3 | `[required]` | 値付きオプションで、必須である。位置引数には出ない。位置引数が必須かどうかは、見出しの表記で分かる |

- 説明文と注記の間、注記と注記の間には、空白を 1 つ置く。
- 説明文が設定されていない項目では、注記が、説明の列から始まる。
- 説明文が改行で終わるときは、注記は、空白を置かずに、次の行の説明の列から始まる。注記を独立した行にしたいときに使える。説明文が 2 つ以上の改行で終わるときは、2 つ目からの改行が、説明文と注記の間の空の行になる。注記のない項目では、説明文の末尾の改行は出力されない。
- 注記も、説明文と同じに折り返される。注記の中の空白でも折り返される（[折り返し](#折り返し)）。
- 必須で、かつ既定値を持つオプションは、検証で誤りになる（[スキーマの使い方](schema.md#検証の時点の誤り)）。だから、`[default: …]` と `[required]` が同じ項目に出ることはない。

次のヘルプの `--kind` は、説明文がない。`--title` は、説明文が改行で終わっている（`"The title of the archive.\n"`）。`--color` は初期の状態がオン、`--sort` はオフである。

```
Usage: tool [OPTIONS] --output <FILE> --kind <choice> <format> [level]

Arguments:
  <format>             The format of the archive. [choices: tar, zip, cpio]
  [level]              The compression level. [default: 6]

Options:
  -m, --mode <MODE>    What to prefer. [choices: fast, small] [default: fast]
  -o, --output <FILE>  The archive to write. [required]
  -k, --kind <choice>  [choices: fast, small] [required]
      --[no-]color     Color the output. [default: on]
      --[no-]sort      Sort the names.
  -t, --title <TEXT>   The title of the archive.
                       [default: untitled]
```

### 説明の列

- 項目は、字下げの桁数の空白で始まり、見出しが続く。字下げは、設定の項目である（既定は 2）。
- 説明は、説明の列から始まる。説明の列は、1 つのヘルプに 1 つで、そのヘルプのすべての一覧に共通である。
- 説明のない項目（説明文も注記もない項目）は、見出しだけの行になる。

説明の列は、次のように決まる。

1. 上限を決める。上限は、設定の「説明の列の上限」（既定は 32）と、表示幅の半分（端数は切り捨て）の、小さいほうである。表示幅が 0 のときは、表示幅の半分を考えない。
2. 上限が「字下げ + 2」より小さければ、上限を「字下げ + 2」にする。
3. 説明の列は、「字下げ + そのヘルプで最も広い見出しの幅 + 2」である。これが上限を超えるなら、説明の列は上限になる。

項目の行の形は、見出しの幅で決まる。

| 条件 | 行の形 |
|---|---|
| 「字下げ + 見出しの幅 + 2」が説明の列以下 | 見出しと同じ行の、説明の列から、説明が始まる。見出しと説明の間の空白は、2 桁以上になる |
| 「字下げ + 見出しの幅 + 2」が説明の列を超える | 見出しだけの行になり、次の行の説明の列から、説明が始まる |

`pack create` のヘルプ（[`--help` とエラーの表示](#--help-とエラーの表示)の例の出力）では、最も広い見出しは `-o, --output <FILE>` と `-l, --level <LEVEL>` の 19 桁である。説明の列は 2 + 19 + 2 = 23 で、上限の 32 を超えないので、`Arguments:` と `Options:` のどちらの説明も、23 桁から始まる。

次のヘルプは、既定の設定で、最も広い見出しが 41 桁ある。説明の列は、上限の 32 になる。`--exactly-two-blanks-fit` の見出しは 28 桁で、2 + 28 + 2 = 32 なので、説明は同じ行に出る。それより 1 桁広い `--one-blank-is-not-enough` の説明は、次の行に出る。最後のオプションは、説明がないので、見出しだけの行である。

```
Usage: tool [OPTIONS]

Options:
  -v, --verbose...              Say more
      --a-rather-long-name-of-option <FILE>
                                Write to FILE
      --exactly-two-blanks-fit  On the line of the heading
      --one-blank-is-not-enough
                                On the next line
      --a-long-name-without-a-description
```

表示幅を 40 にすると、上限は、表示幅の半分の 20 になる。

```
Usage: tool [OPTIONS]

Options:
  -o, --output <FILE>
                    The archive to
                    write.
      --compression-level <LEVEL>
                    The compression
                    level.
```

### 折り返し

コマンドの説明文と、項目の説明は、表示幅を超えないように折り返される。コマンドの説明文は 0 桁から、項目の説明は説明の列から始まり、折り返した続きの行も、同じ桁から始まる。

- 語は、空白（0x20）と改行（0x0A）以外のバイトの並びである。タブなど、ほかのバイトは、語の一部として扱う。
- 折り返すのは、語と語の間の空白の位置だけである。語の途中では折り返さない。
- 語を置くと表示幅を超えるときに、その語の前で折り返す。折り返した位置の空白は、出力しない。
- 行の最初の語は、表示幅を超えても、そのまま出力する。説明の列から表示幅までに収まらない語は、表示幅を超えて出力される。
- 説明文の中の改行では、必ず行を改める。続きは、同じ桁から始まる。連続する改行は、空の行になる。
- 行の途中の連続する空白と、改行の直後の空白は、そのままの個数で出力する。改行の直後の空白で、字下げした行を書ける。
- 行末の空白は、出力しない。
- 表示幅が 0 なら、説明文の中の改行の位置でだけ、行を改める。

表示幅を 60 にしたヘルプである。コマンドの説明文には、空の行（連続する改行）がある。`--exclude` の説明文は、改行と、行の先頭の空白を含む。`--url` の説明文には、表示幅に収まらない語がある。

```
Usage: tool [OPTIONS] --output <FILE>

Pack the files that are named on the command line into one
archive, in the order in which they are named.

An archive that exists is replaced.

Options:
  -o, --output <FILE>      The archive to write. A file of
                           this name that exists is replaced
                           without a question. [required]
  -x, --exclude <PATTERN>  Leave out the files that match:
                             *.o    one kind of file
                             build/ a directory
  -u, --url                See
                           https://example.com/a/very/long/address/that/has/no/blank/in/it/at/all
                           for more.
```

## 設定

設定（`clargs_help_options_t`）は、文面の整形を決める値を持つオブジェクトである。利用者が生成し、生成の関数に渡し、破棄する。

| すること | 関数 | 備考 |
|---|---|---|
| 生成する | `clargs_help_options_create` | すべての項目が既定の値になる。確保に失敗すると `CLARGS_ERROR_OUT_OF_MEMORY` を返し、`*out` は `NULL` になる |
| 破棄する | `clargs_help_options_destroy` | `NULL` は何もしない |
| 表示幅を設定する | `clargs_help_options_set_width` | 失敗しない |
| 字下げを設定する | `clargs_help_options_set_indent` | 失敗しない |
| 説明の列の上限を設定する | `clargs_help_options_set_max_column` | 失敗しない |

| 項目 | 意味 | 既定の値 |
|---|---|---|
| 表示幅 | 折り返しの基準になる、1 行の桁数。0 は「制限なし」を表し、折り返さない | 80 |
| 字下げ | 一覧の項目の前に置く空白の桁数 | 2 |
| 説明の列の上限 | 説明の列が超えない桁。ただし、表示幅が 0 でなく、表示幅の半分（端数は切り捨て）のほうが小さければ、その半分が上限になる。そのうえで、上限が「字下げ + 2」より小さければ、「字下げ + 2」が上限になる。「字下げ + 2」が優先するので、このときは、説明の列が、設定した上限や表示幅の半分を超えることがある（[説明の列](#説明の列)） | 32 |

- どの項目も、65535 が上限である。65535 を超える値を設定すると、65535 として扱う。設定の関数は、値を拒まない。
- 生成の関数の `options` に `NULL` を渡すと、すべての項目が既定の値になる。生成したままの設定を渡したときと、同じ文面になる。
- 設定は、usage 行にも効く。usage 行が使うのは、表示幅だけである。
- 生成は、設定を変更しない。1 つの設定を、何回の生成にも、別々のスキーマの生成にも使える。
- 変更していない設定は、複数のスレッドから同時に生成に渡せる。設定の関数と破棄の関数を呼んでいる間は、ほかのスレッドが、その設定を使ってはならない。
- ライブラリは、端末の幅も環境変数も調べない。端末の幅に合わせたいときは、利用者が幅を調べて、表示幅に設定する。
- 設定の中身は公開していない。後のバージョンで、項目と、その設定の関数が増えることがある。

次の関数は、表示幅を 40 にして、ヘルプを標準出力に出す（`write_to_stream` は、[`--help` とエラーの表示](#--help-とエラーの表示)の例のものである）。

```c
/* Prints the help of command to the standard output, 40 columns wide. */
static clargs_error_t print_narrow_help(const clargs_command_t *command)
{
    clargs_help_options_t *options = NULL;
    clargs_error_t error;

    /* The settings start with their default values: 80, 2 and 32. */
    error = clargs_help_options_create(&options);
    if (error != CLARGS_ERROR_NONE) {
        /* CLARGS_ERROR_OUT_OF_MEMORY: options is NULL. */
        return error;
    }

    /* The functions that set a value cannot fail. */
    clargs_help_options_set_width(options, 40);

    error = clargs_help_write(command, options, write_to_stream, stdout);

    /* The settings belong to the caller. */
    clargs_help_options_destroy(options);
    return error;
}
```

`pack create` を渡したときの出力:

```
Usage: pack create [OPTIONS]
       --output <FILE> <file>...

Create an archive.

Arguments:
  <file>...         The files to pack.

Options:
  -h, --help        Print this help.
  -o, --output <FILE>
                    The archive to
                    write. [required]
  -l, --level <LEVEL>
                    The compression
                    level. [default: 6]
```

表示幅の半分の 20 が説明の列の上限になり、`--output` と `--level` の説明は、次の行に出ている。

## 出力先

生成の関数は、文面を、書き込み関数に渡すか、利用者のバッファに書く。ライブラリは、`<stdio.h>` を使わず、利用者が指定していない場所には、何も書かない。`FILE *` に直接出力する関数はない。`FILE *` に出すには、`fwrite()` を呼ぶ書き込み関数を書く（[標準出力に出す](#標準出力に出す)）。

### 書き込み関数の契約

```
typedef bool (*clargs_help_write_fn_t)(void *user_data, const char *text, size_t length);
```

ライブラリは、文面を、先頭から順に、いくつかの部分に分けて、書き込み関数に渡す。

- `user_data` は、生成の関数に渡したポインタが、そのまま渡る。ライブラリは、指す先を読まない。`NULL` でもよい。
- `text` は、文面の一部の先頭を指し、`length` は、そのバイト数である。**終端のナル文字は付かない。** ちょうど `length` バイトを読む。
- `length` は 1 以上である。長さ 0 では呼ばれない。
- 呼び出しは、文面の先頭から順である。渡された部分を、呼び出しの順につなげると、文面の全体になる。
- 文面をいくつに、どこで分けるかは、決まっていない。バージョンによって変わりうる。1 つの部分が、1 行や 1 語であることを前提にしない。
- `text` は、その呼び出しの間だけ有効である。後で使うなら、呼び出しの中で複製する。
- 書き込めたら `true`、書き込めなかったら `false` を返す。`false` を返すと、生成はそこで止まり、書き込み関数は、その生成ではもう呼ばれない。生成の関数は `CLARGS_ERROR_WRITE_FAILED` を返す。
- 書き込み関数の中から、生成に渡したスキーマを、変更したり破棄したりしてはならない。

### 標準出力に出す

`FILE *` に書く書き込み関数は、`fwrite()` を呼ぶだけである。`user_data` に、ストリームを渡す。

```c
/* The write function: writes one part of the text to the stream that
 * user_data points to. The part is not terminated, so it is written with
 * its length. */
static bool write_to_stream(void *user_data, const char *text, size_t length)
{
    return fwrite(text, 1, length, (FILE *)user_data) == length;
}
```

```
error = clargs_help_write(root, NULL, write_to_stream, stdout);
```

- `fwrite()` が書けたバイト数が `length` より少なければ、`false` を返す。生成が止まり、`CLARGS_ERROR_WRITE_FAILED` が返る。
- `text` は終端されていないので、`fputs()` や `printf("%s")` には渡せない。
- 書き込み関数の呼び出しは、語や空白の単位で、回数が多い。`FILE *` は標準ライブラリがバッファするので、問題にならない。バッファしない出力先に書くときは、書き込み関数の側でまとめる。
- **テキストモードのストリームでは、Windows で改行が変換される。** ライブラリが渡す改行は `\n` だけである。`stdout` はふつうテキストモードで、Windows では、書いた `\n` が `\r\n` になって出力される。端末に出すぶんには、これで正しい。出力をバイト単位で比べるとき、または `\n` のままファイルに残すときは、バイナリモードで開いたストリーム（`fopen()` のモードに `b` を付ける）に書く。

### バッファに生成する

`clargs_help_format` と `clargs_help_format_usage` は、文面を、利用者が用意した配列に、文字列として書く。書き込み関数は要らない。

| `size` | `buffer` に書くもの |
|---|---|
| 0 | 何も書かない。`buffer` は `NULL` でもよい |
| 1 以上 | 文面の先頭から、収まる分（多くて `size - 1` バイト）と、その直後の終端のナル文字。配列の外には書かない |

- `length` が `NULL` でなければ、`*length` に、文面の全体の長さ（終端のナル文字を除く）が入る。収まったかどうかによらない。
- 収まらない文面は、切り詰められる。**切り詰めは失敗ではなく、`CLARGS_ERROR_NONE` が返る。** `*length` が `size` 以上なら、切り詰められている。切り詰められた文面は、行の途中で終わっていることがある。
- 文面は、同じコマンドと設定で `clargs_help_write`、`clargs_help_write_usage` が書き込み関数に渡す文面と、バイト単位で同じである。

文面の全体を得るには、2 回呼ぶ。1 回目は、`size` を 0、`buffer` を `NULL` にして、長さを得る。2 回目は、`*length + 1` バイトの配列を渡す。

```c
/* Returns the help of command as a string that the caller frees with
 * free(), or NULL when it cannot be made. */
static char *help_string(const clargs_command_t *command)
{
    char *text = NULL;
    size_t length = 0;

    /* First the length: with the size 0 nothing is written, and the buffer
     * may be NULL. */
    if (clargs_help_format(command, NULL, NULL, 0, &length) != CLARGS_ERROR_NONE) {
        return NULL;
    }

    text = malloc(length + 1);
    if (text == NULL) {
        return NULL;
    }

    /* Then the text. The schema and the settings are the same as in the
     * first call, so the text has the same length and fits. */
    if (clargs_help_format(command, NULL, text, length + 1, NULL) != CLARGS_ERROR_NONE) {
        free(text);
        return NULL;
    }
    return text;
}
```

固定長の配列に、usage 行を受け取る例である。`clargs_help_format_usage` は失敗しないので、戻り値を確かめていない。

```c
/* Prints an error message with the usage line of command behind it. */
static void print_error(const clargs_command_t *command, const char *message)
{
    char usage[256];
    size_t length = 0;

    /* The usage line cannot fail. It is cut when it does not fit. */
    clargs_help_format_usage(command, NULL, usage, sizeof(usage), &length);

    if (length >= sizeof(usage)) {
        /* Cut: the array holds the first sizeof(usage) - 1 bytes. */
        fprintf(stderr, "pack: %s\n", message);
    } else {
        /* The text ends with a newline of its own. */
        fprintf(stderr, "pack: %s\n%s", message, usage);
    }
}
```

## 制限

- **幅は、バイトで数える。** 1 バイトを 1 桁と数えるので、ASCII 以外の文字（UTF-8 では 1 文字が 2 バイト以上）を含む説明文は、見た目の幅が表示幅に届く前に折り返される。ASCII 以外の文字を含む名前の見出しは、見た目より広いものとして数えられ、説明の列が右に寄る。ライブラリは、文字の表示幅を調べない。

  次は、表示幅を 50 にしたヘルプである。`--quiet` の説明は、見た目では 50 桁に収まるが、バイト数で数えると収まらないので、折り返されている。

  ```
  Usage: tool [OPTIONS]

  Options:
    -q, --quiet    何も 出力 しない ように
                   する
    -v, --verbose  Print more than usual
  ```

- **名前に改行を入れると、列が崩れる。** 位置引数の名前、値の表示名、ルートコマンドの名前、利用者定義の値型の名前は、改行を含む文字列でも定義できる。ヘルプは、これらを変換せずに、改行ごと出力する。そのとき、見出しや usage 行の項目の幅は、改行も含めた全体のバイト数で数える。一方、出力は改行で行が改まり、続きは、次の行の先頭から始まる。そのため、説明の列までの空白と、usage 行の折り返しの位置が、見た目と合わなくなる。名前と値の表示名には、改行を入れないこと。

  次は、名前が `"first\nsecond"` の位置引数を持つコマンドのヘルプである。見出し `<first\nsecond>` の幅は 14 桁と数えられ、説明の列は 18 になる。出力では、`second>` が行の先頭から始まるので、説明の前の空白が足りないように見える。

  ```
  Usage: tool [OPTIONS] <first
  second>

  Arguments:
    <first
  second>           A name with a newline

  Options:
    -q, --quiet     Print nothing
  ```

- **名前の空白と、文字列の中の `\r` は、そのまま出力される。** 名前の末尾の空白と、名前の中の改行の直前の空白は、そのまま出力されるので、行が空白で終わることがある（名前が `"tool "` で、オプションも位置引数もサブコマンドもないルートコマンドの usage 行は、`Usage: tool ` と空白で終わる）。ルートコマンドの名前が改行で始まるときは、`Usage: ` の空白が行末に残る（名前が `"\ntool"` なら、1 行目は `Usage: ` と空白で終わる）。スキーマの文字列（名前、説明文、値の表示名、既定値、選択肢）の中の `\r` も、そのまま出力される。
- タブや制御文字も、1 バイトを 1 桁と数える。
- 固定の語句は英語だけである。ほかの言語にする方法、色を付ける方法、ヘルプの部分を並べ替える方法はない。
- オプションを一覧から隠す方法、グループに分ける方法はない。
- サブコマンドのヘルプに、親のコマンドのオプションは載らない。

### 説明文の書き方についての注意

- 空白（0x20）と改行（0x0A）以外のバイトを持たない説明文は、設定されていないものとして扱う。空の文字列、`"   "`、`" \n"` は、どれも「説明文なし」である。コマンドの説明文でも、オプションと位置引数の説明文でも同じである。`Commands:` の一覧のサブコマンドの説明は、説明文の最初の行だけで判断する。最初の行が空白だけなら、後ろに続きがあっても、一覧では見出しだけの行になる。
- **説明文を、改行や、空白だけの行（空白を 1 つ以上持ち、ほかのバイトを持たない行）で始めない。** コマンドの説明文（そのコマンド自身のヘルプ）と、オプションと位置引数の説明文では、先頭の改行も、先頭の空白だけの行も、取り除かれない。そのため、文面に空の行が増えたり、説明や注記の始まる位置が変わったりすることがある。[構成](#構成)の「部分と部分の間の空の行は 1 つ」が、成り立たなくなることがある。たとえば、コマンドの説明文が `"\nPack files."` のヘルプは、次のようになる。

  ```
  Usage: tool [OPTIONS]


  Pack files.

  Options:
    -q, --quiet    Print nothing.
    -v, --verbose  Print more.
  ```

  このような説明文の文面は、後のバージョンで変わることがある。

## 失敗したとき

実行時の失敗は、書き込みの失敗と、メモリ確保の失敗の 2 つである。どちらも、戻り値の `clargs_error_t` で分かる。

| 戻り値 | 意味 | 返す関数 |
|---|---|---|
| `CLARGS_ERROR_NONE` | 成功。バッファに生成する関数では、切り詰められたときも成功である | すべて |
| `CLARGS_ERROR_WRITE_FAILED` | 書き込み関数が `false` を返した | `clargs_help_write`、`clargs_help_write_usage` |
| `CLARGS_ERROR_OUT_OF_MEMORY` | メモリの確保に失敗した | `clargs_help_write`、`clargs_help_format`、`clargs_help_options_create` |

**生成が失敗したとき、失敗までに書き込み関数に渡した文面は、出力先に残る。** ライブラリは、渡した文面を取り消せない。

| 関数 | 失敗 | 出力先に残るもの |
|---|---|---|
| `clargs_help_write`、`clargs_help_write_usage` | 書き込みの失敗 | 書き込み関数が `true` を返した呼び出しで受け取った部分。文面の先頭から、ある位置までである。行の途中で終わっていることがある |
| `clargs_help_write` | 確保の失敗 | 文面の先頭から、確保を必要とした項目（オプションまたは位置引数）の行の手前まで。直前の行の改行で終わる |
| `clargs_help_format` | 確保の失敗 | `size` が 0 でなければ、`buffer` は空の文字列になる。`*length` は 0 になる。`size` が 0 なら、`buffer` には何も書かない |
| `clargs_help_options_create` | 確保の失敗 | `*out` は `NULL` になる |

- 書き込みの失敗の後、書き込み関数は、その生成ではもう呼ばれない。
- `clargs_help_format` が確保に失敗したとき、`buffer` は空の文字列になるが、配列の中の、終端のナル文字より後ろのバイトの内容は決まっていない。失敗までに生成した文面の一部が残っていることがある。配列の外には書かない。
- usage 行だけの生成（`clargs_help_write_usage`、`clargs_help_format_usage`）は、メモリを確保しない。`CLARGS_ERROR_OUT_OF_MEMORY` を返すことはない。`clargs_help_format_usage` は、失敗しない。
- `clargs_help_format` は、`CLARGS_ERROR_WRITE_FAILED` を返さない。
- 失敗しても、スキーマと設定は変わらない。生成のために確保したメモリは、成功でも失敗でも、関数が戻る前にすべて解放される。失敗の後に、同じスキーマと設定で、もう一度生成できる。
- 途中までの文面を出力先に残したくないときは、`clargs_help_format` でバッファに生成し、成功を確かめてから出力する。
- `NULL` の引数や、検証を済ませていないスキーマは、失敗ではなく、プログラムの誤りである（[前提条件](#前提条件)）。

## 寿命の規則

| もの | 所有者 | 有効な期間 |
|---|---|---|
| 設定（`clargs_help_options_t`） | 生成した利用者 | `clargs_help_options_destroy` で破棄するまで。スキーマとは独立で、どちらを先に破棄してもよい |
| 書き込み関数に渡される `text` | ライブラリまたはスキーマ | その呼び出しの間だけ。利用者は、解放も変更もしない |
| バッファ（`buffer`） | 利用者 | 生成の呼び出しの間、有効であればよい。ライブラリは、呼び出しが戻った後は参照しない |
| スキーマと、それが使う値型 | 利用者 | 生成の呼び出しの間、変更も破棄もしない |

- 生成の関数は、スキーマと設定を、呼び出しの間だけ借りる。どちらも変更せず、戻った後は参照しない。
- 生成の関数が返すもののうち、利用者が解放するものはない。ライブラリが確保した文字列を返す関数はない。
- 生成のためのメモリは、ライブラリに設定されたアロケータ（`clargs_set_allocator`）から確保し、関数が戻る前に解放する。設定のオブジェクトも、同じアロケータから確保する。設定のオブジェクトが残っている間に、アロケータを設定し直してはならない（[所有権と寿命の規約](ownership.md#5-アロケータ)）。
- スキーマと設定が変更されていない間は、同じスキーマと設定の生成を、複数のスレッドから同時に呼べる。バッファは、呼び出しごとに別のものを渡す。書き込み関数が同時に呼ばれても正しく動くかどうかは、利用者の書き込み関数による。

## 前提条件

前提条件を破るのは、プログラムの誤りである。エラーコードは返らない。検査できるものは `assert` で検査され、Debug 構成では、破るとプロセスが中断する。Release 構成では検査されず、動作は定まらない（[所有権と寿命の規約](ownership.md#実行時の失敗とプログラムの誤り)）。関数ごとの前提条件は、公開ヘッダ `include/clargs/help.h` のコメントにも `Preconditions:` として書いてある。

| 関数 | `assert` で検査する前提条件 | 検査しない前提条件 |
|---|---|---|
| `clargs_help_options_create` | `out` は `NULL` でない | なし |
| `clargs_help_options_destroy` | なし | `options` は、`NULL` か、破棄されていない設定である。ほかのスレッドが、その設定を使っていない |
| `clargs_help_options_set_width`、`clargs_help_options_set_indent`、`clargs_help_options_set_max_column` | `options` は `NULL` でない | `options` は、破棄されていない。呼び出しの間、ほかのスレッドが、その設定を使っていない |
| `clargs_help_write`、`clargs_help_write_usage` | `command` は `NULL` でない。`command` のスキーマは検証済みである（`clargs_command_is_validated`）。`write_fn` は `NULL` でない | 生成に共通のもの。書き込み関数は、スキーマを変更も破棄もしない |
| `clargs_help_format`、`clargs_help_format_usage` | `command` は `NULL` でない。`command` のスキーマは検証済みである（`clargs_command_is_validated`）。`size` が 0 でなければ、`buffer` は `NULL` でない | 生成に共通のもの。`buffer` は、`size` バイト以上の配列を指す |

生成に共通の、検査しない前提条件:

- `options` は、`NULL` か、破棄されていない設定である。呼び出しの間、ほかのスレッドが、その設定を変更していない。
- 呼び出しの間、スキーマと、それが使う値型は、変更も破棄もされない。

`NULL` でもよい引数:

- 生成の関数の `options`（既定の値で生成する）。
- `clargs_help_write`、`clargs_help_write_usage` の `user_data`。
- `clargs_help_format`、`clargs_help_format_usage` の `length`（長さを受け取らない）と、`size` が 0 のときの `buffer`。
- `clargs_help_options_destroy` の `options`。

検証を済ませていないスキーマを生成に渡すのは、前提条件の違反である。スキーマに定義要素を足したり、属性を設定したりすると、スキーマは未検証に戻るので、生成の前に、もう一度検証する（[スキーマの使い方](schema.md#検証と失敗の調べ方)）。サブコマンドを渡すときも、検証するのは、そのルートである。
