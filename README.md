# CLine

[![CI](https://github.com/ktdevx/cargs/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/ktdevx/cargs/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

CLine is a command-line argument parser library written in C99. You define the command line that your program accepts (the schema) with function calls and validate it, parse the argv that `main` received, and read the values from the result. From the schema, CLine can also generate the help text and the usage line.

## Features

- Written in C99 without compiler extensions. It depends on nothing but the C standard library.
- Builds on Linux (GCC, Clang), macOS (Clang) and Windows (MSVC).
- Can be built as a static library or as a shared library.
- Can be used from CMake with `FetchContent`, `add_subdirectory` or `find_package`, and with pkg-config.
- You can define options (flags, counts, flags that can be negated with `--no-`, and options with a value), positionals and subcommands. Values can be converted and read as strings, integers, floating-point numbers, choices, or with a user-defined value type.
- The text of the help and the usage line is made from the definitions in the schema. The text goes to a write function that you pass, or into your buffer.
- The library prints nothing, never exits the process, and does not read environment variables. When the arguments do not match the schema, it returns the kind of error and a one-line message. Printing them is up to your code. `--help` is also defined in the schema by you, and your code prints the help.

The only header you include is `<cline/cline.h>`, and the only target you link to is `CLine::cline`.

## Example

The following example is a complete program that accepts a command line of the form `greet [-h] [-l] [-n COUNT] name`. It defines and validates the schema, parses argv, prints the help if `--help` was given, and otherwise reads the values. Finally it destroys what it created.

```c
#include <cline/cline.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* Keeps the result of a call in *error and tells whether the call failed. */
static bool failed(cline_error_t *error, cline_error_t result)
{
    *error = result;
    return result != CLINE_ERROR_NONE;
}

/*
 * The write function: the library passes the text of the help to it, part
 * by part. This one writes each part to the stream in user_data.
 */
static bool write_to_stream(void *user_data, const char *text, size_t length)
{
    return fwrite(text, 1, length, (FILE *)user_data) == length;
}

int main(int argc, char **argv)
{
    cline_command_t *root = NULL;
    cline_option_t *help = NULL;
    cline_option_t *loud = NULL;
    cline_option_t *count = NULL;
    cline_positional_t *name = NULL;
    cline_result_t *result = NULL;
    cline_error_t error;
    int status = EXIT_FAILURE;

    /*
     * The schema: greet [-h] [-l] [-n COUNT] name. The descriptions are
     * for the help. The schema is validated before it is parsed with.
     */
    if (failed(&error, cline_command_create("greet", &root)) ||
        failed(&error, cline_command_set_description(root, "Greet somebody.")) ||
        failed(&error, cline_command_add_flag(root, 'h', "help", &help)) ||
        failed(&error, cline_option_set_description(help, "Print this help.")) ||
        failed(&error, cline_command_add_flag(root, 'l', "loud", &loud)) ||
        failed(&error, cline_option_set_description(loud, "Add an exclamation mark.")) ||
        failed(&error, cline_command_add_value(root, 'n', "count", cline_type_int(), &count)) ||
        failed(&error, cline_option_set_description(count, "How often to greet.")) ||
        failed(&error, cline_option_set_value_name(count, "COUNT")) ||
        failed(&error, cline_option_set_default(count, "1")) ||
        failed(&error, cline_command_add_positional(root, "name", cline_type_string(),
                                                     CLINE_ARITY_REQUIRED, &name)) ||
        failed(&error, cline_positional_set_description(name, "Whom to greet.")) ||
        failed(&error, cline_command_validate(root, NULL, NULL, NULL)))
    {
        fprintf(stderr, "greet: cannot define the command line: %s\n", cline_error_message(error));
    }
    else
    {
        /* argc and argv go in as main() received them. */
        error = cline_parse(root, argc, argv, &result);

        if (result == NULL)
        {
            /* CLINE_ERROR_OUT_OF_MEMORY: there is no result. */
            fprintf(stderr, "greet: %s\n", cline_error_message(error));
        }
        else if (cline_result_flag(result, help))
        {
            /*
             * --help first: "greet --help" leaves out the name, and that
             * must not hide the help. The text is made from the schema.
             */
            if (cline_help_write(root, NULL, write_to_stream, stdout) == CLINE_ERROR_NONE)
            {
                status = EXIT_SUCCESS;
            }
        }
        else if (error == CLINE_ERROR_INVALID_COMMAND_LINE)
        {
            /* The message is one line with no prefix and no line break. */
            fprintf(stderr, "greet: %s\n", cline_result_error_message(result));
        }
        else
        {
            const char *text = NULL;
            int64_t times = 0;
            int64_t i;

            /*
             * The name is required and --count has a default value: after
             * a parse that succeeded, both have a value.
             */
            if (cline_result_positional_string(result, name, 0, &text) &&
                cline_result_option_int(result, count, &times))
            {
                for (i = 0; i < times; ++i)
                {
                    printf("hello, %s%s\n", text, cline_result_flag(result, loud) ? "!" : "");
                }
                printf("greeted %" PRId64 " time(s)\n", times);
                status = EXIT_SUCCESS;
            }
        }
    }

    /*
     * The result first, then the schema it was made from. A parse error
     * also leaves a result to destroy. Destroying NULL does nothing.
     */
    cline_result_destroy(result);
    cline_command_destroy(root);

    return status;
}
```

Example run (lines starting with `$` are input, the lines after them are output):

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

- The help text is made from the definitions in the schema (names, descriptions, value names, default values). You do not need to write the help text in your program.
- Printing the help is up to your code. The library does not add `--help` to the schema, and it does not print the help and exit.
- On a parse error (the arguments do not match the schema), you still get a result, and you destroy it. Destroy the result before the schema.
- The detailed usage is described in the documents listed in [Documentation](#documentation).

## Documentation

For users of the library:

- [Schemas](docs/schema.md): defining and validating commands, options, positionals and value types.
- [Parsing](docs/parse.md): parsing, the command-line syntax, reading the result, the kinds and messages of parse errors, handling `--help`, and wide-character argv (`wmain`).
- [Help and Usage](docs/help.md): generating the help and the usage line, the text layout rules, settings such as the width, the output (write functions and buffers), and an example that adds the usage line to a parse error.
- [Ownership and Lifetime](docs/ownership.md): who frees what, and how long what you pass and what you get back stay valid.
- [Documentation index](docs/README.md): all documents, grouped by reader, with a one-line description of each.

For contributors:

- [CONTRIBUTING.md](CONTRIBUTING.md): how to report problems, set up a development environment, and submit a pull request.
- [Development Conventions](docs/conventions.md): naming conventions, the directory layout, and the procedures for adding public functions, headers, sources and tests. Read it before you change the code.

## Building and testing

You need CMake 3.21 or later and a C99 compiler. Run the following three commands in the root of the repository.

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
  cmake -S . -B build -DCLINE_BUILD_TESTS=OFF
  cmake --build build
  ```

- With a generator where one build directory holds several configurations, such as Visual Studio, `cmake --build` and `ctest` use Debug when no configuration is given. Give other configurations explicitly, as in `cmake --build build --config Release` and `ctest --test-dir build -C Release`.

## Build options

| Option | Description | Default when built directly | Default when embedded |
|---|---|---|---|
| `CLINE_BUILD_TESTS` | Builds the tests and registers them with CTest (fetches Naos) | `ON` | `OFF` |
| `CLINE_INSTALL` | Creates the install rules and the package files for `find_package` and pkg-config | `ON` | `OFF` |
| `CLINE_WARNINGS_AS_ERRORS` | Turns compiler warnings into errors for the CLine targets | `ON` | `OFF` |

- "Embedded" means that another project brought CLine in with `FetchContent` or `add_subdirectory`. By default, CLine adds no tests, no install rules and no warnings-as-errors to the parent project.
- Set an option with `-D<option>=ON` or `-D<option>=OFF`. A project that embeds CLine can also write `set(<option> ON)` before it brings CLine in.
- Choose between a static and a shared library with the standard CMake variable `BUILD_SHARED_LIBS`. If you do not set it, you get a static library.

  ```sh
  cmake -S . -B build -DBUILD_SHARED_LIBS=ON
  ```

## Using CLine in your project

Whichever way you choose, write `#include <cline/cline.h>` in your sources. In CMake, link to `CLine::cline` (only when you use pkg-config from CMake, link to `PkgConfig::CLINE`). Linking adds the include path and, for a shared library, the required definition (`CLINE_SHARED`) automatically. A static library needs no definition.

Each example below is a complete `CMakeLists.txt` that builds the executable `app` from the program in [Example](#example), saved as `main.c`.

### FetchContent

Fetches the sources of CLine at configure time and builds them together with your project.

```cmake
cmake_minimum_required(VERSION 3.21)
project(app LANGUAGES C)

include(FetchContent)
FetchContent_Declare(CLine
    GIT_REPOSITORY https://github.com/ktdevx/cargs.git
    GIT_TAG v0.1.0
)
FetchContent_MakeAvailable(CLine)

add_executable(app main.c)
target_link_libraries(app PRIVATE CLine::cline)
```

- Replace `v0.1.0` with the latest release tag. Release tags have the form `v<SemVer>`, such as `v0.2.0`; pre-releases look like `v0.2.0-rc.1`. All releases are listed on the [Releases](https://github.com/ktdevx/cargs/releases) page.
- `GIT_TAG` accepts a tag, a commit hash or a branch name. For a reproducible build, pin it to a tag or a commit hash. A branch name also works, but its contents can change.
- To use a local checkout, pass `-DFETCHCONTENT_SOURCE_DIR_CLINE=<path-to-checkout>` at configure time. No network is needed.

### add_subdirectory

Put the sources of CLine inside your project (as a submodule or a copy) and build them together. The following example assumes they are in `third_party/cline`.

```cmake
cmake_minimum_required(VERSION 3.21)
project(app LANGUAGES C)

add_subdirectory(third_party/cline)

add_executable(app main.c)
target_link_libraries(app PRIVATE CLine::cline)
```

- When the sources are outside your project, give a name for the build directory as the second argument, as in `add_subdirectory(<path-to-checkout> cline)`.

### find_package

Uses an installed CLine. Build and install CLine first.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCLINE_BUILD_TESTS=OFF
cmake --build build --config Release
cmake --install build --config Release --prefix <install-prefix>
```

Then write the following in your project.

```cmake
cmake_minimum_required(VERSION 3.21)
project(app LANGUAGES C)

find_package(CLine CONFIG REQUIRED)

add_executable(app main.c)
target_link_libraries(app PRIVATE CLine::cline)
```

- If you installed to a non-standard location, pass `-DCMAKE_PREFIX_PATH=<install-prefix>` when you configure your project.
- You may later move the installed directory as a whole.
- To request a version, write `find_package(CLine <major>.<minor> CONFIG REQUIRED)`. While the major version is 0, only versions with the same minor version are treated as compatible, so write the version down to the minor version. A request with only the major version, or a version range, finds nothing. The compatibility rules are described in [Compatibility before and after 1.0](docs/versioning.md#compatibility-before-and-after-10) in the versioning policy. For contributors, [Version notes](docs/conventions.md#version-notes) in the development conventions describes where the version is defined.

### pkg-config

Uses an installed CLine with pkg-config (Linux and macOS). Install CLine as described in [find_package](#find_package). The module name is `cline`.

Without CMake, pass the output of pkg-config to the compiler.

```sh
cc main.c -o app $(pkg-config --cflags --libs cline)
```

From CMake, write the following.

```cmake
cmake_minimum_required(VERSION 3.21)
project(app LANGUAGES C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(CLINE REQUIRED IMPORTED_TARGET cline)

add_executable(app main.c)
target_link_libraries(app PRIVATE PkgConfig::CLINE)
```

- If you installed to a non-standard location, add `<install-prefix>/lib/pkgconfig` to the environment variable `PKG_CONFIG_PATH` (on systems where the library directory is `lib64` or similar, the `pkgconfig` directory inside it). From CMake, `-DCMAKE_PREFIX_PATH=<install-prefix>` also works.
- The usage is the same for a static and a shared library.
- If you installed a shared library to a non-standard location and built without CMake, make sure the library can be found when the program runs (on Linux, for example, with `LD_LIBRARY_PATH=<install-prefix>/lib`).

### Running the tests of an embedded CLine

When CLine is embedded with `FetchContent` or `add_subdirectory`, its tests are not built by default. To run them from the parent project, set `CLINE_BUILD_TESTS` to ON before you bring CLine in, and **call `enable_testing()` in the top-level `CMakeLists.txt` of the parent project**. Otherwise `ctest` run in the parent's build directory does not find the tests of CLine (this is how CTest works).

```cmake
enable_testing()

set(CLINE_BUILD_TESTS ON)
add_subdirectory(third_party/cline)
```

In this case too, Naos is fetched at configure time (see [Building and testing](#building-and-testing)).

## Releases and changes

- Changes in each version are recorded in [CHANGELOG.md](CHANGELOG.md).
- Releases are published on the [Releases](https://github.com/ktdevx/cargs/releases) page.
- Release tags have the form `v<SemVer>`, such as `v0.2.0`; pre-releases look like `v0.2.0-rc.1`.
- How versions are numbered and what stays compatible is described in the [versioning policy](docs/versioning.md).

## Contributing

Contributions are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md) before you open an issue or a pull request: it explains the development environment, how to build and test, and how to submit changes. Everyone who takes part follows the [Code of Conduct](CODE_OF_CONDUCT.md). Report security vulnerabilities as described in [SECURITY.md](SECURITY.md), not in a public issue.

## License

CLine is distributed under the MIT License. The full text is in [LICENSE](LICENSE).
