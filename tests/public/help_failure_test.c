/*
 * Tests of the failure of an allocation, across the help: the functions of
 * <clargs/help.h> that allocate.
 *
 * The help options are the only object of the help that the caller creates
 * and destroys. The tests check that creating and destroying them leaves
 * nothing allocated, that a creation whose allocation fails returns
 * CLARGS_ERROR_OUT_OF_MEMORY, writes NULL to its output and keeps no
 * block, that destroying NULL does nothing, and that the functions that
 * set a setting never allocate.
 *
 * Every test uses the test allocator ("../support/test_allocator.h"). Its
 * state and the allocator that is set are shared by all the tests of this
 * executable. A test starts with an install, destroys what it created, sets
 * the default allocator back before it ends, and ends with check_clean().
 */
#include <clargs/clargs.h>

#include "../support/test_allocator.h"

#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>

/* The most allocation requests that an operation is given to fail in. */
#define REQUEST_LIMIT 100

/* The end of every test: the test allocator has no live block and saw no
 * violation of the allocator contract. */
static void check_clean(void)
{
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
}

/* Help options that are not a result of clargs_help_options_create(): what
 * an output holds before the call, so that a call that does not write its
 * output is seen. It is never read through. */
static clargs_help_options_t *stale_options(void)
{
    /* Aligned for an object of any type, as allocated memory is. */
    static union {
        long long integer;
        long double number;
        void *pointer;
        void (*function)(void);
    } object;

    return (clargs_help_options_t *)(void *)&object;
}

NAOS_TEST(creating_and_destroying_help_options_leaves_nothing_allocated)
{
    clargs_help_options_t *options = stale_options();

    clargs_test_allocator_install();

    NAOS_CHECK_TRUE(clargs_help_options_create(&options) == CLARGS_ERROR_NONE);
    NAOS_CHECK_TRUE(options != NULL);
    NAOS_CHECK_TRUE(options != stale_options());
    NAOS_CHECK_TRUE(clargs_test_allocator_requests() >= 1);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), clargs_test_allocator_requests());

    if (options != stale_options()) {
        clargs_help_options_destroy(options);
    }

    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(creating_help_options_fails_cleanly_at_every_allocation_point)
{
    bool completed = false;
    size_t n;

    clargs_test_allocator_install();

    for (n = 1; n <= REQUEST_LIMIT; ++n) {
        clargs_help_options_t *options = stale_options();
        clargs_error_t error;

        clargs_test_allocator_reset();
        clargs_test_allocator_fail_at(n);

        error = clargs_help_options_create(&options);

        if (error == CLARGS_ERROR_NONE) {
            /* The creation ended before the request to fail was reached:
             * every allocation point has failed once. The creation is not
             * an operation without requests. */
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), n - 1);
            NAOS_CHECK_TRUE(n > 1);
            NAOS_CHECK_TRUE(options != NULL);
            NAOS_CHECK_TRUE(options != stale_options());
            if (options != stale_options()) {
                clargs_help_options_destroy(options);
            }
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
            completed = true;
            break;
        }

        /* The nth request failed, and the creation reported it: the output
         * is NULL and no block is kept. */
        NAOS_CHECK_TRUE(error == CLARGS_ERROR_OUT_OF_MEMORY);
        NAOS_CHECK_TRUE(options == NULL);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), n);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
    }

    NAOS_CHECK_TRUE(completed);

    /* After the failures, a creation succeeds as before. */
    {
        clargs_help_options_t *options = stale_options();

        clargs_test_allocator_reset();
        NAOS_CHECK_TRUE(clargs_help_options_create(&options) == CLARGS_ERROR_NONE);
        NAOS_CHECK_TRUE(options != NULL);
        if (options != stale_options()) {
            clargs_help_options_destroy(options);
        }
    }

    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(destroying_null_help_options_does_nothing)
{
    clargs_test_allocator_install();

    clargs_help_options_destroy(NULL);
    clargs_help_options_destroy(NULL);

    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 0);

    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(setting_help_options_never_allocates)
{
    clargs_help_options_t *options = NULL;
    size_t requests;
    size_t live;

    clargs_test_allocator_install();

    NAOS_REQUIRE_TRUE(clargs_help_options_create(&options) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(options != NULL);
    requests = clargs_test_allocator_requests();
    live = clargs_test_allocator_live();
    NAOS_CHECK_UINT_EQ(live, 1);

    /* An allocation would fail, and none is made: ordinary values, 0, the
     * limit, a value above the limit and the greatest size_t, each setting
     * set more than once. */
    clargs_test_allocator_fail_at(requests + 1);

    clargs_help_options_set_width(options, 100);
    clargs_help_options_set_width(options, 0);
    clargs_help_options_set_width(options, 65535);
    clargs_help_options_set_width(options, 65536);
    clargs_help_options_set_width(options, (size_t)-1);

    clargs_help_options_set_indent(options, 4);
    clargs_help_options_set_indent(options, 0);
    clargs_help_options_set_indent(options, 65535);
    clargs_help_options_set_indent(options, 65536);
    clargs_help_options_set_indent(options, (size_t)-1);

    clargs_help_options_set_max_column(options, 24);
    clargs_help_options_set_max_column(options, 0);
    clargs_help_options_set_max_column(options, 65535);
    clargs_help_options_set_max_column(options, 65536);
    clargs_help_options_set_max_column(options, (size_t)-1);

    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);

    clargs_help_options_destroy(options);

    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_SUITE(help_failure,
    NAOS_ADD_TEST(creating_and_destroying_help_options_leaves_nothing_allocated),
    NAOS_ADD_TEST(creating_help_options_fails_cleanly_at_every_allocation_point),
    NAOS_ADD_TEST(destroying_null_help_options_does_nothing),
    NAOS_ADD_TEST(setting_help_options_never_allocates)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(help_failure)
)
