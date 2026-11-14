# Testing

The kinds of tests, how to run them, and how to write and add them. What to run before you submit a change is in [Verifying a change](procedures.md#verifying-a-change).

## Kinds of tests

| Kind | Where | CTest name | Label | Registered with |
| --- | --- | --- | --- | --- |
| Public API | `tests/public/` | `<name>.<suite>.<test>` | `public` | `argx_add_test()` in `tests/CMakeLists.txt` |
| Internal functions | `tests/internal/` | `<name>.<suite>.<test>` | `internal` | `argx_add_test(... INTERNAL)` in `tests/CMakeLists.txt` |
| Examples | `examples/` | `example.<name>.<case>` | `example` | `argx_add_example_test()` in `examples/CMakeLists.txt` |
| Release scripts | `tests/release/` | `release.<name>` | `release` | `argx_add_release_test()` in `tests/CMakeLists.txt` |
| Allocation check | `cmake/ArgXCheckAllocations.cmake` | `argx.allocations` | — | `tests/CMakeLists.txt` |
| Export check (shared library only) | `cmake/ArgXCheckExports.cmake` | `argx.exports` | — | `tests/CMakeLists.txt` |
| Consumer check | `tests/consumer/` | Not a CTest test | — | Run by CI with `cmake -P` |

`tests/support/` holds no tests: it has the test allocator and the code shared by the tests.

The tests are built only when `ARGX_BUILD_TESTS` is `ON`, the default when you build the repository directly. Configuring then fetches Naos from GitHub.

## Running tests

Run all the tests of a build directory:

```sh
ctest --test-dir build --output-on-failure
```

With a multi-config generator such as Visual Studio, `ctest` without `-C` runs the Debug configuration ([Building and testing](../getting-started/installation.md#building-and-testing)).

Run some of them by label (`-L`) or by a regular expression on the name (`-R`):

```sh
ctest --test-dir build -L internal
ctest --test-dir build -R '^parse_'
```

The consumer check installs a built ArgX and builds a separate project against it, in each of the four ways of [Installation](../getting-started/installation.md). The build directory must be built, with `ARGX_INSTALL` `ON` (the default when you build the repository directly). `ARGX_CONFIG` must be a configuration of the build directory, and `ARGX_SHARED` must match `BUILD_SHARED_LIBS`. For the `build-static` directory of [Verifying a change](procedures.md#verifying-a-change):

```sh
cmake -D ARGX_BUILD_DIR=build-static -D ARGX_CONFIG=Debug -D ARGX_SHARED=OFF -P tests/consumer/run.cmake
```

## Adding a test of the public API

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

## Adding a test of an internal function

Create `tests/internal/example_internal_test.c`, named after the internal header, and include `"example_internal.h"`. The test is written like a public one.

```cmake
argx_add_test(example_internal SOURCES internal/example_internal_test.c INTERNAL)
```

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

### Rules of `argx_add_test()`

```cmake
argx_add_test(<name> SOURCES <source>... [INTERNAL])
```

- `<name>` uses letters, digits and underscores, and is unique across all tests. For a test of internal functions, it is the name of the internal header.
- Without `INTERNAL`, the test links to `ArgX::argx`; with it, to `argx_internal`.
- `<source>` is relative to `tests/`, including files from `tests/support/`.
- Tests are registered as `<name>.<suite>.<test>` with the label `public` or `internal`.

### Release tests

The tests in `tests/release/` run with `cmake -P`, need no build, and have the label `release`. To add one, create `tests/release/<name>_test.cmake` and add `argx_add_release_test(<name>)` to `tests/CMakeLists.txt`. On Windows, `file(WRITE)` turns LF into CRLF, so files whose bytes matter are written with `argx_write_bytes()` (`tools/release/ArgXReadBytes.cmake`). Set `ARGX_WRITE_BYTES_WITH_CONFIGURE=1` to write them as on Windows on any host.

## Using the test allocator

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

- **Pair `install` with `uninstall`.** The state is shared by all tests of one executable, so restore the default allocator before the test ends.
- **Call `reset` only while nothing is allocated.**
- **End every test by checking that `live` and `violations` are 0.**
- **Never pass huge sizes to the standard allocation functions;** ASan and valgrind abort. Use an allocator that records its arguments and returns `NULL`.
- **Check both sides of an overflow check.** For the side that just fits, use `argx_test_allocator_fail_at(1)` and check that exactly one request reached the allocator.

To see the kind or size of each request instead, use the wrap allocator (`tests/support/wrap_allocator.h`). It forwards to the standard functions and counts the calls.

### The failure of every allocation in turn

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
