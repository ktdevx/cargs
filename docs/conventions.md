# CLArgs の規約と手順

CLArgs に公開関数、ヘッダ、ソース、テストを足すときに従う規約と手順をまとめる。新しい機能を足す人は、独自に名前や置き場所を決めず、この文書に従う。

公開関数や、メモリを持つオブジェクトを設計するときは、先に[所有権と寿命の規約](ownership.md)を読む。生成と破棄の対、文字列の持ち方、`NULL` の扱い、失敗の返し方は、そちらで決めている。この文書は、それをコードとテストに書くときの決まりと手順を扱う。

- [命名規約](#命名規約)
- [ディレクトリ構成](#ディレクトリ構成)
- [手順](#手順)
- [実装の規則](#実装の規則)
- [テストを書くときの注意](#テストを書くときの注意)
- [確かめ方](#確かめ方)
- [バージョンについての注意](#バージョンについての注意)

## 命名規約

| 対象 | 規約 | 例 |
|---|---|---|
| 公開する関数と型 | `clargs_` + 小文字のスネークケース。型の名前は `_t` で終える | `clargs_version`、`clargs_example_t` |
| 公開するマクロと列挙定数 | `CLARGS_` + 大文字のスネークケース | `CLARGS_VERSION_MAJOR` |
| 内部の関数と型（ファイルをまたぐもの） | 公開と同じ `clargs_` + 小文字のスネークケース。型の名前は `_t` で終える。`CLARGS_API` を付けず、`src/` の内部ヘッダで宣言する | `clargs_version_pack` |
| 構造体と列挙のタグ | 型の名前から `_t` を除いたもの。`_t` を付けない | `struct clargs_example`、`enum clargs_example_kind` |
| 関数ポインタの型と、それを持つフィールド | 型は `clargs_<名前>_fn_t`、フィールドは `<名前>_fn` | `clargs_example_fn_t`、`example_fn` |
| 内部のマクロ | 公開と同じ `CLARGS_` + 大文字のスネークケース。`src/` の内部ヘッダで定義する | `CLARGS_VERSION_INTERNAL_H`（インクルードガード） |
| ファイル内だけの関数 | `static` を付ける。接頭辞は不要 | — |
| 公開ヘッダ | `include/clargs/<名前>.h` | `include/clargs/version.h` |
| ソース | `src/<名前>.c` | `src/version.c` |
| 内部ヘッダ | 同じ名前の公開ヘッダ（`include/clargs/<名前>.h`）があるモジュールは `src/<名前>_internal.h`、ないモジュールは `src/<名前>.h` | `src/version_internal.h`（`include/clargs/version.h` がある） |
| 公開 API のテスト | `tests/public/<名前>_test.c` | `tests/public/version_test.c` |
| 内部関数のテスト | `tests/internal/<内部ヘッダの名前>_test.c` | `tests/internal/version_internal_test.c` |
| CMake のオプション、関数、モジュール | `CLARGS_<名前>`、`clargs_<名前>()`、`cmake/Clargs<名前>.cmake` | `CLARGS_BUILD_TESTS`、`clargs_add_test()`、`cmake/ClargsTesting.cmake` |

- 公開と内部は接頭辞で区別しない。公開かどうかは、`include/clargs/` のヘッダで `CLARGS_API` を付けて宣言されているかで決まる。`CLARGS_API`（`include/clargs/export.h`）は、公開関数に付ける唯一の印である。ほかの方法（コンパイラの属性を直接書く、`.def` ファイルを足すなど）で関数を公開しない。
- どの公開名（成果物、ヘッダ、識別子、パッケージ）にも `cargs` を使わない。プロジェクト名は `CLArgs`、ライブラリと取り込みパスは `clargs` である。
- 公開する変数を持たない。公開するのは関数、型、マクロだけである。`CLARGS_API` を変数に付けると、export の確認（`clargs.exports`）が失敗する。
- インクルードガードは `CLARGS_<ファイル名を大文字にしたもの>_H` とする（`CLARGS_VERSION_H`、`CLARGS_VERSION_INTERNAL_H`）。
- 型の名前（`typedef` で付ける名前）は、公開・内部・テスト支援のどれも `_t` で終える。構造体と列挙のタグには `_t` を付けない（`typedef struct clargs_example clargs_example_t;`）。
- 関数ポインタの型は `clargs_<名前>_fn_t` とし、それを持つ構造体のフィールドは `<名前>_fn` とする。
- 内部ヘッダに `_internal` を付けるのは、同じ名前の公開ヘッダと区別するためである。公開ヘッダのないモジュールでは付けない。内部関数のテストのファイル名と、`clargs_add_test()` の `<name>` は、内部ヘッダの名前に合わせる。
- 標準の関数に対応するものは、名前と引数を標準に合わせる（`malloc` に対応する関数は `clargs_malloc`、`strdup` に対応する関数は `clargs_strdup`。引数の名前も `ptr`、`str` のように標準に合わせる）。
- モジュールの名前（`<名前>`）は短くする（`alloc`、`str`、`vec` など）。

## ディレクトリ構成

```
CMakeLists.txt        組み立てだけ。バージョンの読み取り、project()、オプション、各ディレクトリの追加
cmake/                ビルド補助ファイル（CMake のモジュール、スクリプト、ひな形）
include/
└── clargs/           公開ヘッダ。すべてここに置く。インストールされる
    ├── clargs.h      利用者が取り込む唯一の入口。ほかの公開ヘッダを取り込むだけ
    ├── alloc.h       アロケータの型と clargs_set_allocator() の宣言
    ├── error.h       エラーコードの一覧 CLARGS_ERROR_LIST、clargs_error_t、clargs_error_message() の宣言
    ├── export.h      CLARGS_API
    └── version.h     バージョンのマクロと clargs_version() の宣言
src/                  ソースと内部ヘッダ。インストールされない
├── CMakeLists.txt    ソース一覧 CLARGS_SOURCES、ターゲット clargs と clargs_internal
├── <名前>.c
└── <名前>_internal.h  内部ヘッダ。同じ名前の公開ヘッダがなければ <名前>.h
tests/
├── CMakeLists.txt    clargs_add_test() の呼び出しと、確保関数の検査、export の確認の登録
├── public/           公開 API のテスト。CLArgs::clargs にリンクする
├── internal/         内部関数のテスト。clargs_internal にリンクする
├── support/          テスト支援のソースとヘッダ（テスト用アロケータ）。ライブラリの一部ではなく、インストールされない
└── consumer/         利用側からの取り込みの確認。本体のビルドには含めない
docs/                 文書
├── conventions.md    この文書
└── ownership.md      所有権と寿命の規約
```

- 利用者は `#include <clargs/clargs.h>` だけを書く。公開ヘッダどうしも `<clargs/<名前>.h>` の形で取り込む。
- `src/` の内部ヘッダは、`src/` のソースと内部関数のテストだけが `"<名前>_internal.h"`（同じ名前の公開ヘッダがなければ `"<名前>.h"`）の形で取り込む。公開ヘッダから内部ヘッダを取り込まない。
- `cmake/` のスクリプトのうち、`ClargsCheckAllocations.cmake`（確保関数の検査 `clargs.allocations`）と `ClargsCheckExports.cmake`（export の確認 `clargs.exports`）は、`cmake -P` で単独でも実行できる（[確かめ方](#確かめ方)）。
- `tests/support/` のファイルは、使うテストが `clargs_add_test()` の `SOURCES` に足して取り込む（[テスト用アロケータを使う](#テスト用アロケータを使う)）。
- ビルドディレクトリは、リポジトリの外か、ルートの `build/` または `build-*/` に作る（バージョン管理から除外されている）。

## 手順

| したいこと | 手順 |
|---|---|
| 公開関数を足す | 公開ヘッダの宣言の先頭に `CLARGS_API` を付ける。`CLARGS_API`、戻り値の型、関数名、`(` をこの順に書く。`extern "C"` のガードの中に書く。実装は `src/` のソースに書く |
| 公開ヘッダを足す | `include/clargs/` に置き、`clargs.h` に取り込みを 1 行足す |
| ソースを足す | `src/` に置き、`src/CMakeLists.txt` の `CLARGS_SOURCES` に 1 行足す |
| 内部関数を足す | 内部ヘッダ（`src/<名前>_internal.h`。同じ名前の公開ヘッダがなければ `src/<名前>.h`）で `CLARGS_API` なしで宣言し、`src/` のソースで実装する |
| 公開 API のテストを足す | `tests/public/` に置き、`tests/CMakeLists.txt` に `clargs_add_test(<name> SOURCES public/<file>)` を足す |
| 内部関数のテストを足す | `tests/internal/<内部ヘッダの名前>_test.c` に置き、`tests/CMakeLists.txt` に `clargs_add_test(<name> SOURCES internal/<file> INTERNAL)` を足す |
| CMake のモジュールを足す | `cmake/Clargs<名前>.cmake` に置き、`include("${CMAKE_CURRENT_LIST_DIR}/cmake/Clargs<名前>.cmake")` の形で取り込む |
| エラーコードを足す | `include/clargs/error.h` の `CLARGS_ERROR_LIST` の末尾に `X(名前, 説明文)` を 1 行足す。ほかは直さない（[エラーコードを足す](#エラーコードを足す)） |
| テスト用アロケータを使う | `clargs_add_test()` の `SOURCES` に `support/test_allocator.c` を足し、テストで `"../support/test_allocator.h"` を取り込む（[テスト用アロケータを使う](#テスト用アロケータを使う)） |

どの手順でも、上の 1 行以外に CMake のファイルを直す必要はない。インストール規則は `include/clargs/` の `*.h` をすべて対象にし、export の確認は公開ヘッダから期待する名前を読み取る。

以下、`<名前>` を `example` として書く。

### 公開関数を足す

公開ヘッダ（新しく作る場合は[公開ヘッダを足す](#公開ヘッダを足す)）の `extern "C"` のガードの中に、宣言を書く。

```c
CLARGS_API int clargs_example_add(int left, int right);
```

- `CLARGS_API`、戻り値の型、関数名、`(` をこの順に書く。export の確認が、`CLARGS_API` から最初の `(` までの最後の識別子を関数名として読み取るためである。
- 関数ポインタを直接返す宣言（`CLARGS_API int (*clargs_example_get(void))(int);`）は、名前を読み取れない。`typedef` で型に名前を付けてから使う。

  ```c
  typedef int (*clargs_example_fn_t)(int value);

  CLARGS_API clargs_example_fn_t clargs_example_get(void);
  ```

- 定義（`src/` のソース）には `CLARGS_API` を書かない。ソースは、その関数を宣言している公開ヘッダを取り込む。取り込まないと、印が定義に届かず、動的ライブラリから export されない（GCC と Clang では、その前に `-Wmissing-prototypes` の警告でビルドが止まる）。

  ```c
  #include <clargs/example.h>

  int clargs_example_add(int left, int right)
  {
      return left + right;
  }
  ```

- 公開関数には、`tests/public/` に公開 API のテストを足す。
- 公開関数から `CLARGS_API` を外すと、その関数は公開でなくなる。export の確認は印を基準にするので、外したこと自体は検出しない（その関数を呼ぶ公開 API のテストが、動的ライブラリの構成でリンクに失敗する）。公開ヘッダに、印のない関数宣言を書かない。

### 公開ヘッダを足す

`include/clargs/example.h` を次の形で作る。

```c
/*
 * CLArgs - <このヘッダの内容>.
 */
#ifndef CLARGS_EXAMPLE_H
#define CLARGS_EXAMPLE_H

#include <clargs/export.h>

#ifdef __cplusplus
extern "C" {
#endif

CLARGS_API int clargs_example_add(int left, int right);

#ifdef __cplusplus
}
#endif

#endif /* CLARGS_EXAMPLE_H */
```

`include/clargs/clargs.h` に、取り込みを 1 行足す。

```c
#include <clargs/example.h>
```

公開ヘッダは C99・拡張なしで、単独で取り込んでもコンパイルできるようにする（必要なヘッダを自分で取り込む）。

### ソースを足す

`src/example.c` を作り、`src/CMakeLists.txt` の `CLARGS_SOURCES` に 1 行足す。

```cmake
set(CLARGS_SOURCES
    example.c
    version.c
)
```

ライブラリのソースは、C99・拡張なしで、`-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes -Wmissing-prototypes`（MSVC は `/W4`）の警告つきでコンパイルされる。`CLARGS_WARNINGS_AS_ERRORS` が ON（トップレベルでの既定）のとき、警告はエラーになる。特に次の点に注意する。

- `static` でない関数は、定義より前に宣言が要る（`-Wmissing-prototypes`）。公開関数は公開ヘッダ、内部関数は内部ヘッダを、定義するソースが取り込む。ファイル内だけで使う関数には `static` を付ける。
- 引数のない関数は `(void)` と書く（`-Wstrict-prototypes`）。
- 値が変わりうる暗黙の型変換は、キャストを明示する（`-Wconversion`）。
- ライブラリは C 標準ライブラリ以外に依存しない。ほかのライブラリへリンクしない。

### 内部関数を足す

ファイルをまたいで使う内部関数は、内部ヘッダで宣言する。`CLARGS_API` を付けず、`extern "C"` のガードも要らない。

内部ヘッダの名前は、同じ名前の公開ヘッダがあるかどうかで決まる。

| モジュール | 内部ヘッダ | インクルードガード |
|---|---|---|
| 公開ヘッダ `include/clargs/example.h` がある | `src/example_internal.h` | `CLARGS_EXAMPLE_INTERNAL_H` |
| 公開ヘッダがない | `src/example.h` | `CLARGS_EXAMPLE_H` |

ここでは、公開ヘッダ `include/clargs/example.h` がある場合を書く。

```c
/*
 * CLArgs - internal declarations for <内容>.
 */
#ifndef CLARGS_EXAMPLE_INTERNAL_H
#define CLARGS_EXAMPLE_INTERNAL_H

int clargs_example_twice(int value);

#endif /* CLARGS_EXAMPLE_INTERNAL_H */
```

定義するソースは `#include "example_internal.h"` と書く。公開ヘッダがないモジュールでは、ファイル名が `src/example.h`、インクルードガードが `CLARGS_EXAMPLE_H`、取り込みが `#include "example.h"` になるだけで、ほかは同じである。内部関数は動的ライブラリから export されず、内部ヘッダはインストールされない。内部関数には、`tests/internal/` に内部関数のテストを足す。

### 公開 API のテストを足す

`tests/public/example_test.c` を作る。公開ヘッダだけを取り込む。

```c
/*
 * Tests of the public example API: <clargs/example.h>.
 */
#include <clargs/clargs.h>

#include <naos/naos.h>

NAOS_TEST(adds_two_numbers)
{
    NAOS_CHECK_INT_EQ(clargs_example_add(1, 2), 3);
}

NAOS_SUITE(example,
    NAOS_ADD_TEST(adds_two_numbers)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(example)
)
```

`tests/CMakeLists.txt` に 1 行足す。

```cmake
clargs_add_test(example SOURCES public/example_test.c)
```

公開 API のテストは `CLArgs::clargs` にリンクする。つまり、構成で選ばれた形（静的または動的）のライブラリに対して実行される。動的のとき、内部関数は呼べない。

### 内部関数のテストを足す

内部ヘッダの名前に `_test` を付けたファイルを作る。内部ヘッダが `src/example_internal.h` なら `tests/internal/example_internal_test.c` である。内部ヘッダを `"example_internal.h"` の形で取り込む。

```c
/*
 * Tests of the internal example functions: "example_internal.h".
 */
#include "example_internal.h"

#include <naos/naos.h>

NAOS_TEST(doubles_the_value)
{
    NAOS_CHECK_INT_EQ(clargs_example_twice(21), 42);
}

NAOS_SUITE(twice,
    NAOS_ADD_TEST(doubles_the_value)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(twice)
)
```

`tests/CMakeLists.txt` に、`INTERNAL` を付けて 1 行足す。

```cmake
clargs_add_test(example_internal SOURCES internal/example_internal_test.c INTERNAL)
```

`<name>` は内部ヘッダの名前にする。公開ヘッダがないモジュール（内部ヘッダが `src/example.h`）では、次のようになる。

```cmake
clargs_add_test(example SOURCES internal/example_test.c INTERNAL)
```

内部関数のテストは `clargs_internal` にリンクする。静的と動的のどちらの構成でも、ライブラリのソースが静的にリンクされるので、内部関数を呼べる。

### エラーコードを足す

直すのは 1 か所だけである。`include/clargs/error.h` の `CLARGS_ERROR_LIST` の**末尾**に、`X(名前, 説明文)` を 1 行足す。それまで最後だった行の終わりには `\` を足す。

```c
#define CLARGS_ERROR_LIST(X) \
    X(CLARGS_ERROR_NONE, "success") \
    X(CLARGS_ERROR_OUT_OF_MEMORY, "out of memory") \
    X(CLARGS_ERROR_EXAMPLE, "example failure")
```

- **末尾にだけ足す。** 値は書かない。一覧の順に 0 から付く（`CLARGS_ERROR_NONE` が 0、`CLARGS_ERROR_OUT_OF_MEMORY` が 1）。値は公開されていて、一度公開した値は変えず、別の意味にも使わない。だから、既存の行を削除せず、並べ替えず、途中に行を足さない。使わなくなったエラーコードも、行は残す。
- **ほかは直さない。** 列挙 `clargs_error_t`、`clargs_error_message()`（`src/error.c`）、テスト（`tests/public/error_test.c`）は、この一覧から生成される。テストは、足したコードについても、説明文が一覧のとおりで、空でなく、ほかのコードの説明文とも `"unknown error code"` とも異なることを確かめる。
- **一覧は 1 つだけ。** すべてのエラーコードをこの一覧に書く。機能ごとに別の型や番号の範囲を作らない。
- **名前**は `CLARGS_ERROR_<内容>` とする。成功は `CLARGS_ERROR_NONE` だけである。
- **説明文**は、英語の小文字で始まる短い句にする（`"out of memory"`）。ほかのコードの説明文と同じにしない。
- **エラーコードにするのは、実行時の失敗だけである。** 足す前に、それが正しいプログラムでも起こりうる失敗かどうかを確かめる。呼び出し側のプログラムの誤り（必須の引数の `NULL` など）は、エラーコードにせず、前提条件にする（[前提条件の検査](#前提条件の検査)）。
  - スキーマの定義の誤りのうち、文字列の内容に由来するもの（規則に合わない名前、値型が変換できない既定値など）と、定義どうしの関係に由来するもの（名前の衝突、併用できない定義の組み合わせなど）は、実行時の失敗として扱い、エラーコードにする（`CLARGS_ERROR_INVALID_NAME`、`CLARGS_ERROR_NAME_CONFLICT`、`CLARGS_ERROR_INVALID_DEFINITION`、`CLARGS_ERROR_INVALID_DEFAULT`、`CLARGS_ERROR_INVALID_VALUE`）。文字列は実行時に決まるデータから組み立てられることがあり、規則や既存の定義との突き合わせはライブラリにしか確かめられず、`assert` は Release 構成で検査しないためである。
  - 空のポインタ、範囲外の添字、列挙にない値は、定義の関数でも前提条件のままにする。呼び出し側のコードだけで決まり、呼び出しの場所で確かめられるためである。
  - 分け方と理由の詳しい説明は、[所有権と寿命の規約](ownership.md#実行時の失敗とプログラムの誤り)にある。
- エラーコードは失敗の種類だけを表す。位置や対象を持たせない。

### テスト用アロケータを使う

テスト用アロケータ（`tests/support/test_allocator.h`、`tests/support/test_allocator.c`）は、ライブラリのアロケータとして設定できる、テストのためのアロケータである。指定した回の要求を失敗させ、要求の回数、未解放のブロックの個数、契約違反の回数を数える。メモリを確保する関数を足したら、これを使って、確保の失敗の扱いと解放漏れを確かめる。

取り込むには、`tests/CMakeLists.txt` の `clargs_add_test()` の `SOURCES` に `support/test_allocator.c` を足す。ほかの CMake のファイルは直さない。

```cmake
clargs_add_test(example SOURCES internal/example_test.c support/test_allocator.c INTERNAL)
```

テストのソース（`tests/public/` または `tests/internal/`）では、次の形で取り込む。

```c
#include "../support/test_allocator.h"
```

テスト用アロケータは `<clargs/clargs.h>` と C 標準ライブラリだけに依存するので、公開 API のテストでも内部関数のテストでも使える。

| 関数 | 働き |
|---|---|
| `void clargs_test_allocator_install(void)` | 数を初期化し（`reset` と同じ）、テスト用アロケータをライブラリのアロケータとして設定する |
| `void clargs_test_allocator_uninstall(void)` | ライブラリを既定のアロケータに戻す。数は残るので、後から読める |
| `void clargs_test_allocator_reset(void)` | 要求の回数、未解放の個数、契約違反の回数を 0 に戻し、失敗させる回の指定を消す |
| `void clargs_test_allocator_fail_at(size_t request)` | 最後の `reset` から数えて `request` 回目の要求を失敗させる。1 から数え、その回だけが失敗する。0 を渡すと、どの要求も失敗しない |
| `size_t clargs_test_allocator_requests(void)` | 最後の `reset` からの要求の回数。失敗させた要求も数える |
| `size_t clargs_test_allocator_live(void)` | 未解放のブロックの個数。成功した確保で 1 増え、解放で 1 減り、再確保では変わらない |
| `size_t clargs_test_allocator_violations(void)` | 最後の `reset` からの契約違反の回数 |
| `clargs_allocator_t clargs_test_allocator_get(void)` | テスト用アロケータの 4 つの関数を返す。状態を変えない。自分で `clargs_set_allocator()` に渡すときや、4 つの関数を直接呼ぶときに使う |

- **要求**は、`malloc_fn`、`calloc_fn`、`realloc_fn` の呼び出しである。3 つを通して数える。`free_fn` は要求ではない。数えるのはアロケータまで届いた呼び出しなので、`clargs_malloc(0)` のように利用者の関数を呼ばないものは数に入らない。
- **契約違反**は、ライブラリがしてはならない呼び出しである（`include/clargs/alloc.h` の「How the library calls the functions of the allocator」）: 大きさ 0 または個数 0 の要求、`NULL` の再確保、`NULL` の解放。ライブラリのコードが内部のメモリ関数を通していれば、0 のままである。
- 失敗させた再確保は、元のブロックを有効なまま残す。
- **`install` と `uninstall` を対にする。** 状態は `static` 変数にあり、1 つの実行ファイルのすべてのテストが共有する。設定されたアロケータも同じである。`install` を呼んだテストは、作ったものをすべて破棄してから、終わる前に `uninstall` を呼ぶ。`uninstall` を忘れると、後のテストがテスト用アロケータを使い続ける。
- **`reset` は、何も確保されていないときに呼ぶ。** 未解放のブロックがあるときに呼ぶと、それ以後の未解放の個数が合わなくなる。
- **どのテストも、終わりに `live` と `violations` が 0 であることを確かめる。** `uninstall` の後でも数は読める。
- テスト用アロケータは同期しない。

#### 確保の各回を順に失敗させる

メモリを確保する操作のテストは、次の流れで、確保の 1 回目、2 回目、… を順に失敗させる。操作が何回確保するかを、テストが知っている必要はない。

```
install
n = 1, 2, … について
    reset、fail_at(n)
    操作を実行する
    成功し、要求の回数が n より少ない → すべての確保点を失敗させ終えた。破棄して、繰り返しを終える
    エラーコードと、失敗が変えてはならないものを確かめる
    残っているものを破棄する
    live と violations が 0 であることを確かめる
uninstall
```

手本は `tests/internal/vec_test.c` の `fails_at_every_allocation_point` である。新しいテストは、この形を写して書く。次の例は、同じ流れを短くしたもので、空の配列に 10 個の要素を足す操作を確かめる（内部関数のテストとして、そのままコンパイルできる）。

```c
/*
 * Tests of the failure of an allocation: "vec.h".
 */
#include <clargs/clargs.h>

#include "vec.h"

#include "../support/test_allocator.h"

#include <naos/naos.h>

#include <stddef.h>

/* Pushes the items count, count + 1, ... so that the array has total items
 * afterwards. Stops at the first push that fails and returns its error. */
static clargs_error_t push_up_to(clargs_vec_t *vec, size_t total)
{
    while (vec->count < total) {
        int value = (int)vec->count;
        clargs_error_t error = clargs_vec_push(vec, &value);

        if (error != CLARGS_ERROR_NONE) {
            return error;
        }
    }
    return CLARGS_ERROR_NONE;
}

NAOS_TEST(fails_at_every_allocation_point)
{
    const size_t total = 10;
    const size_t limit = 100;
    int completed = 0;
    size_t n;

    clargs_test_allocator_install();

    for (n = 1; n <= limit; ++n) {
        clargs_vec_t vec;
        clargs_error_t error;
        size_t index;

        /* Nothing is allocated here, so the counts can start again. */
        clargs_test_allocator_reset();
        clargs_test_allocator_fail_at(n);

        /* The operation. */
        clargs_vec_init(&vec, sizeof(int));
        error = push_up_to(&vec, total);

        if (error == CLARGS_ERROR_NONE
            && clargs_test_allocator_requests() < n) {
            /* The operation ended before the request to fail was reached:
             * every allocation point has failed once. */
            NAOS_CHECK_UINT_EQ(vec.count, total);
            clargs_vec_destroy(&vec);
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
            completed = 1;
            break;
        }

        /* The n-th request failed, and the operation reported it. */
        NAOS_CHECK_INT_EQ(error, CLARGS_ERROR_OUT_OF_MEMORY);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), n);

        /* What the failure must leave unchanged: the array is as it was
         * before the push that failed. */
        NAOS_CHECK_TRUE(vec.count < total);
        NAOS_CHECK_UINT_EQ(vec.count, vec.capacity);
        for (index = 0; index < vec.count; ++index) {
            const int *item = clargs_vec_at(&vec, index);

            NAOS_CHECK_INT_EQ(*item, (int)index);
        }

        /* Destroy what exists; nothing is left and nothing was violated. */
        clargs_vec_destroy(&vec);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
    }

    /* The loop ended because the operation completed, not at the limit. */
    NAOS_CHECK_TRUE(completed);

    clargs_test_allocator_uninstall();
}

NAOS_SUITE(failure,
    NAOS_ADD_TEST(fails_at_every_allocation_point)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(failure)
)
```

- 成功しても、要求の回数が `n` 以上なら、`n` 回目の失敗を操作が握りつぶしている。そのため、終了の条件は「成功した」だけでなく「要求の回数が `n` より少ない」を含める。握りつぶした場合は、続く `CLARGS_ERROR_OUT_OF_MEMORY` の確認が失敗する。
- 繰り返しには上限（`limit`）を付け、終わった理由が操作の完了であること（`completed`）を確かめる。誤りがあっても、繰り返しが終わらなくなることがない。
- 失敗の後に確かめるのは、[所有権と寿命の規約](ownership.md)の規則 7 が求めることである。エラーコードが返ること、途中の確保が残っていないこと、既存のオブジェクトが呼び出しの前のままで、引き続き使えて破棄できること。`fails_at_every_allocation_point` は、失敗の後にもう一度足せることまで確かめている。
- 確保の回数が決まっている操作では、失敗した回数がその数と等しいことも確かめる（手本の `failures` と `allocation_points`）。

### `clargs_add_test()` の規則

```cmake
clargs_add_test(<name> SOURCES <source>... [INTERNAL])
```

- `<name>` は英数字と下線だけで書き、公開・内部を通して一意にする。実行ファイルは `clargs_test_<name>` になる。内部関数のテストの `<name>` は、内部ヘッダの名前（`<名前>_internal`。同じ名前の公開ヘッダがなければ `<名前>`）とする。公開 API のテストの `<name>` と重ならないようにする。
- `INTERNAL` がなければ公開 API のテストになる。`PUBLIC` というキーワードはない。
- `<source>` は `tests/` からの相対パスで書く。1 ファイル 1 実行ファイルを基本とする。
- CTest には、`<name>.<suite>.<test>` の名前（例: `version.version.number_matches_the_headers`）と、`public` または `internal` のラベルで登録される。`ctest -L public` のように選べる。
- 引数が誤っていると、誤りを示して構成が失敗する。

## 実装の規則

ライブラリのソース（`src/`）を書くときの決まりである。所有権、寿命、失敗の返し方そのものは、[所有権と寿命の規約](ownership.md)にある。

### メモリを確保する

ライブラリのコードは、メモリの確保と解放を、内部のメモリ関数（`src/alloc_internal.h`）で行う。利用者が `clargs_set_allocator()` で設定したアロケータを、すべての確保が通るようにするためである。

| 内部のメモリ関数 | 対応する標準の関数 |
|---|---|
| `void *clargs_malloc(size_t size)` | `malloc` |
| `void *clargs_calloc(size_t count, size_t size)` | `calloc` |
| `void *clargs_realloc(void *ptr, size_t size)` | `realloc` |
| `void clargs_free(void *ptr)` | `free` |

- **標準の確保関数を直接使わない。** 標準の確保関数を書いてよいのは `src/alloc.c`（既定のアロケータ）だけである。文字列の複製には `clargs_strdup`（`src/str.h`）を使う。
- **大きさ 0 は、利用者の関数を呼ばずに `NULL` を返す。** `clargs_malloc(0)`、個数か大きさが 0 の `clargs_calloc`、積が `size_t` に収まらない `clargs_calloc` は、アロケータを呼ばずに `NULL` を返す。大きさが 0 になりうる呼び出し側は、先に大きさを調べ、この `NULL` を失敗として扱わない。
- **大きさが 0 でない要求の `NULL` は、確保の失敗である。** 受けた関数は、途中で確保したものを解放し、`CLARGS_ERROR_OUT_OF_MEMORY` として上へ返す（[内部関数の戻り値](#内部関数の戻り値)）。握りつぶさない。`errno` を書かず、何も出力せず、中断しない。
- **`clargs_realloc` が失敗しても、元のメモリは有効なまま残る。** 戻り値を確かめてから、元のポインタを置き換える。`ptr` が `NULL` なら `clargs_malloc(size)` と同じ、`ptr` が `NULL` でなく `size` が 0 なら `clargs_free(ptr)` と同じで `NULL` を返す。
- **大きさを積で求めるときは、呼び出し側が桁あふれを検査する。** `clargs_realloc` と `clargs_malloc` は積を検査しない。あふれる場合は、アロケータを呼ばずに `CLARGS_ERROR_OUT_OF_MEMORY` を返す。
- `clargs_free(NULL)` は何もしない。

```c
/*
 * src/example.h declares:
 *
 *     clargs_error_t clargs_example_resize(int **values, size_t count);
 */
#include "example.h"

#include "alloc_internal.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

clargs_error_t clargs_example_resize(int **values, size_t count)
{
    int *resized;

    assert(values != NULL);
    assert(count > 0);

    /* count * sizeof(int) would not fit in a size_t. */
    if (count > SIZE_MAX / sizeof(int)) {
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }

    resized = clargs_realloc(*values, count * sizeof(int));
    if (resized == NULL) {
        /* *values is still valid and unchanged. */
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }

    *values = resized;
    return CLARGS_ERROR_NONE;
}
```

#### 確保関数の検査 `clargs.allocations`

標準の確保関数を直接使っていないことは、検査 `clargs.allocations`（`cmake/ClargsCheckAllocations.cmake`）が確かめる。静的と動的のどちらの構成でも、`ctest` が実行する。ビルドは要らず、単独でも実行できる。

```sh
cmake -P cmake/ClargsCheckAllocations.cmake
```

- 対象は、`src/` とそのサブディレクトリの `*.c` と `*.h` のすべてである。`src/alloc.c` だけを除く（`src/alloc_internal.h` は対象である）。ソースを足しても、検査を直す必要はない。別のディレクトリを調べるときは、`-DCLARGS_SOURCE_DIR=<ディレクトリ>` を `-P` の前に渡す。
- 次の名前が、単独の識別子として現れたら失敗する。呼び出しでも、値としての使用でも、プリプロセッサの行でも同じである。

  `malloc`、`calloc`、`realloc`、`free`、`aligned_alloc`、`strdup`、`strndup`、`wcsdup`、`asprintf`、`vasprintf`

- コメント、文字列リテラル、文字リテラルの中は対象にしない。`clargs_malloc`、`malloc_fn`、`freed` のように、名前を含むだけの別の識別子は検出されない。
- 失敗すると、ファイル名、行番号、名前をすべて挙げる。対象のファイルが 1 つもない場合も失敗する。ファイルは変更しない。
- **検査の限界。** 検査は名前を読むだけで、プリプロセスをしない。マクロが `##` で組み立てた名前と、行末の `\` で分けて書いた名前は検出しない。`src/` に、そのような書き方で標準の確保関数を呼ぶコードやマクロを書かない。

#### グローバル状態

ライブラリが持つ変更可能なグローバル状態は、設定されたアロケータ（`src/alloc.c` の `current_allocator`）だけである。

- `src/` に、これ以外の変更可能な `static` 変数とファイルスコープの変数を書かない。関数の中の `static` 変数も同じである。
- 変更しない表は、`static const` で書いてよい。
- 状態は、利用者が生成して破棄するオブジェクトに持たせる。別々のオブジェクトを別々のスレッドから使えるのは、この決まりによる（[所有権と寿命の規約](ownership.md)の規則 8）。
- これは自動では検査されない。レビューで確かめる。

### 前提条件の検査

失敗を 2 種類に分ける。分け方の詳しい説明は、[所有権と寿命の規約](ownership.md#実行時の失敗とプログラムの誤り)にある。

| 種類 | 例 | 書き方 |
|---|---|---|
| 実行時の失敗（正しいプログラムでも起こりうる） | メモリ確保の失敗 | `clargs_error_t` で返す |
| プログラムの誤り（呼び出し側のコードを直さない限り必ず起こる） | 必須の引数の `NULL`、関数の欠けたアロケータ、範囲外の添字 | 関数の前提条件とし、`assert` で検査する。エラーコードを割り当てない |

- **関数の入口に、標準の `assert`（`<assert.h>`）で書く。** 公開関数も内部関数も同じである。独自のマクロや、動作を差し替える仕組みを作らない。条件ごとに `assert` を分けて書くと、どの条件に違反したかが出力でわかる。

  ```c
  #include "vec.h"

  #include <assert.h>
  #include <stddef.h>

  void *clargs_vec_at(const clargs_vec_t *vec, size_t index)
  {
      assert(vec != NULL);
      assert(index < vec->count);

      return (char *)vec->items + index * vec->item_size;
  }
  ```

- **式に副作用を書かない。** 関数の呼び出しの結果を確かめたいときも、呼び出しを `assert` の中に書かない。`assert` の有無で、正しい呼び出しの動作が変わらないようにする。
- **Debug 構成では中断し、Release 構成では検査しない。** `NDEBUG` が定義されていないビルド（CMake の Debug 構成）では、違反すると `assert` が条件、ファイル名、行番号を標準エラー出力に書いて、プロセスを中断する。`NDEBUG` が定義されたビルド（Release、RelWithDebInfo、MinSizeRel 構成）では、`assert` は何もせず、前提条件を破ったときの動作は定めない。
- **前提条件をコメントに書く。** 公開関数は公開ヘッダ、内部関数は内部ヘッダの、その関数のコメントに `Preconditions:` として書く。`assert` で検査するものと、検査できないもの（オブジェクトが存在しないときにアロケータを設定する、など）の両方を書き、検査するものはそう書く（`src/vec.h`、`include/clargs/alloc.h` が例である）。
- **Release 構成でも警告なくビルドできることを確かめる。** `NDEBUG` のもとでは `assert` の式がなくなるので、`assert` の中だけで使う変数は「使われていない」という警告になり、警告がエラーの構成ではビルドが止まる。`assert` のためだけの変数を作らず、式を `assert` に直接書く。

  ```sh
  cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
  cmake --build build-release
  ctest --test-dir build-release --output-on-failure
  ```

- 違反で中断することは、自動のテストでは確かめない。テストから、前提条件を破る呼び出しをしない。
- 生成したオブジェクトの `destroy` に `NULL` を渡すと何もしない、という規約（[所有権と寿命の規約](ownership.md)の規則 4）は、`create` で生成するオブジェクトの話である。呼び出し側が持つ構造体を初期化して使う型（`clargs_vec_t`）では、`clargs_vec_destroy` の `NULL` は前提条件の違反である。

### 内部関数の戻り値

| 内部関数 | 戻り値 | 例 |
|---|---|---|
| 標準の関数と対になるもの | 標準と同じ形で返す。失敗は `NULL` | `clargs_malloc`、`clargs_calloc`、`clargs_realloc`、`clargs_strdup` |
| それ以外の、失敗しうるもの | `clargs_error_t` を返す | `clargs_vec_push` |
| 失敗しないもの | 自然な型、または `void` | `clargs_vec_init`、`clargs_vec_at`、`clargs_vec_destroy` |

- `NULL` で失敗を返す関数を呼んだ関数は、それを `CLARGS_ERROR_OUT_OF_MEMORY` に直して上へ返す。
- 成功の判定は `error == CLARGS_ERROR_NONE` と書く。
- 失敗を戻り値のほかの手段（`errno`、出力、グローバル変数）で伝えない。

## テストを書くときの注意

テストは [Naos](https://github.com/vabelorg/naos) v0.4.0 で書く。テストのコードも C99・拡張なしで、`-Wall -Wextra -Wpedantic`（MSVC は `/W4`）の警告つきでコンパイルされ、既定では警告がエラーになる。

- **エントリポイントを 1 つ持つ。** 1 つの実行ファイル（`clargs_add_test()` の 1 回の呼び出し）のソースは、`NAOS_MAIN` をちょうど 1 つ持つ。Naos は `main()` を提供しない。
- **スイートにはテストを 1 つ以上入れる。** テストを 1 つも持たない `NAOS_SUITE` は、コンパイルエラーになる。
- **どのテストもアサーションを 1 つ以上実行する。** アサーションを 1 つも実行しなかったテストは、Naos が `no assertion ran` で失敗にする。条件分岐や、要素のないループの中だけにアサーションを書かない。「実行できること」だけを確かめるテストでは `NAOS_PASS()` を書く。
- **Naos v0.4.0 の既知の警告を避ける。** 次の使い方は警告になり、警告がエラーの構成ではビルドが止まる。
  - `NAOS_CHECK_NOT_NULL` / `NAOS_REQUIRE_NOT_NULL` に、まだ何も書き込んでいないメモリ（`malloc` の直後のポインタなど）を渡さない。GCC が `-Wmaybe-uninitialized` を出す。先にメモリへ書き込むか、`NAOS_CHECK_TRUE(pointer != NULL)` / `NAOS_REQUIRE_TRUE(pointer != NULL)` を使う。
  - `NAOS_CHECK_NULL` / `NAOS_CHECK_NOT_NULL` / `NAOS_REQUIRE_NULL` / `NAOS_REQUIRE_NOT_NULL` に関数ポインタを渡さない。`-Wpedantic` が警告する。`NAOS_CHECK_TRUE(function != NULL)` のように比較を書く。
- **マクロの書き方。** `NAOS_SUITE` と `NAOS_MAIN` の最後の要素の後にカンマを書かない。`NAOS_TEST(...) { ... }`、`NAOS_SUITE(...)`、`NAOS_MAIN(...)` の後に `;` を付けない（`-Wpedantic` が警告する）。
- 符号なしの値は `NAOS_CHECK_UINT_EQ`、符号つきの値は `NAOS_CHECK_INT_EQ` のように、型に合うアサーションを使う。
- **MSVC で標準の関数のアドレスを取るとき。** MSVC の DLL ランタイムでは、標準の関数（`malloc` など）は import される。そのアドレスは静的な初期化子に書けず、`/W4` では局所の初期化子や代入でも警告（C4204、C4232）になる。次のどちらかで書く。
  - ファイル内の `static` 関数で標準の関数を包み、その関数を渡す（`tests/internal/vec_test.c` の `wrap_malloc` など。ライブラリでは `src/alloc.c` の `default_malloc` など）。
  - 標準の関数をそのまま並べること自体を確かめるテスト（`tests/public/alloc_test.c`）では、ファイルの先頭に次を書く。

    ```c
    #ifdef _MSC_VER
    #pragma warning(disable : 4204 4232)
    #endif
    ```

- **初期化子のない `const` オブジェクトを書かない。** MSVC の `/W4` で C4132 になる。`= { 0 }` を付ける。
- **標準の確保関数へ、巨大な大きさを実際に渡さない。** ASan と valgrind が異常終了する。大きな要求の扱いを確かめるときは、引数を記録して `NULL` を返すだけのアロケータを `clargs_set_allocator()` で設定する。
- **桁あふれの検査は、両側を確かめる。** あふれる側（アロケータが呼ばれずに失敗する）だけでなく、ちょうど収まる側（アロケータまで届く）も確かめる。ちょうど収まる側は、テスト用アロケータで 1 回目の要求を失敗させ（`clargs_test_allocator_fail_at(1)`）、要求の回数が 1 であることで「アロケータまで届いた」ことを確かめる。こうすれば、巨大な大きさを標準の確保関数へ渡さずに済む。
- **アロケータを設定したテストは、終わる前に既定へ戻す。** 設定は 1 つの実行ファイルのすべてのテストが共有する。`clargs_set_allocator(NULL)` か、テスト用アロケータなら `clargs_test_allocator_uninstall()` を呼ぶ（[テスト用アロケータを使う](#テスト用アロケータを使う)）。
- **文書に載せる C の例は、取り出してそのままコンパイルできる形で書く。** 宣言している公開ヘッダや内部ヘッダを取り込む。仮の名前を使う例は、仮のヘッダの内容をコメントに書く。
- テストを足したら、ビルドしてから `ctest` を実行する。ビルドしていない実行ファイルは、失敗する `<name>.NOT_BUILT` として現れる。

## 確かめ方

変更したら、静的と動的の両方で、構成、ビルド、テストを実行する。

```sh
cmake -S . -B build-static -DCMAKE_BUILD_TYPE=Debug
cmake --build build-static
ctest --test-dir build-static --output-on-failure

cmake -S . -B build-shared -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=ON
cmake --build build-shared
ctest --test-dir build-shared --output-on-failure
```

- どちらの構成でも、`ctest` が確保関数の検査 `clargs.allocations` を実行する。`src/` のソースとヘッダが、`src/alloc.c` を除いて、標準の確保関数を直接使っていないことを確かめる（[確保関数の検査 `clargs.allocations`](#確保関数の検査-clargsallocations)）。単独でも実行できる。

  ```sh
  cmake -P cmake/ClargsCheckAllocations.cmake
  ```

- `assert` を足したり直したりしたときは、Release 構成（`-DCMAKE_BUILD_TYPE=Release`）でも警告なくビルドできることを確かめる（[前提条件の検査](#前提条件の検査)）。
- 動的の構成では、`ctest` が export の確認 `clargs.exports` も実行する。動的ライブラリが export する名前が、公開ヘッダで `CLARGS_API` を付けて宣言した関数とちょうど一致すること、その名前がすべて `clargs_` で始まることを確かめる。期待する名前は公開ヘッダから読み取るので、公開関数を足しても一覧を直す必要はない。静的の構成では登録されない。
- export の確認は、単独でも実行できる。

  ```sh
  cmake -DCLARGS_LIBRARY=<動的ライブラリのパス> -P cmake/ClargsCheckExports.cmake
  ```

- テストを有効にした構成（トップレベルでの既定）は、構成のときに Naos を GitHub から取得する。取得済みのコピーがあれば、`-DFETCHCONTENT_SOURCE_DIR_NAOS=<ビルドディレクトリ>/_deps/naos-src` を渡すとネットワークなしで構成できる。

## バージョンについての注意

- バージョンの唯一の出どころは `include/clargs/version.h` の `CLARGS_VERSION_MAJOR`、`CLARGS_VERSION_MINOR`、`CLARGS_VERSION_PATCH` である。ビルドがこのヘッダを文字列として読むので、3 つのマクロは 1 行ずつ、`#define CLARGS_VERSION_<PART> <整数>` の形で、先頭にゼロを付けない 10 進の 0〜255 で書く。ほかの場所にバージョンの数字を書かない。
- `CLARGS_VERSION_NUMBER` と `CLARGS_MAKE_VERSION()` は `(unsigned long)` のキャストを含むので、`#if` では使えない。`#if` では `CLARGS_VERSION_MAJOR` などを個別に比べる。

  ```c
  #if CLARGS_VERSION_MAJOR > 0 || CLARGS_VERSION_MINOR >= 2
  ```

- メジャーが 0 の間、`find_package` の互換判定は「同じマイナー」である。`find_package(CLArgs 0)` や範囲指定（`0.1...1`）では見つからない。バージョンを指定するなら、`find_package(CLArgs 0.1)` のようにマイナーまで書く。メジャーが 1 以上になると「同じメジャー」になる。
