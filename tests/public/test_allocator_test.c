/*
 * Tests of the test allocator itself: "../support/test_allocator.h".
 *
 * The test allocator is test support, not a part of the library. No public
 * function of the library allocates memory yet, so these tests call the
 * four functions of clargs_test_allocator_get() directly. The tests of the
 * internal memory functions check that install and uninstall really route
 * the allocations of the library; here they are only exercised.
 *
 * The state of the test allocator is shared by all the tests of this
 * executable. Every test starts with a reset (or an install, which resets),
 * frees what it allocated, and uninstalls what it installed.
 */
#include <clargs/clargs.h>

#include "../support/test_allocator.h"

#include <naos/naos.h>

#include <stddef.h>
#include <string.h>

/* get returns four functions and does not change the state. */
NAOS_TEST(get_returns_the_four_functions)
{
    clargs_allocator_t allocator;

    clargs_test_allocator_reset();
    allocator = clargs_test_allocator_get();

    NAOS_CHECK_TRUE(allocator.malloc_fn != NULL);
    NAOS_CHECK_TRUE(allocator.realloc_fn != NULL);
    NAOS_CHECK_TRUE(allocator.calloc_fn != NULL);
    NAOS_CHECK_TRUE(allocator.free_fn != NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
}

/* Every call of get returns the same four functions. */
NAOS_TEST(get_returns_the_same_functions_every_time)
{
    clargs_allocator_t first = clargs_test_allocator_get();
    clargs_allocator_t second = clargs_test_allocator_get();

    NAOS_CHECK_TRUE(first.malloc_fn == second.malloc_fn);
    NAOS_CHECK_TRUE(first.realloc_fn == second.realloc_fn);
    NAOS_CHECK_TRUE(first.calloc_fn == second.calloc_fn);
    NAOS_CHECK_TRUE(first.free_fn == second.free_fn);
}

/* Without a request to fail, every kind of request succeeds. */
NAOS_TEST(succeeds_when_no_failure_is_set)
{
    clargs_allocator_t allocator = clargs_test_allocator_get();
    void *blocks[8];
    void *grown;
    size_t i;

    clargs_test_allocator_reset();

    for (i = 0; i < 8; ++i)
    {
        blocks[i] = (i % 2 == 0) ? allocator.malloc_fn(16) : allocator.calloc_fn(4, 4);
        NAOS_CHECK_TRUE(blocks[i] != NULL);
    }
    for (i = 0; i < 8; ++i)
    {
        grown = allocator.realloc_fn(blocks[i], 64);
        NAOS_CHECK_TRUE(grown != NULL);
        if (grown != NULL)
        {
            blocks[i] = grown;
        }
    }
    for (i = 0; i < 8; ++i)
    {
        allocator.free_fn(blocks[i]);
    }

    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 16);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
}

/* fail_at(n) fails the n-th request and only that one: malloc_fn. */
NAOS_TEST(fails_only_the_requested_malloc)
{
    clargs_allocator_t allocator = clargs_test_allocator_get();
    void *first;
    void *second;
    void *third;

    clargs_test_allocator_reset();
    clargs_test_allocator_fail_at(2);

    first = allocator.malloc_fn(8);
    second = allocator.malloc_fn(8);
    third = allocator.malloc_fn(8);

    NAOS_CHECK_TRUE(first != NULL);
    NAOS_CHECK_TRUE(second == NULL);
    NAOS_CHECK_TRUE(third != NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 3);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 2);

    allocator.free_fn(first);
    allocator.free_fn(third);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
}

/* fail_at(n) fails the n-th request and only that one: calloc_fn. */
NAOS_TEST(fails_only_the_requested_calloc)
{
    clargs_allocator_t allocator = clargs_test_allocator_get();
    void *first;
    void *second;
    void *third;

    clargs_test_allocator_reset();
    clargs_test_allocator_fail_at(1);

    first = allocator.calloc_fn(2, 8);
    second = allocator.calloc_fn(2, 8);
    third = allocator.calloc_fn(2, 8);

    NAOS_CHECK_TRUE(first == NULL);
    NAOS_CHECK_TRUE(second != NULL);
    NAOS_CHECK_TRUE(third != NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 3);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 2);

    allocator.free_fn(second);
    allocator.free_fn(third);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
}

