/*
 * Tests of the growing array: "vec.h".
 *
 * The tests use the test allocator ("../support/test_allocator.h") to see
 * the requests that a argx_vec_t makes: its count of requests shows when
 * the array grows, its count of live blocks shows that argx_vec_destroy
 * frees everything and that a failed push leaves nothing behind, and its
 * count of violations shows a call that the library must never make.
 *
 * The state of the test allocator and the allocator that is set are shared
 * by all the tests of this executable. Every test starts with a reset (or
 * an install, which resets), destroys what it made, sets the default
 * allocator back before it ends, and ends with check_clean().
 *
 * The test fails_at_every_allocation_point is the pattern for the tests of
 * the failure of an allocation; copy its shape.
 */
#include <argx/argx.h>

#include "alloc_internal.h"
#include "vec.h"

#include "../support/test_allocator.h"
#include "../support/wrap_allocator.h"

#include <naos/naos.h>

#include <stddef.h>
#include <stdint.h>
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
 * The array is in the empty state, the state right after
 * argx_vec_init.
 */
static void check_empty(const argx_vec_t *vec, size_t item_size)
{
    NAOS_CHECK_TRUE(vec->items == NULL);
    NAOS_CHECK_UINT_EQ(vec->count, 0);
    NAOS_CHECK_UINT_EQ(vec->capacity, 0);
    NAOS_CHECK_UINT_EQ(vec->item_size, item_size);
}

/*
 * The value of the item at index in the arrays of int of these tests. Any
 * value that differs from item to item will do.
 */
static int int_value(size_t index)
{
    return ((int)index * 7) - 3;
}

/*
 * Pushes the items int_value(vec->count) ... int_value(total - 1), so that
 * the array has total items afterwards. Stops at the first push that fails
 * and returns its error.
 */
static argx_error_t push_ints_up_to(argx_vec_t *vec, size_t total)
{
    while (vec->count < total)
    {
        int value = int_value(vec->count);
        argx_error_t error = argx_vec_push(vec, &value);

        if (error != ARGX_ERROR_NONE)
        {
            return error;
        }
    }
    return ARGX_ERROR_NONE;
}

/*
 * Every item of an array of int has the value that push_ints_up_to gave
 * it.
 */
static void check_ints(const argx_vec_t *vec)
{
    size_t i;

    for (i = 0; i < vec->count; ++i)
    {
        const int *item = argx_vec_at(vec, i);

        NAOS_CHECK_INT_EQ(*item, int_value(i));
    }
}

/* ---- The empty state -------------------------------------------------- */

/* argx_vec_init makes an empty array and allocates nothing. */
NAOS_TEST(init_makes_an_empty_array_without_allocating)
{
    argx_vec_t vec;

    argx_test_allocator_install();

    /*
     * Not the values of the empty state, so that the checks below see what
     * argx_vec_init wrote.
     */
    memset(&vec, 0x5a, sizeof vec);

    argx_vec_init(&vec, sizeof(int));
    check_empty(&vec, sizeof(int));
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 0);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);

    argx_vec_destroy(&vec);

    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * Destroying an array that never had an item asks nothing of the
 * allocator: no free_fn of NULL.
 */
NAOS_TEST(destroy_of_an_empty_array_does_not_call_the_allocator)
{
    argx_vec_t vec;

    argx_test_allocator_install();

    argx_vec_init(&vec, sizeof(long));
    argx_vec_destroy(&vec);
    check_empty(&vec, sizeof(long));

    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 0);

    argx_test_allocator_uninstall();
    check_clean();
}

/* ---- Pushing and growing ---------------------------------------------- */

/*
 * The first push allocates room for 4 items. The next three pushes use
 * it.
 */
