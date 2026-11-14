/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * Tests of the internal memory functions: "alloc_internal.h".
 *
 * The tests use the test allocator ("../support/test_allocator.h") to see
 * which requests reach the allocator that is set: its count of requests
 * rises only when the library calls it, its count of live blocks shows what
 * was freed, and its count of violations shows a call that the library must
 * never make.
 *
 * The state of the test allocator and the allocator that is set are shared
 * by all the tests of this executable. Every test starts with a reset (or
 * an install, which resets), frees what it allocated, sets the default
 * allocator back before it ends, and ends with check_clean().
 */
#include <argx/argx.h>

#include "alloc_internal.h"

#include "../support/test_allocator.h"
#include "../support/wrap_allocator.h"

#include <naos/naos.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* More than any block that the tests read back. */
#define ZERO_COUNT 128

static const unsigned char zeros[ZERO_COUNT] = { 0 };

/*
 * A calloc_fn that allocates nothing: it records its arguments and the
 * number of its calls, and returns NULL. With it, a test can see a request
 * that no real allocator could serve.
 */
static size_t probe_calloc_calls;
static size_t probe_calloc_count;
static size_t probe_calloc_size;

static void *probe_calloc(size_t count, size_t size)
{
    ++probe_calloc_calls;
    probe_calloc_count = count;
    probe_calloc_size = size;
    return NULL;
}

/* Writes a pattern that depends on the position and on seed. */
static void fill(unsigned char *bytes, size_t size, unsigned char seed)
{
    size_t i;

    for (i = 0; i < size; ++i)
    {
        bytes[i] = (unsigned char)(seed + (i * 7u));
    }
}

/*
 * The end of every test: the test allocator has no live block and saw no
 * violation of the allocator contract.
 */
static void check_clean(void)
{
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_violations(), 0);
}

/* ---- The default allocator ------------------------------------------- */

/*
 * With nothing set, the four functions work. This test is the first one, and
 * it does not call argx_set_allocator().
 */
NAOS_TEST(works_when_nothing_is_set)
{
    unsigned char expected[16];
    unsigned char *block;
    unsigned char *dirty;
    unsigned char *cleared;
    unsigned char *grown;

    argx_test_allocator_reset();
    fill(expected, sizeof expected, 3);

    block = argx_malloc(sizeof expected);
    NAOS_REQUIRE_TRUE(block != NULL);
    memcpy(block, expected, sizeof expected);
    NAOS_CHECK_MEM_EQ(block, sizeof expected, expected, sizeof expected);

    /*
     * Leave a freed block that is not 0 behind, for the allocation below
     * to reuse if it can.
     */
    dirty = argx_malloc(32);
    NAOS_REQUIRE_TRUE(dirty != NULL);
    memset(dirty, 0xA5, 32);
    argx_free(dirty);

    cleared = argx_calloc(4, 8);
    NAOS_REQUIRE_TRUE(cleared != NULL);
    NAOS_CHECK_MEM_EQ(cleared, 32, zeros, 32);

    grown = argx_realloc(block, 4096);
    NAOS_REQUIRE_TRUE(grown != NULL);
    NAOS_CHECK_MEM_EQ(grown, sizeof expected, expected, sizeof expected);
    memset(grown, 0x5A, 4096);

    argx_free(cleared);
    argx_free(grown);

    /* Nothing went to the test allocator: it was never set. */
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 0);
    check_clean();
}

/*
 * With the default allocator, the special cases give the same results as
 * with an allocator that was set.
 */
NAOS_TEST(default_handles_the_special_cases)
{
    unsigned char *block;

    argx_test_allocator_reset();

    NAOS_CHECK_TRUE(argx_malloc(0) == NULL);
    NAOS_CHECK_TRUE(argx_calloc(0, 8) == NULL);
    NAOS_CHECK_TRUE(argx_calloc(8, 0) == NULL);
    NAOS_CHECK_TRUE(argx_calloc(SIZE_MAX, 2) == NULL);
    NAOS_CHECK_TRUE(argx_realloc(NULL, 0) == NULL);
    argx_free(NULL);

    block = argx_realloc(NULL, 8);
    NAOS_REQUIRE_TRUE(block != NULL);
    memset(block, 1, 8);
    NAOS_CHECK_TRUE(argx_realloc(block, 0) == NULL);

    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 0);
    check_clean();
}

