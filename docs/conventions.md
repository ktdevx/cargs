# Development Conventions

This document collects the conventions and procedures to follow when you add public functions, headers, sources and tests to CLArgs. If you add a new feature, do not choose names or locations on your own; follow this document.

Before you design a public function or an object that owns memory, read [Ownership and Lifetime](ownership.md) first. The pairs of creation and destruction, how strings are held, how `NULL` is handled and how failures are returned are decided there. This document covers the rules and procedures for writing those decisions in code and tests.

The entry point for contributors is [CONTRIBUTING.md](../CONTRIBUTING.md) at the root of the repository. It describes how to report issues, build and test, and submit a pull request, and points to this document for the conventions.

Contents:

- [Naming conventions](#naming-conventions)
- [Directory layout](#directory-layout)
- [Procedures](#procedures)
- [Implementation rules](#implementation-rules)
- [Writing tests](#writing-tests)
- [Verifying a change](#verifying-a-change)
- [Version notes](#version-notes)

## Naming conventions

| What | Convention | Example |
|---|---|---|
| Public functions and types | `clargs_` + lowercase snake case. Type names end with `_t` | `clargs_version`, `clargs_example_t` |
| Public macros and enumeration constants | `CLARGS_` + uppercase snake case | `CLARGS_VERSION_MAJOR` |
| Internal functions and types (used across files) | The same `clargs_` + lowercase snake case as public ones. Type names end with `_t`. Do not add `CLARGS_API`; declare them in an internal header in `src/` | `clargs_version_pack` |
| Structure and enumeration tags | The type name without `_t`. Do not add `_t` | `struct clargs_example`, `enum clargs_example_kind` |
| Function pointer types and the fields that hold them | The type is `clargs_<name>_fn_t`, the field is `<name>_fn` | `clargs_example_fn_t`, `example_fn` |
| Internal macros | The same `CLARGS_` + uppercase snake case as public ones. Define them in an internal header in `src/` | `CLARGS_VERSION_INTERNAL_H` (include guard) |
| Functions used only within one file | Add `static`. No prefix is needed | — |
| Public headers | `include/clargs/<name>.h` | `include/clargs/version.h` |
| Sources | `src/<name>.c` | `src/version.c` |
| Internal headers | `src/<name>_internal.h` for a module that has a public header of the same name (`include/clargs/<name>.h`), `src/<name>.h` for a module that does not | `src/version_internal.h` (`include/clargs/version.h` exists) |
| Tests of the public API | `tests/public/<name>_test.c` | `tests/public/version_test.c` |
| Tests of internal functions | `tests/internal/<internal-header-name>_test.c` | `tests/internal/version_internal_test.c` |
| CMake options, functions and modules | `CLARGS_<name>`, `clargs_<name>()`, `cmake/Clargs<name>.cmake` | `CLARGS_BUILD_TESTS`, `clargs_add_test()`, `cmake/ClargsTesting.cmake` |

- Public and internal names are not told apart by prefix. Whether a function is public is decided by whether it is declared with `CLARGS_API` in a header under `include/clargs/`. `CLARGS_API` (`include/clargs/export.h`) is the only mark that makes a function public. Do not make functions public in any other way (writing compiler attributes directly, adding a `.def` file and so on).
- Do not use `cargs` in any public name (artifacts, headers, identifiers, packages). The project name is `CLArgs`; the library and the include path are `clargs`.
- Do not have public variables. Only functions, types and macros are public. If you put `CLARGS_API` on a variable, the export check (`clargs.exports`) fails.
- Include guards are `CLARGS_<file name in uppercase>_H` (`CLARGS_VERSION_H`, `CLARGS_VERSION_INTERNAL_H`).
- Type names (the names given with `typedef`) end with `_t`, whether they are public, internal or test support. Structure and enumeration tags do not take `_t` (`typedef struct clargs_example clargs_example_t;`).
- Function pointer types are `clargs_<name>_fn_t`, and the structure fields that hold them are `<name>_fn`.
- An internal header takes `_internal` to tell it apart from the public header of the same name. In a module without a public header, do not add it. The file name of a test of internal functions and the `<name>` of `clargs_add_test()` follow the name of the internal header.
- Functions that correspond to standard functions match the standard in name and arguments (the function that corresponds to `malloc` is `clargs_malloc`, the one that corresponds to `strdup` is `clargs_strdup`; argument names such as `ptr` and `str` also match the standard).
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
    ├── release-pr.yml       Release PR: started by hand with a version; rewrites include/clargs/version.h and CHANGELOG.md and opens a release pull request
    └── release-publish.yml  Release: when a release pull request is merged, creates the tag and the GitHub Release
CHANGELOG.md          The record of changes in each release (Keep a Changelog)
CMakeLists.txt        Assembly only: reads the version, project(), options, adds each directory
CODE_OF_CONDUCT.md    The code of conduct for everyone who takes part in the project
CONTRIBUTING.md       The entry point for contributors
LICENSE               The MIT License
README.md             The entry point of the project for users: overview, building, links to the documents
SECURITY.md           Supported versions and how to report a vulnerability privately
cmake/                Build support files (CMake modules, scripts, templates)
├── CLArgsConfig.cmake.in       The package file for find_package(CLArgs)
├── ClargsCTestDefaults.cmake   The configuration ctest uses when none is given
├── ClargsCheckAllocations.cmake The allocation check clargs.allocations
├── ClargsCheckExports.cmake    The export check clargs.exports
├── ClargsInstall.cmake         The install rules and the package files
├── ClargsTargetDefaults.cmake  The compile settings shared by the targets
├── ClargsTesting.cmake         Fetches Naos and defines clargs_add_test()
├── ClargsVersion.cmake         Reads the version from include/clargs/version.h
└── clargs.pc.in                The pkg-config file
include/
└── clargs/           Public headers. All of them go here. They are installed
    ├── clargs.h      The only entry point users include. It only includes the other public headers
    ├── alloc.h       The allocator type and the declaration of clargs_set_allocator()
    ├── error.h       The list of error codes CLARGS_ERROR_LIST, clargs_error_t, the declaration of clargs_error_message()
    ├── export.h      CLARGS_API
    ├── help.h        Help: the write function type, the help options clargs_help_options_t, declarations for generating help and the usage line
    ├── parse.h       Parsing: declarations of clargs_parse(), clargs_parse_wide(), reading and destroying the result clargs_result_t, parse error information
    ├── schema.h      Schemas: declarations for defining commands, options and positionals, attributes, validation, reading
    ├── type.h        Value types: declarations of clargs_type_t, the built-in value types, creation, destruction, reading, conversion
    └── version.h     The version macros and the declaration of clargs_version()
src/                  Sources and internal headers. Not installed
├── CMakeLists.txt    The source list CLARGS_SOURCES, the targets clargs and clargs_internal
├── <name>.c
└── <name>_internal.h  Internal header. <name>.h if there is no public header of the same name
tests/
├── CMakeLists.txt    The clargs_add_test() calls, the registration of the allocation check and the export check, and the registration of the release tests
├── public/           Tests of the public API. Linked to CLArgs::clargs
├── internal/         Tests of internal functions. Linked to clargs_internal
├── support/          Test support sources and headers (the test allocator). Not part of the library and not installed
├── consumer/         Checks of using the library from a consuming project. Not part of the main build
└── release/          Tests of the release scripts and modules, run with cmake -P. Need no build
    ├── ReleaseTestHelpers.cmake  Checks and fixtures shared by the release tests
    ├── <name>_test.cmake         One test per file
    └── fixtures/                 Input and expected files (changelogs, a version header)
tools/                Scripts that are not part of the build
├── ci/
│   └── changes.cmake Decides whether a change touches documentation only, so that CI can skip the builds and tests
└── release/          The release scripts and the modules they include, run with cmake -P by the release workflows
    ├── prepare.cmake             Checks a release and rewrites the version header and the changelog (Release PR)
    ├── publish.cmake             Checks a merged release commit and makes the release notes (Release)
    ├── ClargsChangelog.cmake     Reads and updates CHANGELOG.md
    ├── ClargsReadBytes.cmake     Reads a text file byte for byte, line endings included
    ├── ClargsSemver.cmake        Checks a version and compares versions by SemVer precedence
    └── ClargsVersionHeader.cmake Rewrites the version in a version header
docs/                 Documents
├── README.md         The index of the documents (for users and developers)
├── conventions.md    This document (for developers)
├── help.md           Using help (for users): examples of generating help and the usage line, text rules, options, output, limits, preconditions
├── ownership.md      Ownership and lifetime rules (for users): who allocates and frees, how long things stay valid, how failures are returned
├── parse.md          Using parsing (for users): an example from parsing to destruction, the list of argument forms, parse errors, lifetime rules, preconditions
├── releasing.md      The release procedure (for developers who make releases): releasing a version, the repository settings it needs, recovering from failures
├── schema.md         Using schemas (for users): an example from definition through validation to destruction, the value type contract, the order of destruction
└── versioning.md     The versioning policy (for users and contributors): how versions are numbered, what a new version may change, checking the version
```

- [CONTRIBUTING.md](../CONTRIBUTING.md) is the entry point for contributors, and [docs/README.md](README.md) is the index of the documents.
- [CHANGELOG.md](../CHANGELOG.md) records the changes of each release. The [versioning policy](versioning.md) and the [release guide](releasing.md) describe the versions and the release procedure.
- [`tools/release/`](../tools/release/) holds the release scripts and modules, and [`tools/ci/`](../tools/ci/) holds [`changes.cmake`](../tools/ci/changes.cmake), which CI uses to decide whether a change touches documentation only. The workflows [`ci.yml`](../.github/workflows/ci.yml), [`release-pr.yml`](../.github/workflows/release-pr.yml) and [`release-publish.yml`](../.github/workflows/release-publish.yml) in [`.github/workflows/`](../.github/workflows/) run them.
- The tests in [`tests/release/`](../tests/release/) are registered by `tests/CMakeLists.txt` with `clargs_add_release_test(<name>)`, under the name `release.<name>` and with the label `release`. They need no build, so they are registered in both the static and the shared configuration. You can select them with `ctest -L release`.
- Users write only `#include <clargs/clargs.h>`. Public headers also include each other in the form `<clargs/<name>.h>`.
- The internal headers in `src/` are included only by the sources in `src/` and by tests of internal functions, in the form `"<name>_internal.h"` (`"<name>.h"` if there is no public header of the same name). Do not include internal headers from public headers.
- Of the scripts in `cmake/`, `ClargsCheckAllocations.cmake` (the allocation check `clargs.allocations`) and `ClargsCheckExports.cmake` (the export check `clargs.exports`) can also be run on their own with `cmake -P` ([Verifying a change](#verifying-a-change)).
- A test that uses files from `tests/support/` adds them to the `SOURCES` of `clargs_add_test()` ([Using the test allocator](#using-the-test-allocator)).
- Create build directories outside the repository, or as `build/` or `build-*/` at the root (they are excluded from version control).

## Procedures

| What you want to do | Procedure |
|---|---|
| Add a public function | Put `CLARGS_API` at the start of the declaration in the public header. Write `CLARGS_API`, the return type, the function name and `(` in this order. Write it inside the `extern "C"` guard. Write the implementation in a source in `src/` |
| Add a public header | Put it in `include/clargs/` and add one include line to `clargs.h` |
| Add a source file | Put it in `src/` and add one line to `CLARGS_SOURCES` in `src/CMakeLists.txt` |
| Add an internal function | Declare it without `CLARGS_API` in an internal header (`src/<name>_internal.h`, or `src/<name>.h` if there is no public header of the same name) and implement it in a source in `src/` |
| Add a test of the public API | Put it in `tests/public/` and add `clargs_add_test(<name> SOURCES public/<file>)` to `tests/CMakeLists.txt` |
| Add a test of an internal function | Put it in `tests/internal/<internal-header-name>_test.c` and add `clargs_add_test(<name> SOURCES internal/<file> INTERNAL)` to `tests/CMakeLists.txt` |
| Add a CMake module | Put it in `cmake/Clargs<name>.cmake` and include it in the form `include("${CMAKE_CURRENT_LIST_DIR}/cmake/Clargs<name>.cmake")` |
| Add an error code | Add one line `X(name, description)` at the end of `CLARGS_ERROR_LIST` in `include/clargs/error.h`. Change nothing else ([Adding an error code](#adding-an-error-code)) |
| Use the test allocator | Add `support/test_allocator.c` to the `SOURCES` of `clargs_add_test()`, and include `"../support/test_allocator.h"` in the test ([Using the test allocator](#using-the-test-allocator)) |

None of these procedures needs any change to the CMake files other than the one line above. The install rules cover every `*.h` in `include/clargs/`, and the export check reads the expected names from the public headers.

Below, `<name>` is written as `example`.

### Adding a public function

Write the declaration inside the `extern "C"` guard of the public header (if you create a new one, see [Adding a public header](#adding-a-public-header)).

```c
CLARGS_API int clargs_example_add(int left, int right);
```

- Write `CLARGS_API`, the return type, the function name and `(` in this order. The export check reads the last identifier between `CLARGS_API` and the first `(` as the function name.
- A declaration that returns a function pointer directly (`CLARGS_API int (*clargs_example_get(void))(int);`) does not let the check read the name. Give the type a name with `typedef` first, and use that.

  ```c
  typedef int (*clargs_example_fn_t)(int value);

  CLARGS_API clargs_example_fn_t clargs_example_get(void);
  ```

- Do not write `CLARGS_API` on the definition (in the source in `src/`). The source includes the public header that declares the function. If it does not, the mark does not reach the definition and the function is not exported from the shared library (with GCC and Clang, the build stops before that with a `-Wmissing-prototypes` warning).

  ```c
  #include <clargs/example.h>

  int clargs_example_add(int left, int right)
  {
      return left + right;
  }
  ```

- For a public function, add a test of the public API in `tests/public/`.
- If you remove `CLARGS_API` from a public function, the function is no longer public. The export check is based on the mark, so it does not detect the removal itself (a test of the public API that calls the function fails to link in the shared library configuration). Do not write a function declaration without the mark in a public header.

### Adding a public header

Create `include/clargs/example.h` in the following form.

```c
/*
 * CLArgs - <what this header contains>.
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

Add one include line to `include/clargs/clargs.h`.

```c
#include <clargs/example.h>
```

Public headers are C99 without extensions, and each compiles when included on its own (it includes the headers it needs itself).

### Adding a source file

Create `src/example.c` and add one line to `CLARGS_SOURCES` in `src/CMakeLists.txt`.

```cmake
set(CLARGS_SOURCES
    example.c
    version.c
)
```

The library sources are compiled as C99 without extensions, with the warnings `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes -Wmissing-prototypes` (`/W4` with MSVC). When `CLARGS_WARNINGS_AS_ERRORS` is ON (the default at the top level), warnings are errors. Pay particular attention to the following.

- A function that is not `static` needs a declaration before its definition (`-Wmissing-prototypes`). The source that defines a public function includes its public header; the source that defines an internal function includes its internal header. Add `static` to functions used only within one file.
- Write `(void)` for a function without parameters (`-Wstrict-prototypes`).
- Write an explicit cast for an implicit conversion that can change the value (`-Wconversion`).
- The library depends on nothing but the C standard library. Do not link it to other libraries.

### Adding an internal function

Declare an internal function that is used across files in an internal header. Do not add `CLARGS_API`; no `extern "C"` guard is needed either.

The name of the internal header depends on whether there is a public header of the same name.

| Module | Internal header | Include guard |
|---|---|---|
| Has the public header `include/clargs/example.h` | `src/example_internal.h` | `CLARGS_EXAMPLE_INTERNAL_H` |
| Has no public header | `src/example.h` | `CLARGS_EXAMPLE_H` |

The example below is for the case where the public header `include/clargs/example.h` exists.

```c
/*
 * CLArgs - internal declarations for <contents>.
 */
#ifndef CLARGS_EXAMPLE_INTERNAL_H
#define CLARGS_EXAMPLE_INTERNAL_H

int clargs_example_twice(int value);

#endif /* CLARGS_EXAMPLE_INTERNAL_H */
```

The defining source writes `#include "example_internal.h"`. In a module without a public header, the only differences are the file name `src/example.h`, the include guard `CLARGS_EXAMPLE_H` and the include `#include "example.h"`; everything else is the same. Internal functions are not exported from the shared library, and internal headers are not installed. For an internal function, add a test of internal functions in `tests/internal/`.

### Adding a test of the public API

Create `tests/public/example_test.c`. Include only public headers.

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

Add one line to `tests/CMakeLists.txt`.

```cmake
clargs_add_test(example SOURCES public/example_test.c)
```

Tests of the public API link to `CLArgs::clargs`. That is, they run against the library in the form chosen by the configuration (static or shared). With a shared library, internal functions cannot be called.

### Adding a test of an internal function

Create a file whose name is the name of the internal header followed by `_test`. If the internal header is `src/example_internal.h`, the file is `tests/internal/example_internal_test.c`. Include the internal header in the form `"example_internal.h"`.

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

Add one line with `INTERNAL` to `tests/CMakeLists.txt`.

```cmake
clargs_add_test(example_internal SOURCES internal/example_internal_test.c INTERNAL)
```

`<name>` is the name of the internal header. For a module without a public header (the internal header is `src/example.h`), it looks like this.

```cmake
clargs_add_test(example SOURCES internal/example_test.c INTERNAL)
```

Tests of internal functions link to `clargs_internal`. In both the static and the shared configuration, the library sources are linked statically, so internal functions can be called.

### Adding an error code

There is only one place to change. Add one line `X(name, description)` at the **end** of `CLARGS_ERROR_LIST` in `include/clargs/error.h`. Add `\` at the end of the line that was last until then.

```c
#define CLARGS_ERROR_LIST(X) \
    X(CLARGS_ERROR_NONE, "success") \
    X(CLARGS_ERROR_OUT_OF_MEMORY, "out of memory") \
    X(CLARGS_ERROR_EXAMPLE, "example failure")
```

- **Add only at the end.** Do not write values. Values are given from 0 in the order of the list (`CLARGS_ERROR_NONE` is 0, `CLARGS_ERROR_OUT_OF_MEMORY` is 1). The values are public; a value once published is never changed and never reused for another meaning. So do not delete existing lines, do not reorder them, and do not add lines in the middle. Keep the line of an error code that is no longer used.
- **Change nothing else.** The enumeration `clargs_error_t`, `clargs_error_message()` (`src/error.c`) and the test (`tests/public/error_test.c`) are generated from this list. For the added code too, the test checks that the description matches the list, is not empty, and differs from the descriptions of the other codes and from `"unknown error code"`.
- **There is only one list.** Write every error code in this list. Do not create a separate type or range of numbers per feature.
- **Names** are `CLARGS_ERROR_<WHAT>`. Success is only `CLARGS_ERROR_NONE`.
- **Descriptions** are short English phrases that start with a lowercase letter (`"out of memory"`). Do not reuse the description of another code.
- **Only runtime failures become error codes.** Before you add one, check whether the failure can happen even in a correct program. A programming error of the calling program (such as `NULL` for a required argument) does not become an error code; it becomes a precondition ([Checking preconditions](#checking-preconditions)).
  - Errors in a schema definition that come from the contents of a string (a name that breaks the rules, a default value the value type cannot convert, and so on) and those that come from the relation between definitions (a name conflict, a combination of definitions that cannot be used together, and so on) are handled as runtime failures and become error codes (`CLARGS_ERROR_INVALID_NAME`, `CLARGS_ERROR_NAME_CONFLICT`, `CLARGS_ERROR_INVALID_DEFINITION`, `CLARGS_ERROR_INVALID_DEFAULT`, `CLARGS_ERROR_INVALID_VALUE`). The reasons are that strings can be built from data decided at run time, that only the library can match them against the rules and the existing definitions, and that `assert` does not check anything in the Release configuration.
  - A null pointer, an index out of range and a value that is not in the enumeration stay preconditions, even in the definition functions. They are decided only by the calling code and can be checked at the place of the call.
  - The detailed explanation of this division and its reasons is in [Ownership and Lifetime](ownership.md#runtime-failures-and-programming-errors).
- An error code represents only the kind of failure. Do not make it carry a position or a target.

### Using the test allocator

The test allocator (`tests/support/test_allocator.h`, `tests/support/test_allocator.c`) is an allocator for tests that can be set as the library's allocator. It fails the request you specify, and counts the requests, the blocks not yet freed and the contract violations. When you add a function that allocates memory, use it to check how allocation failures are handled and that nothing leaks.

To use it, add `support/test_allocator.c` to the `SOURCES` of `clargs_add_test()` in `tests/CMakeLists.txt`. Do not change any other CMake file.

```cmake
clargs_add_test(example SOURCES internal/example_test.c support/test_allocator.c INTERNAL)
```

In the test source (`tests/public/` or `tests/internal/`), include it in this form.

```c
#include "../support/test_allocator.h"
```

The test allocator depends only on `<clargs/clargs.h>` and the C standard library, so it can be used both in tests of the public API and in tests of internal functions.

| Function | What it does |
|---|---|
| `void clargs_test_allocator_install(void)` | Resets the counts (the same as `reset`) and sets the test allocator as the library's allocator |
| `void clargs_test_allocator_uninstall(void)` | Sets the library back to the default allocator. The counts are kept, so you can read them afterwards |
| `void clargs_test_allocator_reset(void)` | Sets the number of requests, the number of live blocks and the number of contract violations back to 0, and clears the request to fail |
| `void clargs_test_allocator_fail_at(size_t request)` | Fails the `request`-th request counted from the last `reset`. Counting starts at 1, and only that request fails. Passing 0 fails no request |
| `size_t clargs_test_allocator_requests(void)` | The number of requests since the last `reset`. Requests that were made to fail are counted too |
| `size_t clargs_test_allocator_live(void)` | The number of blocks not yet freed. It goes up by 1 on a successful allocation, down by 1 on a free, and does not change on a reallocation |
| `size_t clargs_test_allocator_violations(void)` | The number of contract violations since the last `reset` |
| `clargs_allocator_t clargs_test_allocator_get(void)` | Returns the four functions of the test allocator. Does not change the state. Use it when you pass them to `clargs_set_allocator()` yourself, or call the four functions directly |

- A **request** is a call of `malloc_fn`, `calloc_fn` or `realloc_fn`. The three are counted together. `free_fn` is not a request. Only calls that reach the allocator are counted, so something that does not call the user's function, such as `clargs_malloc(0)`, is not counted.
- A **contract violation** is a call that the library must not make (see "How the library calls the functions of the allocator" in `include/clargs/alloc.h`): a request of size 0 or count 0, a reallocation of `NULL`, a free of `NULL`. As long as the library code goes through the internal memory functions, it stays 0.
- A reallocation that was made to fail leaves the original block valid.
- **Pair `install` with `uninstall`.** The state is in `static` variables and is shared by all the tests of one executable. So is the allocator that is set. A test that called `install` destroys everything it created and then calls `uninstall` before it ends. If you forget `uninstall`, later tests keep using the test allocator.
- **Call `reset` while nothing is allocated.** If you call it while there are blocks not yet freed, the count of live blocks is wrong from then on.
- **Every test checks at its end that `live` and `violations` are 0.** The counts can still be read after `uninstall`.
- The test allocator is not synchronized.

#### The failure of every allocation in turn

A test of an operation that allocates memory makes the first allocation, the second allocation, … fail in turn, with the following flow. The test does not need to know how many times the operation allocates.

```
install
for n = 1, 2, …
    reset, fail_at(n)
    run the operation
    it succeeded and the number of requests is less than n → every allocation point has been made to fail. Destroy, and end the loop
    check the error code and what the failure must not change
    destroy what is left
    check that live and violations are 0
uninstall
```

The model is `fails_at_every_allocation_point` in `tests/internal/vec_test.c`. Write new tests by copying this form. The following example is a shortened version of the same flow; it checks an operation that adds 10 items to an empty array (it compiles as it is as a test of internal functions).

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

- If the operation succeeds while the number of requests is `n` or more, the operation swallowed the `n`-th failure. So the condition for ending the loop includes not only "it succeeded" but also "the number of requests is less than `n`". If the failure was swallowed, the following check for `CLARGS_ERROR_OUT_OF_MEMORY` fails.
- Give the loop a limit (`limit`) and check that it ended because the operation completed (`completed`). Even if there is a bug, the loop cannot run forever.
- What you check after a failure is what rule 7 of [Ownership and Lifetime](ownership.md) requires: an error code is returned, nothing allocated along the way is left, and existing objects are as they were before the call and can still be used and destroyed. `fails_at_every_allocation_point` also checks that a push succeeds again after the failure.
- For an operation that allocates a fixed number of times, also check that the number of failures equals that number (`failures` and `allocation_points` in the model).

### Rules of `clargs_add_test()`

```cmake
clargs_add_test(<name> SOURCES <source>... [INTERNAL])
```

- Write `<name>` with letters, digits and underscores only, and make it unique across public and internal tests. The executable is `clargs_test_<name>`. The `<name>` of a test of internal functions is the name of the internal header (`<name>_internal`, or `<name>` if there is no public header of the same name). Make sure it does not collide with the `<name>` of a test of the public API.
- Without `INTERNAL`, it is a test of the public API. There is no `PUBLIC` keyword.
- Write each `<source>` as a path relative to `tests/`. The basic form is one file per executable.
- Tests are registered with CTest under the name `<name>.<suite>.<test>` (for example `version.version.number_matches_the_headers`) and with the label `public` or `internal`. You can select them with `ctest -L public` and so on.
- If the arguments are wrong, configuration fails and shows the mistake.

## Implementation rules

These are the rules for writing the library sources (`src/`). Ownership, lifetime and how failures are returned themselves are in [Ownership and Lifetime](ownership.md).

### Allocating memory

The library code allocates and frees memory with the internal memory functions (`src/alloc_internal.h`). This makes every allocation go through the allocator that the user set with `clargs_set_allocator()`.

| Internal memory function | Corresponding standard function |
|---|---|
| `void *clargs_malloc(size_t size)` | `malloc` |
| `void *clargs_calloc(size_t count, size_t size)` | `calloc` |
| `void *clargs_realloc(void *ptr, size_t size)` | `realloc` |
| `void clargs_free(void *ptr)` | `free` |

- **Do not use the standard allocation functions directly.** Only `src/alloc.c` (the default allocator) may write the standard allocation functions. To duplicate a string, use `clargs_strdup` (`src/str.h`).
- **A size of 0 returns `NULL` without calling the user's function.** `clargs_malloc(0)`, `clargs_calloc` with a count or size of 0, and `clargs_calloc` whose product does not fit in `size_t` return `NULL` without calling the allocator. A caller whose size can be 0 checks the size first and does not treat this `NULL` as a failure.
- **`NULL` for a request whose size is not 0 is an allocation failure.** The function that receives it frees what it allocated along the way and returns `CLARGS_ERROR_OUT_OF_MEMORY` upward ([Return values of internal functions](#return-values-of-internal-functions)). Do not swallow it. Do not write `errno`, print anything or abort.
- **When `clargs_realloc` fails, the original memory stays valid.** Check the return value before you replace the original pointer. If `ptr` is `NULL`, it is the same as `clargs_malloc(size)`; if `ptr` is not `NULL` and `size` is 0, it is the same as `clargs_free(ptr)` and returns `NULL`.
- **When you compute a size as a product, the caller checks for overflow.** `clargs_realloc` and `clargs_malloc` do not check products. If it overflows, return `CLARGS_ERROR_OUT_OF_MEMORY` without calling the allocator.
- `clargs_free(NULL)` does nothing.

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

#### The allocation check `clargs.allocations`

The check `clargs.allocations` (`cmake/ClargsCheckAllocations.cmake`) makes sure the standard allocation functions are not used directly. `ctest` runs it in both the static and the shared configuration. It needs no build and can also be run on its own.

```sh
cmake -P cmake/ClargsCheckAllocations.cmake
```

- It covers every `*.c` and `*.h` in `src/` and its subdirectories. Only `src/alloc.c` is excluded (`src/alloc_internal.h` is covered). When you add a source, the check does not need to change. To check a different directory, pass `-DCLARGS_SOURCE_DIR=<directory>` before `-P`.
- It fails when any of the following names appears as an identifier of its own, whether in a call, used as a value, or on a preprocessor line.

  `malloc`, `calloc`, `realloc`, `free`, `aligned_alloc`, `strdup`, `strndup`, `wcsdup`, `asprintf`, `vasprintf`

- Comments, string literals and character literals are not checked. Other identifiers that merely contain one of the names, such as `clargs_malloc`, `malloc_fn` and `freed`, are not detected.
- On failure, it lists every file name, line number and name. It also fails when there is no file to check. It does not change any file.
- **Limits of the check.** The check only reads names; it does not preprocess. It does not detect a name built by a macro with `##`, or a name split across lines with `\` at the end of a line. Do not write code or macros in `src/` that call a standard allocation function in such a way.

#### Global state

The only mutable global state the library has is the allocator that is set (`current_allocator` in `src/alloc.c`). Only `clargs_set_allocator()` writes it; the internal memory functions only read it.

- Do not write any other mutable `static` variables or file-scope variables in `src/`. The same goes for `static` variables inside functions.
- Tables that do not change may be written as `static const`.
- Keep state in objects that the user creates and destroys. This rule is what lets different objects be used from different threads ([rule 8 of Ownership and Lifetime](ownership.md#8-threads)).
- This is not checked automatically. Check it in review.

### Applying the ownership rules

Every public function follows the rules in [Ownership and Lifetime](ownership.md); do not decide ownership, lifetime or how failures are returned on your own for a new function. This section shows how the library code applies those rules.

#### Forms that public functions do not take

Following [rule 6](ownership.md#6-whoever-allocates-frees), do not write public functions of the following forms.

| Form not to write | Form to write instead |
|---|---|
| Returns an allocated string and has the user `free` it | Return a string owned by an object ([rule 3](ownership.md#3-strings-and-objects-the-library-returns)) |
| Takes a buffer allocated by the user and frees it in the library | Duplicate it ([rule 2](ownership.md#2-strings-you-pass)). The user frees the original buffer |

#### Returning failures

The following example shows how a function is written to follow [rule 7](ownership.md#7-how-failures-are-returned). The names are placeholders; the memory functions and `clargs_strdup` are real.

```c
/*
 * Placeholder public header. It includes <clargs/error.h> and has
 * typedef struct clargs_example clargs_example_t; and the declarations of the two functions below.
 */
#include <clargs/example.h>

#include "alloc_internal.h"
#include "str.h"

#include <assert.h>
#include <stddef.h>

struct clargs_example {
    char *name;
};

clargs_error_t clargs_example_create(const char *name, clargs_example_t **out)
{
    clargs_example_t *example;

    assert(name != NULL);   /* Precondition. Not an error code. */
    assert(out != NULL);

    *out = NULL;            /* On failure, the output is NULL. */

    example = clargs_calloc(1, sizeof(*example));
    if (example == NULL) {
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }

    example->name = clargs_strdup(name);   /* Rule 2: duplicate it. */
    if (example->name == NULL) {
        clargs_free(example);              /* Free what this call allocated. */
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }

    *out = example;
    return CLARGS_ERROR_NONE;
}

void clargs_example_destroy(clargs_example_t *example)
{
    if (example == NULL) {  /* Rule 4: NULL does nothing. */
        return;
    }
    clargs_free(example->name);
    clargs_free(example);
}
```

- Handle `NULL` from the internal memory functions (`clargs_malloc`, `clargs_calloc`, `clargs_realloc`) and `clargs_strdup` as [Allocating memory](#allocating-memory) and [Return values of internal functions](#return-values-of-internal-functions) say: turn it into `CLARGS_ERROR_OUT_OF_MEMORY` and return it upward without swallowing it, and check the result of `clargs_realloc` before you replace the original pointer.
- Write a test that makes each allocation fail in turn to check these rules. Use the test allocator (`tests/support/test_allocator.h`; see [The failure of every allocation in turn](#the-failure-of-every-allocation-in-turn)).

#### How the internal utilities follow the rules

| Function | How it follows the rules |
|---|---|
| `clargs_malloc`, `clargs_calloc`, `clargs_realloc`, `clargs_free` (`src/alloc_internal.h`) | They call only the allocator that is set, so memory they allocate is freed by the same allocator ([rules 5](ownership.md#5-allocator) and [6](ownership.md#6-whoever-allocates-frees)). `NULL` for `clargs_free` does nothing. When `clargs_realloc` fails, the original memory stays valid ([rule 7](ownership.md#7-how-failures-are-returned)). They do not write `errno`, print anything or abort |
| `char *clargs_strdup(const char *str)` (`src/str.h`) | The caller owns the duplicate and frees it with `clargs_free`. On failure it returns `NULL` and leaves nothing allocated ([rule 7](ownership.md#7-how-failures-are-returned)). `NULL` for `str` is a precondition violation, checked with `assert` ([rule 4](ownership.md#4-null)) |
| `clargs_vec_init`, `clargs_vec_push`, `clargs_vec_at`, `clargs_vec_destroy` (`src/vec.h`) | `clargs_error_t clargs_vec_push(clargs_vec_t *vec, const void *item)` returns `CLARGS_ERROR_OUT_OF_MEMORY` on failure, and `count`, `capacity` and the existing items stay as they were before the call ([rule 7](ownership.md#7-how-failures-are-returned)). `clargs_vec_destroy` frees all the memory of the array. It does not free what the items point to; the caller owns that ([rule 6](ownership.md#6-whoever-allocates-frees)). It may be called twice. The pointer that `clargs_vec_at` returns is valid until the next `clargs_vec_push` or `clargs_vec_destroy` (an example of the "shorter period" of [rule 3](ownership.md#3-strings-and-objects-the-library-returns), stated in the comment). `NULL` for `vec` and `item`, and an index out of range, are precondition violations, checked with `assert` ([rule 4](ownership.md#4-null)) |

There are two differences from the form of public functions. Both apply only to internal functions.

- `clargs_malloc`, `clargs_calloc`, `clargs_realloc` and `clargs_strdup` return failure as `NULL`, not as a `clargs_error_t`. This is because internal functions that pair with a standard function take the same form as the standard. "Returns `clargs_error_t`" in [rule 7](ownership.md#7-how-failures-are-returned) is a rule for public functions.
- `clargs_vec_t` is not an object created with `create`. The caller initializes a structure it holds with `clargs_vec_init` and makes it empty again with `clargs_vec_destroy`. So `vec` for `clargs_vec_destroy` cannot be `NULL` (a precondition). The form of [rule 1](ownership.md#1-creation-and-destruction) and "`NULL` for `destroy` does nothing" of [rule 4](ownership.md#4-null) are rules for objects created with `create`.

The only global state these functions use is the allocator that is set ([Global state](#global-state)).

### Checking preconditions

Failures are divided into two kinds. The detailed explanation of the division is in [Ownership and Lifetime](ownership.md#runtime-failures-and-programming-errors).

| Kind | Example | How to write it |
|---|---|---|
| Runtime failure (can happen even in a correct program) | Failure to allocate memory | Return it as a `clargs_error_t` |
| Programming error (always happens until the calling code is fixed) | `NULL` for a required argument, an allocator with a missing function, an index out of range | Make it a precondition of the function and check it with `assert`. Do not assign an error code |

- **Write the checks at the entry of the function with the standard `assert` (`<assert.h>`).** The same applies to public and internal functions. Do not create your own macros or a mechanism that replaces the behavior. If you write one `assert` per condition, the output shows which condition was violated.

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

- **Do not write side effects in the expression.** Even when you want to check the result of a function call, do not put the call inside `assert`. The behavior of a correct call must not depend on whether `assert` is there.
- **Debug configurations abort; Release configurations do not check.** In a build where `NDEBUG` is not defined (the CMake Debug configuration), a violation makes `assert` write the condition, the file name and the line number to standard error and abort the process. In a build where `NDEBUG` is defined (the Release, RelWithDebInfo and MinSizeRel configurations), `assert` does nothing, and the behavior when a precondition is broken is undefined.
- **Write the preconditions in comments.** Write them as `Preconditions:` in the comment of the function: in the public header for a public function, in the internal header for an internal function. Write both the ones that `assert` checks and the ones that cannot be checked (such as setting the allocator while no object exists), and say which ones are checked (`src/vec.h` and `include/clargs/alloc.h` are examples).
- **Make sure the Release configuration also builds without warnings.** Under `NDEBUG` the expression of `assert` disappears, so a variable used only inside `assert` produces an "unused" warning, and the build stops in a configuration where warnings are errors. Do not create variables only for `assert`; write the expression directly in `assert`.

  ```sh
  cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
  cmake --build build-release
  ctest --test-dir build-release --output-on-failure
  ```

- Aborting on a violation is not checked by the automated tests. Do not make calls that break preconditions from tests.
- The convention that passing `NULL` to the `destroy` of a created object does nothing (rule 4 of [Ownership and Lifetime](ownership.md)) is about objects created with `create`. For types where the caller initializes and uses a structure it holds (`clargs_vec_t`), `NULL` for `clargs_vec_destroy` is a precondition violation.

### Return values of internal functions

| Internal function | Return value | Example |
|---|---|---|
| Those that pair with a standard function | Return in the same form as the standard. Failure is `NULL` | `clargs_malloc`, `clargs_calloc`, `clargs_realloc`, `clargs_strdup` |
| Other functions that can fail | Return `clargs_error_t` | `clargs_vec_push` |
| Those that cannot fail | The natural type, or `void` | `clargs_vec_init`, `clargs_vec_at`, `clargs_vec_destroy` |

- A function that calls a function that returns failure as `NULL` turns it into `CLARGS_ERROR_OUT_OF_MEMORY` and returns it upward.
- Write the test for success as `error == CLARGS_ERROR_NONE`.
- Do not report failure by any means other than the return value (`errno`, output, global variables).

## Writing tests

Tests are written with [Naos](https://github.com/vabelorg/naos) v0.4.0. Test code is also C99 without extensions, compiled with the warnings `-Wall -Wextra -Wpedantic` (`/W4` with MSVC), and warnings are errors by default.

- **Have exactly one entry point.** The sources of one executable (one call of `clargs_add_test()`) have exactly one `NAOS_MAIN`. Naos does not provide `main()`.
- **Put at least one test in a suite.** A `NAOS_SUITE` with no test is a compile error.
- **Every test runs at least one assertion.** Naos fails a test that ran no assertion, with `no assertion ran`. Do not write assertions only inside a conditional branch or inside a loop that may have no items. In a test that only checks that something can run, write `NAOS_PASS()`.
- **Avoid the known warnings of Naos v0.4.0.** The following uses produce warnings, and the build stops in a configuration where warnings are errors.
  - Do not pass memory that nothing has been written to yet (such as a pointer right after `malloc`) to `NAOS_CHECK_NOT_NULL` / `NAOS_REQUIRE_NOT_NULL`. GCC produces `-Wmaybe-uninitialized`. Write to the memory first, or use `NAOS_CHECK_TRUE(pointer != NULL)` / `NAOS_REQUIRE_TRUE(pointer != NULL)`.
  - Do not pass function pointers to `NAOS_CHECK_NULL` / `NAOS_CHECK_NOT_NULL` / `NAOS_REQUIRE_NULL` / `NAOS_REQUIRE_NOT_NULL`. `-Wpedantic` warns. Write a comparison such as `NAOS_CHECK_TRUE(function != NULL)`.
- **How to write the macros.** Do not write a comma after the last element of `NAOS_SUITE` and `NAOS_MAIN`. Do not put `;` after `NAOS_TEST(...) { ... }`, `NAOS_SUITE(...)` or `NAOS_MAIN(...)` (`-Wpedantic` warns).
- Use the assertion that matches the type: `NAOS_CHECK_UINT_EQ` for unsigned values, `NAOS_CHECK_INT_EQ` for signed values, and so on.
- **Taking the address of a standard function with MSVC.** With the MSVC DLL runtime, standard functions (such as `malloc`) are imported. Their addresses cannot be written in static initializers, and with `/W4` even local initializers and assignments produce warnings (C4204, C4232). Write it in one of the following ways.
  - Wrap the standard function in a `static` function within the file, and pass that function (such as `wrap_malloc` in `tests/internal/vec_test.c`; in the library, `default_malloc` and others in `src/alloc.c`).
  - In a test that checks that the standard functions can be listed as they are (`tests/public/alloc_test.c`), write the following at the top of the file.

    ```c
    #ifdef _MSC_VER
    #pragma warning(disable : 4204 4232)
    #endif
    ```

- **Do not write `const` objects without an initializer.** It is C4132 with MSVC `/W4`. Add `= { 0 }`.
- **After `NAOS_FAIL` and `NAOS_SKIP`, write no statement other than `break;`.** Neither returns. Writing `return` or another statement after them is C4702 (unreachable code) with MSVC `/W4`. Even in the `default:` of a `switch` in a function that returns a value, write only `NAOS_FAIL(...)`.
- **Write bytes outside the ASCII range as character constants.** `(char)0x80` is C4310 (truncation of a constant) with MSVC `/W4`. Write `'\x80'`.
- **Do not actually pass huge sizes to the standard allocation functions.** ASan and valgrind end abnormally. To check how large requests are handled, set with `clargs_set_allocator()` an allocator that only records its arguments and returns `NULL`.
- **Check both sides of an overflow check.** Check not only the side that overflows (it fails without calling the allocator) but also the side that just fits (it reaches the allocator). For the side that just fits, make the first request fail with the test allocator (`clargs_test_allocator_fail_at(1)`) and check that the number of requests is 1, which shows that the request reached the allocator. This way you never pass a huge size to the standard allocation functions.
- **A test that set an allocator sets it back to the default before it ends.** The setting is shared by all the tests of one executable. Call `clargs_set_allocator(NULL)`, or `clargs_test_allocator_uninstall()` for the test allocator ([Using the test allocator](#using-the-test-allocator)).
- **Write the C examples in the documents so that they can be extracted and compiled as they are.** Include the public or internal headers that declare what they use. An example that uses placeholder names writes the contents of its placeholder header in a comment.
- After adding a test, build before you run `ctest`. An executable that has not been built appears as `<name>.NOT_BUILT`, which fails.

## Verifying a change

After a change, configure, build and run the tests with both a static and a shared library.

```sh
cmake -S . -B build-static -DCMAKE_BUILD_TYPE=Debug
cmake --build build-static
ctest --test-dir build-static --output-on-failure

cmake -S . -B build-shared -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=ON
cmake --build build-shared
ctest --test-dir build-shared --output-on-failure
```

- In both configurations, `ctest` runs the allocation check `clargs.allocations`. It checks that the sources and headers in `src/`, except `src/alloc.c`, do not use the standard allocation functions directly ([The allocation check `clargs.allocations`](#the-allocation-check-clargsallocations)). It can also be run on its own.

  ```sh
  cmake -P cmake/ClargsCheckAllocations.cmake
  ```

- When you add or change an `assert`, make sure the Release configuration (`-DCMAKE_BUILD_TYPE=Release`) also builds without warnings ([Checking preconditions](#checking-preconditions)).
- In the shared configuration, `ctest` also runs the export check `clargs.exports`. It checks that the names the shared library exports are exactly the functions declared with `CLARGS_API` in the public headers, and that all of them start with `clargs_`. The expected names are read from the public headers, so adding a public function needs no change to a list. It is not registered in the static configuration.
- The export check can also be run on its own.

  ```sh
  cmake -DCLARGS_LIBRARY=<path-to-shared-library> -P cmake/ClargsCheckExports.cmake
  ```

- A configuration with tests enabled (the default at the top level) fetches Naos from GitHub when it is configured. If you have a copy that was already fetched, passing `-DFETCHCONTENT_SOURCE_DIR_NAOS=<build-directory>/_deps/naos-src` lets you configure without the network.

## Version notes

- The only source of the version is the four macros `CLARGS_VERSION_MAJOR`, `CLARGS_VERSION_MINOR`, `CLARGS_VERSION_PATCH` and `CLARGS_VERSION_PRERELEASE` in `include/clargs/version.h`. The build reads this header as text, so write each of the four macros on its own line. Write the three numeric macros in the form `#define CLARGS_VERSION_<PART> <integer>`, as decimal 0 to 255 without leading zeros. Do not write the version numbers anywhere else.
- Write `CLARGS_VERSION_PRERELEASE` as a single string literal in the form `#define CLARGS_VERSION_PRERELEASE "<identifier>"`. Its contents are SemVer 2.0.0 pre-release identifiers without the leading hyphen (for example `"rc.1"`). Separate identifiers with `.` and write each with the characters `[0-9A-Za-z-]`. Numeric identifiers have no leading zeros. For a final release, make it the empty string `""`. If the form is wrong, configuration fails.
- Do not bump the version by hand. The release workflow **Release PR** rewrites `include/clargs/version.h` and `CHANGELOG.md` together and opens a release PR. The procedure is in the [release guide](releasing.md), and the policy for bumping the version is in the [versioning policy](versioning.md).
- `CLARGS_VERSION_NUMBER` and `CLARGS_MAKE_VERSION()` contain an `(unsigned long)` cast, so they cannot be used in `#if`. In `#if`, compare `CLARGS_VERSION_MAJOR` and the others one by one.

  ```c
  #if CLARGS_VERSION_MAJOR > 0 || CLARGS_VERSION_MINOR >= 2
  ```

- While the major version is 0, `find_package` treats "the same minor version" as compatible. `find_package(CLArgs 0)` and ranges (`0.1...1`) do not find the package. If you give a version, write it down to the minor version, as in `find_package(CLArgs 0.1)`. Once the major version is 1 or more, "the same major version" is compatible.
- The package treats the numeric part and the full version separately. Only the numeric part is passed to `project()`, so `PROJECT_VERSION`, the compatibility check of `find_package` and the SOVERSION are numeric only.
  - `CLArgs_VERSION`, set by `find_package(CLArgs)`, is the numeric part (for example `0.2.0`).
  - `CLArgs_VERSION_FULL` is the full version (for example `0.2.0-rc.1`). `CLArgs_VERSION_PRERELEASE` is the pre-release identifier (for example `rc.1`). For a final release, `CLArgs_VERSION_FULL` is the same as `CLArgs_VERSION`, and `CLArgs_VERSION_PRERELEASE` is empty.
  - `Version` in the pkg-config file `clargs.pc` is the full version.
  - `find_package` compares only the numeric part, so `0.2.0-rc.1` is also found by `find_package(CLArgs 0.2)`. pkg-config considers `0.2.0-rc.1` newer than `0.2.0`. See the [versioning policy](versioning.md) for details.