NAOS_TEST(first_push_allocates_four_items)
{
    argx_vec_t vec;
    void *items;

    argx_test_allocator_install();
    argx_vec_init(&vec, sizeof(int));

    NAOS_CHECK_INT_EQ(push_ints_up_to(&vec, 1), ARGX_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(vec.count, 1);
    NAOS_CHECK_UINT_EQ(vec.capacity, 4);
    NAOS_CHECK_UINT_EQ(vec.item_size, sizeof(int));
    NAOS_CHECK_TRUE(vec.items != NULL);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 1);

    items = vec.items;
    NAOS_CHECK_INT_EQ(push_ints_up_to(&vec, 4), ARGX_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(vec.count, 4);
    NAOS_CHECK_UINT_EQ(vec.capacity, 4);
    /* No request, so the items did not move. */
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 1);
    NAOS_CHECK_TRUE(vec.items == items);
    check_ints(&vec);

    argx_vec_destroy(&vec);

    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * When the array is full, a push doubles the capacity: 4, 8, 16, ... One
 * request each time, and one live block all the time.
 */
NAOS_TEST(capacity_doubles_when_the_array_is_full)
{
    argx_vec_t vec;
    size_t capacity = 4;
    size_t requests = 1;

    argx_test_allocator_install();
    argx_vec_init(&vec, sizeof(int));

    while (vec.count < 100)
    {
        NAOS_CHECK_INT_EQ(push_ints_up_to(&vec, vec.count + 1),
            ARGX_ERROR_NONE);
        if (vec.count > capacity)
        {
            capacity *= 2;
            ++requests;
        }
        NAOS_CHECK_UINT_EQ(vec.capacity, capacity);
        NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), requests);
        NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 1);
    }
    NAOS_CHECK_UINT_EQ(vec.capacity, 128);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 6);

    argx_vec_destroy(&vec);

    argx_test_allocator_uninstall();
    check_clean();
}

