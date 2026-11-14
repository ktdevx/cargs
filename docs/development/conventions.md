# Development Conventions

This document collects the rules to follow when you change ArgX: naming, the directory layout, how the code is written, how tests are written, and where the version is defined. If you add a new feature, do not choose names or locations on your own; follow this document. The steps for adding public functions, headers, sources and tests are in [Development Procedures](procedures.md).

Before you design a public function or an object that owns memory, read [Ownership and Lifetime](../references/ownership.md) first. The pairs of creation and destruction, how strings are held, how `NULL` is handled and how failures are returned are decided there. This document covers the rules for writing those decisions in code and tests.

The entry point for contributors is [CONTRIBUTING.md](../../CONTRIBUTING.md) at the root of the repository. It describes how to report issues, build and test, and submit a pull request, and points to this document for the conventions.

## Naming conventions

| What | Convention | Example |
| --- | --- | --- |
| Public functions and types | `argx_` + lowercase snake case. Type names end with `_t` | `argx_version`, `argx_example_t` |
| Public macros and enumeration constants | `ARGX_` + uppercase snake case | `ARGX_VERSION_MAJOR` |
| Internal functions and types (used across files) | The same `argx_` + lowercase snake case as public ones. Type names end with `_t`. Do not add `ARGX_API`; declare them in an internal header in `src/` | `argx_vec_push`, `argx_vec_t` |
| Structure and enumeration tags | The type name without `_t`. Do not add `_t` | `struct argx_example`, `enum argx_example_kind` |
| Function pointer types and the fields that hold them | The type is `argx_<name>_fn_t`, the field is `<name>_fn` | `argx_example_fn_t`, `example_fn` |
| Internal macros | The same `ARGX_` + uppercase snake case as public ones. Define them in an internal header in `src/` | `ARGX_ALLOC_INTERNAL_H` (include guard) |
| Functions used only within one file | Add `static`. No prefix is needed | — |
| Public headers | `include/argx/<name>.h` | `include/argx/version.h` |
| Sources | `src/<name>.c` | `src/version.c` |
| Internal headers | `src/<name>_internal.h` for a module that has a public header of the same name (`include/argx/<name>.h`), `src/<name>.h` for a module that does not | `src/alloc_internal.h` (`include/argx/alloc.h` exists), `src/vec.h` |
| Tests of the public API | `tests/public/<name>_test.c` | `tests/public/version_test.c` |
| Tests of internal functions | `tests/internal/<internal-header-name>_test.c` | `tests/internal/alloc_internal_test.c` |
| CMake options, functions and modules | `ARGX_<name>`, `argx_<name>()`, `cmake/ArgX<name>.cmake` | `ARGX_BUILD_TESTS`, `argx_add_test()`, `cmake/ArgXTesting.cmake` |

- Public and internal names are not told apart by prefix. Whether a function is public is decided by whether it is declared with `ARGX_API` in a header under `include/argx/`. `ARGX_API` (`include/argx/export.h`) is the only mark that makes a function public. Do not make functions public in any other way (writing compiler attributes directly, adding a `.def` file and so on).
- Do not use `cargs` in any public name (artifacts, headers, identifiers, packages). The project name is `ArgX`; the library and the include path are `argx`.
- Do not have public variables. Only functions, types and macros are public. If you put `ARGX_API` on a variable, the export check (`argx.exports`) fails.
- Include guards are `ARGX_<file name in uppercase>_H` (`ARGX_VERSION_H`, `ARGX_VERSION_INTERNAL_H`).
- Type names (the names given with `typedef`) end with `_t`, whether they are public, internal or test support. Structure and enumeration tags do not take `_t` (`typedef struct argx_example argx_example_t;`).
- Function pointer types are `argx_<name>_fn_t`, and the structure fields that hold them are `<name>_fn`.
- An internal header takes `_internal` to tell it apart from the public header of the same name. In a module without a public header, do not add it. The file name of a test of internal functions and the `<name>` of `argx_add_test()` follow the name of the internal header.
- Functions that correspond to standard functions match the standard in name and arguments (the function that corresponds to `malloc` is `argx_malloc`, the one that corresponds to `strdup` is `argx_strdup`; argument names such as `ptr` and `str` also match the standard).
- Keep module names (`<name>`) short (`alloc`, `str`, `vec` and so on).

## Directory layout

The repository contains the following files and directories.

