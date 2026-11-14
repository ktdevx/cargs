/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - internal declarations for the memory functions.
 *
 * All library memory goes through these four functions. Their names and
 * parameters match the standard functions. Each one calls the matching
 * function of the current allocator (argx_set_allocator(),
 * <argx/alloc.h>), and only as that header promises: never with a size or
 * count of 0, and never with a NULL pointer.
 *
 * None of them writes errno, prints anything or ends the process when an
 * allocation fails. Their only preconditions are those of the standard
 * functions: a pointer passed in is NULL, or came from one of these
 * functions while the same allocator was set and has not been freed.
 *
 * For a request with a nonzero size, NULL means the allocation failed, and
 * the caller reports it as ARGX_ERROR_OUT_OF_MEMORY. A caller whose size
 * can be 0 checks the size first and does not mistake that NULL for a
 * failure.
 */
#ifndef ARGX_ALLOC_INTERNAL_H
#define ARGX_ALLOC_INTERNAL_H

#include <stddef.h>

/*
 * Allocates size bytes. The contents are indeterminate. Returns NULL when
 * the allocation fails. If size is 0, returns NULL without calling the
 * allocator.
 */
void *argx_malloc(size_t size);

/*
 * Allocates count * size bytes, all set to 0. Returns NULL when the
 * allocation fails. If count or size is 0, or if count * size does not fit
 * in a size_t, returns NULL without calling the allocator.
 */
void *argx_calloc(size_t count, size_t size);

/*
 * Changes the size of the memory at ptr to size bytes and returns the new
 * pointer. The contents are kept up to the smaller of the old and new
 * sizes.
 *
 * - If ptr is NULL, this is argx_malloc(size).
 * - If ptr is not NULL and size is 0, this is argx_free(ptr), and the
 *   result is NULL. Do not use ptr afterwards.
 * - Otherwise, NULL means the allocation failed. The memory at ptr then
 *   stays valid and unchanged, so check the result before replacing the
 *   old pointer.
 *
 * A caller that computes size as a product checks for overflow itself.
 */
void *argx_realloc(void *ptr, size_t size);

/* Frees the memory at ptr. If ptr is NULL, does nothing. */
void argx_free(void *ptr);

#endif /* ARGX_ALLOC_INTERNAL_H */
