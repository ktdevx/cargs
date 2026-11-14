# Installation

This document describes how to build and test ArgX, the build options, and the four ways to use it from your project: `FetchContent`, `add_subdirectory`, `find_package` and pkg-config. It is written for you as a user of the library. How to write a program with ArgX is in the [Guide](../guides/guide.md), and every reference is listed in the [documentation index](../README.md#references).

## Requirements

- CMake 3.21 or later.
- A C99 compiler. ArgX builds on Linux (GCC, Clang), macOS (Clang) and Windows (MSVC).

## Building and testing

Run the following three commands in the root of the repository.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

- The tests are written with the test framework [Naos](https://github.com/vabelorg/naos). A configuration with the tests enabled (the default when you build the repository directly) **fetches Naos from GitHub at configure time, so it needs the network**.
- If you have a local copy of Naos, pass its location to configure without the network. The `_deps/naos-src` directory of a build directory that was configured once can also be used as the copy.

  ```sh
  cmake -S . -B build -DFETCHCONTENT_SOURCE_DIR_NAOS=<path-to-naos>
  ```

- If you do not need the tests, disable them. Naos is then not fetched, and no network is needed.

  ```sh
  cmake -S . -B build -DARGX_BUILD_TESTS=OFF
  cmake --build build
  ```

- With a generator where one build directory holds several configurations, such as Visual Studio, `cmake --build` and `ctest` use Debug when no configuration is given. Give other configurations explicitly, as in `cmake --build build --config Release` and `ctest --test-dir build -C Release`.

## Build options

| Option | Description | Default when built directly | Default when embedded |
| --- | --- | --- | --- |
| `ARGX_BUILD_TESTS` | Builds the tests and registers them with CTest (fetches Naos) | `ON` | `OFF` |
| `ARGX_INSTALL` | Creates the install rules and the package files for `find_package` and pkg-config | `ON` | `OFF` |
| `ARGX_WARNINGS_AS_ERRORS` | Turns compiler warnings into errors for the ArgX targets | `ON` | `OFF` |

- "Embedded" means that another project brought ArgX in with `FetchContent` or `add_subdirectory`. By default, ArgX adds no tests, no install rules and no warnings-as-errors to the parent project.
- Set an option with `-D<option>=ON` or `-D<option>=OFF`. A project that embeds ArgX can also write `set(<option> ON)` before it brings ArgX in.
- Choose between a static and a shared library with the standard CMake variable `BUILD_SHARED_LIBS`. If you do not set it, you get a static library.

  ```sh
  cmake -S . -B build -DBUILD_SHARED_LIBS=ON
  ```

## Using ArgX in your project

Whichever way you choose, write `#include <argx/argx.h>` in your sources. In CMake, link to `ArgX::argx` (only when you use pkg-config from CMake, link to `PkgConfig::ARGX`). Linking adds the include path and, for a shared library, the required definition (`ARGX_SHARED`) automatically. A static library needs no definition.

Each example below is a complete `CMakeLists.txt` that builds the executable `app` from the program in the [Example](../../README.md#example) of the README, saved as `main.c`.

### FetchContent

Fetches the sources of ArgX at configure time and builds them together with your project.

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

- Replace `v0.1.0` with the latest release tag. Release tags have the form `v<SemVer>`, such as `v0.2.0`; pre-releases look like `v0.2.0-rc.1`. All releases are listed on the [Releases](https://github.com/ktdevx/cargs/releases) page.
- `GIT_TAG` accepts a tag, a commit hash or a branch name. For a reproducible build, pin it to a tag or a commit hash. A branch name also works, but its contents can change.
- To use a local checkout, pass `-DFETCHCONTENT_SOURCE_DIR_ARGX=<path-to-checkout>` at configure time. No network is needed.

### add_subdirectory

Put the sources of ArgX inside your project (as a submodule or a copy) and build them together. The following example assumes they are in `third_party/argx`.

```cmake
cmake_minimum_required(VERSION 3.21)
project(app LANGUAGES C)

add_subdirectory(third_party/argx)

add_executable(app main.c)
target_link_libraries(app PRIVATE ArgX::argx)
```

- When the sources are outside your project, give a name for the build directory as the second argument, as in `add_subdirectory(<path-to-checkout> argx)`.

### find_package

Uses an installed ArgX. Build and install ArgX first.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DARGX_BUILD_TESTS=OFF
cmake --build build --config Release
cmake --install build --config Release --prefix <install-prefix>
```

Then write the following in your project.

```cmake
cmake_minimum_required(VERSION 3.21)
project(app LANGUAGES C)

find_package(ArgX CONFIG REQUIRED)

add_executable(app main.c)
target_link_libraries(app PRIVATE ArgX::argx)
```

- If you installed to a non-standard location, pass `-DCMAKE_PREFIX_PATH=<install-prefix>` when you configure your project.
- You may later move the installed directory as a whole.
- To request a version, write `find_package(ArgX <major>.<minor> CONFIG REQUIRED)`. While the major version is 0, only versions with the same minor version are treated as compatible, so write the version down to the minor version. A request with only the major version, or a version range, finds nothing. The compatibility rules are described in [Compatibility](../references/version.md#compatibility). For contributors, [Version notes](../development/conventions.md#version-notes) in the development conventions describes where the version is defined.

### pkg-config

Uses an installed ArgX with pkg-config (Linux and macOS). Install ArgX as described in [find_package](#find_package). The module name is `argx`.

Without CMake, pass the output of pkg-config to the compiler.

```sh
cc main.c -o app $(pkg-config --cflags --libs argx)
```

From CMake, write the following.

```cmake
cmake_minimum_required(VERSION 3.21)
project(app LANGUAGES C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ARGX REQUIRED IMPORTED_TARGET argx)

add_executable(app main.c)
target_link_libraries(app PRIVATE PkgConfig::ARGX)
```

- If you installed to a non-standard location, add `<install-prefix>/lib/pkgconfig` to the environment variable `PKG_CONFIG_PATH` (on systems where the library directory is `lib64` or similar, the `pkgconfig` directory inside it). From CMake, `-DCMAKE_PREFIX_PATH=<install-prefix>` also works.
- The usage is the same for a static and a shared library.
- If you installed a shared library to a non-standard location and built without CMake, make sure the library can be found when the program runs (on Linux, for example, with `LD_LIBRARY_PATH=<install-prefix>/lib`).

### The version of the package

The installed package files hold the version of ArgX:

| Where | Value |
| --- | --- |
| `ArgX_VERSION`, set by `find_package(ArgX)` | The numeric part, such as `0.2.0` |
| `ArgX_VERSION_FULL`, set by `find_package(ArgX)` | The full version, such as `0.2.0-rc.1`; equal to `ArgX_VERSION` for a release |
| `ArgX_VERSION_PRERELEASE`, set by `find_package(ArgX)` | The pre-release identifiers, such as `rc.1`; empty for a release |
| `Version` in `argx.pc` (`pkg-config --modversion argx`) | The full version, such as `0.2.0-rc.1` |

These variables are set only by `find_package`. A project that adds ArgX with `FetchContent` or `add_subdirectory` reads the version from the header. The version macros of the header are in [Version](../references/version.md).

Under Semantic Versioning, a pre-release has a lower precedence than its release: `0.2.0-rc.1` comes before `0.2.0`. `find_package` and pkg-config compare versions in a different way:

- `find_package` compares only the numeric part, `<major>.<minor>.<patch>`. So `find_package(ArgX 0.2)` and `find_package(ArgX 0.2.0)` also find an installed `0.2.0-rc.1`. To tell a pre-release from a release, check `ArgX_VERSION_PRERELEASE` after `find_package`; it is empty for a release.
- pkg-config compares the full version, and considers `0.2.0-rc.1` newer than `0.2.0`. So `pkg-config --atleast-version=0.2.0 argx` also accepts an installed `0.2.0-rc.1`.

### Running the tests of an embedded ArgX

When ArgX is embedded with `FetchContent` or `add_subdirectory`, its tests are not built by default. To run them from the parent project, set `ARGX_BUILD_TESTS` to ON before you bring ArgX in, and **call `enable_testing()` in the top-level `CMakeLists.txt` of the parent project**. Otherwise `ctest` run in the parent's build directory does not find the tests of ArgX (this is how CTest works).

```cmake
enable_testing()

set(ARGX_BUILD_TESTS ON)
add_subdirectory(third_party/argx)
```

In this case too, Naos is fetched at configure time (see [Building and testing](#building-and-testing)).
