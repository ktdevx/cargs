# CLArgs

CLArgs は、C99 で書かれたコマンドライン引数パーサーのライブラリである。現時点であるのは、ビルドと配布の基盤（公開関数は、バージョンを返す `clargs_version()` だけ）である。引数パーサーの機能は、これから追加する。

- C99・コンパイラ拡張なしで書かれており、C 標準ライブラリ以外に依存しない。
- Linux（GCC、Clang）、macOS（Clang）、Windows（MSVC）でビルドできる。
- 静的ライブラリと動的ライブラリのどちらにもなる。
- CMake の `FetchContent`、`add_subdirectory`、`find_package`、および pkg-config から取り込める。

利用者が取り込むヘッダは `<clargs/clargs.h>` だけで、リンクするターゲットは `CLArgs::clargs` である。

```c
#include <stdio.h>

#include <clargs/clargs.h>

int main(void)
{
    printf("CLArgs %d.%d.%d\n",
        CLARGS_VERSION_MAJOR, CLARGS_VERSION_MINOR, CLARGS_VERSION_PATCH);
    return clargs_version() == CLARGS_VERSION_NUMBER ? 0 : 1;
}
```

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
cmake --build build
cmake --install build --prefix <インストール先>
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
