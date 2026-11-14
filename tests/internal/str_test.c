/*
 * Tests of the internal string functions: "str.h".
 *
 * The tests use the test allocator ("../support/test_allocator.h") to see
 * that a copy takes one request, that it is freed with argx_free and that a
 * failed copy leaves nothing behind, and the wrap allocator
 * ("../support/wrap_allocator.h") to see the size that is requested.
 *
 * The state of the test allocator and the allocator that is set are shared
 * by all the tests of this executable. Every test starts with a reset (or
 * an install, which resets), frees what it allocated, sets the default
 * allocator back before it ends, and ends with check_clean().
 */
#include <argx/argx.h>

#include "alloc_internal.h"
#include "str.h"

#include "../support/test_allocator.h"
#include "../support/wrap_allocator.h"

#include <naos/naos.h>

#include <stddef.h>
#include <string.h>

/*
 * The end of every test: the test allocator has no live block and saw no
 * violation of the allocator contract.
 */
static void check_clean(void)
{
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_violations(), 0);
}

/*
 * The copy has the contents of the string and its terminator, in a region
 * of its own: writing to one of them does not change the other. It takes
 * one request, and argx_free releases it. An empty string is copied as one
 * byte.
 */
NAOS_TEST(copies_the_contents_and_the_terminator)
{
    char original[] = "--output=file.txt";
    char *copy;
    char *empty;

    argx_test_allocator_install();

    copy = argx_strdup(original);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 1);
    NAOS_REQUIRE_TRUE(copy != NULL);
    NAOS_CHECK_TRUE(copy != original);
    /* Every byte, the terminator included. */
    NAOS_CHECK_MEM_EQ(copy, sizeof original, original, sizeof original);

    original[0] = 'x';
    NAOS_CHECK_STR_EQ(copy, "--output=file.txt");
    copy[0] = 'y';
    NAOS_CHECK_STR_EQ(original, "x-output=file.txt");

    empty = argx_strdup("");
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 2);
    if (empty != NULL)
    {
        NAOS_CHECK_INT_EQ(empty[0], '\0');
    }

    argx_free(empty);
    argx_free(copy);

    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * A copy is one malloc_fn of the length of the string plus the terminator,
 * and nothing else. The copy ends at the first terminator of the string.
 */
NAOS_TEST(requests_the_bytes_of_the_string)
{
    argx_allocator_t allocator = argx_wrap_allocator_get();
    const char with_terminator_inside[] = "ab\0cd";
    argx_wrap_counts_t counts;
    char *copy;

    argx_test_allocator_reset();
    argx_set_allocator(&allocator);

    argx_wrap_allocator_reset();
    copy = argx_strdup("");
    counts = argx_wrap_allocator_counts();
    NAOS_CHECK_UINT_EQ(counts.malloc_calls, 1);
    NAOS_CHECK_UINT_EQ(counts.malloc_size, 1);
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_calls(), 1);
    argx_free(copy);

    argx_wrap_allocator_reset();
    copy = argx_strdup(with_terminator_inside);
    counts = argx_wrap_allocator_counts();
    NAOS_CHECK_UINT_EQ(counts.malloc_calls, 1);
    NAOS_CHECK_UINT_EQ(counts.malloc_size, 3);
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_calls(), 1);
    if (copy != NULL)
    {
        NAOS_CHECK_STR_EQ(copy, "ab");
    }
    argx_free(copy);
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_counts().free_calls, 1);

    argx_set_allocator(NULL);
    check_clean();
}

/*
 * When the request fails, argx_strdup returns NULL, nothing is left
 * allocated, and the string is as it was.
 */
NAOS_TEST(failed_allocation_returns_null)
{
    char original[] = "--output=file.txt";
    char *copy;

    argx_test_allocator_install();
    argx_test_allocator_fail_at(1);

    copy = argx_strdup(original);
    NAOS_CHECK_TRUE(copy == NULL);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 1);
    NAOS_CHECK_STR_EQ(original, "--output=file.txt");

    /* Only if the copy was made after all: it must not stay allocated. */
    argx_free(copy);

    argx_test_allocator_uninstall();
    check_clean();
}

NAOS_SUITE(str,
    NAOS_ADD_TEST(copies_the_contents_and_the_terminator),
    NAOS_ADD_TEST(requests_the_bytes_of_the_string),
    NAOS_ADD_TEST(failed_allocation_returns_null)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(str)
)
