# CLArgs

CLArgs は、C99 で書かれたコマンドライン引数パーサーのライブラリである。プログラムが受け付けるコマンドライン（スキーマ）を関数呼び出しで定義して検証し、`main` が受け取った argv を解析して、結果から値を読める。スキーマから、ヘルプと usage 行の文面も生成できる。

- C99・コンパイラ拡張なしで書かれており、C 標準ライブラリ以外に依存しない。
- Linux（GCC、Clang）、macOS（Clang）、Windows（MSVC）でビルドできる。
- 静的ライブラリと動的ライブラリのどちらにもなる。
- CMake の `FetchContent`、`add_subdirectory`、`find_package`、および pkg-config から取り込める。
- オプション（フラグ、カウント、`--no-` で否定できるフラグ、値付き）、位置引数、サブコマンドを定義できる。値は、文字列、整数、浮動小数、選択肢、利用者定義の値型で変換して読める。
- ヘルプと usage 行の文面は、スキーマの定義から作る。文面は、利用者が渡す書き込み関数、または利用者のバッファに出る。
- ライブラリは、何も出力せず、プロセスを終了させず、環境変数を読まない。引数がスキーマに合わないときは、エラーの種類と 1 行のメッセージを返す。表示するのは利用者のコードである。`--help` も、利用者がスキーマに定義し、利用者のコードがヘルプを出す。

利用者が取り込むヘッダは `<clargs/clargs.h>` だけで、リンクするターゲットは `CLArgs::clargs` である。

次の例は、`greet [-h] [-l] [-n COUNT] name` という形のコマンドラインを受け付けるプログラムの全体である。スキーマを定義して検証し、argv を解析し、`--help` が指定されていればヘルプを出し、そうでなければ値を読んで、破棄する。

```c
#include <clargs/clargs.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* Keeps the result of a call in *error and tells whether the call failed. */
static bool failed(clargs_error_t *error, clargs_error_t result)
{
    *error = result;
    return result != CLARGS_ERROR_NONE;
}

/* The write function: the library passes the text of the help to it, part
 * by part. This one writes each part to the stream in user_data. */
static bool write_to_stream(void *user_data, const char *text, size_t length)
{
    return fwrite(text, 1, length, (FILE *)user_data) == length;
}

int main(int argc, char **argv)
{
    clargs_command_t *root = NULL;
    clargs_option_t *help = NULL;
    clargs_option_t *loud = NULL;
    clargs_option_t *count = NULL;
    clargs_positional_t *name = NULL;
    clargs_result_t *result = NULL;
    clargs_error_t error;
    int status = EXIT_FAILURE;

    /* The schema: greet [-h] [-l] [-n COUNT] name. The descriptions are
     * for the help. The schema is validated before it is parsed with. */
    if (failed(&error, clargs_command_create("greet", &root)) ||
        failed(&error, clargs_command_set_description(root, "Greet somebody.")) ||
        failed(&error, clargs_command_add_flag(root, 'h', "help", &help)) ||
        failed(&error, clargs_option_set_description(help, "Print this help.")) ||
        failed(&error, clargs_command_add_flag(root, 'l', "loud", &loud)) ||
        failed(&error, clargs_option_set_description(loud, "Add an exclamation mark.")) ||
        failed(&error, clargs_command_add_value(root, 'n', "count", clargs_type_int(), &count)) ||
        failed(&error, clargs_option_set_description(count, "How often to greet.")) ||
        failed(&error, clargs_option_set_value_name(count, "COUNT")) ||
        failed(&error, clargs_option_set_default(count, "1")) ||
        failed(&error, clargs_command_add_positional(root, "name", clargs_type_string(),
                                                     CLARGS_ARITY_REQUIRED, &name)) ||
        failed(&error, clargs_positional_set_description(name, "Whom to greet.")) ||
        failed(&error, clargs_command_validate(root, NULL, NULL, NULL))) {
        fprintf(stderr, "greet: cannot define the command line: %s\n", clargs_error_message(error));
    } else {
        /* argc and argv go in as main() received them. */
        error = clargs_parse(root, argc, argv, &result);

        if (result == NULL) {
            /* CLARGS_ERROR_OUT_OF_MEMORY: there is no result. */
            fprintf(stderr, "greet: %s\n", clargs_error_message(error));
        } else if (clargs_result_flag(result, help)) {
            /* --help first: "greet --help" leaves out the name, and that
             * must not hide the help. The text is made from the schema. */
            if (clargs_help_write(root, NULL, write_to_stream, stdout) == CLARGS_ERROR_NONE) {
                status = EXIT_SUCCESS;
            }
        } else if (error == CLARGS_ERROR_INVALID_COMMAND_LINE) {
            /* The message is one line with no prefix and no line break. */
            fprintf(stderr, "greet: %s\n", clargs_result_error_message(result));
        } else {
            const char *text = NULL;
            int64_t times = 0;
            int64_t i;

            /* The name is required and --count has a default value: after
             * a parse that succeeded, both have a value. */
            if (clargs_result_positional_string(result, name, 0, &text) &&
                clargs_result_option_int(result, count, &times)) {
                for (i = 0; i < times; ++i) {
                    printf("hello, %s%s\n", text, clargs_result_flag(result, loud) ? "!" : "");
                }
                printf("greeted %" PRId64 " time(s)\n", times);
                status = EXIT_SUCCESS;
            }
        }
    }

    /* The result first, then the schema it was made from. A parse error
     * also leaves a result to destroy. Destroying NULL does nothing. */
    clargs_result_destroy(result);
    clargs_command_destroy(root);

    return status;
}
```

