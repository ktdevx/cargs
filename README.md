# ArgX

[![CI](https://github.com/ktdevx/cargs/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/ktdevx/cargs/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

ArgX is a command-line argument parser for C99. Define your command line once as a schema, and ArgX parses `argv`, checks it, converts the values and writes the help, all from that one definition.

## Features

- Plain C99 with no compiler extensions and no dependencies beyond the C standard library.
- Builds on Linux (GCC, Clang), macOS (Clang) and Windows (MSVC), as a static or a shared library.
- Options (flags, counts, `--no-` negatable flags, options with a value), positionals and nested subcommands.
- Typed values: strings, integers, floating-point numbers, choices, and value types you define.
- Help and usage text generated from the schema, written through your write function or into your buffer.
- No side effects: the library prints nothing, never exits the process and reads no environment variables. Errors come back as an error code and a one-line message.

## Example

A complete program that accepts `greet [-h] [-l] [-n COUNT] name`. A larger one with subcommands is [examples/pack.c](examples/pack.c).

```c
#include <argx/argx.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* Checks a call that can fail; on a failure, goes to the message. */
#define TRY(call)                     \
    do                                \
    {                                 \
        error = (call);               \
        if (error != ARGX_ERROR_NONE) \
        {                             \
            goto define_failed;       \
        }                             \
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
    TRY(argx_command_create("greet", &root));
    TRY(argx_command_set_description(root, "Greet somebody."));
    TRY(argx_command_add_flag(root, 'h', "help", &help));
    TRY(argx_option_set_description(help, "Print this help."));
    TRY(argx_command_add_flag(root, 'l', "loud", &loud));
    TRY(argx_option_set_description(loud, "Add an exclamation mark."));
    TRY(argx_command_add_value(root, 'n', "count", argx_type_int(), &count));
    TRY(argx_option_set_description(count, "How often to greet."));
    TRY(argx_option_set_value_name(count, "COUNT"));
    TRY(argx_option_set_default(count, "1"));
    TRY(argx_command_add_positional(root, "name", argx_type_string(), ARGX_ARITY_REQUIRED, &name));
    TRY(argx_positional_set_description(name, "Whom to greet."));
    TRY(argx_command_validate(root, NULL, NULL, NULL));

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

Running it:

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

## Installation

With CMake `FetchContent` (replace `v0.1.0` with the latest [release](https://github.com/ktdevx/cargs/releases)):

```cmake
include(FetchContent)
FetchContent_Declare(ArgX
    GIT_REPOSITORY https://github.com/ktdevx/cargs.git
    GIT_TAG v0.1.0
)
FetchContent_MakeAvailable(ArgX)

add_executable(app main.c)
target_link_libraries(app PRIVATE ArgX::argx)
```

`add_subdirectory`, `find_package` and pkg-config are covered in [Installation](docs/getting-started/installation.md).

## Building and testing

Requires CMake 3.21 or later and a C99 compiler.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

The tests fetch Naos from GitHub; to build offline, see [Building and testing](docs/getting-started/installation.md#building-and-testing).

## Documentation

Start with the [Tutorial](docs/guides/tutorial.md), then the [How-to](docs/guides/how-to.md) and the [FAQ](docs/guides/faq.md). Every document is listed in the [documentation index](docs/README.md).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) and the [Code of Conduct](CODE_OF_CONDUCT.md). Report vulnerabilities privately as described in [SECURITY.md](SECURITY.md). Changes are recorded in [CHANGELOG.md](CHANGELOG.md).

## License

[MIT](LICENSE)
