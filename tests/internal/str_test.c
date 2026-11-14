/*
 * Tests of the internal string functions: "str.h".
 *
 * The tests use the test allocator ("../support/test_allocator.h") to see
 * the requests that cline_strdup makes: its count of requests shows how
 * many allocations a copy takes, its count of live blocks shows that a copy
 * is freed with cline_free and that a failed copy leaves nothing behind,
 * and its count of violations shows a call that the library must never
 * make.
 *
 * The state of the test allocator and the allocator that is set are shared
 * by all the tests of this executable. Every test starts with a reset (or
 * an install, which resets), frees what it allocated, sets the default
 * allocator back before it ends, and ends with check_clean().
 */
#include <cline/cline.h>

#include "alloc_internal.h"
#include "str.h"

#include "../support/test_allocator.h"

#include <naos/naos.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/*
 * An allocator that only wraps the standard functions and records what it
 * was asked for: the number of calls of each function and the size of the
 * last malloc_fn. They are functions of this file, not the standard
 * functions themselves, so that no initializer takes the address of an
 * imported function (MSVC warns about that).
 */
static size_t wrap_malloc_calls;
static size_t wrap_malloc_size;
static size_t wrap_other_calls;
static size_t wrap_free_calls;

static void *wrap_malloc(size_t size)
{
    ++wrap_malloc_calls;
    wrap_malloc_size = size;
    return malloc(size);
}

static void *wrap_realloc(void *ptr, size_t size)
{
    ++wrap_other_calls;
    return realloc(ptr, size);
}

static void *wrap_calloc(size_t count, size_t size)
{
    ++wrap_other_calls;
    return calloc(count, size);
}

static void wrap_free(void *ptr)
{
    ++wrap_free_calls;
    free(ptr);
}

static cline_allocator_t wrap_allocator(void)
{
    cline_allocator_t allocator;

    allocator.malloc_fn = wrap_malloc;
    allocator.realloc_fn = wrap_realloc;
    allocator.calloc_fn = wrap_calloc;
    allocator.free_fn = wrap_free;
    return allocator;
}

static void wrap_reset(void)
{
    wrap_malloc_calls = 0;
    wrap_malloc_size = 0;
    wrap_other_calls = 0;
    wrap_free_calls = 0;
}

/*
 * The end of every test: the test allocator has no live block and saw no
 * violation of the allocator contract.
 */
static void check_clean(void)
{
    NAOS_CHECK_UINT_EQ(cline_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_violations(), 0);
}

/* ---- The copy --------------------------------------------------------- */

/*
 * The copy has the contents of the string and its terminator. It takes one
 * request, and cline_free releases it.
 */
NAOS_TEST(copies_the_contents_and_the_terminator)
{
    const char original[] = "--output=file.txt";
    char *copy;

    cline_test_allocator_install();

    copy = cline_strdup(original);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_live(), 1);
    NAOS_REQUIRE_TRUE(copy != NULL);

    NAOS_CHECK_STR_EQ(copy, original);
    NAOS_CHECK_UINT_EQ(strlen(copy), sizeof original - 1);
    /* Every byte, the terminator included. */
    NAOS_CHECK_MEM_EQ(copy, sizeof original, original, sizeof original);
    NAOS_CHECK_INT_EQ(copy[sizeof original - 1], '\0');

    cline_free(copy);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_live(), 0);
    /* free_fn is not a request. */
    NAOS_CHECK_UINT_EQ(cline_test_allocator_requests(), 1);

    cline_test_allocator_uninstall();
    check_clean();
}

/*
 * The copy of an empty string succeeds: it is a string of one byte, the
 * terminator.
 */
NAOS_TEST(copies_an_empty_string)
{
    char *copy;

    cline_test_allocator_install();

    copy = cline_strdup("");
    NAOS_CHECK_UINT_EQ(cline_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_live(), 1);
    NAOS_REQUIRE_TRUE(copy != NULL);

    NAOS_CHECK_INT_EQ(copy[0], '\0');
    NAOS_CHECK_UINT_EQ(strlen(copy), 0);

    cline_free(copy);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_live(), 0);

    cline_test_allocator_uninstall();
    check_clean();
}