/*
 * fail_at(n) fails the n-th request and only that one: realloc_fn. The
 * block of the failed realloc_fn stays valid and keeps its contents.
 */
NAOS_TEST(fails_only_the_requested_realloc_and_keeps_the_block)
{
    static const char text[] = "the original contents";
    clargs_allocator_t allocator = clargs_test_allocator_get();
    char *block;
    char *failed;
    char *grown;

    clargs_test_allocator_reset();
    clargs_test_allocator_fail_at(3);

    block = allocator.malloc_fn(sizeof text);
    NAOS_REQUIRE_TRUE(block != NULL);
    memcpy(block, text, sizeof text);

    grown = allocator.realloc_fn(block, 64);
    NAOS_REQUIRE_TRUE(grown != NULL);
    block = grown;

    failed = allocator.realloc_fn(block, 4096);
    NAOS_CHECK_TRUE(failed == NULL);
    NAOS_CHECK_MEM_EQ(block, sizeof text, text, sizeof text);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 1);

    grown = allocator.realloc_fn(block, 4096);
    NAOS_REQUIRE_TRUE(grown != NULL);
    block = grown;
    NAOS_CHECK_MEM_EQ(block, sizeof text, text, sizeof text);

    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 4);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 1);

    allocator.free_fn(block);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
}

/* The request to fail is counted from the last reset, across the kinds. */
NAOS_TEST(counts_the_request_to_fail_across_the_kinds)
{
    clargs_allocator_t allocator = clargs_test_allocator_get();
    void *first;
    void *second;
    void *third;
    void *fourth;

    clargs_test_allocator_reset();

    first = allocator.malloc_fn(8);
    NAOS_REQUIRE_TRUE(first != NULL);
    clargs_test_allocator_fail_at(3);
    second = allocator.realloc_fn(first, 16);
    NAOS_REQUIRE_TRUE(second != NULL);
    third = allocator.calloc_fn(1, 8);
    fourth = allocator.calloc_fn(1, 8);

    NAOS_CHECK_TRUE(third == NULL);
    NAOS_CHECK_TRUE(fourth != NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 4);

    allocator.free_fn(second);
    allocator.free_fn(fourth);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
}

/* fail_at(0) takes a request to fail back. */
NAOS_TEST(fail_at_zero_fails_no_request)
{
    clargs_allocator_t allocator = clargs_test_allocator_get();
    void *first;
    void *second;

    clargs_test_allocator_reset();
    clargs_test_allocator_fail_at(1);
    clargs_test_allocator_fail_at(0);

    first = allocator.malloc_fn(8);
    second = allocator.malloc_fn(8);

    NAOS_CHECK_TRUE(first != NULL);
    NAOS_CHECK_TRUE(second != NULL);

    allocator.free_fn(first);
    allocator.free_fn(second);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
}

/* calloc_fn returns count * size bytes that are all 0. */
NAOS_TEST(calloc_returns_zeroed_memory)
{
    enum { COUNT = 37, SIZE = 11 };
    clargs_allocator_t allocator = clargs_test_allocator_get();
    unsigned char zeros[COUNT * SIZE];
    unsigned char *block;

    clargs_test_allocator_reset();
    memset(zeros, 0, sizeof zeros);

    /* Dirty a block of the same size first, so that a reused block shows. */
    block = allocator.malloc_fn(sizeof zeros);
    NAOS_REQUIRE_TRUE(block != NULL);
    memset(block, 0xA5, sizeof zeros);
    allocator.free_fn(block);

    block = allocator.calloc_fn(COUNT, SIZE);
    NAOS_REQUIRE_TRUE(block != NULL);
    NAOS_CHECK_MEM_EQ(block, sizeof zeros, zeros, sizeof zeros);

    allocator.free_fn(block);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
}

