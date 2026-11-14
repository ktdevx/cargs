# Development Conventions

The rules to follow when you change ArgX. The steps for common changes are in [Development Procedures](procedures.md). Before you design a public function or an object that owns memory, read [Ownership and Lifetime](../references/ownership.md).

## Naming conventions

| What | Convention | Example |
| --- | --- | --- |
| Public and internal functions and types | `argx_` + lowercase snake case | `argx_version`, `argx_vec_push` |
| Functions used only within one file | `static`, no prefix | — |
| Type names (`typedef`) | End with `_t` | `argx_example_t` |
| Structure and enumeration tags | The type name without `_t` | `struct argx_example` |
| Function pointer types and their fields | Type `argx_<name>_fn_t`, field `<name>_fn` | `argx_example_fn_t`, `example_fn` |
| Macros and enumeration constants | `ARGX_` + uppercase snake case | `ARGX_VERSION_MAJOR` |
| Include guards | `ARGX_<FILE NAME>_H` | `ARGX_VERSION_INTERNAL_H` |
| Public headers | `include/argx/<name>.h` | `include/argx/version.h` |
| Sources | `src/<name>.c` | `src/version.c` |
| Internal headers | `src/<name>_internal.h` if `include/argx/<name>.h` exists, otherwise `src/<name>.h` | `src/alloc_internal.h`, `src/vec.h` |
| Tests | `tests/public/<name>_test.c`, `tests/internal/<internal header name>_test.c` | `tests/internal/alloc_internal_test.c` |
| CMake options, functions and modules | `ARGX_<NAME>`, `argx_<name>()`, `cmake/ArgX<Name>.cmake` | `ARGX_BUILD_TESTS` |

- **A function is public only when a header in `include/argx/` declares it with `ARGX_API`.** Do not export functions any other way, and do not export variables (the export check `argx.exports` fails).
- **Do not use `cargs` in any public name.** The project is `ArgX`; the library and the include path are `argx`.
- **Functions that correspond to standard functions match the standard** in name and arguments (`argx_malloc(size)`, `argx_strdup(str)`).

## Directory layout

```
.
├── .github/        Issue forms, pull request template, CI and release workflows
├── cmake/          CMake modules: testing, checks, install, reading the version
├── docs/           Documents
├── examples/       Complete programs, built and run as tests
├── include/argx/   Public headers. Installed
├── src/            Sources and internal headers. Not installed
├── tests/          Tests: public API, internal functions, support code, release scripts
├── tools/          Scripts outside the build, for CI and releases
└── CMakeLists.txt  Assembly only: version, project(), options, subdirectories
```

Create build directories outside the repository, or as `build/` or `build-*/` at the root.

## Includes

- Users include only `<argx/argx.h>`. Public headers include each other as `<argx/<name>.h>`.
- Internal headers are included only by `src/` and by tests of internal functions, never by public headers.

## Documents

| Directory | Contents |
| --- | --- |
| `guides/` | The walkthrough, the FAQ and the design rationale |
| `references/` | Rules, tables, code fragments and output only: no walkthroughs, advice or rationale |

