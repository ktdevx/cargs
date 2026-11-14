/*
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
#include <clargs/clargs.h>

#include "alloc_internal.h"

#include "../support/test_allocator.h"

#include <naos/naos.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* More than any block that the tests read back. */
#define ZERO_COUNT 128

static const unsigned char zeros[ZERO_COUNT] = { 0 };

/*
 * An allocator that only wraps the standard functions. The wrappers count
 * their calls, so that a test can tell that they were called, and which of
 * them. They are functions of this file, not the standard functions
 * themselves, so that no initializer takes the address of an imported
 * function (MSVC warns about that).
 */
static size_t wrap_malloc_calls;
static size_t wrap_realloc_calls;
static size_t wrap_calloc_calls;
static size_t wrap_free_calls;

static void *wrap_malloc(size_t size)
{
    ++wrap_malloc_calls;
    return malloc(size);
}

static void *wrap_realloc(void *ptr, size_t size)
{
    ++wrap_realloc_calls;
    return realloc(ptr, size);
}

static void *wrap_calloc(size_t count, size_t size)
{
    ++wrap_calloc_calls;
    return calloc(count, size);
}

static void wrap_free(void *ptr)
{
    ++wrap_free_calls;
    free(ptr);
}

static clargs_allocator_t wrap_allocator(void)
{
    clargs_allocator_t allocator;

    allocator.malloc_fn = wrap_malloc;
    allocator.realloc_fn = wrap_realloc;
    allocator.calloc_fn = wrap_calloc;
    allocator.free_fn = wrap_free;
    return allocator;
}

static void wrap_reset(void)
{
    wrap_malloc_calls = 0;
    wrap_realloc_calls = 0;
    wrap_calloc_calls = 0;
    wrap_free_calls = 0;
}

static size_t wrap_calls(void)
{
    return wrap_malloc_calls + wrap_realloc_calls + wrap_calloc_calls + wrap_free_calls;
}

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
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
}

/* ---- The default allocator ------------------------------------------- */

/*
 * With nothing set, the four functions work. This test is the first one, and
 * it does not call clargs_set_allocator().
 */
NAOS_TEST(works_when_nothing_is_set)
{
    unsigned char expected[16];
    unsigned char *block;
    unsigned char *dirty;
    unsigned char *cleared;
    unsigned char *grown;

    clargs_test_allocator_reset();
    fill(expected, sizeof expected, 3);

    block = clargs_malloc(sizeof expected);
    NAOS_REQUIRE_TRUE(block != NULL);
    memcpy(block, expected, sizeof expected);
    NAOS_CHECK_MEM_EQ(block, sizeof expected, expected, sizeof expected);

    /*
     * Leave a freed block that is not 0 behind, for the allocation below
     * to reuse if it can.
     */
    dirty = clargs_malloc(32);
    NAOS_REQUIRE_TRUE(dirty != NULL);
    memset(dirty, 0xA5, 32);
    clargs_free(dirty);

    cleared = clargs_calloc(4, 8);
    NAOS_REQUIRE_TRUE(cleared != NULL);
    NAOS_CHECK_MEM_EQ(cleared, 32, zeros, 32);

    grown = clargs_realloc(block, 4096);
    NAOS_REQUIRE_TRUE(grown != NULL);
    NAOS_CHECK_MEM_EQ(grown, sizeof expected, expected, sizeof expected);
    memset(grown, 0x5A, 4096);

    clargs_free(cleared);
    clargs_free(grown);

    /* Nothing went to the test allocator: it was never set. */
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 0);
    check_clean();
}

/*
 * With the default allocator, the special cases give the same results as
 * with an allocator that was set.
 */
NAOS_TEST(default_handles_the_special_cases)
{
    unsigned char *block;

    clargs_test_allocator_reset();

    NAOS_CHECK_TRUE(clargs_malloc(0) == NULL);
    NAOS_CHECK_TRUE(clargs_calloc(0, 8) == NULL);
    NAOS_CHECK_TRUE(clargs_calloc(8, 0) == NULL);
    NAOS_CHECK_TRUE(clargs_calloc(SIZE_MAX, 2) == NULL);
    NAOS_CHECK_TRUE(clargs_realloc(NULL, 0) == NULL);
    clargs_free(NULL);

    block = clargs_realloc(NULL, 8);
    NAOS_REQUIRE_TRUE(block != NULL);
    memset(block, 1, 8);
    NAOS_CHECK_TRUE(clargs_realloc(block, 0) == NULL);

    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 0);
    check_clean();
}