実行の例（`$` の行が入力、次の行からが出力）:

```
$ greet --loud -n 2 world
hello, world!
hello, world!
greeted 2 time(s)

$ greet
greet: missing required argument 'name'

$ greet --count=many world
greet: invalid value 'many' for option '--count' (expected integer)

$ greet --help
Usage: greet [OPTIONS] <name>

Greet somebody.

Arguments:
  <name>               Whom to greet.

Options:
  -h, --help           Print this help.
  -l, --loud           Add an exclamation mark.
  -n, --count <COUNT>  How often to greet. [default: 1]
```

- ヘルプの文面は、スキーマの定義（名前、説明文、値の表示名、既定値）から作られる。プログラムの中に、ヘルプの文面を書く必要はない。
- ヘルプを出すのは、利用者のコードである。ライブラリは、`--help` をスキーマに足さず、ヘルプを出して終了することもない。
- 解析エラー（引数がスキーマに合わない）のときも結果が渡るので、利用者が破棄する。結果は、スキーマより先に破棄する。
- 使い方の詳しい説明は、次の文書にある。
  - [スキーマの使い方](docs/schema.md): コマンド、オプション、位置引数、値型の定義と検証。
  - [解析の使い方](docs/parse.md): 解析、引数の書き方の一覧、結果の読み取り、解析エラーの種類とメッセージ、`--help` の扱い、ワイド文字の argv（`wmain`）。
  - [ヘルプの使い方](docs/help.md): ヘルプと usage 行の生成、文面の規則、表示幅などの設定、出力先（書き込み関数とバッファ）、解析エラーに usage 行を添える例。
  - [所有権と寿命の規約](docs/ownership.md): 誰が何を解放するか、渡したものと返されたものがいつまで有効か。

