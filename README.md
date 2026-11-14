# ArgX

[![CI](https://github.com/ktdevx/cargs/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/ktdevx/cargs/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

ArgX is a command-line argument parser library written in C99. You define the command line your program accepts (the schema) with function calls, parse `argv` with it, and read the values from the result. ArgX also generates the help text and the usage line from the schema.

## Features

- Plain C99 with no compiler extensions and no dependencies beyond the C standard library.
- Builds on Linux (GCC, Clang), macOS (Clang) and Windows (MSVC), as a static or a shared library.
- Options (flags, counts, `--no-` negatable flags, options with a value), positionals and nested subcommands.
- Typed values: strings, integers, floating-point numbers, choices, and value types you define.
- Help and usage text generated from the schema, written through your write function or into your buffer.
- No side effects: the library prints nothing, never exits the process and reads no environment variables. Errors come back as an error code and a one-line message.

Include `<argx/argx.h>` and link to `ArgX::argx`.

## Example

A complete program that accepts `greet [-h] [-l] [-n COUNT] name`. A larger one, with subcommands, every kind of option and value types of its own, is [examples/pack.c](examples/pack.c).

```c
#include <argx/argx.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* Checks a call that can fail; on a failure, goes to the message. */
#define CHECK(call)                              \
    do                                           \
    {                                            \
        if ((error = (call)) != ARGX_ERROR_NONE) \
        {                                        \
            goto define_failed;                  \
        }                                        \
    } while (0)

/* The library passes the help text to this function, part by part. */
static bool write_to_stream(void *user_data, const char *text, size_t length)
{
    return fwrite(text, 1, length, (FILE *)user_data) == length;
}

int main(int argc, char **argv)
{
    argx_command_t *root = NULL;
    argx_option_t *help = NULL;
    argx_option_t *loud = NULL;
    argx_option_t *count = NULL;
    argx_positional_t *name = NULL;
    argx_result_t *result = NULL;
    argx_error_t error;
    int status = EXIT_FAILURE;

    /* The schema. The descriptions are for the help. */
    CHECK(argx_command_create("greet", &root));
    CHECK(argx_command_set_description(root, "Greet somebody."));
    CHECK(argx_command_add_flag(root, 'h', "help", &help));
    CHECK(argx_option_set_description(help, "Print this help."));
    CHECK(argx_command_add_flag(root, 'l', "loud", &loud));
    CHECK(argx_option_set_description(loud, "Add an exclamation mark."));
    CHECK(argx_command_add_value(root, 'n', "count", argx_type_int(), &count));
    CHECK(argx_option_set_description(count, "How often to greet."));
    CHECK(argx_option_set_value_name(count, "COUNT"));
    CHECK(argx_option_set_default(count, "1"));
    CHECK(argx_command_add_positional(root, "name", argx_type_string(), ARGX_ARITY_REQUIRED, &name));
    CHECK(argx_positional_set_description(name, "Whom to greet."));
    CHECK(argx_command_validate(root, NULL, NULL, NULL));

    error = argx_parse(root, argc, argv, &result);
    if (result == NULL)
    {
        /* ARGX_ERROR_OUT_OF_MEMORY: there is no result. */
        fprintf(stderr, "greet: %s\n", argx_error_message(error));
    }
    else if (argx_result_flag(result, help))
    {
        /* Checked first, so that "greet --help" works without a name. */
        if (argx_help_write(root, NULL, write_to_stream, stdout) == ARGX_ERROR_NONE)
        {
            status = EXIT_SUCCESS;
        }
    }
    else if (error == ARGX_ERROR_INVALID_COMMAND_LINE)
    {
        fprintf(stderr, "greet: %s\n", argx_result_error_message(result));
    }
    else
    {
        const char *text = NULL;
        int64_t times = 0;
        int64_t i;

        if (argx_result_positional_string(result, name, 0, &text) &&
            argx_result_option_int(result, count, &times))
        {
            for (i = 0; i < times; ++i)
            {
                printf("hello, %s%s\n", text, argx_result_flag(result, loud) ? "!" : "");
            }
            printf("greeted %" PRId64 " time(s)\n", times);
            status = EXIT_SUCCESS;
        }
    }

    goto cleanup;

define_failed:
    fprintf(stderr, "greet: cannot define the command line: %s\n", argx_error_message(error));

cleanup:
    /* The result first, then the schema. Destroying NULL does nothing. */
    argx_result_destroy(result);
    argx_command_destroy(root);

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

- `--help` is an ordinary flag in your schema. The help text comes from the schema; printing it is up to your code.
- A parse error still leaves a result, which you destroy. Destroy the result before the schema.

## Installation

With CMake `FetchContent`:

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

Replace `v0.1.0` with the latest tag from the [Releases](https://github.com/ktdevx/cargs/releases) page. `add_subdirectory`, `find_package`, pkg-config and the build options are described in [Installation](docs/getting-started/install.md).

## Building and testing

You need CMake 3.21 or later and a C99 compiler. Run the following three commands in the root of the repository.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

The tests fetch the test framework Naos from GitHub at configure time. To build offline or without the tests, see [Building and testing](docs/getting-started/install.md#building-and-testing) in the installation guide.

## Documentation

The [documentation index](docs/README.md) lists every document. To start:

- **Getting started:** [Installation](docs/getting-started/install.md) covers building, the build options, and the four ways to use ArgX from your project.
- **Guides:** the [Guide](docs/guides/guide.md) walks through [examples/pack.c](examples/pack.c), a complete program, step by step. The [FAQ](docs/guides/faq.md) answers common questions, and [Design](docs/guides/design.md) explains why the library works the way it does.
- **References:** one API page for each public header ([Schemas](docs/references/schema.md), [Value Types](docs/references/types.md), [Parsing](docs/references/parse.md), [Help and Usage](docs/references/help.md), [Allocator](docs/references/alloc.md), [Error Codes](docs/references/errors.md), [Version](docs/references/version.md)), and the specifications of the [command-line syntax](docs/references/syntax.md), the [help text layout](docs/references/help-layout.md), [ownership and lifetime](docs/references/ownership.md) and [versioning](docs/references/versioning.md).
- **Development:** [Contributing to ArgX](CONTRIBUTING.md) is the entry point for contributors.

## Releases and changes

Changes in each version are recorded in [CHANGELOG.md](CHANGELOG.md), and releases are published on the [Releases](https://github.com/ktdevx/cargs/releases) page. Release tags have the form `v<SemVer>`, such as `v0.2.0`. What stays compatible between versions is described in the [versioning policy](docs/references/versioning.md).

## Contributing

Contributions are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md) before you open an issue or a pull request: it explains the development environment, how to build and test, and how to submit changes. Everyone who takes part follows the [Code of Conduct](CODE_OF_CONDUCT.md). Report security vulnerabilities as described in [SECURITY.md](SECURITY.md), not in a public issue.

## License

ArgX is distributed under the MIT License. The full text is in [LICENSE](LICENSE).