/* A string that is longer than any small block is copied whole. */
NAOS_TEST(copies_a_long_string)
{
    char original[4096];
    char *copy;
    size_t i;

    for (i = 0; i < (sizeof original - 1); ++i)
    {
        original[i] = (char)('a' + (i % 26));
    }
    original[sizeof original - 1] = '\0';

    cline_test_allocator_install();

    copy = cline_strdup(original);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_requests(), 1);
    NAOS_REQUIRE_TRUE(copy != NULL);
    NAOS_CHECK_MEM_EQ(copy, sizeof original, original, sizeof original);

    cline_free(copy);

    cline_test_allocator_uninstall();
    check_clean();
}

/*
 * The copy is another region than the string: writing to one of them does
 * not change the other.
 */
NAOS_TEST(copy_is_a_separate_region)
{
    char original[] = "verbose";
    char *copy;

    cline_test_allocator_install();

    copy = cline_strdup(original);
    NAOS_REQUIRE_TRUE(copy != NULL);
    NAOS_CHECK_TRUE(copy != original);

    /* Writing to the copy leaves the string as it was. */
    copy[0] = 'V';
    NAOS_CHECK_STR_EQ(copy, "Verbose");
    NAOS_CHECK_STR_EQ(original, "verbose");

    /* Writing to the string leaves the copy as it was. */
    memset(original, 'x', sizeof original - 1);
    NAOS_CHECK_STR_EQ(original, "xxxxxxx");
    NAOS_CHECK_STR_EQ(copy, "Verbose");

    cline_free(copy);

    cline_test_allocator_uninstall();
    check_clean();
}

/* Two copies of one string are two regions. */
NAOS_TEST(copies_are_separate_from_each_other)
{
    const char original[] = "name";
    char *first;
    char *second;

    cline_test_allocator_install();

    first = cline_strdup(original);
    NAOS_REQUIRE_TRUE(first != NULL);
    second = cline_strdup(original);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_requests(), 2);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_live(), 2);
    if (second == NULL)
    {
        cline_free(first);
    }
    NAOS_REQUIRE_TRUE(second != NULL);
    NAOS_CHECK_TRUE(first != second);

    first[0] = 'N';
    NAOS_CHECK_STR_EQ(first, "Name");
    NAOS_CHECK_STR_EQ(second, "name");

    cline_free(first);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_live(), 1);
    /* The second copy is still whole after the first one is freed. */
    NAOS_CHECK_STR_EQ(second, "name");
    cline_free(second);

    cline_test_allocator_uninstall();
    check_clean();
}

/* ---- The allocator ---------------------------------------------------- */

/*
 * A copy is one malloc_fn of the length of the string plus the terminator,
 * and nothing else: an empty string asks for one byte. The copy ends at the
 * first terminator of the string.
 */
NAOS_TEST(requests_the_bytes_of_the_string)
{
    cline_allocator_t allocator = wrap_allocator();
    const char with_terminator_inside[] = "ab\0cd";
    char *copy;

    cline_test_allocator_reset();
    cline_set_allocator(&allocator);

    wrap_reset();
    copy = cline_strdup("");
    NAOS_CHECK_UINT_EQ(wrap_malloc_calls, 1);
    NAOS_CHECK_UINT_EQ(wrap_malloc_size, 1);
    NAOS_CHECK_UINT_EQ(wrap_other_calls, 0);
    NAOS_CHECK_UINT_EQ(wrap_free_calls, 0);
    cline_free(copy);
    NAOS_CHECK_UINT_EQ(wrap_free_calls, 1);

    wrap_reset();
    copy = cline_strdup("count");
    NAOS_CHECK_UINT_EQ(wrap_malloc_calls, 1);
    NAOS_CHECK_UINT_EQ(wrap_malloc_size, 6);
    NAOS_CHECK_UINT_EQ(wrap_other_calls, 0);
    NAOS_CHECK_UINT_EQ(wrap_free_calls, 0);
    cline_free(copy);
    NAOS_CHECK_UINT_EQ(wrap_free_calls, 1);

    wrap_reset();
    copy = cline_strdup(with_terminator_inside);
    NAOS_CHECK_UINT_EQ(wrap_malloc_calls, 1);
    NAOS_CHECK_UINT_EQ(wrap_malloc_size, 3);
    NAOS_CHECK_UINT_EQ(wrap_other_calls, 0);
    if (copy != NULL)
    {
        NAOS_CHECK_STR_EQ(copy, "ab");
    }
    cline_free(copy);
    NAOS_CHECK_UINT_EQ(wrap_free_calls, 1);

    cline_set_allocator(NULL);
    /* Nothing went to the test allocator: it was not set. */
    NAOS_CHECK_UINT_EQ(cline_test_allocator_requests(), 0);
    check_clean();
}