- [ビルドとテスト](#ビルドとテスト)
- [オプション](#オプション)
- [取り込み方法](#取り込み方法)
- [開発に参加する](#開発に参加する)
- [ライセンス](#ライセンス)

## ビルドとテスト

必要なものは、CMake 3.21 以上と C99 のコンパイラである。リポジトリのルートで、次の 3 つのコマンドを実行する。

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

- テストは、テストフレームワーク [Naos](https://github.com/vabelorg/naos) で書かれている。テストを有効にした構成（リポジトリを直接ビルドするときの既定）は、**構成のときに Naos を GitHub から取得するので、ネットワークが要る**。
- 手元に Naos のコピーがあれば、その場所を渡すとネットワークなしで構成できる。一度構成したビルドディレクトリの `_deps/naos-src` も、コピーとして使える。

  ```sh
  cmake -S . -B build -DFETCHCONTENT_SOURCE_DIR_NAOS=<Naos のコピーのパス>
  ```

- テストが要らなければ、無効にする。Naos は取得されず、ネットワークも要らない。

  ```sh
  cmake -S . -B build -DCLARGS_BUILD_TESTS=OFF
  cmake --build build
  ```

- Visual Studio のように 1 つのビルドディレクトリが複数の構成を持つジェネレータでは、構成を指定しない `cmake --build` と `ctest` は Debug を使う。ほかの構成は `cmake --build build --config Release` と `ctest --test-dir build -C Release` のように指定する。

## オプション

| オプション | 内容 | 直接ビルドするときの既定 | 組み込まれたときの既定 |
|---|---|---|---|
| `CLARGS_BUILD_TESTS` | テストをビルドし、CTest に登録する（Naos を取得する） | `ON` | `OFF` |
| `CLARGS_INSTALL` | インストール規則と、`find_package` および pkg-config 用のパッケージ情報を作る | `ON` | `OFF` |
| `CLARGS_WARNINGS_AS_ERRORS` | CLArgs のターゲットで、コンパイラの警告をエラーにする | `ON` | `OFF` |

- 「組み込まれたとき」とは、ほかのプロジェクトが `FetchContent` または `add_subdirectory` で CLArgs を取り込んだときである。既定では、親のプロジェクトにテスト、インストール規則、警告のエラー化を足さない。
- オプションは `-D<オプション>=ON` または `-D<オプション>=OFF` で指定する。組み込む側は、CLArgs を取り込む前に `set(<オプション> ON)` と書いてもよい。
- 静的ライブラリか動的ライブラリかは、CMake 標準の `BUILD_SHARED_LIBS` で選ぶ。指定しなければ静的ライブラリになる。

  ```sh
  cmake -S . -B build -DBUILD_SHARED_LIBS=ON
  ```

## 取り込み方法

どの方法でも、ソースには `#include <clargs/clargs.h>` と書く。CMake では `CLArgs::clargs` にリンクする（pkg-config を CMake から使うときだけ `PkgConfig::CLARGS`）。取り込みパスと、動的ライブラリのときに必要な定義（`CLARGS_SHARED`）は、リンクすると自動で付く。静的ライブラリのときは、定義は要らない。

以下の例は、冒頭のプログラムを `main.c` として、実行ファイル `app` を作る `CMakeLists.txt` の全体である。

### FetchContent

構成のときに、CLArgs のソースを取得して一緒にビルドする。

```cmake
cmake_minimum_required(VERSION 3.21)
project(app LANGUAGES C)

include(FetchContent)
FetchContent_Declare(CLArgs
    GIT_REPOSITORY https://github.com/ktdevx/cargs.git
    GIT_TAG main
)
FetchContent_MakeAvailable(CLArgs)

add_executable(app main.c)
target_link_libraries(app PRIVATE CLArgs::clargs)
```

- `GIT_TAG` には、ブランチ名、タグ、コミットのハッシュを書ける。同じ内容を再現してビルドしたいときは、`main` の代わりにコミットのハッシュを書く。
- 手元のチェックアウトを使うときは、構成のときに `-DFETCHCONTENT_SOURCE_DIR_CLARGS=<チェックアウトのパス>` を渡す。ネットワークは要らない。

### add_subdirectory

CLArgs のソースを自分のプロジェクトの中（サブモジュールやコピー）に置き、一緒にビルドする。次の例は、`third_party/clargs` に置いた場合である。

```cmake
cmake_minimum_required(VERSION 3.21)
project(app LANGUAGES C)

add_subdirectory(third_party/clargs)

add_executable(app main.c)
target_link_libraries(app PRIVATE CLArgs::clargs)
```

- プロジェクトの外にあるソースを指すときは、`add_subdirectory(<チェックアウトのパス> clargs)` のように、2 つ目の引数でビルド用のディレクトリ名を与える。

### find_package

インストール済みの CLArgs を使う。先に、CLArgs をビルドしてインストールする。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCLARGS_BUILD_TESTS=OFF
cmake --build build --config Release
cmake --install build --config Release --prefix <インストール先>
```

利用側は、次のように書く。

```cmake
cmake_minimum_required(VERSION 3.21)
project(app LANGUAGES C)

find_package(CLArgs CONFIG REQUIRED)

add_executable(app main.c)
target_link_libraries(app PRIVATE CLArgs::clargs)
```

- 標準でない場所へインストールした場合は、利用側の構成のときに `-DCMAKE_PREFIX_PATH=<インストール先>` を渡す。
- インストールした場所は、あとからディレクトリごと移動してもよい。
- バージョンを指定するときは、`find_package(CLArgs <メジャー>.<マイナー> CONFIG REQUIRED)` のように書く。メジャーバージョンが 0 の間は、マイナーバージョンが同じものだけが互換と判定されるので、マイナーまで書く。メジャーだけの指定や範囲の指定では見つからない（[バージョンについての注意](docs/conventions.md#バージョンについての注意)）。

### pkg-config

インストール済みの CLArgs を、pkg-config で使う（Linux と macOS）。インストールの手順は [find_package](#find_package) と同じで、モジュール名は `clargs` である。

CMake を使わない場合は、コンパイラに pkg-config の出力を渡す。

```sh
cc main.c -o app $(pkg-config --cflags --libs clargs)
```

CMake から使う場合は、次のように書く。

```cmake
cmake_minimum_required(VERSION 3.21)
project(app LANGUAGES C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(CLARGS REQUIRED IMPORTED_TARGET clargs)

add_executable(app main.c)
target_link_libraries(app PRIVATE PkgConfig::CLARGS)
```

- 標準でない場所へインストールした場合は、環境変数 `PKG_CONFIG_PATH` に `<インストール先>/lib/pkgconfig` を入れる（ライブラリのディレクトリが `lib64` などの環境では、そのディレクトリの `pkgconfig`）。CMake から使う場合は、`-DCMAKE_PREFIX_PATH=<インストール先>` でもよい。
- 書き方は、静的ライブラリでも動的ライブラリでも同じである。
- 動的ライブラリを標準でない場所へインストールし、CMake を使わずにビルドした場合は、実行するときにライブラリを見つけられるようにする（Linux では `LD_LIBRARY_PATH=<インストール先>/lib` など）。

### 組み込んだ CLArgs のテストを実行する

`FetchContent` または `add_subdirectory` で組み込んだ CLArgs のテストは、既定では作られない。親のプロジェクトから実行したいときは、CLArgs を取り込む前に `CLARGS_BUILD_TESTS` を ON にし、**親のトップレベルの `CMakeLists.txt` で `enable_testing()` を呼ぶ**。呼ばないと、親のビルドディレクトリで実行した `ctest` は CLArgs のテストを見つけない（CTest の仕様による）。

```cmake
enable_testing()

set(CLARGS_BUILD_TESTS ON)
add_subdirectory(third_party/clargs)
```

この場合も、構成のときに Naos を取得する（[ビルドとテスト](#ビルドとテスト)）。

## 開発に参加する

命名規約、ディレクトリ構成、公開関数・ヘッダ・ソース・テストを足す手順は、[CLArgs の規約と手順](docs/conventions.md)にまとめてある。変更する前に読むこと。

## ライセンス

MIT License で配布する。全文は [LICENSE](LICENSE) にある。