/* ---- The allocator that is set --------------------------------------- */

/* After install, every request reaches the test allocator. */
NAOS_TEST(install_routes_the_requests)
{
    unsigned char *block;
    unsigned char *cleared;
    unsigned char *grown;

    argx_test_allocator_install();

    block = argx_malloc(16);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 1);
    NAOS_REQUIRE_TRUE(block != NULL);
    memset(block, 1, 16);

    cleared = argx_calloc(2, 8);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 2);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 2);
    NAOS_REQUIRE_TRUE(cleared != NULL);

    grown = argx_realloc(block, 64);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 3);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 2);
    NAOS_REQUIRE_TRUE(grown != NULL);

    argx_free(cleared);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 1);
    argx_free(grown);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);

    /* free_fn is not a request. */
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 3);

    argx_test_allocator_uninstall();
    check_clean();
}

/* Each internal function calls the function of the same kind. */
NAOS_TEST(calls_the_matching_function)
{
    argx_allocator_t allocator = argx_wrap_allocator_get();
    unsigned char *block;
    unsigned char *cleared;
    unsigned char *grown;

    argx_test_allocator_reset();
    argx_wrap_allocator_reset();
    argx_set_allocator(&allocator);

    block = argx_malloc(16);
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_counts().malloc_calls, 1);
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_calls(), 1);
    NAOS_REQUIRE_TRUE(block != NULL);
    memset(block, 1, 16);

    cleared = argx_calloc(2, 8);
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_counts().calloc_calls, 1);
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_calls(), 2);
    NAOS_REQUIRE_TRUE(cleared != NULL);

    grown = argx_realloc(block, 64);
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_counts().realloc_calls, 1);
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_calls(), 3);
    NAOS_REQUIRE_TRUE(grown != NULL);

    argx_free(cleared);
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_counts().free_calls, 1);
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_calls(), 4);
    argx_free(grown);
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_counts().free_calls, 2);
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_calls(), 5);

    argx_set_allocator(NULL);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 0);
    check_clean();
}

/* After uninstall, no request reaches the test allocator. */
NAOS_TEST(uninstall_stops_the_requests)
{
    unsigned char *block;
    unsigned char *cleared;
    unsigned char *grown;

    argx_test_allocator_install();
    block = argx_malloc(16);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 1);
    argx_free(block);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);

    argx_test_allocator_uninstall();

    block = argx_malloc(16);
    NAOS_REQUIRE_TRUE(block != NULL);
    memset(block, 1, 16);
    cleared = argx_calloc(2, 8);
    NAOS_REQUIRE_TRUE(cleared != NULL);
    grown = argx_realloc(block, 64);
    NAOS_REQUIRE_TRUE(grown != NULL);

    /* The counts are those from before the uninstall. */
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);

    argx_free(cleared);
    argx_free(grown);
    check_clean();
}

/*
 * After argx_set_allocator(NULL), no request reaches the allocator that
 * was set before.
 */
NAOS_TEST(null_stops_the_requests)
{
    argx_allocator_t allocator = argx_test_allocator_get();
    unsigned char *block;
    unsigned char *cleared;
    unsigned char *grown;

    argx_test_allocator_reset();
    argx_set_allocator(&allocator);
    block = argx_malloc(16);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 1);
    argx_free(block);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);

    argx_set_allocator(NULL);

    block = argx_malloc(16);
    NAOS_REQUIRE_TRUE(block != NULL);
    memset(block, 1, 16);
    cleared = argx_calloc(2, 8);
    NAOS_REQUIRE_TRUE(cleared != NULL);
    grown = argx_realloc(block, 64);
    NAOS_REQUIRE_TRUE(grown != NULL);

    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);

    argx_free(cleared);
    argx_free(grown);
    check_clean();
}