```
.editorconfig         Editor settings: UTF-8, LF line endings, indentation per file type
.gitattributes        Normalizes text files to LF line endings
.gitignore            Files excluded from version control: build directories, local CMake settings, editor and OS files
.github/
├── ISSUE_TEMPLATE/
│   ├── bug_report.yml       The issue form for bug reports
│   ├── feature_request.yml  The issue form for feature requests
│   └── config.yml           Disables blank issues and points security reports to private reporting
├── pull_request_template.md The pull request template: summary, related issues and checklist
└── workflows/
    ├── ci.yml               CI: builds and tests pull requests and pushes to main; skips the builds and tests for documentation-only changes
    ├── release-pr.yml       Release PR: started by hand with a version; rewrites include/argx/version.h and CHANGELOG.md and opens a release pull request
    └── release-publish.yml  Release: when a release pull request is merged, creates the tag and the GitHub Release
CHANGELOG.md          The record of changes in each release (Keep a Changelog)
CMakeLists.txt        Assembly only: reads the version, project(), options, adds each directory
CODE_OF_CONDUCT.md    The code of conduct for everyone who takes part in the project
CONTRIBUTING.md       The entry point for contributors
examples/             Complete example programs, linked from the README. Built and run as tests (label example) when ARGX_BUILD_TESTS is ON
├── CMakeLists.txt    argx_add_example() per program and argx_add_example_test() per command line it is run with
└── pack.c            The complete example: schema, validation, parsing, --help, parse errors, reading the values
LICENSE               The MIT License
README.md             The entry point of the project for users: overview, an example, quick installation, links to the documents
SECURITY.md           Supported versions and how to report a vulnerability privately
cmake/                Build support files (CMake modules, scripts, templates)
├── ArgXConfig.cmake.in       The package file for find_package(ArgX)
├── ArgXCTestDefaults.cmake   The configuration ctest uses when none is given
├── ArgXCheckAllocations.cmake The allocation check argx.allocations
├── ArgXCheckExports.cmake    The export check argx.exports
├── ArgXInstall.cmake         The install rules and the package files
├── ArgXTargetDefaults.cmake  The compile settings shared by the targets
├── ArgXTesting.cmake         Fetches Naos and defines argx_add_test()
├── ArgXVersion.cmake         Reads the version from include/argx/version.h
└── argx.pc.in                The pkg-config file
include/
└── argx/             Public headers. All of them go here. They are installed
    ├── argx.h        The only entry point users include. It only includes the other public headers
    ├── alloc.h       The allocator type and the declaration of argx_set_allocator()
    ├── error.h       The list of error codes ARGX_ERROR_LIST, argx_error_t, the declaration of argx_error_message()
    ├── export.h      ARGX_API
    ├── help.h        Help: the write function type, the help options argx_help_options_t, declarations for generating help and the usage line
    ├── parse.h       Parsing: declarations of argx_parse(), argx_parse_wide(), reading and destroying the result argx_result_t, parse error information
    ├── schema.h      Schemas: declarations for defining commands, options and positionals, attributes, validation, reading
    ├── type.h        Value types: declarations of argx_type_t, the built-in value types, creation, destruction, reading, conversion
    └── version.h     The version macros and the declaration of argx_version()
src/                  Sources and internal headers. Not installed
├── CMakeLists.txt    The source list ARGX_SOURCES, the targets argx and argx_internal
├── <name>.c
└── <name>_internal.h  Internal header. <name>.h if there is no public header of the same name
tests/
├── CMakeLists.txt    The argx_add_test() calls, the registration of the allocation check and the export check, and the registration of the release tests
├── public/           Tests of the public API. Linked to ArgX::argx
├── internal/         Tests of internal functions. Linked to argx_internal
├── support/          Test support sources and headers (the test allocator and the wrap allocator). Not part of the library and not installed
├── consumer/         Checks of using the library from a consuming project. Not part of the main build
└── release/          Tests of the release scripts and modules, run with cmake -P. Need no build
    ├── ReleaseTestHelpers.cmake  Checks and fixtures shared by the release tests
    ├── <name>_test.cmake         One test per file
    └── fixtures/                 Input and expected files (changelogs, a version header)
tools/                Scripts that are not part of the build
├── ci/
│   └── changes.cmake Decides whether a change touches documentation only, so that CI can skip the builds and tests
└── release/          The release scripts and the modules they include, run with cmake -P by the release workflows
    ├── prepare.cmake           Checks a release and rewrites the version header and the changelog (Release PR)
    ├── publish.cmake           Checks a merged release commit and makes the release notes (Release)
    ├── ArgXChangelog.cmake     Reads and updates CHANGELOG.md
    ├── ArgXReadBytes.cmake     Reads and writes a text file byte for byte, line endings included
    ├── ArgXSemver.cmake        Checks a version and compares versions by SemVer precedence
    └── ArgXVersionHeader.cmake Rewrites the version in a version header
docs/                       Documents; the subdirectories match the sections of the index
├── README.md               The index of the documents
├── getting-started/
│   └── install.md          Installation: building and testing, the build options, using the library from a project
├── guides/
│   ├── guide.md            The guide: concepts and a walk through examples/pack.c
│   ├── faq.md              Short answers to common questions
│   └── design.md           Why the library works the way it does
├── references/
│   ├── schema.md           API of schema.h: commands, options, positionals, validation, definition errors, reading
│   ├── types.md            API of type.h: value types, conversion, the order of destruction
│   ├── parse.md            API of parse.h: parsing, reading the result, parse errors, --help, wide-character argv
│   ├── help.md             API of help.h: generating functions, help options, output, failures
│   ├── alloc.md            API of alloc.h: the allocator
│   ├── errors.md           API of error.h: the error codes and their messages
│   ├── version.md          API of version.h: checking the version in code
│   ├── syntax.md           Specification: the command-line syntax
│   ├── help-layout.md      Specification: the text of the help and the usage line
│   ├── ownership.md        Specification: ownership, lifetime and failures
│   └── versioning.md       Specification: the versioning policy
└── development/
    ├── conventions.md      This document (for contributors)
    ├── procedures.md       The steps for common changes (for contributors)
    └── releasing.md        The release procedure (for maintainers)
```

