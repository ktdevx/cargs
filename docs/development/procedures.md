# Development Procedures

This document gives the steps for common changes to ArgX: adding a public function, a header, a source file, an internal function, a test, a CMake module or an error code, using the test allocator in a test, and verifying a change before you submit it. The rules that these steps follow are in [Development Conventions](conventions.md), and setting up a development environment and submitting a pull request are in [Contributing to ArgX](../../CONTRIBUTING.md).

## Procedures

| What you want to do | Procedure |
| --- | --- |
| Add a public function | Put `ARGX_API` at the start of the declaration in the public header. Write `ARGX_API`, the return type, the function name and `(` in this order. Write it inside the `extern "C"` guard. Write the implementation in a source in `src/`. Add the name to the `## Functions` index of the API page of the header |
| Add a public header | Put it in `include/argx/` and add one include line to `argx.h`. Add an API page for it to `docs/references/` and to the index |
| Add a source file | Put it in `src/` and add one line to `ARGX_SOURCES` in `src/CMakeLists.txt` |
| Add an internal function | Declare it without `ARGX_API` in an internal header (`src/<name>_internal.h`, or `src/<name>.h` if there is no public header of the same name) and implement it in a source in `src/` |
| Add a test of the public API | Put it in `tests/public/` and add `argx_add_test(<name> SOURCES public/<file>)` to `tests/CMakeLists.txt` |
| Add a test of an internal function | Put it in `tests/internal/<internal-header-name>_test.c` and add `argx_add_test(<name> SOURCES internal/<file> INTERNAL)` to `tests/CMakeLists.txt` |
| Add a CMake module | Put it in `cmake/ArgX<name>.cmake` and include it in the form `include("${CMAKE_CURRENT_LIST_DIR}/cmake/ArgX<name>.cmake")` |
| Add an error code | Add one line `X(name, description)` at the end of `ARGX_ERROR_LIST` in `include/argx/error.h`, and one row to [Error Codes](../references/errors.md). Change no other code ([Adding an error code](#adding-an-error-code)) |
| Use the test allocator | Add `support/test_allocator.c` to the `SOURCES` of `argx_add_test()`, and include `"../support/test_allocator.h"` in the test ([Using the test allocator](#using-the-test-allocator)) |

Every new file under `include/`, `src/`, `tests/`, `tools/` and `cmake/`, and every `CMakeLists.txt`, starts with the license header below, followed by the comment that describes the file. In C files, write it inside the first comment (`/*`, ` * Copyright ...`, ` * SPDX-...`, ` *`). In CMake files, write it with `#`; if the file does not start with a comment, put an empty line after the header. Keep the year as it is: it is the year of the first publication, not the year you add the file. The input files of the tests (`tests/release/fixtures/`) do not take the header.

```
# Copyright (c) 2026 The ArgX Authors
# SPDX-License-Identifier: MIT
```

None of these procedures needs any change to the CMake files other than the one line above. The install rules cover every `*.h` in `include/argx/`, and the export check reads the expected names from the public headers.

Below, `<name>` is written as `example`.

### Adding a public function

Write the declaration inside the `extern "C"` guard of the public header (if you create a new one, see [Adding a public header](#adding-a-public-header)).

```c
ARGX_API int argx_example_add(int left, int right);
```

- Write `ARGX_API`, the return type, the function name and `(` in this order. The export check reads the last identifier between `ARGX_API` and the first `(` as the function name.
- A declaration that returns a function pointer directly (`ARGX_API int (*argx_example_get(void))(int);`) does not let the check read the name. Give the type a name with `typedef` first, and use that.

  ```c
  typedef int (*argx_example_fn_t)(int value);

  ARGX_API argx_example_fn_t argx_example_get(void);
  ```

- Do not write `ARGX_API` on the definition (in the source in `src/`). The source includes the public header that declares the function. If it does not, the mark does not reach the definition and the function is not exported from the shared library (with GCC and Clang, the build stops before that with a `-Wmissing-prototypes` warning).

  ```c
  #include <argx/example.h>

  int argx_example_add(int left, int right)
  {
      return left + right;
  }
  ```

- For a public function, add a test of the public API in `tests/public/`.
- Add the name of the function to the `## Functions` index of the API page of its header in `docs/references/`: to the row of its family, or as a new row that links to the section that describes it. Describe in that section the rules the function shares with others; the detailed description and the preconditions go in the comment of the declaration.
- If you remove `ARGX_API` from a public function, the function is no longer public. The export check is based on the mark, so it does not detect the removal itself (a test of the public API that calls the function fails to link in the shared library configuration). Do not write a function declaration without the mark in a public header.

### Adding a public header

Create `include/argx/example.h` in the following form.

```c
/*
 * ArgX - <what this header contains>.
 */
#ifndef ARGX_EXAMPLE_H
#define ARGX_EXAMPLE_H

#include <argx/export.h>

#ifdef __cplusplus
extern "C"
{
#endif

ARGX_API int argx_example_add(int left, int right);

#ifdef __cplusplus
}
#endif

#endif /* ARGX_EXAMPLE_H */
```

Add one include line to `include/argx/argx.h`.

```c
#include <argx/example.h>
```

Public headers are C99 without extensions, and each compiles when included on its own (it includes the headers it needs itself).

Add an API page for the header to `docs/references/`, starting with the sentence that names the header and a `## Functions` index, and list it in the API list of the [documentation index](../README.md#references).

### Adding a source file

Create `src/example.c` and add one line to `ARGX_SOURCES` in `src/CMakeLists.txt`.

```cmake
set(ARGX_SOURCES
    example.c
    version.c
)
```

The library sources are compiled as C99 without extensions, with the warnings `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes -Wmissing-prototypes` (`/W4` with MSVC). When `ARGX_WARNINGS_AS_ERRORS` is ON (the default at the top level), warnings are errors. Pay particular attention to the following.

- A function that is not `static` needs a declaration before its definition (`-Wmissing-prototypes`). The source that defines a public function includes its public header; the source that defines an internal function includes its internal header. Add `static` to functions used only within one file.
- Write `(void)` for a function without parameters (`-Wstrict-prototypes`).
- Write an explicit cast for an implicit conversion that can change the value (`-Wconversion`).
- The library depends on nothing but the C standard library. Do not link it to other libraries.

### Adding an internal function

Declare an internal function that is used across files in an internal header. Do not add `ARGX_API`; no `extern "C"` guard is needed either.

The name of the internal header depends on whether there is a public header of the same name.

| Module | Internal header | Include guard |
| --- | --- | --- |
| Has the public header `include/argx/example.h` | `src/example_internal.h` | `ARGX_EXAMPLE_INTERNAL_H` |
| Has no public header | `src/example.h` | `ARGX_EXAMPLE_H` |

The example below is for the case where the public header `include/argx/example.h` exists.

```c
/*
 * ArgX - internal declarations for <contents>.
 */
#ifndef ARGX_EXAMPLE_INTERNAL_H
#define ARGX_EXAMPLE_INTERNAL_H

int argx_example_twice(int value);

#endif /* ARGX_EXAMPLE_INTERNAL_H */
```

The defining source writes `#include "example_internal.h"`. In a module without a public header, the only differences are the file name `src/example.h`, the include guard `ARGX_EXAMPLE_H` and the include `#include "example.h"`; everything else is the same. Internal functions are not exported from the shared library, and internal headers are not installed. For an internal function, add a test of internal functions in `tests/internal/`.

### Adding a test of the public API

Create `tests/public/example_test.c`. Include only public headers.

```c
/*
 * Tests of the public example API: <argx/example.h>.
 */
#include <argx/argx.h>

#include <naos/naos.h>

NAOS_TEST(adds_two_numbers)
{
    NAOS_CHECK_INT_EQ(argx_example_add(1, 2), 3);
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
argx_add_test(example SOURCES public/example_test.c)
```

Tests of the public API link to `ArgX::argx`. That is, they run against the library in the form chosen by the configuration (static or shared). With a shared library, internal functions cannot be called.

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
    NAOS_CHECK_INT_EQ(argx_example_twice(21), 42);
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
argx_add_test(example_internal SOURCES internal/example_internal_test.c INTERNAL)
```

`<name>` is the name of the internal header. For a module without a public header (the internal header is `src/example.h`), it looks like this.

```cmake
argx_add_test(example SOURCES internal/example_test.c INTERNAL)
```

Tests of internal functions link to `argx_internal`. In both the static and the shared configuration, the library sources are linked statically, so internal functions can be called.

### Adding an error code

There is only one place to change in the code. Add one line `X(name, description)` at the **end** of `ARGX_ERROR_LIST` in `include/argx/error.h`. Add `\` at the end of the line that was last until then.

```c
#define ARGX_ERROR_LIST(X) \
    X(ARGX_ERROR_NONE, "success") \
    X(ARGX_ERROR_OUT_OF_MEMORY, "out of memory") \
    X(ARGX_ERROR_EXAMPLE, "example failure")
```

- **Add only at the end.** Do not write values. Values are given from 0 in the order of the list (`ARGX_ERROR_NONE` is 0, `ARGX_ERROR_OUT_OF_MEMORY` is 1). The values are public; a value once published is never changed and never reused for another meaning. So do not delete existing lines, do not reorder them, and do not add lines in the middle. Keep the line of an error code that is no longer used.
- **Change no other code.** Add a row for the code to the table of [Error Codes](../references/errors.md), with its message, its meaning and the functions that return it. The enumeration `argx_error_t`, `argx_error_message()` (`src/error.c`) and the test (`tests/public/error_test.c`) are generated from this list. For the added code too, the test checks that the description matches the list, is not empty, and differs from the descriptions of the other codes and from `"unknown error code"`.
- **There is only one list.** Write every error code in this list. Do not create a separate type or range of numbers per feature.
- **Names** are `ARGX_ERROR_<WHAT>`. Success is only `ARGX_ERROR_NONE`.
- **Descriptions** are short English phrases that start with a lowercase letter (`"out of memory"`). Do not reuse the description of another code.
- **Only runtime failures become error codes.** Before you add one, check whether the failure can happen even in a correct program. A programming error of the calling program (such as `NULL` for a required argument) does not become an error code; it becomes a precondition ([Checking preconditions](conventions.md#checking-preconditions)).
  - Errors in a schema definition that come from the contents of a string (a name that breaks the rules, a default value the value type cannot convert, and so on) and those that come from the relation between definitions (a name conflict, a combination of definitions that cannot be used together, and so on) are handled as runtime failures and become error codes (`ARGX_ERROR_INVALID_NAME`, `ARGX_ERROR_NAME_CONFLICT`, `ARGX_ERROR_INVALID_DEFINITION`, `ARGX_ERROR_INVALID_DEFAULT`, `ARGX_ERROR_INVALID_VALUE`). The reasons are that strings can be built from data decided at run time, that only the library can match them against the rules and the existing definitions, and that `assert` does not check anything in the Release configuration.
  - A null pointer, an index out of range and a value that is not in the enumeration stay preconditions, even in the definition functions. They are decided only by the calling code and can be checked at the place of the call.
  - The division is in [Ownership and Lifetime](../references/ownership.md#runtime-failures-and-programming-errors), and its reasons in [Design](../guides/design.md#runtime-failures-are-error-codes-programming-errors-are-preconditions).
- An error code represents only the kind of failure. Do not make it carry a position or a target.

### Using the test allocator

The test allocator (`tests/support/test_allocator.h`, `tests/support/test_allocator.c`) is an allocator for tests that can be set as the library's allocator. It fails the request you specify, and counts the requests, the blocks not yet freed and the contract violations. When you add a function that allocates memory, use it to check how allocation failures are handled and that nothing leaks.

To use it, add `support/test_allocator.c` to the `SOURCES` of `argx_add_test()` in `tests/CMakeLists.txt`. Do not change any other CMake file.

```cmake
argx_add_test(example SOURCES internal/example_test.c support/test_allocator.c INTERNAL)
```

In the test source (`tests/public/` or `tests/internal/`), include it in this form.

```c
#include "../support/test_allocator.h"
```

The test allocator depends only on `<argx/argx.h>` and the C standard library, so it can be used both in tests of the public API and in tests of internal functions.

| Function | What it does |
| --- | --- |
| `void argx_test_allocator_install(void)` | Resets the counts (the same as `reset`) and sets the test allocator as the library's allocator |
| `void argx_test_allocator_uninstall(void)` | Sets the library back to the default allocator. The counts are kept, so you can read them afterwards |
| `void argx_test_allocator_reset(void)` | Sets the number of requests, the number of live blocks and the number of contract violations back to 0, and clears the request to fail |
| `void argx_test_allocator_fail_at(size_t request)` | Fails the `request`-th request counted from the last `reset`. Counting starts at 1, and only that request fails. Passing 0 fails no request |
| `size_t argx_test_allocator_requests(void)` | The number of requests since the last `reset`. Requests that were made to fail are counted too |
| `size_t argx_test_allocator_live(void)` | The number of blocks not yet freed. It goes up by 1 on a successful allocation, down by 1 on a free, and does not change on a reallocation |
| `size_t argx_test_allocator_violations(void)` | The number of contract violations since the last `reset` |
| `argx_allocator_t argx_test_allocator_get(void)` | Returns the four functions of the test allocator. Does not change the state. Use it when you pass them to `argx_set_allocator()` yourself, or call the four functions directly |

- A **request** is a call of `malloc_fn`, `calloc_fn` or `realloc_fn`. The three are counted together. `free_fn` is not a request. Only calls that reach the allocator are counted, so something that does not call the user's function, such as `argx_malloc(0)`, is not counted.
- A **contract violation** is a call that the library must not make (see "How the library calls the functions of the allocator" in `include/argx/alloc.h`): a request of size 0 or count 0, a reallocation of `NULL`, a free of `NULL`. As long as the library code goes through the internal memory functions, it stays 0.
- A reallocation that was made to fail leaves the original block valid.
- **Pair `install` with `uninstall`.** The state is in `static` variables and is shared by all the tests of one executable. So is the allocator that is set. A test that called `install` destroys everything it created and then calls `uninstall` before it ends. If you forget `uninstall`, later tests keep using the test allocator.
- **Call `reset` while nothing is allocated.** If you call it while there are blocks not yet freed, the count of live blocks is wrong from then on.
- **Every test checks at its end that `live` and `violations` are 0.** The counts can still be read after `uninstall`.
- The test allocator is not synchronized.

When a test needs to tell the kinds of request apart, or to see the size that was requested, use the wrap allocator (`tests/support/wrap_allocator.h`, `tests/support/wrap_allocator.c`) instead of writing one in the test file. It only forwards to the standard functions, and counts the calls of each of the four functions and the size of the last `malloc_fn` and `realloc_fn`. It cannot fail a request and does not check the contract. Add `support/wrap_allocator.c` to the `SOURCES` of `argx_add_test()`, include `"../support/wrap_allocator.h"`, set it with `argx_set_allocator()` (its functions come from `argx_wrap_allocator_get()`), and read the counts with `argx_wrap_allocator_counts()` after `argx_wrap_allocator_reset()`.

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
#include <argx/argx.h>

#include "vec.h"

#include "../support/test_allocator.h"

#include <naos/naos.h>

#include <stddef.h>

/*
 * Pushes the items count, count + 1, ... so that the array has total items
 * afterwards. Stops at the first push that fails and returns its error.
 */
static argx_error_t push_up_to(argx_vec_t *vec, size_t total)
{
    while (vec->count < total)
    {
        int value = (int)vec->count;
        argx_error_t error = argx_vec_push(vec, &value);

        if (error != ARGX_ERROR_NONE)
        {
            return error;
        }
    }
    return ARGX_ERROR_NONE;
}

NAOS_TEST(fails_at_every_allocation_point)
{
    const size_t total = 10;
    const size_t limit = 100;
    int completed = 0;
    size_t n;

    argx_test_allocator_install();

    for (n = 1; n <= limit; ++n)
    {
        argx_vec_t vec;
        argx_error_t error;
        size_t index;

        /* Nothing is allocated here, so the counts can start again. */
        argx_test_allocator_reset();
        argx_test_allocator_fail_at(n);

        /* The operation. */
        argx_vec_init(&vec, sizeof(int));
        error = push_up_to(&vec, total);

        if (error == ARGX_ERROR_NONE
            && argx_test_allocator_requests() < n)
        {
            /*
             * The operation ended before the request to fail was reached:
             * every allocation point has failed once.
             */
            NAOS_CHECK_UINT_EQ(vec.count, total);
            argx_vec_destroy(&vec);
            NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);
            NAOS_CHECK_UINT_EQ(argx_test_allocator_violations(), 0);
            completed = 1;
            break;
        }

        /* The n-th request failed, and the operation reported it. */
        NAOS_CHECK_INT_EQ(error, ARGX_ERROR_OUT_OF_MEMORY);
        NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), n);

        /*
         * What the failure must leave unchanged: the array is as it was
         * before the push that failed.
         */
        NAOS_CHECK_TRUE(vec.count < total);
        NAOS_CHECK_UINT_EQ(vec.count, vec.capacity);
        for (index = 0; index < vec.count; ++index)
        {
            const int *item = argx_vec_at(&vec, index);

            NAOS_CHECK_INT_EQ(*item, (int)index);
        }

        /* Destroy what exists; nothing is left and nothing was violated. */
        argx_vec_destroy(&vec);
        NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);
        NAOS_CHECK_UINT_EQ(argx_test_allocator_violations(), 0);
    }

    /* The loop ended because the operation completed, not at the limit. */
    NAOS_CHECK_TRUE(completed);

    argx_test_allocator_uninstall();
}

NAOS_SUITE(failure,
    NAOS_ADD_TEST(fails_at_every_allocation_point)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(failure)
)
```

- If the operation succeeds while the number of requests is `n` or more, the operation swallowed the `n`-th failure. So the condition for ending the loop includes not only "it succeeded" but also "the number of requests is less than `n`". If the failure was swallowed, the following check for `ARGX_ERROR_OUT_OF_MEMORY` fails.
- Give the loop a limit (`limit`) and check that it ended because the operation completed (`completed`). Even if there is a bug, the loop cannot run forever.
- What you check after a failure is what rule 7 of [Ownership and Lifetime](../references/ownership.md) requires: an error code is returned, nothing allocated along the way is left, and existing objects are as they were before the call and can still be used and destroyed. `fails_at_every_allocation_point` also checks that a push succeeds again after the failure.
- For an operation that allocates a fixed number of times, also check that the number of failures equals that number (`failures` and `allocation_points` in the model).

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

- In both configurations, `ctest` runs the allocation check `argx.allocations`. It checks that the sources and headers in `src/`, except `src/alloc.c`, do not use the standard allocation functions directly ([The allocation check `argx.allocations`](conventions.md#the-allocation-check-argxallocations)). It can also be run on its own.

  ```sh
  cmake -P cmake/ArgXCheckAllocations.cmake
  ```

- When you add or change an `assert`, make sure the Release configuration (`-DCMAKE_BUILD_TYPE=Release`) also builds without warnings ([Checking preconditions](conventions.md#checking-preconditions)).
- In the shared configuration, `ctest` also runs the export check `argx.exports`. It checks that the names the shared library exports are exactly the functions declared with `ARGX_API` in the public headers, and that all of them start with `argx_`. The expected names are read from the public headers, so adding a public function needs no change to a list. It is not registered in the static configuration.
- The export check can also be run on its own.

  ```sh
  cmake -DARGX_LIBRARY=<path-to-shared-library> -P cmake/ArgXCheckExports.cmake
  ```

- A configuration with tests enabled (the default at the top level) fetches Naos from GitHub when it is configured. If you have a copy that was already fetched, passing `-DFETCHCONTENT_SOURCE_DIR_NAOS=<build-directory>/_deps/naos-src` lets you configure without the network.
