/*
 * Tests of the public allocator API: <argx/alloc.h>.
 *
 * No public function allocates memory yet, so these tests cannot observe
 * which allocator the library uses. They check that the type takes the
 * standard functions as they are and that the allocator can be set and
 * reset. The tests of the internal memory functions check that the
 * allocator that was set is the one that is called.
 *
 * Every test ends with argx_set_allocator(NULL), so that it leaves the
 * default allocator behind.
 */
#include <argx/argx.h>

#include <naos/naos.h>

/*
 * These tests name the standard functions on purpose: an allocator of the
 * standard functions is what a consumer writes. With the DLL runtime of
 * MSVC the standard functions are imported, and /W4 warns about the address
 * of an imported function (C4232) and about using it in an aggregate
 * initializer (C4204). Both are harmless here.
 */
#ifdef _MSC_VER
#pragma warning(disable : 4204 4232)
#endif

#include <stddef.h>
#include <stdlib.h>

/* The four fields take the standard functions without a cast. */
NAOS_TEST(type_takes_the_standard_functions)
{
    argx_allocator_t allocator = { malloc, realloc, calloc, free };

    NAOS_CHECK_TRUE(allocator.malloc_fn == malloc);
    NAOS_CHECK_TRUE(allocator.realloc_fn == realloc);
    NAOS_CHECK_TRUE(allocator.calloc_fn == calloc);
    NAOS_CHECK_TRUE(allocator.free_fn == free);
}

/* The type of each field is the public function pointer type of its name. */
NAOS_TEST(fields_have_the_function_pointer_types)
{
    argx_malloc_fn_t malloc_fn = malloc;
    argx_realloc_fn_t realloc_fn = realloc;
    argx_calloc_fn_t calloc_fn = calloc;
    argx_free_fn_t free_fn = free;
    argx_allocator_t allocator;

    allocator.malloc_fn = malloc_fn;
    allocator.realloc_fn = realloc_fn;
    allocator.calloc_fn = calloc_fn;
    allocator.free_fn = free_fn;

    NAOS_CHECK_TRUE(allocator.malloc_fn == malloc);
    NAOS_CHECK_TRUE(allocator.realloc_fn == realloc);
    NAOS_CHECK_TRUE(allocator.calloc_fn == calloc);
    NAOS_CHECK_TRUE(allocator.free_fn == free);
}

/* An allocator made of the standard functions can be set. */
NAOS_TEST(sets_an_allocator_of_the_standard_functions)
{
    argx_allocator_t allocator = { malloc, realloc, calloc, free };

    argx_set_allocator(&allocator);
    NAOS_PASS();

    argx_set_allocator(NULL);
}

/* The argument is not modified: it is taken as a pointer to const. */
NAOS_TEST(setting_leaves_the_argument_unchanged)
{
    const argx_allocator_t allocator = { malloc, realloc, calloc, free };

    argx_set_allocator(&allocator);

    NAOS_CHECK_TRUE(allocator.malloc_fn == malloc);
    NAOS_CHECK_TRUE(allocator.realloc_fn == realloc);
    NAOS_CHECK_TRUE(allocator.calloc_fn == calloc);
    NAOS_CHECK_TRUE(allocator.free_fn == free);

    argx_set_allocator(NULL);
}

/* NULL resets to the default allocator, also when nothing was set. */
NAOS_TEST(null_resets_to_the_default)
{
    argx_allocator_t allocator = { malloc, realloc, calloc, free };

    argx_set_allocator(NULL);
    argx_set_allocator(&allocator);
    argx_set_allocator(NULL);
    argx_set_allocator(NULL);
    NAOS_PASS();
}

/* An allocator can be set again after another one or after a reset. */
NAOS_TEST(sets_an_allocator_more_than_once)
{
    argx_allocator_t first = { malloc, realloc, calloc, free };
    argx_allocator_t second = { malloc, realloc, calloc, free };

    argx_set_allocator(&first);
    argx_set_allocator(&second);
    argx_set_allocator(NULL);
    argx_set_allocator(&first);
    NAOS_PASS();

    argx_set_allocator(NULL);
}

NAOS_SUITE(allocator,
    NAOS_ADD_TEST(type_takes_the_standard_functions),
    NAOS_ADD_TEST(fields_have_the_function_pointer_types),
    NAOS_ADD_TEST(sets_an_allocator_of_the_standard_functions),
    NAOS_ADD_TEST(setting_leaves_the_argument_unchanged),
    NAOS_ADD_TEST(null_resets_to_the_default),
    NAOS_ADD_TEST(sets_an_allocator_more_than_once)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(allocator)
)
