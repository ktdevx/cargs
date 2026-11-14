# Development Procedures

The steps for common changes to ArgX. The rules they follow are in [Development Conventions](conventions.md).

## Procedures

| What you want to do | Procedure |
| --- | --- |
| [Add a public function](#adding-a-public-function) | Declare it with `ARGX_API` in a public header, implement it in `src/`, and add it to the `## Functions` index of its API page |
| [Add a public header](#adding-a-public-header) | Put it in `include/argx/`, include it from `argx.h`, and add an API page |
| [Add a source file](#adding-a-source-file) | Put it in `src/` and add it to `ARGX_SOURCES` in `src/CMakeLists.txt` |
| [Add an internal function](#adding-an-internal-function) | Declare it without `ARGX_API` in an internal header in `src/` |
| [Add a test](#adding-a-test-of-the-public-api) | Put it in `tests/public/` or `tests/internal/` and add `argx_add_test()` to `tests/CMakeLists.txt` |
| Add a CMake module | Put it in `cmake/ArgX<Name>.cmake` and `include("${CMAKE_CURRENT_LIST_DIR}/cmake/ArgX<Name>.cmake")` |
| [Add an error code](#adding-an-error-code) | Add a line at the end of `ARGX_ERROR_LIST` and a row to [Error Codes](../references/errors.md) |

No other CMake changes are needed: the install rules and the export check pick up public headers by themselves.

Every new file under `include/`, `src/`, `tests/`, `tools/` and `cmake/`, and every `CMakeLists.txt`, starts with the license header (as a `/* */` comment in C). Keep the year as it is. Test fixtures do not take it.

```
# Copyright (c) 2026 The ArgX Authors
# SPDX-License-Identifier: MIT
```

### Adding a public function

Declare it inside the `extern "C"` guard of the public header.

```c
ARGX_API int argx_example_add(int left, int right);
```

- **Write `ARGX_API`, the return type, the name and `(` in this order.** The export check reads the name from there, so to return a function pointer, `typedef` the type first (`ARGX_API argx_example_fn_t argx_example_get(void);`).
- **Do not write `ARGX_API` on the definition.** The source includes the public header instead.
- Add a test in `tests/public/`, and add the function to the `## Functions` index of its API page.
- Do not declare a function without `ARGX_API` in a public header.

### Adding a public header

Create `include/argx/example.h`, and add `#include <argx/example.h>` to `include/argx/argx.h`. Each public header is C99 and compiles on its own.

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

Add an API page to `docs/references/`, starting with a `## Functions` index, and list it in the [documentation index](../README.md#references).

### Adding a source file

Create `src/example.c` and add it to `ARGX_SOURCES` in `src/CMakeLists.txt`.

The library is C99 without extensions, depends only on the C standard library, and is compiled with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes -Wmissing-prototypes` (`/W4`) as errors. In practice:

- A non-`static` function needs a prior declaration: include its header.
- Write `(void)` for a function without parameters.
- Cast explicitly when a conversion can change the value.

### Adding an internal function

Declare it in the internal header (`src/example_internal.h`, or `src/example.h` if there is no public header) without `ARGX_API` or an `extern "C"` guard, and add a test in `tests/internal/`.

```c
/*
 * ArgX - internal declarations for <contents>.
 */
#ifndef ARGX_EXAMPLE_INTERNAL_H
#define ARGX_EXAMPLE_INTERNAL_H

int argx_example_twice(int value);

#endif /* ARGX_EXAMPLE_INTERNAL_H */
```

### Adding a test of the public API

Create `tests/public/example_test.c`, including only public headers.

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

```cmake
argx_add_test(example SOURCES public/example_test.c)
```

### Adding a test of an internal function

Create `tests/internal/example_internal_test.c`, named after the internal header, and include `"example_internal.h"`. The test is written like a public one.

```cmake
argx_add_test(example_internal SOURCES internal/example_internal_test.c INTERNAL)
```

### Adding an error code

Add `X(name, description)` at the **end** of `ARGX_ERROR_LIST` in `include/argx/error.h`, and a row to [Error Codes](../references/errors.md). `argx_error_t`, `argx_error_message()` and their test are generated from the list.

```c
#define ARGX_ERROR_LIST(X) \
    X(ARGX_ERROR_NONE, "success") \
    X(ARGX_ERROR_OUT_OF_MEMORY, "out of memory") \
    X(ARGX_ERROR_EXAMPLE, "example failure")
```

- **Add only at the end.** Values are numbered from 0 in list order and are public, so never delete, reorder or insert lines.
- **Name** it `ARGX_ERROR_<WHAT>`, with a short, unique, lowercase description (`"out of memory"`).
- **Only runtime failures become error codes.** A programming error of the caller (`NULL`, an index out of range, a value outside an enumeration) is a [precondition](conventions.md#checking-preconditions). Schema definitions whose strings or combinations are invalid are runtime failures ([Design](../guides/design.md#runtime-failures-are-error-codes-programming-errors-are-preconditions)).
- An error code tells only the kind of failure, not its position or target.

### Using the test allocator

The test allocator fails the request you choose and counts requests, live blocks and contract violations. Add `support/test_allocator.c` to the `SOURCES` of `argx_add_test()` and include `"../support/test_allocator.h"`. It works in both public and internal tests.

| Function | What it does |
| --- | --- |
| `argx_test_allocator_install()` | Resets the counts and sets the test allocator |
| `argx_test_allocator_uninstall()` | Sets the default allocator back. The counts are kept |
| `argx_test_allocator_reset()` | Sets the counts to 0 and clears the request to fail |
| `argx_test_allocator_fail_at(n)` | Fails only the `n`-th request (from 1) after `reset`. 0 fails none |
| `argx_test_allocator_requests()` | Requests (`malloc_fn`, `calloc_fn`, `realloc_fn`) since `reset`, failed ones included |
| `argx_test_allocator_live()` | Blocks not yet freed |
| `argx_test_allocator_violations()` | Calls the library must not make: size 0, reallocating or freeing `NULL` |
| `argx_test_allocator_get()` | The four functions, to pass to `argx_set_allocator()` yourself |

- **Pair `install` with `uninstall`.** The state is shared by all tests of one executable.
- **Call `reset` only while nothing is allocated.**
- **End every test by checking that `live` and `violations` are 0.**

To see the kind or size of each request instead, use the wrap allocator (`tests/support/wrap_allocator.h`). It forwards to the standard functions and counts the calls.

#### The failure of every allocation in turn

A test of an operation that allocates fails its first, second, … allocation in turn, without knowing how many there are:

```
install
for n = 1, 2, … up to a limit
    reset, fail_at(n)
    run the operation
    if it succeeded and requests < n: every point has failed once; destroy and stop
    check the error code and that the failure changed nothing
    destroy, and check that live and violations are 0
check that the loop stopped before the limit
uninstall
```

Copy `fails_at_every_allocation_point` in `tests/internal/vec_test.c`.

- **Stop only when it succeeded *and* `requests < n`.** Success with `requests >= n` means the failure was swallowed.
- **After a failure, check [rule 7](../references/ownership.md#7-how-failures-are-returned):** the error code, nothing leaked, and existing objects unchanged and still usable.

## Verifying a change

Build and test with both a static and a shared library.

```sh
cmake -S . -B build-static -DCMAKE_BUILD_TYPE=Debug
cmake --build build-static
ctest --test-dir build-static --output-on-failure

cmake -S . -B build-shared -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=ON
cmake --build build-shared
ctest --test-dir build-shared --output-on-failure
```

- `ctest` also runs the [allocation check](conventions.md#the-allocation-check-argxallocations), and in the shared configuration the export check `argx.exports`, which compares the exported names with the `ARGX_API` declarations. To run it on its own: `cmake -DARGX_LIBRARY=<shared-library> -P cmake/ArgXCheckExports.cmake`.
- When you change an `assert`, also build with `-DCMAKE_BUILD_TYPE=Release` ([Checking preconditions](conventions.md#checking-preconditions)).
- Configuring fetches Naos from GitHub. To work offline, pass `-DFETCHCONTENT_SOURCE_DIR_NAOS=<build-directory>/_deps/naos-src`.
