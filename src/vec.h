/*
 * CLArgs - internal declarations for the growing array.
 *
 * A clargs_vec_t is an array of items of one size, which is given at run
 * time. Items are added at the end and stay where they were added: there is
 * no removal, no insertion and no sorting.
 *
 * The array holds copies of the items. If the items are pointers, the array
 * holds the pointers only; what they point to belongs to the caller.
 *
 * All its memory comes from the allocator that is set
 * (clargs_set_allocator(), <clargs/alloc.h>), through clargs_realloc() and
 * clargs_free() ("alloc_internal.h").
 *
 * Nothing here is synchronized: do not change one clargs_vec_t from several
 * threads at the same time.
 */
#ifndef CLARGS_VEC_H
#define CLARGS_VEC_H

#include <clargs/error.h>

#include <stddef.h>

/*
 * The array. Read the number of items from count; treat the other fields
 * as private.
 *
 * It is empty (items == NULL, count == 0, capacity == 0) after
 * clargs_vec_init() and after clargs_vec_destroy(); otherwise
 * count <= capacity, and items has room for capacity items.
 */
typedef struct clargs_vec {
    void *items;      /* The items, one after the other; NULL when empty. */
    size_t count;     /* The number of items. */
    size_t capacity;  /* The number of items there is room for. */
    size_t item_size; /* The size of one item, in bytes. */
} clargs_vec_t;

/*
 * Makes vec an empty array of items of item_size bytes. Allocates nothing
 * and cannot fail. Whatever vec held before is overwritten, not freed.
 *
 * Preconditions: vec is not NULL; item_size is not 0. They are checked with
 * assert.
 */
void clargs_vec_init(clargs_vec_t *vec, size_t item_size);

/*
 * Adds a copy of the item_size bytes at item to the end of the array. When
 * the array is full it grows first: to 4 items the first time, to twice
 * its capacity after that. Growing may move the items.
 *
 * Returns CLARGS_ERROR_NONE, or CLARGS_ERROR_OUT_OF_MEMORY when the
 * allocation fails or when the size of the grown array cannot be
 * represented in a size_t (the allocator is not called then). After a
 * failure the count, the capacity and the items are as they were before
 * the call, and the array can be used and destroyed as before. Does not
 * write errno, print anything or end the process.
 *
 * Preconditions: vec is not NULL and was initialized with
 * clargs_vec_init(); item is not NULL and does not point into the array.
 * That neither is NULL is checked with assert.
 */
clargs_error_t clargs_vec_push(clargs_vec_t *vec, const void *item);

/*
 * Returns a pointer to the item at index, counted from 0. The pointer is
 * valid until the next clargs_vec_push() or clargs_vec_destroy() of the
 * array.
 *
 * Preconditions: vec is not NULL; index < vec->count. They are checked with
 * assert.
 */
void *clargs_vec_at(const clargs_vec_t *vec, size_t index);

/*
 * Frees the memory of the array and makes it empty again, as it was after
 * clargs_vec_init(): the item size stays, and the array can be used again.
 * It may be called twice, and on an array that never had an item. What the
 * items point to is not freed.
 *
 * Preconditions: vec is not NULL and was initialized with
 * clargs_vec_init(). That it is not NULL is checked with assert.
 */
void clargs_vec_destroy(clargs_vec_t *vec);

#endif /* CLARGS_VEC_H */