/* Growing past the first capacity, several times, keeps every item. */
NAOS_TEST(growth_keeps_the_contents)
{
    argx_vec_t vec;
    size_t i;

    argx_test_allocator_install();
    argx_vec_init(&vec, sizeof(int));

    for (i = 0; i < 100; ++i)
    {
        int value = int_value(i);

        NAOS_CHECK_INT_EQ(argx_vec_push(&vec, &value), ARGX_ERROR_NONE);
        NAOS_CHECK_UINT_EQ(vec.count, i + 1);
        /* All the items so far, after every push. */
        check_ints(&vec);
    }
    NAOS_CHECK_UINT_EQ(vec.count, 100);
    NAOS_CHECK_TRUE(vec.count <= vec.capacity);

    argx_vec_destroy(&vec);

    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * A push copies the item: changing the variable afterwards does not change
 * the item in the array, and the item is not at the address of the
 * variable.
 */
NAOS_TEST(push_copies_the_item)
{
    argx_vec_t vec;
    int value = 11;
    int *item;

    argx_test_allocator_install();
    argx_vec_init(&vec, sizeof value);

    NAOS_CHECK_INT_EQ(argx_vec_push(&vec, &value), ARGX_ERROR_NONE);
    value = 22;
    NAOS_CHECK_INT_EQ(argx_vec_push(&vec, &value), ARGX_ERROR_NONE);
    value = 33;

    NAOS_CHECK_UINT_EQ(vec.count, 2);
    if (vec.count == 2)
    {
        item = argx_vec_at(&vec, 0);
        NAOS_CHECK_TRUE(item != &value);
        NAOS_CHECK_INT_EQ(*item, 11);
        item = argx_vec_at(&vec, 1);
        NAOS_CHECK_TRUE(item != &value);
        NAOS_CHECK_INT_EQ(*item, 22);
    }

    argx_vec_destroy(&vec);

    argx_test_allocator_uninstall();
    check_clean();
}

/* ---- argx_vec_at ---------------------------------------------------- */

/*
 * argx_vec_at returns the address of the item: index * item_size bytes
 * from the start of the items. The item can be written through it.
 */
NAOS_TEST(at_returns_the_address_of_the_item)
{
    argx_vec_t vec;
    size_t i;

    argx_test_allocator_install();
    argx_vec_init(&vec, sizeof(int));

    NAOS_CHECK_INT_EQ(push_ints_up_to(&vec, 10), ARGX_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(vec.count, 10);

    for (i = 0; i < vec.count; ++i)
    {
        int *item = argx_vec_at(&vec, i);

        NAOS_CHECK_TRUE((char *)item == ((char *)vec.items + (i * sizeof(int))));
        NAOS_CHECK_INT_EQ(*item, int_value(i));
    }

    /* Writing through the pointer changes that item and no other. */
    if (vec.count == 10)
    {
        int *item = argx_vec_at(&vec, 5);

        *item = 12345;
        for (i = 0; i < vec.count; ++i)
        {
            item = argx_vec_at(&vec, i);
            NAOS_CHECK_INT_EQ(*item, (i == 5) ? 12345 : int_value(i));
        }
    }

    argx_vec_destroy(&vec);

    argx_test_allocator_uninstall();
    check_clean();
}

/* ---- Items of different sizes ----------------------------------------- */

/* A structure with members of different sizes, so with padding. */
typedef struct sample
{
    char tag;
    double number;
    short small;
    const char *name;
} sample_t;

NAOS_TEST(holds_structs)
{
    static const char *const names[] = { "zero", "one", "two" };
    argx_vec_t vec;
    size_t i;

    argx_test_allocator_install();
    argx_vec_init(&vec, sizeof(sample_t));

    for (i = 0; i < 30; ++i)
    {
        sample_t value;

        value.tag = (char)('A' + (int)(i % 26));
        value.number = (double)i * 0.5;
        value.small = (short)(1000 + (int)i);
        value.name = names[i % 3];
        NAOS_CHECK_INT_EQ(argx_vec_push(&vec, &value), ARGX_ERROR_NONE);
    }
    NAOS_CHECK_UINT_EQ(vec.count, 30);
    NAOS_CHECK_UINT_EQ(vec.item_size, sizeof(sample_t));

    for (i = 0; i < vec.count; ++i)
    {
        const sample_t *item = argx_vec_at(&vec, i);

        NAOS_CHECK_INT_EQ(item->tag, 'A' + (int)(i % 26));
        NAOS_CHECK_TRUE(item->number == ((double)i * 0.5));
        NAOS_CHECK_INT_EQ(item->small, 1000 + (int)i);
        NAOS_CHECK_TRUE(item->name == names[i % 3]);
    }

    argx_vec_destroy(&vec);

    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * Pointers. The array holds the pointers, not what they point to: destroy
 * frees the array and leaves the strings to the caller.
 */
NAOS_TEST(holds_pointers)
{
    char first[] = "first";
    char second[] = "second";
    char third[] = "third";
    char *strings[3];
    argx_vec_t vec;
    size_t i;

    strings[0] = first;
    strings[1] = second;
    strings[2] = third;

    argx_test_allocator_install();
    argx_vec_init(&vec, sizeof(char *));

    for (i = 0; i < 21; ++i)
    {
        NAOS_CHECK_INT_EQ(argx_vec_push(&vec, &strings[i % 3]),
            ARGX_ERROR_NONE);
    }
    NAOS_CHECK_UINT_EQ(vec.count, 21);
    /* Only the array itself was allocated. */
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 1);

    for (i = 0; i < vec.count; ++i)
    {
        char **item = argx_vec_at(&vec, i);

        NAOS_CHECK_TRUE(*item == strings[i % 3]);
    }

    argx_vec_destroy(&vec);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);
    NAOS_CHECK_STR_EQ(first, "first");
    NAOS_CHECK_STR_EQ(second, "second");
    NAOS_CHECK_STR_EQ(third, "third");

    argx_test_allocator_uninstall();
    check_clean();
}

/* ---- Destroying ------------------------------------------------------- */

/*
 * argx_vec_destroy frees the items and puts the array back in the empty
 * state.
 */
NAOS_TEST(destroy_frees_and_empties)
{
    argx_vec_t vec;

    argx_test_allocator_install();
    argx_vec_init(&vec, sizeof(int));

    NAOS_CHECK_INT_EQ(push_ints_up_to(&vec, 20), ARGX_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 1);

    argx_vec_destroy(&vec);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);
    check_empty(&vec, sizeof(int));

    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * argx_vec_destroy may be called twice: the second call does nothing,
 * and does not free NULL.
 */
NAOS_TEST(destroy_twice_is_allowed)
{
    argx_vec_t vec;

    argx_test_allocator_install();
    argx_vec_init(&vec, sizeof(int));

    NAOS_CHECK_INT_EQ(push_ints_up_to(&vec, 5), ARGX_ERROR_NONE);

    argx_vec_destroy(&vec);
    argx_vec_destroy(&vec);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_violations(), 0);
    check_empty(&vec, sizeof(int));

    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * After argx_vec_destroy the array can be used again, without another
 * argx_vec_init: it starts again from the first capacity.
 */
NAOS_TEST(usable_again_after_destroy)
{
    argx_vec_t vec;
    size_t requests;

    argx_test_allocator_install();
    argx_vec_init(&vec, sizeof(int));

    NAOS_CHECK_INT_EQ(push_ints_up_to(&vec, 30), ARGX_ERROR_NONE);
    argx_vec_destroy(&vec);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);
    requests = argx_test_allocator_requests();

    NAOS_CHECK_INT_EQ(push_ints_up_to(&vec, 1), ARGX_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(vec.count, 1);
    NAOS_CHECK_UINT_EQ(vec.capacity, 4);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), requests + 1);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 1);

    NAOS_CHECK_INT_EQ(push_ints_up_to(&vec, 30), ARGX_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(vec.count, 30);
    check_ints(&vec);

    argx_vec_destroy(&vec);
    check_empty(&vec, sizeof(int));

    argx_test_allocator_uninstall();
    check_clean();
}