/* ---- The allocator that is set --------------------------------------- */

/* After install, every request reaches the test allocator. */
NAOS_TEST(install_routes_the_requests)
{
    unsigned char *block;
    unsigned char *cleared;
    unsigned char *grown;

    clargs_test_allocator_install();

    block = clargs_malloc(16);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 1);
    NAOS_REQUIRE_TRUE(block != NULL);
    memset(block, 1, 16);

    cleared = clargs_calloc(2, 8);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 2);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 2);
    NAOS_REQUIRE_TRUE(cleared != NULL);

    grown = clargs_realloc(block, 64);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 3);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 2);
    NAOS_REQUIRE_TRUE(grown != NULL);

    clargs_free(cleared);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 1);
    clargs_free(grown);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);

    /* free_fn is not a request. */
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 3);

    clargs_test_allocator_uninstall();
    check_clean();
}

/* Each internal function calls the function of the same kind. */
NAOS_TEST(calls_the_matching_function)
{
    clargs_allocator_t allocator = wrap_allocator();
    unsigned char *block;
    unsigned char *cleared;
    unsigned char *grown;

    clargs_test_allocator_reset();
    wrap_reset();
    clargs_set_allocator(&allocator);

    block = clargs_malloc(16);
    NAOS_CHECK_UINT_EQ(wrap_malloc_calls, 1);
    NAOS_CHECK_UINT_EQ(wrap_calls(), 1);
    NAOS_REQUIRE_TRUE(block != NULL);
    memset(block, 1, 16);

    cleared = clargs_calloc(2, 8);
    NAOS_CHECK_UINT_EQ(wrap_calloc_calls, 1);
    NAOS_CHECK_UINT_EQ(wrap_calls(), 2);
    NAOS_REQUIRE_TRUE(cleared != NULL);

    grown = clargs_realloc(block, 64);
    NAOS_CHECK_UINT_EQ(wrap_realloc_calls, 1);
    NAOS_CHECK_UINT_EQ(wrap_calls(), 3);
    NAOS_REQUIRE_TRUE(grown != NULL);

    clargs_free(cleared);
    NAOS_CHECK_UINT_EQ(wrap_free_calls, 1);
    NAOS_CHECK_UINT_EQ(wrap_calls(), 4);
    clargs_free(grown);
    NAOS_CHECK_UINT_EQ(wrap_free_calls, 2);
    NAOS_CHECK_UINT_EQ(wrap_calls(), 5);

    clargs_set_allocator(NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 0);
    check_clean();
}

/* After uninstall, no request reaches the test allocator. */
NAOS_TEST(uninstall_stops_the_requests)
{
    unsigned char *block;
    unsigned char *cleared;
    unsigned char *grown;

    clargs_test_allocator_install();
    block = clargs_malloc(16);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 1);
    clargs_free(block);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);

    clargs_test_allocator_uninstall();

    block = clargs_malloc(16);
    NAOS_REQUIRE_TRUE(block != NULL);
    memset(block, 1, 16);
    cleared = clargs_calloc(2, 8);
    NAOS_REQUIRE_TRUE(cleared != NULL);
    grown = clargs_realloc(block, 64);
    NAOS_REQUIRE_TRUE(grown != NULL);

    /* The counts are those from before the uninstall. */
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);

    clargs_free(cleared);
    clargs_free(grown);
    check_clean();
}

/*
 * After clargs_set_allocator(NULL), no request reaches the allocator that
 * was set before.
 */
NAOS_TEST(null_stops_the_requests)
{
    clargs_allocator_t allocator = clargs_test_allocator_get();
    unsigned char *block;
    unsigned char *cleared;
    unsigned char *grown;

    clargs_test_allocator_reset();
    clargs_set_allocator(&allocator);
    block = clargs_malloc(16);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 1);
    clargs_free(block);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);

    clargs_set_allocator(NULL);

    block = clargs_malloc(16);
    NAOS_REQUIRE_TRUE(block != NULL);
    memset(block, 1, 16);
    cleared = clargs_calloc(2, 8);
    NAOS_REQUIRE_TRUE(cleared != NULL);
    grown = clargs_realloc(block, 64);
    NAOS_REQUIRE_TRUE(grown != NULL);

    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);

    clargs_free(cleared);
    clargs_free(grown);
    check_clean();
}

