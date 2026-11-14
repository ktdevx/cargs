# Installation

How to build ArgX and use it from your project. How to write a program with it is in the [Guide](../guides/guide.md).

## Requirements

- CMake 3.21 or later.
- A C99 compiler. ArgX builds on Linux (GCC, Clang), macOS (Clang) and Windows (MSVC).

## Building and testing

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

- **The tests fetch [Naos](https://github.com/vabelorg/naos) from GitHub at configure time.** To configure offline, pass a local copy with `-DFETCHCONTENT_SOURCE_DIR_NAOS=<path-to-naos>` (`_deps/naos-src` of an earlier build works), or disable the tests with `-DARGX_BUILD_TESTS=OFF`.
- With a multi-config generator such as Visual Studio, Debug is the default. Pass `--config Release` to `cmake --build` and `-C Release` to `ctest` for other configurations.

## Build options

| Option | Description | Built directly | Embedded |
| --- | --- | --- | --- |
| `ARGX_BUILD_TESTS` | Build and register the tests (fetches Naos) | `ON` | `OFF` |
| `ARGX_INSTALL` | Create the install rules and the package files | `ON` | `OFF` |
| `ARGX_WARNINGS_AS_ERRORS` | Treat warnings as errors for the ArgX targets | `ON` | `OFF` |

- "Embedded" means brought in with `FetchContent` or `add_subdirectory`.
- Set an option with `-D<option>=ON`, or with `set(<option> ON)` before embedding ArgX.
- ArgX is a static library unless you set `BUILD_SHARED_LIBS=ON`.

## Using ArgX in your project

Include `<argx/argx.h>` and link to `ArgX::argx`, which also adds the include path and the definitions a shared library needs. Each example below builds `app` from the [Example](../../README.md#example) of the README saved as `main.c`.

### FetchContent

```cmake
cmake_minimum_required(VERSION 3.21)
project(app LANGUAGES C)

include(FetchContent)
FetchContent_Declare(ArgX
    GIT_REPOSITORY https://github.com/ktdevx/cargs.git
    GIT_TAG v0.1.0
)
FetchContent_MakeAvailable(ArgX)

add_executable(app main.c)
target_link_libraries(app PRIVATE ArgX::argx)
```

- Replace `v0.1.0` with the latest tag from the [Releases](https://github.com/ktdevx/cargs/releases) page.
- To use a local checkout, pass `-DFETCHCONTENT_SOURCE_DIR_ARGX=<path-to-checkout>`.

### add_subdirectory

With the sources in your project, for example as a submodule in `third_party/argx`:

```cmake
cmake_minimum_required(VERSION 3.21)
project(app LANGUAGES C)

add_subdirectory(third_party/argx)

add_executable(app main.c)
target_link_libraries(app PRIVATE ArgX::argx)
```

For sources outside your project, give a build directory name: `add_subdirectory(<path-to-checkout> argx)`.

### find_package

Install ArgX first:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DARGX_BUILD_TESTS=OFF
cmake --build build --config Release
cmake --install build --config Release --prefix <install-prefix>
```

```cmake
cmake_minimum_required(VERSION 3.21)
project(app LANGUAGES C)

find_package(ArgX CONFIG REQUIRED)

add_executable(app main.c)
target_link_libraries(app PRIVATE ArgX::argx)
```

- For a non-standard location, pass `-DCMAKE_PREFIX_PATH=<install-prefix>`. The installed directory can be moved as a whole.
- To request a version, give the major and minor version: `find_package(ArgX 0.2 CONFIG REQUIRED)`. While the major version is 0, only the same minor version is compatible, so a major version alone or a range finds nothing ([Compatibility](../references/version.md#compatibility)).

### pkg-config

On Linux and macOS, an installed ArgX can also be used through the module `argx`:

```sh
cc main.c -o app $(pkg-config --cflags --libs argx)
```

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(ARGX REQUIRED IMPORTED_TARGET argx)

add_executable(app main.c)
target_link_libraries(app PRIVATE PkgConfig::ARGX)
```

For a non-standard location, add `<install-prefix>/lib/pkgconfig` to `PKG_CONFIG_PATH`, and for a shared library make it findable at run time (for example with `LD_LIBRARY_PATH`).

### The version of the package

| Where | Value |
| --- | --- |
| `ArgX_VERSION` (`find_package`) | The numeric part, such as `0.2.0` |
| `ArgX_VERSION_FULL` (`find_package`) | The full version, such as `0.2.0-rc.1` |
| `ArgX_VERSION_PRERELEASE` (`find_package`) | The pre-release identifiers, such as `rc.1`; empty for a release |
| `pkg-config --modversion argx` | The full version |

With `FetchContent` or `add_subdirectory`, read the [version macros](../references/version.md) instead.

Neither tool orders pre-releases as SemVer does: `find_package(ArgX 0.2)` also finds `0.2.0-rc.1`, since it compares only the numbers, and pkg-config considers `0.2.0-rc.1` newer than `0.2.0`. Check `ArgX_VERSION_PRERELEASE` to tell them apart.

### Running the tests of an embedded ArgX

Set `ARGX_BUILD_TESTS` before embedding ArgX, and call `enable_testing()` in your top-level `CMakeLists.txt`, or `ctest` will not find the tests.

```cmake
enable_testing()

set(ARGX_BUILD_TESTS ON)
add_subdirectory(third_party/argx)
```