/* ---- The allocator ---------------------------------------------------- */

/*
 * All the memory comes from argx_realloc: the first allocation reaches
 * the allocator as a malloc_fn of 4 items (the pointer is NULL then), every
 * growth as a realloc_fn of twice the items, and destroy as one free_fn.
 */
NAOS_TEST(requests_go_through_realloc_and_free)
{
    argx_allocator_t allocator = argx_wrap_allocator_get();
    argx_vec_t vec;
    double value = 0.5;

    argx_test_allocator_reset();
    argx_set_allocator(&allocator);
    argx_wrap_allocator_reset();

    argx_vec_init(&vec, sizeof value);

    NAOS_CHECK_INT_EQ(argx_vec_push(&vec, &value), ARGX_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_counts().malloc_calls, 1);
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_counts().malloc_size, 4 * sizeof(double));
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_counts().realloc_calls, 0);

    while (vec.count < 5)
    {
        NAOS_CHECK_INT_EQ(argx_vec_push(&vec, &value), ARGX_ERROR_NONE);
    }
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_counts().malloc_calls, 1);
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_counts().realloc_calls, 1);
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_counts().realloc_size, 8 * sizeof(double));

    while (vec.count < 9)
    {
        NAOS_CHECK_INT_EQ(argx_vec_push(&vec, &value), ARGX_ERROR_NONE);
    }
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_counts().malloc_calls, 1);
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_counts().realloc_calls, 2);
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_counts().realloc_size, 16 * sizeof(double));

    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_counts().calloc_calls, 0);
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_counts().free_calls, 0);

    argx_vec_destroy(&vec);
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_counts().free_calls, 1);
    argx_vec_destroy(&vec);
    NAOS_CHECK_UINT_EQ(argx_wrap_allocator_counts().free_calls, 1);

    argx_set_allocator(NULL);
    /* Nothing went to the test allocator: it was not set. */
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 0);
    check_clean();
}

