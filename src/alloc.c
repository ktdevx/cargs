/*
 * CLArgs - the allocator that is set, the default allocator and the internal
 * memory functions.
 *
 * This is the only source file of the library that uses the standard
 * malloc, realloc, calloc and free.
 */
#include <clargs/alloc.h>

#include "alloc_internal.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/*
 * The default allocator. Its functions wrap the standard ones instead of
 * naming them in the initializers below: with the DLL runtime of MSVC the
 * standard functions are imported, and the address of an imported function
 * is not a constant that a static initializer may use.
 */
static void *default_malloc(size_t size)
{
    return malloc(size);
}

static void *default_realloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

static void *default_calloc(size_t count, size_t size)
{
    return calloc(count, size);
}

static void default_free(void *ptr)
{
    free(ptr);
}

#define DEFAULT_ALLOCATOR { default_malloc, default_realloc, default_calloc, default_free }

static const clargs_allocator_t default_allocator = DEFAULT_ALLOCATOR;

/*
 * The allocator that is set. This is the only mutable global state of the
 * library. Only clargs_set_allocator() writes it.
 */
static clargs_allocator_t current_allocator = DEFAULT_ALLOCATOR;

void clargs_set_allocator(const clargs_allocator_t *allocator)
{
    if (allocator == NULL) {
        current_allocator = default_allocator;
        return;
    }

    assert(allocator->malloc_fn != NULL);
    assert(allocator->realloc_fn != NULL);
    assert(allocator->calloc_fn != NULL);
    assert(allocator->free_fn != NULL);

    current_allocator = *allocator;
}

void *clargs_malloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }

    return current_allocator.malloc_fn(size);
}

void *clargs_calloc(size_t count, size_t size)
{
    if (count == 0 || size == 0) {
        return NULL;
    }
    /* count * size would not fit in a size_t. */
    if (count > SIZE_MAX / size) {
        return NULL;
    }

    return current_allocator.calloc_fn(count, size);
}

void *clargs_realloc(void *ptr, size_t size)
{
    if (ptr == NULL) {
        return clargs_malloc(size);
    }
    if (size == 0) {
        clargs_free(ptr);
        return NULL;
    }

    return current_allocator.realloc_fn(ptr, size);
}

void clargs_free(void *ptr)
{
    if (ptr == NULL) {
        return;
    }

    current_allocator.free_fn(ptr);
}