/* An allocator can be set again after a reset to the default. */
NAOS_TEST(routes_again_after_a_reset)
{
    unsigned char *block;

    argx_test_allocator_install();
    argx_test_allocator_uninstall();
    argx_test_allocator_install();

    block = argx_malloc(16);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 1);
    argx_free(block);

    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * The library keeps a copy: changing the caller's structure after the call
 * does not change the allocator that is set.
 */
NAOS_TEST(keeps_a_copy_of_the_structure)
{
    argx_allocator_t allocator = argx_test_allocator_get();
    unsigned char *block;
    unsigned char *cleared;
    unsigned char *grown;

    argx_test_allocator_reset();
    argx_wrap_allocator_reset();
    argx_set_allocator(&allocator);

    /* The structure now names other functions. They must not be called. */
    allocator = argx_wrap_allocator_get();

    block = argx_malloc(16);
    NAOS_REQUIRE_TRUE(block != NULL);
    memset(block, 1, 16);
    cleared = argx_calloc(2, 8);
    NAOS_REQUIRE_TRUE(cleared != NULL);
    grown = argx_realloc(block, 64);
    NAOS_REQUIRE_TRUE(grown != NULL);

    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 3);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 2);

    argx_free(cleared);
    argx_free(grown);

    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_calls(), 0);
    NAOS_CHECK_TRUE(allocator.malloc_fn == argx_wrap_allocator_get().malloc_fn);

    argx_set_allocator(NULL);
    check_clean();
}

/* ---- The contract of the calls --------------------------------------- */

/* The memory of argx_calloc is all 0. */
NAOS_TEST(calloc_returns_zeroed_memory)
{
    unsigned char *dirty;
    unsigned char *cleared;

    argx_test_allocator_install();

    /*
     * Leave a freed block that is not 0 behind, for the allocation below
     * to reuse if it can.
     */
    dirty = argx_malloc(ZERO_COUNT);
    NAOS_REQUIRE_TRUE(dirty != NULL);
    memset(dirty, 0xA5, ZERO_COUNT);
    argx_free(dirty);

    cleared = argx_calloc(ZERO_COUNT / 4, 4);
    NAOS_REQUIRE_TRUE(cleared != NULL);
    NAOS_CHECK_MEM_EQ(cleared, ZERO_COUNT, zeros, ZERO_COUNT);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 2);
    argx_free(cleared);

    argx_test_allocator_uninstall();
    check_clean();
}

/* argx_malloc(0) returns NULL and does not call the allocator. */
NAOS_TEST(malloc_of_size_0_is_not_forwarded)
{
    argx_test_allocator_install();

    NAOS_CHECK_TRUE(argx_malloc(0) == NULL);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 0);

    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * argx_calloc with a count or a size of 0 returns NULL and does not call
 * the allocator.
 */
NAOS_TEST(calloc_of_count_or_size_0_is_not_forwarded)
{
    argx_test_allocator_install();

    NAOS_CHECK_TRUE(argx_calloc(0, 8) == NULL);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 0);
    NAOS_CHECK_TRUE(argx_calloc(8, 0) == NULL);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 0);
    NAOS_CHECK_TRUE(argx_calloc(0, 0) == NULL);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 0);
    NAOS_CHECK_TRUE(argx_calloc(0, SIZE_MAX) == NULL);
    NAOS_CHECK_TRUE(argx_calloc(SIZE_MAX, 0) == NULL);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 0);

    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * When count * size does not fit in a size_t, argx_calloc returns NULL
 * and does not call the allocator.
 */
NAOS_TEST(calloc_overflow_is_not_forwarded)
{
    argx_test_allocator_install();

    NAOS_CHECK_TRUE(argx_calloc(SIZE_MAX, 2) == NULL);
    NAOS_CHECK_TRUE(argx_calloc(2, SIZE_MAX) == NULL);
    NAOS_CHECK_TRUE(argx_calloc(SIZE_MAX, SIZE_MAX) == NULL);
    /* The smallest products that do not fit. */
    NAOS_CHECK_TRUE(argx_calloc((SIZE_MAX / 2) + 1, 2) == NULL);
    NAOS_CHECK_TRUE(argx_calloc(2, (SIZE_MAX / 2) + 1) == NULL);
    NAOS_CHECK_TRUE(argx_calloc((SIZE_MAX / 16) + 1, 16) == NULL);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 0);

    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * A product that fits is forwarded, however large, with the arguments as
 * they were passed: only the overflow is refused by the library. The
 * calloc_fn of this test allocates nothing, so no such block is requested
 * from the standard functions.
 */
