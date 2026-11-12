# CLArgs のスキーマの使い方

プログラムがどんなコマンドラインを受け付けるかを、関数呼び出しで定義する方法をまとめる。定義したものをスキーマと呼ぶ。スキーマは、コマンド、オプション、位置引数と、値の変換の仕方を表す値型からなる。

この文書が扱うのは、スキーマの定義、検証、読み取り、破棄である。argv の解析と、ヘルプの文面の生成は扱わない。スキーマは argv を知らない。

関数ごとの詳しい説明と前提条件は、公開ヘッダ `include/clargs/schema.h` と `include/clargs/type.h` のコメントにある。メモリの持ち方の決まりは、[所有権と寿命の規約](ownership.md)にある。

目次:

- [一連の例](#一連の例)
- [コマンド](#コマンド)
- [オプション](#オプション)
- [位置引数](#位置引数)
- [値型](#値型)
- [利用者定義の値型の書き方](#利用者定義の値型の書き方)
- [値型を借りる契約と破棄の順序](#値型を借りる契約と破棄の順序)
- [誤りの一覧](#誤りの一覧)
- [検証と、失敗の調べ方](#検証と失敗の調べ方)
- [検証済みのスキーマで成り立つこと](#検証済みのスキーマで成り立つこと)
- [読み取り](#読み取り)
- [前提条件](#前提条件)

## 一連の例

`pack` という架空のプログラムのコマンドラインを定義し、検証し、破棄する。次の形のコマンドラインを表す。

```
pack [-v]... [-q] [--color | --no-color] create -o FILE [-l LEVEL] [--ratio RATIO]
                                                [--format tar|zip] [--block-size SIZE] file...
pack [-v]... [-q] [--color | --no-color] list archive [pattern]
```

例は 1 つの翻訳単位で、取り込むのは `<clargs/clargs.h>` と標準ヘッダだけである。取り出して、そのままコンパイルできる。検証に成功すれば `the schema of pack is valid` と書いて、終了コード 0 で終わる。

```c
#include <clargs/clargs.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* The user data of the value type "size": the largest size it takes. */
typedef struct size_limit {
    unsigned long max;
} size_limit_t;

/*
 * The conversion function of the value type "size". Takes decimal digits
 * with an optional unit, 'k' or 'm' ("512", "64k", "2m"), and writes the
 * number of bytes as an unsigned long.
 *
 * It returns CLARGS_ERROR_INVALID_VALUE for an input that it does not take,
 * and it has no side effect: the validation of a schema calls it for every
 * default value of this type.
 */
static clargs_error_t convert_size(const char *text, void *value, void *context)
{
    const size_limit_t *limit = context;
    const char *p = text;
    unsigned long bytes = 0;
    unsigned long unit = 1;

    if (*p < '0' || *p > '9') {
        return CLARGS_ERROR_INVALID_VALUE;
    }
    for (; *p >= '0' && *p <= '9'; ++p) {
        unsigned long digit = (unsigned long)(*p - '0');

        if (bytes > (limit->max - digit) / 10) {
            return CLARGS_ERROR_INVALID_VALUE;
        }
        bytes = bytes * 10 + digit;
    }
    if (*p == 'k') {
        unit = 1024UL;
        ++p;
    } else if (*p == 'm') {
        unit = 1024UL * 1024UL;
        ++p;
    }
    if (*p != '\0' || bytes > limit->max / unit) {
        return CLARGS_ERROR_INVALID_VALUE;
    }

    *(unsigned long *)value = bytes * unit;
    return CLARGS_ERROR_NONE;
}

/* Keeps the result of a call in *error and tells whether the call failed. */
static bool failed(clargs_error_t *error, clargs_error_t result)
{
    *error = result;
    return result != CLARGS_ERROR_NONE;
}

/* The options of the root: they stand in front of the subcommand. */
static clargs_error_t define_common_options(clargs_command_t *root)
{
    clargs_option_t *option;
    clargs_error_t error;

    /* A count: -v, -vv, --verbose. */
    if (failed(&error, clargs_command_add_count(root, 'v', "verbose", &option)) ||
        failed(&error, clargs_option_set_description(option, "Print more; can be repeated."))) {
        return error;
    }

    /* A flag: -q, --quiet. */
    if (failed(&error, clargs_command_add_flag(root, 'q', "quiet", &option)) ||
        failed(&error, clargs_option_set_description(option, "Print nothing."))) {
        return error;
    }

    /* A negatable flag without a short name: --color and --no-color. It is
     * on when neither is given. */
    if (failed(&error, clargs_command_add_negatable(root, '\0', "color", &option)) ||
        failed(&error, clargs_option_set_description(option, "Color the output.")) ||
        failed(&error, clargs_option_set_initial(option, true))) {
        return error;
    }

    return CLARGS_ERROR_NONE;
}

/* The subcommand "create": options that take a value, and its positional. */
static clargs_error_t define_create(clargs_command_t *root, const clargs_type_t *format,
                                    const clargs_type_t *size)
{
    clargs_command_t *create;
    clargs_option_t *option;
    clargs_positional_t *positional;
    clargs_error_t error;

    if (failed(&error, clargs_command_add_subcommand(root, "create", &create)) ||
        failed(&error, clargs_command_set_description(create, "Create an archive."))) {
        return error;
    }

    /* A required option with the built-in value type string. */
    if (failed(&error, clargs_command_add_value(create, 'o', "output", clargs_type_string(),
                                                &option)) ||
        failed(&error, clargs_option_set_description(option, "The archive to write.")) ||
        failed(&error, clargs_option_set_value_name(option, "FILE")) ||
        failed(&error, clargs_option_set_required(option, true))) {
        return error;
    }

    /* The built-in value type integer, with a default value. A default
     * value is a string, written as on a command line. */
    if (failed(&error, clargs_command_add_value(create, 'l', "level", clargs_type_int(),
                                                &option)) ||
        failed(&error, clargs_option_set_description(option, "The compression level.")) ||
        failed(&error, clargs_option_set_value_name(option, "LEVEL")) ||
        failed(&error, clargs_option_set_default(option, "6"))) {
        return error;
    }

    /* The built-in value type float; only a long name. */
    if (failed(&error, clargs_command_add_value(create, '\0', "ratio", clargs_type_float(),
                                                &option)) ||
        failed(&error, clargs_option_set_description(option, "Stop below this ratio.")) ||
        failed(&error, clargs_option_set_value_name(option, "RATIO")) ||
        failed(&error, clargs_option_set_default(option, "0.5"))) {
        return error;
    }

    /* A choice. */
    if (failed(&error, clargs_command_add_value(create, '\0', "format", format, &option)) ||
        failed(&error, clargs_option_set_description(option, "The format of the archive.")) ||
        failed(&error, clargs_option_set_default(option, "tar"))) {
        return error;
    }

    /* A user-defined value type. */
    if (failed(&error, clargs_command_add_value(create, '\0', "block-size", size, &option)) ||
        failed(&error, clargs_option_set_description(option, "The size of a block.")) ||
        failed(&error, clargs_option_set_value_name(option, "SIZE")) ||
        failed(&error, clargs_option_set_default(option, "64k"))) {
        return error;
    }

    /* One or more files. */
    if (failed(&error, clargs_command_add_positional(create, "file", clargs_type_string(),
                                                     CLARGS_ARITY_ONE_OR_MORE, &positional)) ||
        failed(&error, clargs_positional_set_description(positional, "The files to pack."))) {
        return error;
    }

    return CLARGS_ERROR_NONE;
}

/* The subcommand "list": a required positional, then an optional one. */
static clargs_error_t define_list(clargs_command_t *root)
{
    clargs_command_t *list;
    clargs_positional_t *positional;
    clargs_error_t error;

    if (failed(&error, clargs_command_add_subcommand(root, "list", &list)) ||
        failed(&error, clargs_command_set_description(list, "List the files of an archive."))) {
        return error;
    }

    if (failed(&error, clargs_command_add_positional(list, "archive", clargs_type_string(),
                                                     CLARGS_ARITY_REQUIRED, &positional)) ||
        failed(&error, clargs_positional_set_description(positional, "The archive to read."))) {
        return error;
    }

    /* Only an optional positional can have a default value. */
    if (failed(&error, clargs_command_add_positional(list, "pattern", clargs_type_string(),
                                                     CLARGS_ARITY_OPTIONAL, &positional)) ||
        failed(&error, clargs_positional_set_description(positional, "The files to list.")) ||
        failed(&error, clargs_positional_set_default(positional, "*"))) {
        return error;
    }

    return CLARGS_ERROR_NONE;
}

/* Tells which definition the validation found a fault in. */
static void report_fault(clargs_error_t error, const clargs_command_t *command,
                         const clargs_option_t *option, const clargs_positional_t *positional)
{
    fprintf(stderr, "pack: the schema is not valid: %s\n", clargs_error_message(error));

    if (command != NULL) {
        fprintf(stderr, "  in the command \"%s\"\n", clargs_command_name(command));
    }
    if (option != NULL) {
        if (clargs_option_long_name(option) != NULL) {
            fprintf(stderr, "  at the option --%s\n", clargs_option_long_name(option));
        } else {
            fprintf(stderr, "  at the option -%c\n", clargs_option_short_name(option));
        }
        if (clargs_option_default(option) != NULL) {
            fprintf(stderr, "  whose default value is \"%s\"\n", clargs_option_default(option));
        }
    }
    if (positional != NULL) {
        fprintf(stderr, "  at the positional \"%s\"\n", clargs_positional_name(positional));
        if (clargs_positional_default(positional) != NULL) {
            fprintf(stderr, "  whose default value is \"%s\"\n",
                    clargs_positional_default(positional));
        }
    }
}

int main(void)
{
    static const char *const formats[] = { "tar", "zip" };
    size_limit_t limit = { 1024UL * 1024UL * 1024UL };

    clargs_type_t *format = NULL;
    clargs_type_t *size = NULL;
    clargs_command_t *root = NULL;
    const clargs_command_t *faulty_command;
    const clargs_option_t *faulty_option;
    const clargs_positional_t *faulty_positional;
    clargs_error_t error;
    int status = EXIT_FAILURE;

    /* The value types come first: the schema borrows them. */
    if (failed(&error, clargs_type_create_choice(formats, 2, &format)) ||
        failed(&error, clargs_type_create("size", sizeof(unsigned long), convert_size, NULL,
                                          &limit, &size)) ||
        failed(&error, clargs_command_create("pack", &root)) ||
        failed(&error, clargs_command_set_description(root, "Pack files into an archive.")) ||
        failed(&error, define_common_options(root)) ||
        failed(&error, define_create(root, format, size)) ||
        failed(&error, define_list(root))) {
        /* An addition or a set failed. The schema is as it was before the
         * call that failed, and it can still be destroyed. */
        fprintf(stderr, "pack: cannot define the command line: %s\n",
                clargs_error_message(error));
    } else if (failed(&error, clargs_command_validate(root, &faulty_command, &faulty_option,
                                                      &faulty_positional))) {
        report_fault(error, faulty_command, faulty_option, faulty_positional);
    } else if (clargs_command_is_validated(root)) {
        printf("the schema of %s is valid\n", clargs_command_name(root));
        status = EXIT_SUCCESS;
    }

    /* The schema first, then the value types it borrowed. Destroying NULL
     * does nothing, so this is also right after a failure. */
    clargs_command_destroy(root);
    clargs_type_destroy(size);
    clargs_type_destroy(format);

    return status;
}
```

例の要点:

- 生成するのは、値型 2 つ（選択肢と利用者定義）と、ルートコマンド 1 つである。破棄するのも、この 3 つだけである。サブコマンド、オプション、位置引数は、ルートの破棄で解放される。
- 追加の関数が書くハンドル（`option`、`positional`、`create`）は、属性を設定するために使う。利用者は破棄しない。
- どの関数も、失敗したら `CLARGS_ERROR_NONE` 以外を返す。例では、`||` が左から順に評価して最初の失敗で止まることを使い、失敗した追加が書いた `NULL` のハンドルを、次の設定に渡さないようにしている。
- 検証は、定義が終わってから 1 度呼ぶ。失敗したら、3 つの出力先に書かれたハンドルを読み取りの関数に渡して、どの定義に誤りがあるかを調べる（`report_fault`）。
- 破棄は、スキーマが先、値型が後である。

## コマンド

スキーマは、コマンドの木である。ルートコマンドがプログラムを表し、各コマンドはサブコマンドを持てる。入れ子の深さに制限はない。

| すること | 関数 | 備考 |
|---|---|---|
| ルートを生成する | `clargs_command_create` | 名前は、空でない任意の文字列。サブコマンド名の規則は当てはまらない |
| サブコマンドを足す | `clargs_command_add_subcommand` | 名前は 1 文字以上。先頭は ASCII の英字または数字、続きは ASCII の英字、数字、`-`、`_` |
| 説明文を設定する | `clargs_command_set_description` | 空の文字列も設定できる。設定し直すと置き換わる |
| 木全体を破棄する | `clargs_command_destroy` | ルートにだけ呼ぶ。`NULL` は何もしない |

- ルートが木全体を所有する。サブコマンド、オプション、位置引数を個別に破棄する関数はない。
- 渡した文字列は、すべて複製される。呼び出しが戻ったら、利用者は文字列を変更しても解放してもよい。
- 1 つのコマンドは、位置引数とサブコマンドの両方を持てない。オプションは、どちらとも併用できる。
- 名前は、バイト単位で比べる。大文字と小文字は別の名前である。
- 名前の衝突は、コマンドごとに判定する。親と子のコマンド、別々のサブコマンドでは、同じ名前を使える。
- 定義要素を削除する関数と、スキーマを複製する関数はない。

## オプション

オプションは 4 種類あり、種類ごとに追加の関数がある。

| 種類 | 追加の関数 | 意味 |
|---|---|---|
| `CLARGS_OPTION_FLAG` | `clargs_command_add_flag` | 指定されたかどうか |
| `CLARGS_OPTION_NEGATABLE` | `clargs_command_add_negatable` | `--name` と `--no-name` で切り替える |
| `CLARGS_OPTION_COUNT` | `clargs_command_add_count` | 指定された回数 |
| `CLARGS_OPTION_VALUE` | `clargs_command_add_value` | 値を 1 つ取る。値型を指定する |

名前:

- オプションは、短い名前、長い名前、またはその両方を持つ。短い名前がないことは `'\0'`、長い名前がないことは `NULL` で表す。追加の関数の引数でも、読み取りの関数の戻り値でも同じである。
- 短い名前は、ASCII の英字または数字の 1 文字である。
- 長い名前は 2 文字以上で、先頭は ASCII の英字または数字、続きは ASCII の英字、数字、`-`、`_` である。
- 否定できるフラグは、長い名前を必ず持つ。否定名は、`no-` に長い名前を続けたもので、追加の時点で作られる（長い名前が `color` なら `no-color`）。否定名も、同じコマンドの中の名前の衝突の判定に加わる。

属性は、すべて任意である。オプションを足した後に、属性ごとの関数で設定する。

| 属性 | 設定の関数 | 設定できる種類 | 設定しないときの値 |
|---|---|---|---|
| 説明文 | `clargs_option_set_description` | すべて | `NULL` |
| 値の表示名 | `clargs_option_set_value_name` | `CLARGS_OPTION_VALUE` | `NULL` |
| 必須 | `clargs_option_set_required` | `CLARGS_OPTION_VALUE` | `false` |
| 既定値 | `clargs_option_set_default` | `CLARGS_OPTION_VALUE` | `NULL` |
| 初期の状態 | `clargs_option_set_initial` | `CLARGS_OPTION_NEGATABLE` | `false`（オフ） |

- 文字列の属性は複製される。空の文字列も設定でき、設定されていないこと（`NULL`）とは区別される。設定し直すと、前の値と置き換わる。設定を消す関数はない。
- 既定値は、コマンドラインに書くのと同じ形の文字列で渡す。設定の時点では、値型で変換できるかどうかを確かめない。確かめるのは検証である。
- 必須と既定値は、どちらも設定できるが、両方を持つオプションは検証で誤りになる。

## 位置引数

位置引数は `clargs_command_add_positional` で足す。名前、値型、個数の種類を指定する。コマンドの位置引数は、足した順に、コマンドラインの引数を受け取る。

| 個数の種類 | 意味 |
|---|---|
| `CLARGS_ARITY_REQUIRED` | ちょうど 1 個 |
| `CLARGS_ARITY_OPTIONAL` | 0 個または 1 個 |
| `CLARGS_ARITY_ZERO_OR_MORE` | 0 個以上 |
| `CLARGS_ARITY_ONE_OR_MORE` | 1 個以上 |

- 名前は、表示と識別のためのもので、コマンドラインには現れない。空でない任意の文字列を使える。同じコマンドの中では、互いに異なる名前にする。
- 属性は 2 つである。説明文（`clargs_positional_set_description`）は、どの個数の種類にも設定できる。既定値（`clargs_positional_set_default`）は、`CLARGS_ARITY_OPTIONAL` の位置引数にだけ設定できる。文字列の扱いは、オプションの属性と同じである。
- 個数の種類の並びが成り立つかどうか（任意の後に必須がない、可変個の後に位置引数がない）は、追加の時点では確かめない。確かめるのは検証である。

## 値型

値型は、入力の文字列をどう確かめ、どんな値に変換するかを表す。値付きオプションと位置引数は、値型を 1 つ持つ。値型が表すのは変換だけで、変換をいつ求めるか、値をどこに保持するかは表さない。

値型は、作った後は変更されない。だから、1 つの値型を、複数のオプションと位置引数、複数のスキーマ、複数のスレッドで共有できる。

値型の種類と、変換が領域に書く値の型:

| 種類 | 得る方法 | 型の名前 | 値の C の型 | 値 |
|---|---|---|---|---|
| `CLARGS_TYPE_STRING` | `clargs_type_string()` | `"string"` | `const char *` | 入力の文字列へのポインタ（複製ではない） |
| `CLARGS_TYPE_INT` | `clargs_type_int()` | `"integer"` | `int64_t` | 変換した整数 |
| `CLARGS_TYPE_FLOAT` | `clargs_type_float()` | `"float"` | `double` | 変換した値 |
| `CLARGS_TYPE_CHOICE` | `clargs_type_create_choice()` | `"choice"` | `size_t` | 一致した選択肢の番号（0 から） |
| `CLARGS_TYPE_CUSTOM` | `clargs_type_create()` | 利用者が指定 | 利用者が決める | 利用者の変換関数が書く |

組み込みの値型が受け付ける入力:

- 文字列: すべての入力。空の文字列も受け付ける。変換は必ず成功する。
- 整数: 10 進の数字の並び。先頭に符号（`+` または `-`）を 1 つ付けてもよい。値が符号付き 64 ビットの範囲に収まるものだけを受け付ける。空の文字列、空白、ほかの文字は受け付けない。
- 浮動小数: 10 進の表記。指数を付けてもよい（`1`、`1.5`、`.5`、`-1.5e-3`）。値が有限のものだけを受け付ける。小数点は、ロケールによらず `.` である。空の文字列、空白、無限大と非数の表記、16 進の表記は受け付けない。
- 選択肢: 一覧のどれか 1 つと完全に一致する入力。大文字と小文字を区別する。空の文字列も選択肢にできる。

文字列、整数、浮動小数の値型は、関数が毎回同じポインタを返す。確保をしないので、失敗せず、破棄するものもない。選択肢の値型と利用者定義の値型は、利用者が生成し、利用者が `clargs_type_destroy` で破棄する。選択肢の一覧の文字列と、利用者定義の値型の名前は、複製される。

値型は、次の関数で読める: `clargs_type_name`、`clargs_type_kind`、`clargs_type_value_size`、`clargs_type_choice_count`、`clargs_type_choice_at`。選択肢でない値型では、選択肢の個数は 0 である。

変換は、`clargs_type_convert` と `clargs_type_release` で求める。どの値型にも同じ呼び方が使える。種類を知る必要があるのは、値を読むときだけである。スキーマを定義するだけなら、利用者がこの 2 つを呼ぶ必要はない（検証が、既定値を確かめるために呼ぶ）。

- 変換を求める側が、値を書く領域を用意する。領域は `clargs_type_value_size()` バイト以上で、どの型のオブジェクトにも使える整列にする。ライブラリは、この領域を確保も解放もしない。
- 変換が成功したら、領域に値が書かれている。使い終えたら、その値に `clargs_type_release` を 1 度だけ呼ぶ。
- 変換が失敗したら、領域の内容は不定である。`clargs_type_release` を呼んではならない。
- 値は、入力の文字列を参照してよい（文字列の値型の値は、入力へのポインタそのものである）。変換を求めた側は、値を後始末するまで、入力の文字列を有効に、変更せずに保つ。

値は `void *` で受け渡すので、領域の大きさと型の誤りを、コンパイラは検出しない。上の表のとおりの型で読むこと。

## 利用者定義の値型の書き方

`clargs_type_create` に、型の名前、値の大きさ、変換関数、後始末関数、利用者のデータを渡す。

```
clargs_error_t clargs_type_create(const char *name, size_t value_size,
                                  clargs_convert_fn_t convert_fn,
                                  clargs_release_fn_t release_fn, void *context,
                                  clargs_type_t **out);
```

| 引数 | 内容 |
|---|---|
| `name` | 型の名前（`"size"` など）。空でない任意の文字列。複製される |
| `value_size` | 値の大きさ（バイト）。0 より大きい。変換が書く領域は、この大きさ以上である |
| `convert_fn` | 変換関数。`clargs_error_t (*)(const char *text, void *value, void *context)` |
| `release_fn` | 後始末関数。`void (*)(void *value, void *context)`。値に後始末が要らなければ `NULL` |
| `context` | 2 つの関数に渡される利用者のデータ。要らなければ `NULL` |

変換関数が守ること:

- `text` を変換し、`value` の指す領域に値を書いて、`CLARGS_ERROR_NONE` を返す。領域は、`value_size` バイト以上で、どの型にも使える整列である。
- 受け付けない入力には、`CLARGS_ERROR_INVALID_VALUE` を返す。変換の中での確保に失敗したら、`CLARGS_ERROR_OUT_OF_MEMORY` を返す。
- `CLARGS_ERROR_NONE` 以外の戻り値は、どの値でも失敗として扱われる。`clargs_type_convert` は、変換関数の戻り値をそのまま返す。
- 失敗するときは、後始末の要るものを残さない。失敗した変換には、後始末関数は呼ばれない。
- **副作用を持たない。** 変換関数は、検証のたびに、その値型を持つ定義要素の既定値のうち、検証が到達したもの 1 つにつき 1 回呼ばれる。出力する、数える、外部の状態を変える、といったことをすると、検証のたびに起こる。同じ入力には、いつも同じ結果を返すように書く。
- 値は `text` を参照してよい。

後始末関数が守ること:

- 同じ値型の変換が成功して書いた値を、後始末する（値が持つメモリを解放する、など）。領域そのものは解放しない。
- 成功した変換 1 回につき、1 回だけ呼ばれる。検証でも呼ばれる: 検証は、既定値の変換が成功するたびに後始末を求め、値を残さない。

利用者のデータ（`context`）:

- 値型は `context` を借りるだけである。ライブラリは複製も解放もしない。
- 利用者は、値型を破棄するまで、`context` の指す先を有効に保つ。上の例の `limit` は `main` の局所変数で、値型の破棄より後まで有効である。
- 値型を複数のスレッドから同時に使うなら、変換関数と後始末関数は、`context` を同時に読んでも正しく動くように書く。

値型の生成と破棄は、変換関数も後始末関数も呼ばない。

## 値型を借りる契約と破棄の順序

スキーマは、値型を借りる。`clargs_command_add_value` と `clargs_command_add_positional` は、渡された値型を複製せず、ポインタを持つだけである。

| もの | 所有者 | 有効に保つ期間 |
|---|---|---|
| 文字列、整数、浮動小数の値型 | ライブラリ | プログラムの終了まで有効。破棄しない（`clargs_type_destroy` に渡しても何もしない） |
| 選択肢の値型、利用者定義の値型 | 生成した利用者 | それを使うスキーマを、すべて破棄し終えるまで |
| 利用者定義の値型の `context` | 利用者 | 値型を破棄するまで |
| スキーマ（ルートコマンド） | 生成した利用者 | — |

破棄の順序:

1. スキーマを破棄する（`clargs_command_destroy`）。値型は破棄されない。
2. そのスキーマが使っていた値型を破棄する（`clargs_type_destroy`）。
3. 値型が借りていた `context` を、利用者の方法で片付ける。

- 値型を、それを使うスキーマより先に破棄してはならない。ライブラリは、この誤りを検出しない。
- 1 つの値型を複数のスキーマで使っているなら、そのすべてを破棄してから、値型を破棄する。
- `clargs_type_destroy(NULL)` と `clargs_command_destroy(NULL)` は、何もしない。生成に失敗した後でも、同じ順序で破棄を書ける。
- `clargs_option_type` と `clargs_positional_type` が返すのは、追加のときに渡した値型そのものである。利用者のもので、スキーマの破棄では無効にならない。

## 誤りの一覧

定義の誤りは、エラーコードで返る。追加と設定の関数が確かめられるものは、その場で返り、木全体を見ないと分からないものは、検証で返る。

| エラーコード | 説明文（`clargs_error_message`） | 意味 |
|---|---|---|
| `CLARGS_ERROR_OUT_OF_MEMORY` | `"out of memory"` | メモリの確保に失敗した |
| `CLARGS_ERROR_INVALID_NAME` | `"invalid name"` | 名前が規則に合わない、空である、またはない |
| `CLARGS_ERROR_NAME_CONFLICT` | `"name already in use"` | 同じコマンドの中で、名前が衝突する |
| `CLARGS_ERROR_INVALID_DEFINITION` | `"inconsistent definition"` | 定義どうしが矛盾する |
| `CLARGS_ERROR_INVALID_DEFAULT` | `"default value rejected by its value type"` | 検証で、既定値が値型で変換できない |
| `CLARGS_ERROR_INVALID_VALUE` | `"invalid value"` | 値型の変換で、入力が不正である |

追加または設定の関数が失敗したら、スキーマは呼び出しの前と同じ内容で、検証の状態も変わらない。生成したものを受け取る出力先は `NULL` になる。その呼び出しが確保したメモリは残らない。失敗の後も、スキーマは使い続けられ、同じ名前で足し直すこともできる。

### 追加と設定の時点の誤り

1 つの関数の中では、上の行から順に確かめ、最初に当てはまったものを返す。どの関数も、このほかに、確保に失敗すると `CLARGS_ERROR_OUT_OF_MEMORY` を返す。

| 関数 | 条件 | エラーコード |
|---|---|---|
| `clargs_command_create` | 名前が空 | `INVALID_NAME` |
| `clargs_command_add_subcommand` | 名前が規則に合わない（空を含む） | `INVALID_NAME` |
| | 足す先のコマンドが位置引数を持つ | `INVALID_DEFINITION` |
| | 足す先のコマンドに、同じ名前のサブコマンドがある | `NAME_CONFLICT` |
| `clargs_command_add_flag`、`_add_negatable`、`_add_count`、`_add_value` | 短い名前も長い名前もない | `INVALID_NAME` |
| | 否定できるフラグに長い名前がない | `INVALID_NAME` |
| | 短い名前または長い名前が規則に合わない | `INVALID_NAME` |
| | 同じコマンドに、同じ短い名前のオプションがある | `NAME_CONFLICT` |
| | 同じコマンドに、長い名前と同じ長い名前または否定名がある | `NAME_CONFLICT` |
| | 否定できるフラグで、その否定名と同じ長い名前または否定名が、同じコマンドにある | `NAME_CONFLICT` |
| `clargs_command_add_positional` | 名前が空 | `INVALID_NAME` |
| | 足す先のコマンドがサブコマンドを持つ | `INVALID_DEFINITION` |
| | 足す先のコマンドに、同じ名前の位置引数がある | `NAME_CONFLICT` |
| `clargs_option_set_value_name`、`_set_required`、`_set_default` | 値付きオプションでない | `INVALID_DEFINITION` |
| `clargs_option_set_initial` | 否定できるフラグでない | `INVALID_DEFINITION` |
| `clargs_positional_set_default` | 個数の種類が `CLARGS_ARITY_OPTIONAL` でない | `INVALID_DEFINITION` |
| `clargs_type_create_choice` | 選択肢が 0 個、または同じ文字列が 2 回以上ある | `INVALID_DEFINITION` |
| `clargs_type_create` | 型の名前が空 | `INVALID_NAME` |

- 対象でない種類への属性の設定は、設定しようとした値が何であっても（`false` でも）誤りである。
- 説明文の設定（コマンド、オプション、位置引数）は、確保の失敗のほかには失敗しない。

### 検証の時点の誤り

`clargs_command_validate` が返す。確かめる順序は、[検証と、失敗の調べ方](#検証と失敗の調べ方)にある。

| 条件 | エラーコード | 報告される定義要素 |
|---|---|---|
| 位置引数の並びで、`OPTIONAL` または `ZERO_OR_MORE` の後に、`REQUIRED` または `ONE_OR_MORE` がある | `INVALID_DEFINITION` | 後ろの位置引数 |
| 位置引数の並びで、`ZERO_OR_MORE` または `ONE_OR_MORE` の後に、位置引数がある | `INVALID_DEFINITION` | 後ろの位置引数 |
| オプションが必須で、かつ既定値を持つ | `INVALID_DEFINITION` | そのオプション |
| オプションの既定値を、その値型が変換できない | `INVALID_DEFAULT` | そのオプション |
| 位置引数の既定値を、その値型が変換できない | `INVALID_DEFAULT` | その位置引数 |
| 値の領域の確保に失敗した。または、変換が `CLARGS_ERROR_OUT_OF_MEMORY` を返した | `OUT_OF_MEMORY` | なし |

- 既定値の変換が `CLARGS_ERROR_OUT_OF_MEMORY` 以外で失敗したら、変換関数がどのコードを返しても、`CLARGS_ERROR_INVALID_DEFAULT` になる。
- 追加と設定の時点で返る誤り（不正な名前、名前の衝突、位置引数とサブコマンドの併用、対象でない種類への属性）は、検証では確かめない。追加と設定の関数が、そういうスキーマを作らせないからである。

### エラーコードにならないもの

必須の引数の `NULL`、範囲外の添字、列挙にない値、破棄済みのハンドルの使用は、プログラムの誤りである。エラーコードは返らない（[前提条件](#前提条件)）。

## 検証と、失敗の調べ方

```
clargs_error_t clargs_command_validate(clargs_command_t *root,
                                       const clargs_command_t **command,
                                       const clargs_option_t **option,
                                       const clargs_positional_t **positional);
bool clargs_command_is_validated(const clargs_command_t *command);
```

`clargs_command_validate` は、ルートと、その下のすべてのサブコマンドを確かめる。渡すのはルートである。成功すると、木は検証済みになる。

確かめる順序は決まっている。最初に見つけた誤りを返して終えるので、定義が同じなら、結果はいつも同じである。

- コマンドは、ルートから順にたどる。コマンド自身を先に、次にそのサブコマンドを足した順に確かめる。サブコマンドは、その下のすべてを確かめてから、次のサブコマンドへ進む。
- 1 つのコマンドの中では、次の順である。
  1. 位置引数の並び。位置引数を足した順に見る。
  2. オプション。足した順に見て、1 つのオプションにつき、必須と既定値の併用、既定値の変換の順に確かめる。
  3. 位置引数の既定値の変換。位置引数を足した順に見る。

結果と、3 つの出力先:

| 戻り値 | `*command` | `*option` | `*positional` | 検証の状態 |
|---|---|---|---|---|
| `CLARGS_ERROR_NONE` | `NULL` | `NULL` | `NULL` | 検証済みになる |
| `CLARGS_ERROR_INVALID_DEFINITION`、`CLARGS_ERROR_INVALID_DEFAULT`（オプションの誤り） | そのオプションが属するコマンド | 誤りのあるオプション | `NULL` | 変わらない |
| `CLARGS_ERROR_INVALID_DEFINITION`、`CLARGS_ERROR_INVALID_DEFAULT`（位置引数の誤り） | その位置引数が属するコマンド | `NULL` | 誤りのある位置引数 | 変わらない |
| `CLARGS_ERROR_OUT_OF_MEMORY` | `NULL` | `NULL` | `NULL` | 変わらない |

- 3 つの出力先は、それぞれ `NULL` を渡して省略できる。`NULL` でない出力先には、呼び出しのたびに必ず書かれる。
- 書かれるハンドルは、木の中のものである。ルートを破棄するまで有効で、利用者は解放しない。
- 検証は、定義の内容を変えない。何を返しても、コマンド、オプション、位置引数と、その属性は、呼び出しの前のままである。

失敗の調べ方（例の `report_fault`）:

1. 戻り値を `clargs_error_message` に渡して、誤りの種類を得る。
2. `*command` を `clargs_command_name` に渡して、どのコマンドの定義かを得る。親をたどるなら `clargs_command_parent` を使う（ルートでは `NULL`）。
3. `*option` が `NULL` でなければ、`clargs_option_long_name`（なければ `NULL`）、`clargs_option_short_name`（なければ `'\0'`）、`clargs_option_default`、`clargs_option_is_required` などで、どのオプションの何が誤りかを調べる。
4. `*positional` が `NULL` でなければ、`clargs_positional_name`、`clargs_positional_arity`、`clargs_positional_default` などで調べる。
5. 3 つとも `NULL` で、戻り値が `CLARGS_ERROR_OUT_OF_MEMORY` なら、定義の誤りではない。定義は元のままなので、メモリが足りるようになれば、もう一度検証できる。

誤りを直すには、属性を設定し直す（既定値を正しい値に置き換える、必須を `false` にする、など）。定義要素を削除する関数はないので、位置引数の並びの誤りは、スキーマを破棄して作り直す。

検証の状態:

- 生成した直後のルートは、未検証である。
- 検証が成功すると、検証済みになる。失敗した検証は、状態を変えない。
- 成功した追加と設定は、木のどこに対するものでも、木を未検証に戻す。同じ値を設定し直した場合も戻る。失敗した追加と設定は、状態を変えない。
- `clargs_command_is_validated` には、木のどのコマンドを渡してもよい。結果は同じである。
- 検証は、何度呼んでもよい。定義を変えた後は、もう一度呼ぶ。

検証は、到達した既定値 1 つにつき、値の領域を確保し、値型に変換を求め、成功したら後始末を求めて、領域を解放する。だから、利用者定義の値型の変換関数と後始末関数は、検証のたびに呼ばれる（[利用者定義の値型の書き方](#利用者定義の値型の書き方)）。検証の後に、値は残らない。

## 検証済みのスキーマで成り立つこと

どのスキーマでも、いつも成り立つこと（追加と設定の関数が守る）:

- 1 つのコマンドの中で、短い名前は互いに異なる。長い名前と否定名は、合わせて互いに異なる。サブコマンド名は互いに異なる。位置引数名は互いに異なる。
- オプションは、短い名前か長い名前の少なくとも一方を持つ。否定できるフラグは、長い名前と否定名を持つ。
- 短い名前、長い名前、サブコマンド名は、名前の規則に合う。ルートの名前と位置引数の名前は、空でない。
- 1 つのコマンドが、位置引数とサブコマンドの両方を持つことはない。
- 値付きオプションと位置引数は値型を持ち、ほかのオプションは持たない。
- 属性は、対象の種類の定義要素にだけ付いている。

検証済みのスキーマ（`clargs_command_is_validated` が `true` を返す木）では、さらに次のことが、すべてのコマンドで成り立つ:

- 位置引数の並びは、次のどちらかである。
  - `CLARGS_ARITY_REQUIRED` が 0 個以上、次に `CLARGS_ARITY_OPTIONAL` が 0 個以上、最後に `CLARGS_ARITY_ZERO_OR_MORE` が高々 1 個。
  - `CLARGS_ARITY_REQUIRED` が 0 個以上、最後に `CLARGS_ARITY_ONE_OR_MORE` が 1 個。
- 必須の値付きオプションは、既定値を持たない。
- すべての既定値は、検証の時点で、その値型の変換に成功した。変換関数が副作用を持たず、同じ入力に同じ結果を返すなら、後で変換しても成功する。

スキーマを読む側（解析やヘルプの生成）は、検証済みのスキーマについて、これらを前提にできる。未検証のスキーマでは、前提にできない。

## 読み取り

読み取りの関数は、`const` のハンドルを取り、何も変更せず、失敗しない。

| 対象 | 関数 |
|---|---|
| コマンド | `clargs_command_name`、`clargs_command_description`、`clargs_command_parent`、`clargs_command_is_validated` |
| コマンドの中の列挙 | `clargs_command_option_count` と `clargs_command_option_at`、`clargs_command_positional_count` と `clargs_command_positional_at`、`clargs_command_subcommand_count` と `clargs_command_subcommand_at` |
| 名前からの検索 | `clargs_command_find_short`、`clargs_command_find_long`、`clargs_command_find_subcommand` |
| オプション | `clargs_option_command`、`clargs_option_kind`、`clargs_option_short_name`、`clargs_option_long_name`、`clargs_option_negated_name`、`clargs_option_type`、`clargs_option_description`、`clargs_option_value_name`、`clargs_option_is_required`、`clargs_option_default`、`clargs_option_initial` |
| 位置引数 | `clargs_positional_command`、`clargs_positional_name`、`clargs_positional_arity`、`clargs_positional_type`、`clargs_positional_description`、`clargs_positional_default` |

- 列挙は、足した順である。番号は 0 から数える。
- 「ない」ことは、文字列とハンドルでは `NULL`、短い名前では `'\0'` で表される。設定されていない説明文、値の表示名、既定値は `NULL` である。空の文字列を設定した属性は、空の文字列として読める。
- 対象でない種類のオプションでは、否定名、値型、値の表示名、既定値は `NULL`、必須と初期の状態は `false` である。
- 検索は、渡したコマンドの定義だけを対象にする。親や子のコマンドは見ない。バイト単位で、名前の全体を比べる。
- `clargs_command_find_long` は、`name` の先頭 `length` バイトを名前として探す。`name=value` のような文字列の中の名前を、複製せずに探せる。否定名にも一致し、どちらに一致したかを `*negated` に書く（見つからなければ書かない）。
- `clargs_command_find_short` に `'\0'` を渡すと、必ず `NULL` が返る。
- 返された文字列とハンドルは、ルートを破棄するまで有効である。利用者は解放しない。定義を足しても、ハンドルは変わらない。
- ただし、設定し直せる属性（コマンドの説明文、オプションの説明文・値の表示名・既定値、位置引数の説明文・既定値）から読んだ文字列は、ルートを破棄するまでか、その属性を設定し直すまでの、早いほうまで有効である。設定し直すと、前の文字列は解放される。名前は置き換わらない。
- スキーマが変更されていない間は、読み取りの関数を複数のスレッドから同時に呼べる。同じスキーマの変更と検証は、ほかの呼び出しと同時に行えない。利用者が同期する。

## 前提条件

関数ごとの前提条件は、公開ヘッダのコメントに `Preconditions:` として書いてある。共通することは、次のとおりである。

- ポインタの引数は、コメントが「`NULL` でもよい」と書いていない限り、`NULL` でない。`NULL` でもよいのは、破棄の関数の引数、追加の関数の `out`、長い名前がないことを表す `long_name`、`clargs_type_create` の `release_fn` と `context`、`count` が 0 のときの `clargs_type_create_choice` の `choices`、`clargs_command_find_long` の `negated`、`clargs_command_validate` の 3 つの出力先である。
- 番号（`_at` の `index`）は、対応する個数より小さい。
- 列挙の引数（`clargs_command_add_positional` の `arity`）は、定義された定数である。
- `clargs_command_destroy` と `clargs_command_validate` に渡すのは、ルートである。サブコマンドは渡さない。
- ハンドルは、破棄されていない。値型は、それを使うスキーマより先に破棄されていない。

前提条件を破るのは、プログラムの誤りである。エラーコードは返らない。検査できるものは `assert` で検査され、Debug 構成では、破るとプロセスが中断する。Release 構成では検査されず、動作は定まらない。開発中は、Debug 構成のライブラリを使うことを勧める（[所有権と寿命の規約](ownership.md#実行時の失敗とプログラムの誤り)）。