/* An allocator can be set again after a reset to the default. */
NAOS_TEST(routes_again_after_a_reset)
{
    unsigned char *block;

    clargs_test_allocator_install();
    clargs_test_allocator_uninstall();
    clargs_test_allocator_install();

    block = clargs_malloc(16);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 1);
    clargs_free(block);

    clargs_test_allocator_uninstall();
    check_clean();
}

/*
 * The library keeps a copy: changing the caller's structure after the call
 * does not change the allocator that is set.
 */
NAOS_TEST(keeps_a_copy_of_the_structure)
{
    clargs_allocator_t allocator = clargs_test_allocator_get();
    unsigned char *block;
    unsigned char *cleared;
    unsigned char *grown;

    clargs_test_allocator_reset();
    wrap_reset();
    clargs_set_allocator(&allocator);

    /* The structure now names other functions. They must not be called. */
    allocator = wrap_allocator();

    block = clargs_malloc(16);
    NAOS_REQUIRE_TRUE(block != NULL);
    memset(block, 1, 16);
    cleared = clargs_calloc(2, 8);
    NAOS_REQUIRE_TRUE(cleared != NULL);
    grown = clargs_realloc(block, 64);
    NAOS_REQUIRE_TRUE(grown != NULL);

    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 3);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 2);

    clargs_free(cleared);
    clargs_free(grown);

    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(wrap_calls(), 0);
    NAOS_CHECK_TRUE(allocator.malloc_fn == wrap_malloc);

    clargs_set_allocator(NULL);
    check_clean();
}

/* ---- The contract of the calls --------------------------------------- */

/* The memory of clargs_calloc is all 0. */
NAOS_TEST(calloc_returns_zeroed_memory)
{
    unsigned char *dirty;
    unsigned char *cleared;

    clargs_test_allocator_install();

    /*
     * Leave a freed block that is not 0 behind, for the allocation below
     * to reuse if it can.
     */
    dirty = clargs_malloc(ZERO_COUNT);
    NAOS_REQUIRE_TRUE(dirty != NULL);
    memset(dirty, 0xA5, ZERO_COUNT);
    clargs_free(dirty);

    cleared = clargs_calloc(ZERO_COUNT / 4, 4);
    NAOS_REQUIRE_TRUE(cleared != NULL);
    NAOS_CHECK_MEM_EQ(cleared, ZERO_COUNT, zeros, ZERO_COUNT);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 2);
    clargs_free(cleared);

    clargs_test_allocator_uninstall();
    check_clean();
}

/* clargs_malloc(0) returns NULL and does not call the allocator. */
NAOS_TEST(malloc_of_size_0_is_not_forwarded)
{
    clargs_test_allocator_install();

    NAOS_CHECK_TRUE(clargs_malloc(0) == NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 0);

    clargs_test_allocator_uninstall();
    check_clean();
}

/*
 * clargs_calloc with a count or a size of 0 returns NULL and does not call
 * the allocator.
 */
NAOS_TEST(calloc_of_count_or_size_0_is_not_forwarded)
{
    clargs_test_allocator_install();

    NAOS_CHECK_TRUE(clargs_calloc(0, 8) == NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 0);
    NAOS_CHECK_TRUE(clargs_calloc(8, 0) == NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 0);
    NAOS_CHECK_TRUE(clargs_calloc(0, 0) == NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 0);
    NAOS_CHECK_TRUE(clargs_calloc(0, SIZE_MAX) == NULL);
    NAOS_CHECK_TRUE(clargs_calloc(SIZE_MAX, 0) == NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 0);

    clargs_test_allocator_uninstall();
    check_clean();
}

/*
 * When count * size does not fit in a size_t, clargs_calloc returns NULL
 * and does not call the allocator.
 */