NAOS_TEST(calloc_forwards_the_largest_product)
{
    argx_allocator_t allocator = argx_wrap_allocator_get();

    argx_test_allocator_reset();
    argx_wrap_allocator_reset();
    probe_calloc_calls = 0;
    probe_calloc_count = 0;
    probe_calloc_size = 0;
    allocator.calloc_fn = probe_calloc;
    argx_set_allocator(&allocator);

    NAOS_CHECK_TRUE(argx_calloc(SIZE_MAX, 1) == NULL);
    NAOS_CHECK_UINT_EQ(probe_calloc_calls, 1);
    NAOS_CHECK_UINT_EQ(probe_calloc_count, SIZE_MAX);
    NAOS_CHECK_UINT_EQ(probe_calloc_size, 1);

    NAOS_CHECK_TRUE(argx_calloc(SIZE_MAX / 2, 2) == NULL);
    NAOS_CHECK_UINT_EQ(probe_calloc_calls, 2);
    NAOS_CHECK_UINT_EQ(probe_calloc_count, SIZE_MAX / 2);
    NAOS_CHECK_UINT_EQ(probe_calloc_size, 2);

    /* Nothing else was called: no other request, and no free of NULL. */
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_calls(), 0);

    argx_set_allocator(NULL);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 0);
    check_clean();
}

/*
 * argx_realloc(NULL, size) allocates, through malloc_fn: realloc_fn is
 * never called with NULL.
 */
NAOS_TEST(realloc_of_null_allocates)
{
    unsigned char expected[32];
    unsigned char *block;

    argx_test_allocator_install();
    fill(expected, sizeof expected, 9);

    block = argx_realloc(NULL, sizeof expected);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 1);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_violations(), 0);
    NAOS_REQUIRE_TRUE(block != NULL);
    memcpy(block, expected, sizeof expected);
    NAOS_CHECK_MEM_EQ(block, sizeof expected, expected, sizeof expected);

    argx_free(block);
    argx_test_allocator_uninstall();
    check_clean();
}

/* argx_realloc(NULL, 0) is argx_malloc(0): NULL, and no call. */
NAOS_TEST(realloc_of_null_and_size_0_is_not_forwarded)
{
    argx_test_allocator_install();

    NAOS_CHECK_TRUE(argx_realloc(NULL, 0) == NULL);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 0);

    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * argx_realloc(ptr, 0) frees the block and returns NULL: realloc_fn is
 * never called with a size of 0.
 */
NAOS_TEST(realloc_to_size_0_frees)
{
    unsigned char *block;

    argx_test_allocator_install();

    block = argx_malloc(16);
    NAOS_REQUIRE_TRUE(block != NULL);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 1);

    NAOS_CHECK_TRUE(argx_realloc(block, 0) == NULL);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);
    /* The block was freed with free_fn, not with a request. */
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_violations(), 0);

    argx_test_allocator_uninstall();
    check_clean();
}

/* argx_realloc keeps the contents up to the smaller of the two sizes. */
NAOS_TEST(realloc_keeps_the_contents)
{
    unsigned char expected[48];
    unsigned char *block;

    argx_test_allocator_install();
    fill(expected, sizeof expected, 17);

    block = argx_malloc(sizeof expected);
    NAOS_REQUIRE_TRUE(block != NULL);
    memcpy(block, expected, sizeof expected);

    block = argx_realloc(block, 8192);
    NAOS_REQUIRE_TRUE(block != NULL);
    NAOS_CHECK_MEM_EQ(block, sizeof expected, expected, sizeof expected);

    block = argx_realloc(block, 12);
    NAOS_REQUIRE_TRUE(block != NULL);
    NAOS_CHECK_MEM_EQ(block, 12, expected, 12);

    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 3);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 1);

    argx_free(block);
    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * When the reallocation fails, argx_realloc returns NULL and the old
 * block is still valid and unchanged.
 */
