/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - the allocator: the functions the library gets its memory from.
 *
 * The library takes all of its memory from one allocator, a set of four
 * functions of the same form as the standard malloc, realloc, calloc and
 * free. Unless an allocator is set, it uses the standard functions.
 *
 * How the library calls the functions of the allocator:
 *
 * - malloc_fn is called with a size greater than 0.
 * - calloc_fn is called with a count and a size greater than 0 whose product
 *   fits in a size_t.
 * - realloc_fn is called with a pointer that is not NULL and came from the
 *   same allocator, and with a size greater than 0.
 * - free_fn is called with a pointer that is not NULL and came from the same
 *   allocator. No pointer is passed twice.
 *
 * What the functions of the allocator must do:
 *
 * - malloc_fn, calloc_fn and realloc_fn return memory that is aligned for
 *   an object of any type, or NULL when they fail.
 * - calloc_fn returns count * size bytes that are all 0.
 * - realloc_fn keeps the contents up to the smaller of the old and the new
 *   size when it succeeds. When it fails, it leaves the old memory valid
 *   and unchanged.
 * - The four functions stay valid until another allocator has been set and
 *   every object that was allocated with them has been destroyed.
 * - If the library is used from several threads, the four functions work
 *   when they are called at the same time.
 *
 * The allocator carries no user data. An allocator that needs state keeps
 * it on its own side.
 */
#ifndef ARGX_ALLOC_H
#define ARGX_ALLOC_H

#include <argx/export.h>

#include <stddef.h>

/*
 * The types of the four functions: the same form as the standard malloc,
 * realloc, calloc and free.
 */
typedef void *(*argx_malloc_fn_t)(size_t size);
typedef void *(*argx_realloc_fn_t)(void *ptr, size_t size);
typedef void *(*argx_calloc_fn_t)(size_t count, size_t size);
typedef void (*argx_free_fn_t)(void *ptr);

/*
 * An allocator. The standard functions can be used as they are:
 *
 *     argx_allocator_t allocator = { malloc, realloc, calloc, free };
 *
 * The layout is fixed: later versions add no field and change neither the
 * order nor the types of the fields.
 */
typedef struct argx_allocator
{
    argx_malloc_fn_t malloc_fn;
    argx_realloc_fn_t realloc_fn;
    argx_calloc_fn_t calloc_fn;
    argx_free_fn_t free_fn;
} argx_allocator_t;

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Sets the allocator of the whole library. From then on, the library takes
 * and frees all of its memory with the four functions of *allocator.
 *
 * If allocator is NULL, the library goes back to the default allocator,
 * which uses the standard malloc, realloc, calloc and free.
 *
 * The structure is copied: it may be discarded as soon as the call returns.
 * The function allocates no memory and cannot fail.
 *
 * Preconditions:
 *
 * - If allocator is not NULL, none of its four functions is NULL. A
 *   structure that was only filled with zeros breaks this. A Debug build
 *   checks it with assert; a Release build does not check it, and the
 *   behaviour is then undefined.
 * - No object of the library exists, and no other thread is calling a
 *   function of the library. The library cannot check this. Memory that was
 *   taken from one allocator must not be freed with another one, and the
 *   setting is not synchronized. Usually the allocator is set once, when
 *   the program starts.
 */
ARGX_API void argx_set_allocator(const argx_allocator_t *allocator);

#ifdef __cplusplus
}
#endif

#endif /* ARGX_ALLOC_H */