/* With nothing set, the array works with the default allocator. */
NAOS_TEST(works_with_the_default_allocator)
{
    argx_vec_t vec;

    argx_test_allocator_reset();
    argx_set_allocator(NULL);

    argx_vec_init(&vec, sizeof(int));
    NAOS_CHECK_INT_EQ(push_ints_up_to(&vec, 100), ARGX_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(vec.count, 100);
    check_ints(&vec);
    argx_vec_destroy(&vec);
    check_empty(&vec, sizeof(int));

    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 0);
    check_clean();
}

/* ---- The failure of the allocation ------------------------------------ */

/*
 * When the first allocation fails, the push returns
 * ARGX_ERROR_OUT_OF_MEMORY and the array stays empty.
 */
NAOS_TEST(failed_first_push_leaves_the_array_empty)
{
    argx_vec_t vec;
    int value = 7;

    argx_test_allocator_install();
    argx_test_allocator_fail_at(1);
    argx_vec_init(&vec, sizeof value);

    NAOS_CHECK_INT_EQ(argx_vec_push(&vec, &value),
        ARGX_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);
    check_empty(&vec, sizeof value);

    /* The array can still be destroyed, and it can still be used. */
    argx_vec_destroy(&vec);
    check_empty(&vec, sizeof value);
    NAOS_CHECK_INT_EQ(argx_vec_push(&vec, &value), ARGX_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(vec.count, 1);

    argx_vec_destroy(&vec);

    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * The failure of every allocation, one after the other. This is the
 * pattern for testing an operation that allocates:
 *
 *     install
 *     for n = 1, 2, ...
 *         reset, fail_at(n)
 *         run the operation
 *         if it succeeded with fewer than n requests: every allocation
 *             point has failed once; destroy, and stop
 *         check the error and what the failure must leave unchanged
 *         destroy what exists
 *         check that live and violations are 0
 *     uninstall
 *
 * The reset is at a moment when nothing is allocated. The loop has an upper
 * bound, so that a mistake cannot make it run forever.
 *
 * Here the operation is: push 40 items onto an empty array. That allocates
 * 5 times (for 4, 8, 16, 32 and 64 items). A failed push must leave the
 * count, the capacity and the items as they were, and the array must be
 * usable afterwards: the push is made again, and the rest of the items
 * follow.
 */
NAOS_TEST(fails_at_every_allocation_point)
{
    const size_t total = 40;
    const size_t allocation_points = 5;
    const size_t limit = 100;
    size_t failures = 0;
    int completed = 0;
    size_t n;

    argx_test_allocator_install();

    for (n = 1; n <= limit; ++n)
    {
        argx_vec_t vec;
        argx_error_t error;
        size_t count;
        size_t capacity;
        void *items;

        argx_test_allocator_reset();
        argx_test_allocator_fail_at(n);

        argx_vec_init(&vec, sizeof(int));
        error = push_ints_up_to(&vec, total);

        if ((error == ARGX_ERROR_NONE) &&
            (argx_test_allocator_requests() < n))
        {
            /*
             * The operation ended before the request to fail was reached:
             * every allocation point has been passed.
             */
            NAOS_CHECK_UINT_EQ(vec.count, total);
            check_ints(&vec);
            argx_vec_destroy(&vec);
            check_clean();
            completed = 1;
            break;
        }

        /* The n-th request failed, and the push reported it. */
        ++failures;
        NAOS_CHECK_INT_EQ(error, ARGX_ERROR_OUT_OF_MEMORY);
        NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), n);

        /*
         * The array is as it was before the push: it is full, the items
         * that were pushed are all there, and nothing was lost or leaked
         * (one block if there are items, none if there are not).
         */
        count = vec.count;
        capacity = vec.capacity;
        items = vec.items;
        NAOS_CHECK_TRUE(count < total);
        NAOS_CHECK_UINT_EQ(count, capacity);
        NAOS_CHECK_UINT_EQ(vec.item_size, sizeof(int));
        NAOS_CHECK_TRUE((items == NULL) == (count == 0));
        NAOS_CHECK_UINT_EQ(argx_test_allocator_live(),
            (count == 0) ? 0 : 1);
        check_ints(&vec);

        /* A push that fails again changes nothing either. */
        argx_test_allocator_fail_at(n + 1);
        error = push_ints_up_to(&vec, count + 1);
        NAOS_CHECK_INT_EQ(error, ARGX_ERROR_OUT_OF_MEMORY);
        NAOS_CHECK_UINT_EQ(vec.count, count);
        NAOS_CHECK_UINT_EQ(vec.capacity, capacity);
        NAOS_CHECK_TRUE(vec.items == items);
        check_ints(&vec);

        /*
         * The array is still usable: the next push succeeds, and so do
         * the pushes after it.
         */
        error = push_ints_up_to(&vec, count + 1);
        NAOS_CHECK_INT_EQ(error, ARGX_ERROR_NONE);
        NAOS_CHECK_UINT_EQ(vec.count, count + 1);
        check_ints(&vec);

        error = push_ints_up_to(&vec, total);
        NAOS_CHECK_INT_EQ(error, ARGX_ERROR_NONE);
        NAOS_CHECK_UINT_EQ(vec.count, total);
        check_ints(&vec);

        /* Destroy what exists; nothing is left and nothing was violated. */
        argx_vec_destroy(&vec);
        check_clean();
    }

    /*
     * The loop ended because the operation completed, after each of its
     * allocations had failed once.
     */
    NAOS_CHECK_TRUE(completed);
    NAOS_CHECK_UINT_EQ(failures, allocation_points);

    argx_test_allocator_uninstall();
    check_clean();
}