NAOS_TEST(calloc_overflow_is_not_forwarded)
{
    clargs_test_allocator_install();

    NAOS_CHECK_TRUE(clargs_calloc(SIZE_MAX, 2) == NULL);
    NAOS_CHECK_TRUE(clargs_calloc(2, SIZE_MAX) == NULL);
    NAOS_CHECK_TRUE(clargs_calloc(SIZE_MAX, SIZE_MAX) == NULL);
    /* The smallest products that do not fit. */
    NAOS_CHECK_TRUE(clargs_calloc((SIZE_MAX / 2) + 1, 2) == NULL);
    NAOS_CHECK_TRUE(clargs_calloc(2, (SIZE_MAX / 2) + 1) == NULL);
    NAOS_CHECK_TRUE(clargs_calloc((SIZE_MAX / 16) + 1, 16) == NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 0);

    clargs_test_allocator_uninstall();
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
    clargs_allocator_t allocator = wrap_allocator();

    clargs_test_allocator_reset();
    wrap_reset();
    probe_calloc_calls = 0;
    probe_calloc_count = 0;
    probe_calloc_size = 0;
    allocator.calloc_fn = probe_calloc;
    clargs_set_allocator(&allocator);

    NAOS_CHECK_TRUE(clargs_calloc(SIZE_MAX, 1) == NULL);
    NAOS_CHECK_UINT_EQ(probe_calloc_calls, 1);
    NAOS_CHECK_UINT_EQ(probe_calloc_count, SIZE_MAX);
    NAOS_CHECK_UINT_EQ(probe_calloc_size, 1);

    NAOS_CHECK_TRUE(clargs_calloc(SIZE_MAX / 2, 2) == NULL);
    NAOS_CHECK_UINT_EQ(probe_calloc_calls, 2);
    NAOS_CHECK_UINT_EQ(probe_calloc_count, SIZE_MAX / 2);
    NAOS_CHECK_UINT_EQ(probe_calloc_size, 2);

    /* Nothing else was called: no other request, and no free of NULL. */
    NAOS_CHECK_UINT_EQ(wrap_calls(), 0);

    clargs_set_allocator(NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 0);
    check_clean();
}

/*
 * clargs_realloc(NULL, size) allocates, through malloc_fn: realloc_fn is
 * never called with NULL.
 */
NAOS_TEST(realloc_of_null_allocates)
{
    unsigned char expected[32];
    unsigned char *block;

    clargs_test_allocator_install();
    fill(expected, sizeof expected, 9);

    block = clargs_realloc(NULL, sizeof expected);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 1);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
    NAOS_REQUIRE_TRUE(block != NULL);
    memcpy(block, expected, sizeof expected);
    NAOS_CHECK_MEM_EQ(block, sizeof expected, expected, sizeof expected);

    clargs_free(block);
    clargs_test_allocator_uninstall();
    check_clean();
}

/* clargs_realloc(NULL, 0) is clargs_malloc(0): NULL, and no call. */
NAOS_TEST(realloc_of_null_and_size_0_is_not_forwarded)
{
    clargs_test_allocator_install();

    NAOS_CHECK_TRUE(clargs_realloc(NULL, 0) == NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 0);

    clargs_test_allocator_uninstall();
    check_clean();
}

/*
 * clargs_realloc(ptr, 0) frees the block and returns NULL: realloc_fn is
 * never called with a size of 0.
 */
NAOS_TEST(realloc_to_size_0_frees)
{
    unsigned char *block;

    clargs_test_allocator_install();

    block = clargs_malloc(16);
    NAOS_REQUIRE_TRUE(block != NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 1);

    NAOS_CHECK_TRUE(clargs_realloc(block, 0) == NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
    /* The block was freed with free_fn, not with a request. */
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);

    clargs_test_allocator_uninstall();
    check_clean();
}

/* clargs_realloc keeps the contents up to the smaller of the two sizes. */
NAOS_TEST(realloc_keeps_the_contents)
{
    unsigned char expected[48];
    unsigned char *block;

    clargs_test_allocator_install();
    fill(expected, sizeof expected, 17);

    block = clargs_malloc(sizeof expected);
    NAOS_REQUIRE_TRUE(block != NULL);
    memcpy(block, expected, sizeof expected);

    block = clargs_realloc(block, 8192);
    NAOS_REQUIRE_TRUE(block != NULL);
    NAOS_CHECK_MEM_EQ(block, sizeof expected, expected, sizeof expected);

    block = clargs_realloc(block, 12);
    NAOS_REQUIRE_TRUE(block != NULL);
    NAOS_CHECK_MEM_EQ(block, 12, expected, 12);

    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 3);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 1);

    clargs_free(block);
    clargs_test_allocator_uninstall();
    check_clean();
}

/*
 * When the reallocation fails, clargs_realloc returns NULL and the old
 * block is still valid and unchanged.
 */