/* A realloc_fn that succeeds keeps the contents. */
NAOS_TEST(realloc_keeps_the_contents)
{
    static const char text[] = "kept";
    clargs_allocator_t allocator = clargs_test_allocator_get();
    char *block;
    char *grown;

    clargs_test_allocator_reset();

    block = allocator.malloc_fn(sizeof text);
    NAOS_REQUIRE_TRUE(block != NULL);
    memcpy(block, text, sizeof text);

    grown = allocator.realloc_fn(block, 100000);
    NAOS_REQUIRE_TRUE(grown != NULL);
    NAOS_CHECK_MEM_EQ(grown, sizeof text, text, sizeof text);

    allocator.free_fn(grown);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
}

/* requests counts the three kinds of requests, the failed ones included. */
NAOS_TEST(counts_the_requests)
{
    clargs_allocator_t allocator = clargs_test_allocator_get();
    void *block;
    void *other;
    void *grown;

    clargs_test_allocator_reset();
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 0);

    block = allocator.malloc_fn(8);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 1);

    other = allocator.calloc_fn(1, 8);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 2);

    grown = allocator.realloc_fn(block, 32);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 3);
    if (grown != NULL)
    {
        block = grown;
    }

    clargs_test_allocator_fail_at(4);
    NAOS_CHECK_TRUE(allocator.malloc_fn(8) == NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 4);

    /* free_fn is not a request. */
    allocator.free_fn(block);
    allocator.free_fn(other);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 4);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
}

/*
 * live goes up by one with an allocation that succeeds, down by one with a
 * free, and stays with a reallocation or a failed request.
 */
NAOS_TEST(counts_the_live_blocks)
{
    clargs_allocator_t allocator = clargs_test_allocator_get();
    void *first;
    void *second;
    void *grown;

    clargs_test_allocator_reset();
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);

    first = allocator.malloc_fn(8);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 1);

    second = allocator.calloc_fn(2, 8);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 2);

    grown = allocator.realloc_fn(first, 128);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 2);
    if (grown != NULL)
    {
        first = grown;
    }

    clargs_test_allocator_fail_at(4);
    NAOS_CHECK_TRUE(allocator.malloc_fn(8) == NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 2);

    clargs_test_allocator_fail_at(5);
    NAOS_CHECK_TRUE(allocator.realloc_fn(second, 128) == NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 2);

    allocator.free_fn(first);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 1);

    allocator.free_fn(second);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
}

/* A request of size 0 or count 0 is a violation and allocates nothing. */
NAOS_TEST(counts_zero_sized_requests_as_violations)
{
    clargs_allocator_t allocator = clargs_test_allocator_get();
    void *block;

    clargs_test_allocator_reset();
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);

    NAOS_CHECK_TRUE(allocator.malloc_fn(0) == NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 1);

    NAOS_CHECK_TRUE(allocator.calloc_fn(0, 8) == NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 2);

    NAOS_CHECK_TRUE(allocator.calloc_fn(8, 0) == NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 3);

    /* The violating requests are requests, and none of them left a block. */
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 3);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);

    /* A realloc_fn to size 0 is a violation too, and keeps the block. */
    block = allocator.malloc_fn(4);
    NAOS_REQUIRE_TRUE(block != NULL);
    memcpy(block, "abc", 4);
    NAOS_CHECK_TRUE(allocator.realloc_fn(block, 0) == NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 4);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 1);
    NAOS_CHECK_MEM_EQ(block, 4, "abc", 4);

    allocator.free_fn(block);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 4);
}

/* A realloc_fn of NULL and a free_fn of NULL are violations. */
NAOS_TEST(counts_null_pointers_as_violations)
{
    clargs_allocator_t allocator = clargs_test_allocator_get();

    clargs_test_allocator_reset();

    NAOS_CHECK_TRUE(allocator.realloc_fn(NULL, 8) == NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 1);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);

    allocator.free_fn(NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 2);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
}