NAOS_TEST(failed_realloc_keeps_the_old_block)
{
    unsigned char expected[40];
    unsigned char *block;
    unsigned char *grown;

    argx_test_allocator_install();
    fill(expected, sizeof expected, 29);

    block = argx_malloc(sizeof expected);
    NAOS_REQUIRE_TRUE(block != NULL);
    memcpy(block, expected, sizeof expected);

    argx_test_allocator_fail_at(2);
    grown = argx_realloc(block, 4096);
    NAOS_CHECK_TRUE(grown == NULL);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 2);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 1);
    NAOS_CHECK_MEM_EQ(block, sizeof expected, expected, sizeof expected);

    /* The old block can still be reallocated, with its contents. */
    grown = argx_realloc(block, 4096);
    NAOS_REQUIRE_TRUE(grown != NULL);
    NAOS_CHECK_MEM_EQ(grown, sizeof expected, expected, sizeof expected);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 1);

    argx_free(grown);
    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * When the allocation fails, argx_malloc, argx_calloc and
 * argx_realloc(NULL, size) return NULL, and nothing is left allocated.
 */
NAOS_TEST(failed_allocations_return_null)
{
    argx_test_allocator_install();

    argx_test_allocator_fail_at(1);
    NAOS_CHECK_TRUE(argx_malloc(16) == NULL);
    argx_test_allocator_fail_at(2);
    NAOS_CHECK_TRUE(argx_calloc(2, 8) == NULL);
    argx_test_allocator_fail_at(3);
    NAOS_CHECK_TRUE(argx_realloc(NULL, 16) == NULL);

    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 3);

    argx_test_allocator_uninstall();
    check_clean();
}

/* argx_free(NULL) does nothing: free_fn is never called with NULL. */
NAOS_TEST(free_of_null_does_nothing)
{
    unsigned char *block;

    argx_test_allocator_install();

    argx_free(NULL);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_violations(), 0);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 0);

    /* Also when a block is live: it stays. */
    block = argx_malloc(16);
    NAOS_REQUIRE_TRUE(block != NULL);
    argx_free(NULL);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_violations(), 0);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 1);
    argx_free(block);

    argx_test_allocator_uninstall();
    check_clean();
}

NAOS_SUITE(default_allocator,
    NAOS_ADD_TEST(works_when_nothing_is_set),
    NAOS_ADD_TEST(default_handles_the_special_cases)
)

NAOS_SUITE(set_allocator,
    NAOS_ADD_TEST(install_routes_the_requests),
    NAOS_ADD_TEST(calls_the_matching_function),
    NAOS_ADD_TEST(uninstall_stops_the_requests),
    NAOS_ADD_TEST(null_stops_the_requests),
    NAOS_ADD_TEST(routes_again_after_a_reset),
    NAOS_ADD_TEST(keeps_a_copy_of_the_structure)
)

NAOS_SUITE(contract,
    NAOS_ADD_TEST(calloc_returns_zeroed_memory),
    NAOS_ADD_TEST(malloc_of_size_0_is_not_forwarded),
    NAOS_ADD_TEST(calloc_of_count_or_size_0_is_not_forwarded),
    NAOS_ADD_TEST(calloc_overflow_is_not_forwarded),
    NAOS_ADD_TEST(calloc_forwards_the_largest_product),
    NAOS_ADD_TEST(realloc_of_null_allocates),
    NAOS_ADD_TEST(realloc_of_null_and_size_0_is_not_forwarded),
    NAOS_ADD_TEST(realloc_to_size_0_frees),
    NAOS_ADD_TEST(realloc_keeps_the_contents),
    NAOS_ADD_TEST(failed_realloc_keeps_the_old_block),
    NAOS_ADD_TEST(failed_allocations_return_null),
    NAOS_ADD_TEST(free_of_null_does_nothing)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(default_allocator),
    NAOS_ADD_SUITE(set_allocator),
    NAOS_ADD_SUITE(contract)
)