NAOS_TEST(failed_realloc_keeps_the_old_block)
{
    unsigned char expected[40];
    unsigned char *block;
    unsigned char *grown;

    clargs_test_allocator_install();
    fill(expected, sizeof expected, 29);

    block = clargs_malloc(sizeof expected);
    NAOS_REQUIRE_TRUE(block != NULL);
    memcpy(block, expected, sizeof expected);

    clargs_test_allocator_fail_at(2);
    grown = clargs_realloc(block, 4096);
    NAOS_CHECK_TRUE(grown == NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 2);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 1);
    NAOS_CHECK_MEM_EQ(block, sizeof expected, expected, sizeof expected);

    /* The old block can still be reallocated, with its contents. */
    grown = clargs_realloc(block, 4096);
    NAOS_REQUIRE_TRUE(grown != NULL);
    NAOS_CHECK_MEM_EQ(grown, sizeof expected, expected, sizeof expected);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 1);

    clargs_free(grown);
    clargs_test_allocator_uninstall();
    check_clean();
}

/*
 * When the allocation fails, clargs_malloc, clargs_calloc and
 * clargs_realloc(NULL, size) return NULL, and nothing is left allocated.
 */
NAOS_TEST(failed_allocations_return_null)
{
    clargs_test_allocator_install();

    clargs_test_allocator_fail_at(1);
    NAOS_CHECK_TRUE(clargs_malloc(16) == NULL);
    clargs_test_allocator_fail_at(2);
    NAOS_CHECK_TRUE(clargs_calloc(2, 8) == NULL);
    clargs_test_allocator_fail_at(3);
    NAOS_CHECK_TRUE(clargs_realloc(NULL, 16) == NULL);

    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 3);

    clargs_test_allocator_uninstall();
    check_clean();
}

/* clargs_free(NULL) does nothing: free_fn is never called with NULL. */
NAOS_TEST(free_of_null_does_nothing)
{
    unsigned char *block;

    clargs_test_allocator_install();

    clargs_free(NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 0);

    /* Also when a block is live: it stays. */
    block = clargs_malloc(16);
    NAOS_REQUIRE_TRUE(block != NULL);
    clargs_free(NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 1);
    clargs_free(block);

    clargs_test_allocator_uninstall();
    check_clean();
}

/* ---- The default and a wrapper of the standard functions -------------- */

/* What one run of the sequence of operations saw. */
#define RECORD_CAPACITY 256

typedef struct record
{
    unsigned char bytes[RECORD_CAPACITY];
    size_t size;
    int overflowed;
} record_t;

static void record_bytes(record_t *record, const unsigned char *bytes, size_t size)
{
    if (size > (RECORD_CAPACITY - record->size))
    {
        record->overflowed = 1;
        return;
    }
    memcpy(record->bytes + record->size, bytes, size);
    record->size += size;
}

static void record_flag(record_t *record, int flag)
{
    unsigned char byte = (unsigned char)(flag ? 1 : 0);

    record_bytes(record, &byte, 1);
}

/*
 * Runs a fixed sequence of operations with the allocator that is set and
 * records every result that does not depend on the addresses: whether a
 * pointer is NULL, and the contents that the functions promise. Frees all
 * that it allocates.
 */
static void run_sequence(record_t *record)
{
    unsigned char *block;
    unsigned char *cleared;
    unsigned char *other;

    memset(record, 0, sizeof *record);

    block = clargs_malloc(24);
    record_flag(record, block != NULL);
    NAOS_REQUIRE_TRUE(block != NULL);
    fill(block, 24, 41);

    cleared = clargs_calloc(6, 4);
    record_flag(record, cleared != NULL);
    NAOS_REQUIRE_TRUE(cleared != NULL);
    record_bytes(record, cleared, 24);

    block = clargs_realloc(block, 200);
    record_flag(record, block != NULL);
    NAOS_REQUIRE_TRUE(block != NULL);
    record_bytes(record, block, 24);
    fill(block + 24, 200 - 24, 53);

    block = clargs_realloc(block, 40);
    record_flag(record, block != NULL);
    NAOS_REQUIRE_TRUE(block != NULL);
    record_bytes(record, block, 40);

    record_flag(record, clargs_malloc(0) != NULL);
    record_flag(record, clargs_calloc(0, 4) != NULL);
    record_flag(record, clargs_calloc(4, 0) != NULL);
    record_flag(record, clargs_calloc(SIZE_MAX, 2) != NULL);
    record_flag(record, clargs_realloc(NULL, 0) != NULL);

    other = clargs_realloc(NULL, 12);
    record_flag(record, other != NULL);
    NAOS_REQUIRE_TRUE(other != NULL);
    fill(other, 12, 67);
    record_bytes(record, other, 12);
    record_flag(record, clargs_realloc(other, 0) != NULL);

    clargs_free(NULL);
    clargs_free(cleared);
    clargs_free(block);
}