/* reset puts the three counts back to 0 and forgets the request to fail. */
NAOS_TEST(reset_restores_the_initial_state)
{
    clargs_allocator_t allocator = clargs_test_allocator_get();
    void *block;
    void *other;

    clargs_test_allocator_reset();

    block = allocator.malloc_fn(8);
    NAOS_CHECK_TRUE(block != NULL);
    allocator.free_fn(NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 1);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 1);

    /* The reset comes while the block is still live. */
    clargs_test_allocator_fail_at(1);
    clargs_test_allocator_reset();

    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);

    /*
     * The count of live blocks means nothing once that block is freed:
     * reset again.
     */
    allocator.free_fn(block);
    clargs_test_allocator_reset();

    /* The request to fail is gone: the first request succeeds. */
    block = allocator.malloc_fn(8);
    NAOS_CHECK_TRUE(block != NULL);

    /* The requests are counted from the reset again. */
    clargs_test_allocator_fail_at(2);
    other = allocator.malloc_fn(8);
    NAOS_CHECK_TRUE(other == NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 2);

    allocator.free_fn(block);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);

    clargs_test_allocator_reset();
}

/* install resets like reset does; uninstall leaves the counts readable. */
NAOS_TEST(install_resets_and_uninstall_can_follow)
{
    clargs_allocator_t allocator = clargs_test_allocator_get();
    void *block;
    void *leftover;

    clargs_test_allocator_reset();
    allocator.free_fn(NULL);
    NAOS_CHECK_TRUE(allocator.malloc_fn(0) == NULL);
    leftover = allocator.malloc_fn(8);
    NAOS_CHECK_TRUE(leftover != NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 1);
    clargs_test_allocator_fail_at(1);

    /* The install comes while a block is still live. */
    clargs_test_allocator_install();

    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);

    /*
     * The count of live blocks means nothing once that block is freed:
     * install again.
     */
    allocator.free_fn(leftover);
    clargs_test_allocator_install();

    block = allocator.malloc_fn(8);
    NAOS_CHECK_TRUE(block != NULL);
    allocator.free_fn(block);

    clargs_test_allocator_uninstall();

    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
}

/* install and uninstall can be repeated, and uninstall needs no install. */
NAOS_TEST(installs_and_uninstalls_more_than_once)
{
    clargs_test_allocator_uninstall();
    clargs_test_allocator_install();
    clargs_test_allocator_uninstall();
    clargs_test_allocator_install();
    clargs_test_allocator_install();
    clargs_test_allocator_uninstall();
    clargs_test_allocator_uninstall();
    NAOS_PASS();
}

NAOS_SUITE(test_allocator,
    NAOS_ADD_TEST(get_returns_the_four_functions),
    NAOS_ADD_TEST(get_returns_the_same_functions_every_time),
    NAOS_ADD_TEST(succeeds_when_no_failure_is_set),
    NAOS_ADD_TEST(fails_only_the_requested_malloc),
    NAOS_ADD_TEST(fails_only_the_requested_calloc),
    NAOS_ADD_TEST(fails_only_the_requested_realloc_and_keeps_the_block),
    NAOS_ADD_TEST(counts_the_request_to_fail_across_the_kinds),
    NAOS_ADD_TEST(fail_at_zero_fails_no_request),
    NAOS_ADD_TEST(calloc_returns_zeroed_memory),
    NAOS_ADD_TEST(realloc_keeps_the_contents),
    NAOS_ADD_TEST(counts_the_requests),
    NAOS_ADD_TEST(counts_the_live_blocks),
    NAOS_ADD_TEST(counts_zero_sized_requests_as_violations),
    NAOS_ADD_TEST(counts_null_pointers_as_violations),
    NAOS_ADD_TEST(reset_restores_the_initial_state),
    NAOS_ADD_TEST(install_resets_and_uninstall_can_follow),
    NAOS_ADD_TEST(installs_and_uninstalls_more_than_once)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(test_allocator)
)