/* With nothing set, the copy comes from the default allocator. */
NAOS_TEST(works_with_the_default_allocator)
{
    char *copy;

    cline_test_allocator_reset();
    cline_set_allocator(NULL);

    copy = cline_strdup("default");
    NAOS_REQUIRE_TRUE(copy != NULL);
    NAOS_CHECK_STR_EQ(copy, "default");
    cline_free(copy);

    NAOS_CHECK_UINT_EQ(cline_test_allocator_requests(), 0);
    check_clean();
}

/* ---- The failure of the allocation ------------------------------------ */

/*
 * When the first request fails, cline_strdup returns NULL and nothing is
 * left allocated.
 */
NAOS_TEST(failed_allocation_returns_null)
{
    char original[] = "--output=file.txt";
    char *copy;

    cline_test_allocator_install();
    cline_test_allocator_fail_at(1);

    copy = cline_strdup(original);
    NAOS_CHECK_TRUE(copy == NULL);
    /* The request that failed is the only one. */
    NAOS_CHECK_UINT_EQ(cline_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_violations(), 0);
    /* The string is as it was. */
    NAOS_CHECK_STR_EQ(original, "--output=file.txt");

    /* Only if the copy was made after all: it must not stay allocated. */
    cline_free(copy);

    cline_test_allocator_uninstall();
    check_clean();
}

/* The failed copy of an empty string returns NULL too. */
NAOS_TEST(failed_allocation_of_an_empty_string_returns_null)
{
    cline_test_allocator_install();
    cline_test_allocator_fail_at(1);

    NAOS_CHECK_TRUE(cline_strdup("") == NULL);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_live(), 0);

    cline_test_allocator_uninstall();
    check_clean();
}

/* After a failed copy, the next copy succeeds. */
NAOS_TEST(copies_again_after_a_failure)
{
    char *copy;

    cline_test_allocator_install();
    cline_test_allocator_fail_at(1);

    NAOS_CHECK_TRUE(cline_strdup("again") == NULL);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_live(), 0);

    copy = cline_strdup("again");
    NAOS_CHECK_UINT_EQ(cline_test_allocator_requests(), 2);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_live(), 1);
    NAOS_REQUIRE_TRUE(copy != NULL);
    NAOS_CHECK_STR_EQ(copy, "again");

    cline_free(copy);

    cline_test_allocator_uninstall();
    check_clean();
}

NAOS_SUITE(copy,
    NAOS_ADD_TEST(copies_the_contents_and_the_terminator),
    NAOS_ADD_TEST(copies_an_empty_string),
    NAOS_ADD_TEST(copies_a_long_string),
    NAOS_ADD_TEST(copy_is_a_separate_region),
    NAOS_ADD_TEST(copies_are_separate_from_each_other)
)

NAOS_SUITE(allocator,
    NAOS_ADD_TEST(requests_the_bytes_of_the_string),
    NAOS_ADD_TEST(works_with_the_default_allocator)
)

NAOS_SUITE(failure,
    NAOS_ADD_TEST(failed_allocation_returns_null),
    NAOS_ADD_TEST(failed_allocation_of_an_empty_string_returns_null),
    NAOS_ADD_TEST(copies_again_after_a_failure)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(copy),
    NAOS_ADD_SUITE(allocator),
    NAOS_ADD_SUITE(failure)
)
