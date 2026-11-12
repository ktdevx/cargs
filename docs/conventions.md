# CLArgs の規約と手順

CLArgs に公開関数、ヘッダ、ソース、テストを足すときに従う規約と手順をまとめる。新しい機能を足す人は、独自に名前や置き場所を決めず、この文書に従う。

- [命名規約](#命名規約)
- [ディレクトリ構成](#ディレクトリ構成)
- [手順](#手順)
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
    ├── export.h      CLARGS_API
    └── version.h     バージョンのマクロと clargs_version() の宣言
src/                  ソースと内部ヘッダ。インストールされない
├── CMakeLists.txt    ソース一覧 CLARGS_SOURCES、ターゲット clargs と clargs_internal
├── <名前>.c
└── <名前>_internal.h  内部ヘッダ。同じ名前の公開ヘッダがなければ <名前>.h
tests/
├── CMakeLists.txt    clargs_add_test() の呼び出しと、export の確認の登録
├── public/           公開 API のテスト。CLArgs::clargs にリンクする
├── internal/         内部関数のテスト。clargs_internal にリンクする
└── consumer/         利用側からの取り込みの確認。本体のビルドには含めない
docs/                 文書
```

- 利用者は `#include <clargs/clargs.h>` だけを書く。公開ヘッダどうしも `<clargs/<名前>.h>` の形で取り込む。
- `src/` の内部ヘッダは、`src/` のソースと内部関数のテストだけが `"<名前>_internal.h"`（同じ名前の公開ヘッダがなければ `"<名前>.h"`）の形で取り込む。公開ヘッダから内部ヘッダを取り込まない。
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

### `clargs_add_test()` の規則

```cmake
clargs_add_test(<name> SOURCES <source>... [INTERNAL])
```

- `<name>` は英数字と下線だけで書き、公開・内部を通して一意にする。実行ファイルは `clargs_test_<name>` になる。内部関数のテストの `<name>` は、内部ヘッダの名前（`<名前>_internal`。同じ名前の公開ヘッダがなければ `<名前>`）とする。公開 API のテストの `<name>` と重ならないようにする。
- `INTERNAL` がなければ公開 API のテストになる。`PUBLIC` というキーワードはない。
- `<source>` は `tests/` からの相対パスで書く。1 ファイル 1 実行ファイルを基本とする。
- CTest には、`<name>.<suite>.<test>` の名前（例: `version.version.number_matches_the_headers`）と、`public` または `internal` のラベルで登録される。`ctest -L public` のように選べる。
- 引数が誤っていると、誤りを示して構成が失敗する。

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