/*
 * The same sequence of operations gives the same results with the default
 * allocator and with an allocator that only wraps the standard functions.
 */
NAOS_TEST(default_and_wrapper_give_the_same_results)
{
    static record_t with_default;
    static record_t with_wrapper;
    clargs_allocator_t allocator = wrap_allocator();

    clargs_test_allocator_reset();
    wrap_reset();

    clargs_set_allocator(NULL);
    run_sequence(&with_default);
    /* The default allocator is not the wrapper. */
    NAOS_CHECK_UINT_EQ(wrap_calls(), 0);

    clargs_set_allocator(&allocator);
    run_sequence(&with_wrapper);
    clargs_set_allocator(NULL);

    /*
     * The wrapper was called, and only in the way the contract allows:
     * malloc, calloc, 2 x realloc and the malloc of realloc(NULL, 12);
     * the free of realloc(other, 0) and the two calls of free.
     */
    NAOS_CHECK_UINT_EQ(wrap_malloc_calls, 2);
    NAOS_CHECK_UINT_EQ(wrap_calloc_calls, 1);
    NAOS_CHECK_UINT_EQ(wrap_realloc_calls, 2);
    NAOS_CHECK_UINT_EQ(wrap_free_calls, 3);

    NAOS_CHECK_FALSE(with_default.overflowed);
    NAOS_CHECK_FALSE(with_wrapper.overflowed);
    NAOS_CHECK_UINT_GT(with_default.size, 0);
    NAOS_CHECK_MEM_EQ(with_default.bytes, with_default.size, with_wrapper.bytes, with_wrapper.size);

    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 0);
    check_clean();
}

/*
 * The results of the sequence are the ones that the functions promise, not
 * only equal to each other.
 */
NAOS_TEST(sequence_gives_the_promised_results)
{
    static record_t seen;
    static record_t expected;
    unsigned char pattern[200];

    clargs_test_allocator_install();
    run_sequence(&seen);
    clargs_test_allocator_uninstall();

    memset(&expected, 0, sizeof expected);
    fill(pattern, 24, 41);
    fill(pattern + 24, 200 - 24, 53);
    record_flag(&expected, 1);            /* malloc(24) */
    record_flag(&expected, 1);            /* calloc(6, 4) */
    record_bytes(&expected, zeros, 24);
    record_flag(&expected, 1);            /* realloc(block, 200) */
    record_bytes(&expected, pattern, 24);
    record_flag(&expected, 1);            /* realloc(block, 40) */
    record_bytes(&expected, pattern, 40);
    record_flag(&expected, 0);            /* malloc(0) */
    record_flag(&expected, 0);            /* calloc(0, 4) */
    record_flag(&expected, 0);            /* calloc(4, 0) */
    record_flag(&expected, 0);            /* calloc(SIZE_MAX, 2) */
    record_flag(&expected, 0);            /* realloc(NULL, 0) */
    record_flag(&expected, 1);            /* realloc(NULL, 12) */
    fill(pattern, 12, 67);
    record_bytes(&expected, pattern, 12);
    record_flag(&expected, 0);            /* realloc(other, 0) */

    NAOS_CHECK_FALSE(seen.overflowed);
    NAOS_CHECK_FALSE(expected.overflowed);
    NAOS_CHECK_MEM_EQ(seen.bytes, seen.size, expected.bytes, expected.size);

    /* malloc, calloc, 2 x realloc, and the malloc of realloc(NULL, 12). */
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 5);
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

NAOS_SUITE(equivalence,
    NAOS_ADD_TEST(default_and_wrapper_give_the_same_results),
    NAOS_ADD_TEST(sequence_gives_the_promised_results)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(default_allocator),
    NAOS_ADD_SUITE(set_allocator),
    NAOS_ADD_SUITE(contract),
    NAOS_ADD_SUITE(equivalence)
)