- [CONTRIBUTING.md](../../CONTRIBUTING.md) is the entry point for contributors, and [docs/README.md](../README.md) is the index of the documents.
- [CHANGELOG.md](../../CHANGELOG.md) records the changes of each release. The [versioning policy](../references/versioning.md) and the [release guide](releasing.md) describe the versions and the release procedure.
- [`tools/release/`](../../tools/release) holds the release scripts and modules, and [`tools/ci/`](../../tools/ci) holds [`changes.cmake`](../../tools/ci/changes.cmake), which CI uses to decide whether a change touches documentation only. The workflows [`ci.yml`](../../.github/workflows/ci.yml), [`release-pr.yml`](../../.github/workflows/release-pr.yml) and [`release-publish.yml`](../../.github/workflows/release-publish.yml) in [`.github/workflows/`](../../.github/workflows) run them.
- The tests in [`tests/release/`](../../tests/release) are registered by `tests/CMakeLists.txt` with `argx_add_release_test(<name>)`, under the name `release.<name>` and with the label `release`. They need no build, so they are registered in both the static and the shared configuration. You can select them with `ctest -L release`. On Windows, `file(WRITE)` turns LF into CRLF, so the release scripts and the tests write the files whose bytes matter with `argx_write_bytes()` of `ArgXReadBytes.cmake`. To write them as on Windows on any host, run the tests with the environment variable `ARGX_WRITE_BYTES_WITH_CONFIGURE=1`.
- The documents in `docs/` are of four kinds, one directory each. The guides (`guides/`) hold the walkthrough, the recipes, the FAQ and the design rationale; the [Guide](../guides/guide.md) follows [examples/pack.c](../../examples/pack.c) with excerpts that name the function they come from. The references (`references/`) hold rules, tables, code fragments and output, but no walkthroughs, no advice on how to put the functions together, and no rationale. A fact has one place: the guides, the FAQ and the design rationale link to the references instead of repeating them.
- Each public header has one API page in `references/`, which starts with a `## Functions` index of every public function of the header. Each function appears in exactly one index. The detailed description and the preconditions of each function stay in the comment of its declaration; an API page does not copy them, and covers the rules shared by several functions instead. Rules that are not about one header, such as the command-line syntax or the layout of the help, are specification pages.
- Complete programs are the example in the [README](../../README.md#example) and the programs in `examples/`, which are built and run with the tests; a document that refers to one of them links to the file and names the function. When you change `examples/pack.c`, update the excerpts of the Guide too. When an example prints something else, update the expected output of its `argx_add_example_test()` and the run in the comment at the top of the program.
- Users write only `#include <argx/argx.h>`. Public headers also include each other in the form `<argx/<name>.h>`.
- The internal headers in `src/` are included only by the sources in `src/` and by tests of internal functions, in the form `"<name>_internal.h"` (`"<name>.h"` if there is no public header of the same name). Do not include internal headers from public headers.
- Of the scripts in `cmake/`, `ArgXCheckAllocations.cmake` (the allocation check `argx.allocations`) and `ArgXCheckExports.cmake` (the export check `argx.exports`) can also be run on their own with `cmake -P` ([Verifying a change](procedures.md#verifying-a-change)).
- A test that uses files from `tests/support/` adds them to the `SOURCES` of `argx_add_test()` ([Using the test allocator](procedures.md#using-the-test-allocator)).
- Create build directories outside the repository, or as `build/` or `build-*/` at the root (they are excluded from version control).

## Implementation rules

These are the rules for writing the library sources (`src/`). Ownership, lifetime and how failures are returned themselves are in [Ownership and Lifetime](../references/ownership.md).

### Brace style

All C code (`include/`, `src/`, `tests/`) uses the Allman style: the opening brace of a function, a control statement (`if`, `else`, `for`, `while`, `do`, `switch`), a `struct`, `union` or `enum`, and `extern "C"` goes on a line of its own, at the indentation of the line that starts the statement. `else` starts a new line after the closing brace. The braces of an initializer (`= {`) stay on the same line, and the `while` of a `do` statement follows the closing brace (`} while (...);`).

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

All C code (`include/`, `src/`, `tests/`) makes operator precedence explicit, in the style of MISRA C:2012 Rule 12.1. When an operand of a binary operator or of the conditional operator `?:` is itself a binary or conditional expression, put it in parentheses, unless both operators have the same precedence. This applies inside `assert`, the test assertion macros and macro definitions too.

- No parentheses are needed for operands that are primary or postfix expressions (variables, constants, function calls, `a[i]`, `p->m`), unary expressions (`!x`, `*p`, casts, `sizeof`), or operators of the same precedence (`a + b + c`, `x && y && z`).
- Do not put the whole right-hand side of an assignment, or the whole expression of a `return`, in parentheses.

```c
if ((type == NULL) || (type->is_static))
if (unit > 0x10FFFFUL || ((unit >= 0xD800UL) && (unit <= 0xDFFFUL)))    /* wrong: unit > 0x10FFFFUL needs parentheses */
if ((unit > 0x10FFFFUL) || ((unit >= 0xD800UL) && (unit <= 0xDFFFUL)))
if (count > (SIZE_MAX / size))
length = (end != NULL) ? (size_t)(end - text) : strlen(text);
return (c >= '0') && (c <= '9');
```

### Breaking long expressions

When an expression does not fit on one line, break it after a binary operator, not before it: the operator (`&&`, `||`, `+`, `?`, `:` and the others) stays at the end of the line. Align each continuation line with the start of the operand it continues: inside the opening parenthesis of the condition or call, or after `return `.

```c
if (((arity == ARGX_ARITY_REQUIRED) || (arity == ARGX_ARITY_ONE_OR_MORE)) &&
    (argx_result_positional_count(result, positional) == 0))

parts.type_name = argx_type_name((failure->positional != NULL) ?
                                 argx_positional_type(failure->positional) :
                                 argx_option_type(failure->option));

return add_subcommand(sweep, root, "build", &tree->build) &&
       add_subcommand(sweep, root, "run", &tree->run);
```

### Allocating memory

The library code allocates and frees memory with the internal memory functions (`src/alloc_internal.h`). This makes every allocation go through the allocator that the user set with `argx_set_allocator()`.

| Internal memory function | Corresponding standard function |
| --- | --- |
| `void *argx_malloc(size_t size)` | `malloc` |
| `void *argx_calloc(size_t count, size_t size)` | `calloc` |
| `void *argx_realloc(void *ptr, size_t size)` | `realloc` |
| `void argx_free(void *ptr)` | `free` |

- **Do not use the standard allocation functions directly.** Only `src/alloc.c` (the default allocator) may write the standard allocation functions. To duplicate a string, use `argx_strdup` (`src/str.h`).
- **A size of 0 returns `NULL` without calling the user's function.** `argx_malloc(0)`, `argx_calloc` with a count or size of 0, and `argx_calloc` whose product does not fit in `size_t` return `NULL` without calling the allocator. A caller whose size can be 0 checks the size first and does not treat this `NULL` as a failure.
- **`NULL` for a request whose size is not 0 is an allocation failure.** The function that receives it frees what it allocated along the way and returns `ARGX_ERROR_OUT_OF_MEMORY` upward ([Return values of internal functions](#return-values-of-internal-functions)). Do not swallow it. Do not write `errno`, print anything or abort.
- **When `argx_realloc` fails, the original memory stays valid.** Check the return value before you replace the original pointer. If `ptr` is `NULL`, it is the same as `argx_malloc(size)`; if `ptr` is not `NULL` and `size` is 0, it is the same as `argx_free(ptr)` and returns `NULL`.
- **When you compute a size as a product, the caller checks for overflow.** `argx_realloc` and `argx_malloc` do not check products. If it overflows, return `ARGX_ERROR_OUT_OF_MEMORY` without calling the allocator.
- `argx_free(NULL)` does nothing.

```c
/*
 * src/example.h declares:
 *
 *     argx_error_t argx_example_resize(int **values, size_t count);
 */
#include "example.h"

#include "alloc_internal.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

argx_error_t argx_example_resize(int **values, size_t count)
{
    int *resized;

    assert(values != NULL);
    assert(count > 0);

    /* count * sizeof(int) would not fit in a size_t. */
    if (count > SIZE_MAX / sizeof(int))
    {
        return ARGX_ERROR_OUT_OF_MEMORY;
    }

    resized = argx_realloc(*values, count * sizeof(int));
    if (resized == NULL)
    {
        /* *values is still valid and unchanged. */
        return ARGX_ERROR_OUT_OF_MEMORY;
    }

    *values = resized;
    return ARGX_ERROR_NONE;
}
```

#### The allocation check `argx.allocations`

The check `argx.allocations` (`cmake/ArgXCheckAllocations.cmake`) makes sure the standard allocation functions are not used directly. `ctest` runs it in both the static and the shared configuration. It needs no build and can also be run on its own.

```sh
cmake -P cmake/ArgXCheckAllocations.cmake
```

- It covers every `*.c` and `*.h` in `src/` and its subdirectories. Only `src/alloc.c` is excluded (`src/alloc_internal.h` is covered). When you add a source, the check does not need to change. To check a different directory, pass `-DARGX_SOURCE_DIR=<directory>` before `-P`.
- It fails when any of the following names appears as an identifier of its own, whether in a call, used as a value, or on a preprocessor line.

  `malloc`, `calloc`, `realloc`, `free`, `aligned_alloc`, `strdup`, `strndup`, `wcsdup`, `asprintf`, `vasprintf`

- Comments, string literals and character literals are not checked. Other identifiers that merely contain one of the names, such as `argx_malloc`, `malloc_fn` and `freed`, are not detected.
- On failure, it lists every file name, line number and name. It also fails when there is no file to check. It does not change any file.
- **Limits of the check.** The check only reads names; it does not preprocess. It does not detect a name built by a macro with `##`, or a name split across lines with `\` at the end of a line. Do not write code or macros in `src/` that call a standard allocation function in such a way.

#### Global state

The only mutable global state the library has is the allocator that is set (`current_allocator` in `src/alloc.c`). Only `argx_set_allocator()` writes it; the internal memory functions only read it.

- Do not write any other mutable `static` variables or file-scope variables in `src/`. The same goes for `static` variables inside functions.
- Tables that do not change may be written as `static const`.
- Keep state in objects that the user creates and destroys. This rule is what lets different objects be used from different threads ([rule 8 of Ownership and Lifetime](../references/ownership.md#8-threads)).
- This is not checked automatically. Check it in review.

### Applying the ownership rules

Every public function follows the rules in [Ownership and Lifetime](../references/ownership.md); do not decide ownership, lifetime or how failures are returned on your own for a new function. This section shows how the library code applies those rules.

#### Forms that public functions do not take

Following [rule 6](../references/ownership.md#6-whoever-allocates-frees), do not write public functions of the following forms.

| Form not to write | Form to write instead |
| --- | --- |
| Returns an allocated string and has the user `free` it | Return a string owned by an object ([rule 3](../references/ownership.md#3-strings-and-objects-the-library-returns)) |
| Takes a buffer allocated by the user and frees it in the library | Duplicate it ([rule 2](../references/ownership.md#2-strings-you-pass)). The user frees the original buffer |

#### Returning failures

The following example shows how a function is written to follow [rule 7](../references/ownership.md#7-how-failures-are-returned). The names are placeholders; the memory functions and `argx_strdup` are real.

```c
/*
 * Placeholder public header. It includes <argx/error.h> and has
 * typedef struct argx_example argx_example_t; and the declarations of the two functions below.
 */
#include <argx/example.h>

#include "alloc_internal.h"
#include "str.h"

#include <assert.h>
#include <stddef.h>

struct argx_example
{
    char *name;
};

argx_error_t argx_example_create(const char *name, argx_example_t **out)
{
    argx_example_t *example;

    assert(name != NULL);   /* Precondition. Not an error code. */
    assert(out != NULL);

    *out = NULL;            /* On failure, the output is NULL. */

    example = argx_calloc(1, sizeof(*example));
    if (example == NULL)
    {
        return ARGX_ERROR_OUT_OF_MEMORY;
    }

    example->name = argx_strdup(name);   /* Rule 2: duplicate it. */
    if (example->name == NULL)
    {
        argx_free(example);              /* Free what this call allocated. */
        return ARGX_ERROR_OUT_OF_MEMORY;
    }

    *out = example;
    return ARGX_ERROR_NONE;
}

void argx_example_destroy(argx_example_t *example)
{
    if (example == NULL) {  /* Rule 4: NULL does nothing. */
        return;
    }
    argx_free(example->name);
    argx_free(example);
}
```

- Handle `NULL` from the internal memory functions (`argx_malloc`, `argx_calloc`, `argx_realloc`) and `argx_strdup` as [Allocating memory](#allocating-memory) and [Return values of internal functions](#return-values-of-internal-functions) say: turn it into `ARGX_ERROR_OUT_OF_MEMORY` and return it upward without swallowing it, and check the result of `argx_realloc` before you replace the original pointer.
- Write a test that makes each allocation fail in turn to check these rules. Use the test allocator (`tests/support/test_allocator.h`; see [The failure of every allocation in turn](procedures.md#the-failure-of-every-allocation-in-turn)).

#### How the internal utilities follow the rules

| Function | How it follows the rules |
| --- | --- |
| `argx_malloc`, `argx_calloc`, `argx_realloc`, `argx_free` (`src/alloc_internal.h`) | They call only the allocator that is set, so memory they allocate is freed by the same allocator ([rules 5](../references/ownership.md#5-allocator) and [6](../references/ownership.md#6-whoever-allocates-frees)). `NULL` for `argx_free` does nothing. When `argx_realloc` fails, the original memory stays valid ([rule 7](../references/ownership.md#7-how-failures-are-returned)). They do not write `errno`, print anything or abort |
| `char *argx_strdup(const char *str)` (`src/str.h`) | The caller owns the duplicate and frees it with `argx_free`. On failure it returns `NULL` and leaves nothing allocated ([rule 7](../references/ownership.md#7-how-failures-are-returned)). `NULL` for `str` is a precondition violation, checked with `assert` ([rule 4](../references/ownership.md#4-null)) |
| `argx_vec_init`, `argx_vec_push`, `argx_vec_at`, `argx_vec_destroy` (`src/vec.h`) | `argx_error_t argx_vec_push(argx_vec_t *vec, const void *item)` returns `ARGX_ERROR_OUT_OF_MEMORY` on failure, and `count`, `capacity` and the existing items stay as they were before the call ([rule 7](../references/ownership.md#7-how-failures-are-returned)). `argx_vec_destroy` frees all the memory of the array. It does not free what the items point to; the caller owns that ([rule 6](../references/ownership.md#6-whoever-allocates-frees)). It may be called twice. The pointer that `argx_vec_at` returns is valid until the next `argx_vec_push` or `argx_vec_destroy` (an example of the "shorter period" of [rule 3](../references/ownership.md#3-strings-and-objects-the-library-returns), stated in the comment). `NULL` for `vec` and `item`, and an index out of range, are precondition violations, checked with `assert` ([rule 4](../references/ownership.md#4-null)) |

There are two differences from the form of public functions. Both apply only to internal functions.

- `argx_malloc`, `argx_calloc`, `argx_realloc` and `argx_strdup` return failure as `NULL`, not as a `argx_error_t`. This is because internal functions that pair with a standard function take the same form as the standard. "Returns `argx_error_t`" in [rule 7](../references/ownership.md#7-how-failures-are-returned) is a rule for public functions.
- `argx_vec_t` is not an object created with `create`. The caller initializes a structure it holds with `argx_vec_init` and makes it empty again with `argx_vec_destroy`. So `vec` for `argx_vec_destroy` cannot be `NULL` (a precondition). The form of [rule 1](../references/ownership.md#1-creation-and-destruction) and "`NULL` for `destroy` does nothing" of [rule 4](../references/ownership.md#4-null) are rules for objects created with `create`.

The only global state these functions use is the allocator that is set ([Global state](#global-state)).

### Checking preconditions

Failures are divided into two kinds. The detailed explanation of the division is in [Ownership and Lifetime](../references/ownership.md#runtime-failures-and-programming-errors).

| Kind | Example | How to write it |
| --- | --- | --- |
| Runtime failure (can happen even in a correct program) | Failure to allocate memory | Return it as a `argx_error_t` |
| Programming error (always happens until the calling code is fixed) | `NULL` for a required argument, an allocator with a missing function, an index out of range | Make it a precondition of the function and check it with `assert`. Do not assign an error code |

- **Write the checks at the entry of the function with the standard `assert` (`<assert.h>`).** The same applies to public and internal functions. Do not create your own macros or a mechanism that replaces the behavior. If you write one `assert` per condition, the output shows which condition was violated.

  ```c
  #include "vec.h"

  #include <assert.h>
  #include <stddef.h>

  void *argx_vec_at(const argx_vec_t *vec, size_t index)
  {
      assert(vec != NULL);
      assert(index < vec->count);

      return (char *)vec->items + index * vec->item_size;
  }
  ```

- **Do not write side effects in the expression.** Even when you want to check the result of a function call, do not put the call inside `assert`. The behavior of a correct call must not depend on whether `assert` is there.
- **Debug configurations abort; Release configurations do not check.** In a build where `NDEBUG` is not defined (the CMake Debug configuration), a violation makes `assert` write the condition, the file name and the line number to standard error and abort the process. In a build where `NDEBUG` is defined (the Release, RelWithDebInfo and MinSizeRel configurations), `assert` does nothing, and the behavior when a precondition is broken is undefined.
- **Write the preconditions in comments.** Write them as `Preconditions:` in the comment of the function: in the public header for a public function, in the internal header for an internal function. Write both the ones that `assert` checks and the ones that cannot be checked (such as setting the allocator while no object exists), and say which ones are checked (`src/vec.h` and `include/argx/alloc.h` are examples).
- **Make sure the Release configuration also builds without warnings.** Under `NDEBUG` the expression of `assert` disappears, so a variable used only inside `assert` produces an "unused" warning, and the build stops in a configuration where warnings are errors. Do not create variables only for `assert`; write the expression directly in `assert`.

  ```sh
  cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
  cmake --build build-release
  ctest --test-dir build-release --output-on-failure
  ```

- Aborting on a violation is not checked by the automated tests. Do not make calls that break preconditions from tests.
- The convention that passing `NULL` to the `destroy` of a created object does nothing (rule 4 of [Ownership and Lifetime](../references/ownership.md)) is about objects created with `create`. For types where the caller initializes and uses a structure it holds (`argx_vec_t`), `NULL` for `argx_vec_destroy` is a precondition violation.

### Return values of internal functions

| Internal function | Return value | Example |
| --- | --- | --- |
| Those that pair with a standard function | Return in the same form as the standard. Failure is `NULL` | `argx_malloc`, `argx_calloc`, `argx_realloc`, `argx_strdup` |
| Other functions that can fail | Return `argx_error_t` | `argx_vec_push` |
| Those that cannot fail | The natural type, or `void` | `argx_vec_init`, `argx_vec_at`, `argx_vec_destroy` |

- A function that calls a function that returns failure as `NULL` turns it into `ARGX_ERROR_OUT_OF_MEMORY` and returns it upward.
- Write the test for success as `error == ARGX_ERROR_NONE`.
- Do not report failure by any means other than the return value (`errno`, output, global variables).

## Writing tests

Tests are written with [Naos](https://github.com/vabelorg/naos) v0.4.0. Test code is also C99 without extensions, compiled with the warnings `-Wall -Wextra -Wpedantic` (`/W4` with MSVC), and warnings are errors by default.

- **Have exactly one entry point.** The sources of one executable (one call of `argx_add_test()`) have exactly one `NAOS_MAIN`. Naos does not provide `main()`.
- **Put at least one test in a suite.** A `NAOS_SUITE` with no test is a compile error.
- **Every test runs at least one assertion.** Naos fails a test that ran no assertion, with `no assertion ran`. Do not write assertions only inside a conditional branch or inside a loop that may have no items. In a test that only checks that something can run, write `NAOS_PASS()`.
- **Avoid the known warnings of Naos v0.4.0.** The following uses produce warnings, and the build stops in a configuration where warnings are errors.
  - Do not pass memory that nothing has been written to yet (such as a pointer right after `malloc`) to `NAOS_CHECK_NOT_NULL` / `NAOS_REQUIRE_NOT_NULL`. GCC produces `-Wmaybe-uninitialized`. Write to the memory first, or use `NAOS_CHECK_TRUE(pointer != NULL)` / `NAOS_REQUIRE_TRUE(pointer != NULL)`.
  - Do not pass function pointers to `NAOS_CHECK_NULL` / `NAOS_CHECK_NOT_NULL` / `NAOS_REQUIRE_NULL` / `NAOS_REQUIRE_NOT_NULL`. `-Wpedantic` warns. Write a comparison such as `NAOS_CHECK_TRUE(function != NULL)`.
- **How to write the macros.** Do not write a comma after the last element of `NAOS_SUITE` and `NAOS_MAIN`. Do not put `;` after `NAOS_TEST(...) { ... }`, `NAOS_SUITE(...)` or `NAOS_MAIN(...)` (`-Wpedantic` warns).
- Use the assertion that matches the type: `NAOS_CHECK_UINT_EQ` for unsigned values, `NAOS_CHECK_INT_EQ` for signed values, and so on.
- **Taking the address of a standard function with MSVC.** With the MSVC DLL runtime, standard functions (such as `malloc`) are imported. Their addresses cannot be written in static initializers, and with `/W4` even local initializers and assignments produce warnings (C4204, C4232). Write it in one of the following ways.
  - Wrap the standard function in a `static` function within the file, and pass that function (such as `wrap_malloc` in `tests/support/wrap_allocator.c`; in the library, `default_malloc` and others in `src/alloc.c`).
  - In a test that checks that the standard functions can be listed as they are (`tests/public/alloc_test.c`), write the following at the top of the file.

    ```c
    #ifdef _MSC_VER
    #pragma warning(disable : 4204 4232)
    #endif
    ```

- **Do not write `const` objects without an initializer.** It is C4132 with MSVC `/W4`. Add `= { 0 }`.
- **After `NAOS_FAIL` and `NAOS_SKIP`, write no statement other than `break;`.** Neither returns. Writing `return` or another statement after them is C4702 (unreachable code) with MSVC `/W4`. Even in the `default:` of a `switch` in a function that returns a value, write only `NAOS_FAIL(...)`.
- **Write bytes outside the ASCII range as character constants.** `(char)0x80` is C4310 (truncation of a constant) with MSVC `/W4`. Write `'\x80'`.
- **Do not actually pass huge sizes to the standard allocation functions.** ASan and valgrind end abnormally. To check how large requests are handled, set with `argx_set_allocator()` an allocator that only records its arguments and returns `NULL`.
- **Check both sides of an overflow check.** Check not only the side that overflows (it fails without calling the allocator) but also the side that just fits (it reaches the allocator). For the side that just fits, make the first request fail with the test allocator (`argx_test_allocator_fail_at(1)`) and check that the number of requests is 1, which shows that the request reached the allocator. This way you never pass a huge size to the standard allocation functions.
- **A test that set an allocator sets it back to the default before it ends.** The setting is shared by all the tests of one executable. Call `argx_set_allocator(NULL)`, or `argx_test_allocator_uninstall()` for the test allocator ([Using the test allocator](procedures.md#using-the-test-allocator)).
- **Write the C examples in the documents so that they can be extracted and compiled as they are.** Include the public or internal headers that declare what they use. An example that uses placeholder names writes the contents of its placeholder header in a comment.
- After adding a test, build before you run `ctest`. An executable that has not been built appears as `<name>.NOT_BUILT`, which fails.

### Rules of `argx_add_test()`

```cmake
argx_add_test(<name> SOURCES <source>... [INTERNAL])
```

- Write `<name>` with letters, digits and underscores only, and make it unique across public and internal tests. The executable is `argx_test_<name>`. The `<name>` of a test of internal functions is the name of the internal header (`<name>_internal`, or `<name>` if there is no public header of the same name). Make sure it does not collide with the `<name>` of a test of the public API.
- Without `INTERNAL`, it is a test of the public API. There is no `PUBLIC` keyword.
- Write each `<source>` as a path relative to `tests/`. The basic form is one file per executable.
- Tests are registered with CTest under the name `<name>.<suite>.<test>` (for example `version.version.number_matches_the_headers`) and with the label `public` or `internal`. You can select them with `ctest -L public` and so on.
- If the arguments are wrong, configuration fails and shows the mistake.

## Version notes

- The only source of the version is the four macros `ARGX_VERSION_MAJOR`, `ARGX_VERSION_MINOR`, `ARGX_VERSION_PATCH` and `ARGX_VERSION_PRERELEASE` in `include/argx/version.h`. The build reads this header as text, so write each of the four macros on its own line. Write the three numeric macros in the form `#define ARGX_VERSION_<PART> <integer>`, as decimal 0 to 255 without leading zeros. Do not write the version numbers anywhere else.
- Write `ARGX_VERSION_PRERELEASE` as a single string literal in the form `#define ARGX_VERSION_PRERELEASE "<identifier>"`. Its contents are SemVer 2.0.0 pre-release identifiers without the leading hyphen (for example `"rc.1"`). Separate identifiers with `.` and write each with the characters `[0-9A-Za-z-]`. Numeric identifiers have no leading zeros. For a final release, make it the empty string `""`. If the form is wrong, configuration fails.
- Do not bump the version by hand. The release workflow **Release PR** rewrites `include/argx/version.h` and `CHANGELOG.md` together and opens a release PR. The procedure is in the [release guide](releasing.md), and the policy for bumping the version is in the [versioning policy](../references/versioning.md).
- `ARGX_VERSION_NUMBER` and `ARGX_MAKE_VERSION()` contain an `(unsigned long)` cast, so they cannot be used in `#if`. In `#if`, compare `ARGX_VERSION_MAJOR` and the others one by one.

  ```c
  #if ARGX_VERSION_MAJOR > 0 || ARGX_VERSION_MINOR >= 2
  ```

- While the major version is 0, `find_package` treats "the same minor version" as compatible. `find_package(ArgX 0)` and ranges (`0.1...1`) do not find the package. If you give a version, write it down to the minor version, as in `find_package(ArgX 0.1)`. Once the major version is 1 or more, "the same major version" is compatible.
- The package treats the numeric part and the full version separately. Only the numeric part is passed to `project()`, so `PROJECT_VERSION`, the compatibility check of `find_package` and the SOVERSION are numeric only.
  - `ArgX_VERSION`, set by `find_package(ArgX)`, is the numeric part (for example `0.2.0`).
  - `ArgX_VERSION_FULL` is the full version (for example `0.2.0-rc.1`). `ArgX_VERSION_PRERELEASE` is the pre-release identifier (for example `rc.1`). For a final release, `ArgX_VERSION_FULL` is the same as `ArgX_VERSION`, and `ArgX_VERSION_PRERELEASE` is empty.
  - `Version` in the pkg-config file `argx.pc` is the full version.
  - `find_package` compares only the numeric part, so `0.2.0-rc.1` is also found by `find_package(ArgX 0.2)`. pkg-config considers `0.2.0-rc.1` newer than `0.2.0`. See the [versioning policy](../references/versioning.md) for details.
