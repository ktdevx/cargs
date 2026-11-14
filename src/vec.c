/*
 * CLArgs - the growing array.
 */
#include "vec.h"

#include "alloc_internal.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

/* The number of items of the first allocation. */
#define CLARGS_VEC_FIRST_CAPACITY ((size_t)4)

/* The largest value of a size_t. */
#define CLARGS_VEC_SIZE_MAX ((size_t)-1)

/*
 * Makes room for at least one more item in a full array. On failure the
 * array is unchanged.
 */
static clargs_error_t clargs_vec_grow(clargs_vec_t *vec)
{
    size_t new_capacity;
    void *new_items;

    if (vec->capacity == 0)
    {
        new_capacity = CLARGS_VEC_FIRST_CAPACITY;
    }
    else
    {
        if (vec->capacity > CLARGS_VEC_SIZE_MAX / 2)
        {
            return CLARGS_ERROR_OUT_OF_MEMORY;
        }
        new_capacity = vec->capacity * 2;
    }

    /* clargs_realloc does not check the product. */
    if (new_capacity > CLARGS_VEC_SIZE_MAX / vec->item_size)
    {
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }

    /*
     * The first time items is NULL, so this allocates. When it fails, the
     * old items are still valid and vec still points to them.
     */
    new_items = clargs_realloc(vec->items, new_capacity * vec->item_size);
    if (new_items == NULL)
    {
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }

    vec->items = new_items;
    vec->capacity = new_capacity;
    return CLARGS_ERROR_NONE;
}

void clargs_vec_init(clargs_vec_t *vec, size_t item_size)
{
    assert(vec != NULL);
    assert(item_size > 0);

    vec->items = NULL;
    vec->count = 0;
    vec->capacity = 0;
    vec->item_size = item_size;
}

clargs_error_t clargs_vec_push(clargs_vec_t *vec, const void *item)
{
    assert(vec != NULL);
    assert(item != NULL);
    assert(vec->item_size > 0);

    if (vec->count == vec->capacity)
    {
        clargs_error_t error = clargs_vec_grow(vec);

        if (error != CLARGS_ERROR_NONE)
        {
            return error;
        }
    }

    /*
     * count < capacity now, so the product is less than the size of the
     * items and does not overflow.
     */
    memcpy((char *)vec->items + vec->count * vec->item_size, item,
        vec->item_size);
    ++vec->count;
    return CLARGS_ERROR_NONE;
}

void *clargs_vec_at(const clargs_vec_t *vec, size_t index)
{
    assert(vec != NULL);
    assert(index < vec->count);

    return (char *)vec->items + index * vec->item_size;
}

void clargs_vec_destroy(clargs_vec_t *vec)
{
    assert(vec != NULL);

    clargs_free(vec->items);
    vec->items = NULL;
    vec->count = 0;
    vec->capacity = 0;
}