/* ---- The overflow of the capacity ------------------------------------- */

/*
 * The tests below make arrays whose next capacity cannot be represented.
 * No such array can really exist, so the fields are written by hand; items
 * stays NULL, because a push that fails must not read or write the items.
 * The fields are put back by hand at the end, so that nothing is freed.
 */

/*
 * The size of the first allocation, 4 items, does not fit in a size_t: the
 * push fails without calling the allocator, and the array is unchanged.
 */
NAOS_TEST(overflow_of_the_first_allocation)
{
    const size_t item_size = SIZE_MAX / 2;
    argx_vec_t vec;
    char item = 'x';

    argx_test_allocator_install();

    vec.items = NULL;
    vec.count = 0;
    vec.capacity = 0;
    vec.item_size = item_size;

    NAOS_CHECK_INT_EQ(argx_vec_push(&vec, &item),
        ARGX_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 0);
    check_empty(&vec, item_size);

    /*
     * The largest item size whose first allocation overflows by one
     * byte only.
     */
    vec.item_size = (SIZE_MAX / 4) + 1;
    NAOS_CHECK_INT_EQ(argx_vec_push(&vec, &item),
        ARGX_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 0);
    check_empty(&vec, (SIZE_MAX / 4) + 1);

    argx_test_allocator_uninstall();
    check_clean();
}