- **A fact has one place.** Guides link to the references instead of repeating them.
- **One API page per public header,** starting with a `## Functions` index. The description and preconditions of each function stay in the comment of its declaration and are not copied. Rules not about one header (the command-line syntax, the layout of the help) are specification pages.
- **Keep examples in sync.** When you change `examples/pack.c`, update the excerpts in the [Guide](../guides/guide.md). When an example's output changes, update its `argx_add_example_test()` and the comment at the top of the program.
- **Complete C functions in documents compile as they are** and follow [Code style](#code-style). An example with placeholder names shows its placeholder header in a comment.

## Code style

These rules apply to all C code, including the examples in the documents.

### Brace style

Allman style: opening braces go on a line of their own. Exceptions: initializers (`= {`) and `} while (...);`.

```c
if (vec->capacity == 0)
{
    new_capacity = ARGX_VEC_FIRST_CAPACITY;
}
else
{
    new_capacity = vec->capacity * 2;
}
```

### Parentheses in expressions

Make operator precedence explicit (MISRA C:2012 Rule 12.1): when an operand of a binary operator or of `?:` is itself a binary or conditional expression, parenthesize it, unless both operators have the same precedence. Do not parenthesize the whole right-hand side of an assignment or the whole expression of a `return`.

```c
if ((unit > 0x10FFFFUL) || ((unit >= 0xD800UL) && (unit <= 0xDFFFUL)))
if (count > (SIZE_MAX / size))
length = (end != NULL) ? (size_t)(end - text) : strlen(text);
return (c >= '0') && (c <= '9');
```

### Breaking long expressions

Break after a binary operator, and align the continuation with the start of the operand it continues.

```c
if (((arity == ARGX_ARITY_REQUIRED) || (arity == ARGX_ARITY_ONE_OR_MORE)) &&
    (argx_result_positional_count(result, positional) == 0))

return add_subcommand(sweep, root, "build", &tree->build) &&
       add_subcommand(sweep, root, "run", &tree->run);
```

## Implementation rules

### Allocating memory

- **Use the internal memory functions** of `src/alloc_internal.h` (`argx_malloc`, `argx_calloc`, `argx_realloc`, `argx_free`) and `argx_strdup` (`src/str.h`). Only `src/alloc.c` may call the standard allocation functions.
- **A size of 0 returns `NULL` without calling the allocator.** A caller whose size can be 0 checks it first and does not treat this `NULL` as a failure.
- **Otherwise `NULL` is an allocation failure.** Free what the function allocated and return `ARGX_ERROR_OUT_OF_MEMORY`.
- **Check the result of `argx_realloc` before replacing the original pointer;** on failure the original memory stays valid.
- **The caller checks products for overflow** and returns `ARGX_ERROR_OUT_OF_MEMORY` without calling the allocator.

#### The allocation check `argx.allocations`

`ctest` runs `cmake/ArgXCheckAllocations.cmake`, which fails when `malloc`, `calloc`, `realloc`, `free`, `aligned_alloc`, `strdup`, `strndup`, `wcsdup`, `asprintf` or `vasprintf` appears as an identifier in `src/` (except `src/alloc.c`). It can also be run on its own with `cmake -P cmake/ArgXCheckAllocations.cmake`. It does not preprocess, so do not build these names with `##` or split them with `\`.

### Global state

The only mutable global state is the allocator (`current_allocator` in `src/alloc.c`). Do not add other mutable file-scope or `static` variables; tables may be `static const`. Keep state in objects the user creates and destroys ([rule 8](../references/ownership.md#8-threads)).

### Applying the ownership rules

Do not decide ownership, lifetime or how failures are returned on your own; follow [Ownership and Lifetime](../references/ownership.md). Public functions never:

- return an allocated string for the user to `free`. Return a string owned by an object instead ([rule 3](../references/ownership.md#3-strings-and-objects-the-library-returns)).
- free a buffer the user allocated. Duplicate it instead ([rule 2](../references/ownership.md#2-strings-you-pass)).

The typical shape of a `create` and `destroy` pair ([rule 7](../references/ownership.md#7-how-failures-are-returned)):

```c
/* <argx/example.h> declares argx_example_t and the two functions below. */
#include <argx/example.h>

#include "alloc_internal.h"
#include "str.h"

#include <assert.h>

struct argx_example
{
    char *name;
};

argx_error_t argx_example_create(const char *name, argx_example_t **out)
{
    argx_example_t *example;

    assert(name != NULL);
    assert(out != NULL);

    *out = NULL;

    example = argx_calloc(1, sizeof(*example));
    if (example == NULL)
    {
        return ARGX_ERROR_OUT_OF_MEMORY;
    }

    example->name = argx_strdup(name);
    if (example->name == NULL)
    {
        argx_free(example);
        return ARGX_ERROR_OUT_OF_MEMORY;
    }

    *out = example;

    return ARGX_ERROR_NONE;
}

void argx_example_destroy(argx_example_t *example)
{
    if (example == NULL)
    {
        return;
    }

    argx_free(example->name);
    argx_free(example);
}
```

Test these with the test allocator by making each allocation fail in turn ([The failure of every allocation in turn](procedures.md#the-failure-of-every-allocation-in-turn)).

### Checking preconditions

A programming error of the caller, such as `NULL` for a required argument, is not an error code but a precondition ([Runtime failures and programming errors](../references/ownership.md#runtime-failures-and-programming-errors)).

- **Check it with the standard `assert`** at the entry of the function, one condition per `assert`.
- **No side effects inside `assert`,** and no variables used only by `assert`: under `NDEBUG` they become unused and break a build with warnings as errors.
- **List the preconditions under `Preconditions:`** in the comment of the declaration, and say which are checked (see `src/vec.h`).
- **Do not break preconditions in tests.**

### Return values of internal functions

| Internal function | Return value | Example |
| --- | --- | --- |
| Pairs with a standard function | The same form as the standard; failure is `NULL` | `argx_malloc`, `argx_strdup` |
| Can fail otherwise | `argx_error_t` | `argx_vec_push` |
| Cannot fail | The natural type, or `void` | `argx_vec_at` |

Report failure only through the return value: never `errno`, output or global variables.

## Writing tests

Tests use [Naos](https://github.com/vabelorg/naos) v0.4.0, are C99 without extensions, and are compiled with warnings as errors (`-Wall -Wextra -Wpedantic`, `/W4`).

### Naos

- Each executable has exactly one `NAOS_MAIN`; each suite has at least one test.
- Each test runs at least one assertion; use `NAOS_PASS()` in a test that only checks that something runs.
- No comma after the last element of `NAOS_SUITE` and `NAOS_MAIN`, and no `;` after `NAOS_TEST`, `NAOS_SUITE` or `NAOS_MAIN`.
- Use the assertion that matches the type (`NAOS_CHECK_UINT_EQ`, `NAOS_CHECK_INT_EQ`).
- Write nothing but `break;` after `NAOS_FAIL` and `NAOS_SKIP` (MSVC C4702).

### Avoiding compiler warnings

| Do not write | Warning | Write instead |
| --- | --- | --- |
| `NAOS_CHECK_NOT_NULL` on memory not yet written | GCC `-Wmaybe-uninitialized` | `NAOS_CHECK_TRUE(pointer != NULL)` |
| A function pointer in `NAOS_CHECK_NULL` and the like | `-Wpedantic` | `NAOS_CHECK_TRUE(function != NULL)` |
| A `const` object without an initializer | MSVC C4132 | `= { 0 }` |
| `(char)0x80` | MSVC C4310 | `'\x80'` |
| The address of a standard function such as `malloc` | MSVC C4204, C4232 | A `static` wrapper function (`wrap_malloc` in `tests/support/wrap_allocator.c`) |

### Allocators in tests

- **Restore the default allocator before the test ends** ([Using the test allocator](procedures.md#using-the-test-allocator)).
- **Never pass huge sizes to the standard allocation functions;** ASan and valgrind abort. Use an allocator that records its arguments and returns `NULL`.
- **Check both sides of an overflow check.** For the side that just fits, use `argx_test_allocator_fail_at(1)` and check that exactly one request reached the allocator.

### Rules of `argx_add_test()`

```cmake
argx_add_test(<name> SOURCES <source>... [INTERNAL])
```

- `<name>` uses letters, digits and underscores, and is unique across all tests. For a test of internal functions, it is the name of the internal header.
- Without `INTERNAL`, the test links to `ArgX::argx`; with it, to `argx_internal`.
- `<source>` is relative to `tests/`, including files from `tests/support/`.
- Tests are registered as `<name>.<suite>.<test>` with the label `public` or `internal`.

### Release tests

The tests in `tests/release/` run with `cmake -P`, need no build, and have the label `release`. On Windows, `file(WRITE)` turns LF into CRLF, so files whose bytes matter are written with `argx_write_bytes()` (`tools/release/ArgXReadBytes.cmake`). Set `ARGX_WRITE_BYTES_WITH_CONFIGURE=1` to write them as on Windows on any host.

## Version notes

- **The version is defined only by the four `ARGX_VERSION_*` macros in `include/argx/version.h`.** The build reads them as text, so keep each on its own line in the form `#define ARGX_VERSION_<PART> <0-255>` and `#define ARGX_VERSION_PRERELEASE "<identifiers>"` (`""` for a release).
- **Do not bump the version by hand.** The **Release PR** workflow does it ([release guide](releasing.md)).
