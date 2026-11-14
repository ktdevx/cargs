/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - internal declarations for the growing array.
 *
 * An argx_vec_t is an array of items that all have one size, given at run
 * time. Items are added at the end and stay where they were added. There
 * is no removal, insertion or sorting.
 *
 * The array holds copies of the items. If the items are pointers, the
 * array holds only the pointers. What they point to belongs to the caller.
 *
 * All its memory comes from the current allocator (argx_set_allocator(),
 * <argx/alloc.h>), through argx_realloc() and argx_free()
 * ("alloc_internal.h").
 *
 * Nothing here is synchronized. Do not change one argx_vec_t from several
 * threads at the same time.
 */
#ifndef ARGX_VEC_H
#define ARGX_VEC_H

#include <argx/error.h>

#include <stddef.h>

/*
 * The array. Read the item count from count, and treat the other fields
 * as private.
 *
 * It is empty (items == NULL, count == 0, capacity == 0) after
 * argx_vec_init() and after argx_vec_destroy(). Otherwise
 * count <= capacity, and items has room for capacity items.
 */
typedef struct argx_vec
{
    void *items;      /* The items, one after the other; NULL when empty. */
    size_t count;     /* The number of items. */
    size_t capacity;  /* The number of items there is room for. */
    size_t item_size; /* The size of one item, in bytes. */
} argx_vec_t;

/*
 * Makes vec an empty array of item_size-byte items. Allocates nothing and
 * cannot fail. Whatever vec held before is overwritten, not freed.
 *
 * Preconditions: vec is not NULL; item_size is not 0. They are checked with
 * assert.
 */
void argx_vec_init(argx_vec_t *vec, size_t item_size);

/*
 * Appends a copy of the item_size bytes at item to the array. When the
 * array is full, it grows first: to 4 items the first time, and to twice
 * its capacity after that. Growing may move the items.
 *
 * Returns ARGX_ERROR_NONE, or ARGX_ERROR_OUT_OF_MEMORY when the
 * allocation fails or when the grown array's size cannot be represented
 * in a size_t (in which case the allocator is not called). After a
 * failure, the count, capacity and items are unchanged, and the array can
 * still be used and destroyed as before. Does not
 * write errno, print anything or end the process.
 *
 * Preconditions: vec is not NULL and was initialized with
 * argx_vec_init(); item is not NULL and does not point into the array.
 * That neither is NULL is checked with assert.
 */
argx_error_t argx_vec_push(argx_vec_t *vec, const void *item);

/*
 * Returns a pointer to the item at index, counted from 0. The pointer
 * stays valid until the next argx_vec_push() or argx_vec_destroy() on the
 * array.
 *
 * Preconditions: vec is not NULL; index < vec->count. They are checked with
 * assert.
 */
void *argx_vec_at(const argx_vec_t *vec, size_t index);

/*
 * Frees the array's memory and makes it empty again, as after
 * argx_vec_init(). The item size is kept, and the array can be used again.
 * It may be called twice, and on an array that never had an item. What
 * the items point to is not freed.
 *
 * Preconditions: vec is not NULL and was initialized with
 * argx_vec_init(). That it is not NULL is checked with assert.
 */
void argx_vec_destroy(argx_vec_t *vec);

#endif /* ARGX_VEC_H */