/* Twice the capacity does not fit in a size_t. */
NAOS_TEST(overflow_of_the_doubled_capacity)
{
    const size_t full = (SIZE_MAX / 2) + 1;
    argx_vec_t vec;
    int item = 1;

    argx_test_allocator_install();

    vec.items = NULL;
    vec.count = full;
    vec.capacity = full;
    vec.item_size = sizeof item;

    NAOS_CHECK_INT_EQ(argx_vec_push(&vec, &item),
        ARGX_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 0);
    NAOS_CHECK_TRUE(vec.items == NULL);
    NAOS_CHECK_UINT_EQ(vec.count, full);
    NAOS_CHECK_UINT_EQ(vec.capacity, full);
    NAOS_CHECK_UINT_EQ(vec.item_size, sizeof item);

    /* The largest capacity there is. */
    vec.count = SIZE_MAX;
    vec.capacity = SIZE_MAX;
    NAOS_CHECK_INT_EQ(argx_vec_push(&vec, &item),
        ARGX_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 0);
    NAOS_CHECK_TRUE(vec.items == NULL);
    NAOS_CHECK_UINT_EQ(vec.count, SIZE_MAX);
    NAOS_CHECK_UINT_EQ(vec.capacity, SIZE_MAX);

    /* By hand: there is nothing to free. */
    vec.count = 0;
    vec.capacity = 0;

    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * Twice the capacity wraps around to a small number that is not 0: a
 * request of that size would look like any other. The push fails without
 * calling the allocator.
 */
NAOS_TEST(overflow_of_the_doubled_capacity_to_a_small_number)
{
    /* Twice this is 4, modulo SIZE_MAX + 1. */
    const size_t full = (SIZE_MAX / 2) + 3;
    argx_vec_t vec;
    int item = 1;

    argx_test_allocator_install();

    vec.items = NULL;
    vec.count = full;
    vec.capacity = full;
    vec.item_size = sizeof item;

    NAOS_CHECK_INT_EQ(argx_vec_push(&vec, &item),
        ARGX_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 0);
    NAOS_CHECK_TRUE(vec.items == NULL);
    NAOS_CHECK_UINT_EQ(vec.count, full);
    NAOS_CHECK_UINT_EQ(vec.capacity, full);
    NAOS_CHECK_UINT_EQ(vec.item_size, sizeof item);

    /* By hand: there is nothing to free. */
    vec.count = 0;
    vec.capacity = 0;

    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * The other side of the limits: the largest sizes that can still be
 * represented are not an overflow, so the request goes to the allocator.
 * The test allocator fails that request, so that no such size reaches the
 * standard functions; that the request was made shows that the push did
 * not take the size for an overflow.
 */

/*
 * The largest capacity that can be doubled: SIZE_MAX / 2 items of one
 * byte become SIZE_MAX - 1 bytes.
 */
NAOS_TEST(largest_capacity_that_doubles_is_requested)
{
    const size_t full = SIZE_MAX / 2;
    argx_vec_t vec;
    char item = 'x';

    argx_test_allocator_install();
    argx_test_allocator_fail_at(1);

    vec.items = NULL;
    vec.count = full;
    vec.capacity = full;
    vec.item_size = sizeof item;

    NAOS_CHECK_INT_EQ(argx_vec_push(&vec, &item),
        ARGX_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);
    NAOS_CHECK_TRUE(vec.items == NULL);
    NAOS_CHECK_UINT_EQ(vec.count, full);
    NAOS_CHECK_UINT_EQ(vec.capacity, full);
    NAOS_CHECK_UINT_EQ(vec.item_size, sizeof item);

    /* By hand: there is nothing to free. */
    vec.count = 0;
    vec.capacity = 0;

    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * The largest item size whose first allocation fits: 4 items of
 * SIZE_MAX / 4 bytes are SIZE_MAX - 3 bytes.
 */
NAOS_TEST(largest_first_allocation_is_requested)
{
    const size_t item_size = SIZE_MAX / 4;
    argx_vec_t vec;
    char item = 'x';

    argx_test_allocator_install();
    argx_test_allocator_fail_at(1);

    vec.items = NULL;
    vec.count = 0;
    vec.capacity = 0;
    vec.item_size = item_size;

    NAOS_CHECK_INT_EQ(argx_vec_push(&vec, &item),
        ARGX_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);
    check_empty(&vec, item_size);

    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * Twice the capacity fits in a size_t, but that many items do not: the
 * product of the new capacity and the item size overflows.
 */
NAOS_TEST(overflow_of_the_size_in_bytes)
{
    typedef struct eight
    {
        unsigned char bytes[8];
    } eight_t;
    /* Twice this is SIZE_MAX / 4 + 1 items of 8 bytes at the least. */
    const size_t full = (SIZE_MAX / 8) + 1;
    argx_vec_t vec;
    eight_t item = { { 0 } };

    argx_test_allocator_install();

    vec.items = NULL;
    vec.count = full;
    vec.capacity = full;
    vec.item_size = sizeof item;

    NAOS_CHECK_INT_EQ(argx_vec_push(&vec, &item),
        ARGX_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 0);
    NAOS_CHECK_TRUE(vec.items == NULL);
    NAOS_CHECK_UINT_EQ(vec.count, full);
    NAOS_CHECK_UINT_EQ(vec.capacity, full);
    NAOS_CHECK_UINT_EQ(vec.item_size, sizeof item);

    /* By hand: there is nothing to free. */
    vec.count = 0;
    vec.capacity = 0;

    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * After a push that failed for an overflow, the array is still usable once
 * it is in a state that can exist.
 */
NAOS_TEST(usable_after_an_overflow)
{
    argx_vec_t vec;
    int item = 1;

    argx_test_allocator_install();
    argx_vec_init(&vec, sizeof(int));

    /* A full array of a capacity that cannot be doubled. */
    vec.count = SIZE_MAX;
    vec.capacity = SIZE_MAX;
    NAOS_CHECK_INT_EQ(argx_vec_push(&vec, &item),
        ARGX_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 0);
    vec.count = 0;
    vec.capacity = 0;

    NAOS_CHECK_INT_EQ(push_ints_up_to(&vec, 10), ARGX_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(vec.count, 10);
    check_ints(&vec);

    argx_vec_destroy(&vec);

    argx_test_allocator_uninstall();
    check_clean();
}

NAOS_SUITE(empty,
    NAOS_ADD_TEST(init_makes_an_empty_array_without_allocating),
    NAOS_ADD_TEST(destroy_of_an_empty_array_does_not_call_the_allocator)
)

NAOS_SUITE(push,
    NAOS_ADD_TEST(first_push_allocates_four_items),
    NAOS_ADD_TEST(capacity_doubles_when_the_array_is_full),
    NAOS_ADD_TEST(growth_keeps_the_contents),
    NAOS_ADD_TEST(push_copies_the_item),
    NAOS_ADD_TEST(at_returns_the_address_of_the_item)
)

NAOS_SUITE(item_sizes,
    NAOS_ADD_TEST(holds_structs),
    NAOS_ADD_TEST(holds_pointers)
)

NAOS_SUITE(destroy,
    NAOS_ADD_TEST(destroy_frees_and_empties),
    NAOS_ADD_TEST(destroy_twice_is_allowed),
    NAOS_ADD_TEST(usable_again_after_destroy)
)

NAOS_SUITE(allocator,
    NAOS_ADD_TEST(requests_go_through_realloc_and_free),
    NAOS_ADD_TEST(works_with_the_default_allocator)
)

NAOS_SUITE(failure,
    NAOS_ADD_TEST(failed_first_push_leaves_the_array_empty),
    NAOS_ADD_TEST(fails_at_every_allocation_point)
)

NAOS_SUITE(overflow,
    NAOS_ADD_TEST(overflow_of_the_first_allocation),
    NAOS_ADD_TEST(overflow_of_the_doubled_capacity),
    NAOS_ADD_TEST(overflow_of_the_doubled_capacity_to_a_small_number),
    NAOS_ADD_TEST(overflow_of_the_size_in_bytes),
    NAOS_ADD_TEST(largest_capacity_that_doubles_is_requested),
    NAOS_ADD_TEST(largest_first_allocation_is_requested),
    NAOS_ADD_TEST(usable_after_an_overflow)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(empty),
    NAOS_ADD_SUITE(push),
    NAOS_ADD_SUITE(item_sizes),
    NAOS_ADD_SUITE(destroy),
    NAOS_ADD_SUITE(allocator),
    NAOS_ADD_SUITE(failure),
    NAOS_ADD_SUITE(overflow)
)
