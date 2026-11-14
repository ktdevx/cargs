/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - the growing array.
 */
#include "vec.h"

#include "alloc_internal.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

/* The number of items of the first allocation. */
#define ARGX_VEC_FIRST_CAPACITY ((size_t)4)

/* The largest value of a size_t. */
#define ARGX_VEC_SIZE_MAX ((size_t)-1)

/*
 * Makes room for at least one more item in a full array. On failure the
 * array is unchanged.
 */
static argx_error_t argx_vec_grow(argx_vec_t *vec)
{
    size_t new_capacity;
    void *new_items;

    if (vec->capacity == 0)
    {
        new_capacity = ARGX_VEC_FIRST_CAPACITY;
    }
    else
    {
        if (vec->capacity > (ARGX_VEC_SIZE_MAX / 2))
        {
            return ARGX_ERROR_OUT_OF_MEMORY;
        }
        new_capacity = vec->capacity * 2;
    }

    /* argx_realloc does not check the product. */
    if (new_capacity > (ARGX_VEC_SIZE_MAX / vec->item_size))
    {
        return ARGX_ERROR_OUT_OF_MEMORY;
    }

    /*
     * The first time items is NULL, so this allocates. When it fails, the
     * old items are still valid and vec still points to them.
     */
    new_items = argx_realloc(vec->items, new_capacity * vec->item_size);
    if (new_items == NULL)
    {
        return ARGX_ERROR_OUT_OF_MEMORY;
    }

    vec->items = new_items;
    vec->capacity = new_capacity;
    return ARGX_ERROR_NONE;
}

void argx_vec_init(argx_vec_t *vec, size_t item_size)
{
    assert(vec != NULL);
    assert(item_size > 0);

    vec->items = NULL;
    vec->count = 0;
    vec->capacity = 0;
    vec->item_size = item_size;
}

argx_error_t argx_vec_push(argx_vec_t *vec, const void *item)
{
    assert(vec != NULL);
    assert(item != NULL);
    assert(vec->item_size > 0);

    if (vec->count == vec->capacity)
    {
        argx_error_t error = argx_vec_grow(vec);

        if (error != ARGX_ERROR_NONE)
        {
            return error;
        }
    }

    /*
     * count < capacity now, so the product is less than the size of the
     * items and does not overflow.
     */
    memcpy((char *)vec->items + (vec->count * vec->item_size), item,
        vec->item_size);
    ++vec->count;
    return ARGX_ERROR_NONE;
}

void *argx_vec_at(const argx_vec_t *vec, size_t index)
{
    assert(vec != NULL);
    assert(index < vec->count);

    return (char *)vec->items + (index * vec->item_size);
}

void argx_vec_destroy(argx_vec_t *vec)
{
    assert(vec != NULL);

    argx_free(vec->items);
    vec->items = NULL;
    vec->count = 0;
    vec->capacity = 0;
}
