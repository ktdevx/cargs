/*
 * CLArgs - internal declarations for the memory functions.
 *
 * All the memory of the library goes through these four functions. Their
 * names and parameters match the standard functions. Each of them calls the
 * matching function of the allocator that is set (clargs_set_allocator(),
 * <clargs/alloc.h>), and only in the way that header promises: never with a
 * size or a count of 0 and never with a NULL pointer.
 *
 * None of them writes errno, prints anything or ends the process when an
 * allocation fails. They have no preconditions other than those of the
 * standard functions: a pointer that is passed is NULL or came from one of
 * these functions, with the same allocator set, and has not been freed.
 *
 * For a request whose size is not 0, NULL means that the allocation failed;
 * the caller reports it as CLARGS_ERROR_OUT_OF_MEMORY. A caller whose size
 * can be 0 checks the size first and does not take that NULL for a failure.
 */
#ifndef CLARGS_ALLOC_INTERNAL_H
#define CLARGS_ALLOC_INTERNAL_H

#include <stddef.h>

/*
 * Allocates size bytes. The contents are indeterminate. Returns NULL when
 * the allocation fails. If size is 0, returns NULL without calling the
 * allocator.
 */
void *clargs_malloc(size_t size);

/*
 * Allocates count * size bytes that are all 0. Returns NULL when the
 * allocation fails. If count or size is 0, or if count * size does not fit
 * in a size_t, returns NULL without calling the allocator.
 */
void *clargs_calloc(size_t count, size_t size);

/*
 * Changes the size of the memory at ptr to size bytes and returns the new
 * pointer. The contents are kept up to the smaller of the old and the new
 * size.
 *
 * - If ptr is NULL, this is clargs_malloc(size).
 * - If ptr is not NULL and size is 0, this is clargs_free(ptr), and the
 *   result is NULL. Do not use ptr afterwards.
 * - Otherwise, NULL means that the allocation failed. The memory at ptr is
 *   then still valid and unchanged, so check the result before replacing
 *   the old pointer.
 *
 * A caller that computes size as a product checks for overflow itself.
 */
void *clargs_realloc(void *ptr, size_t size);

/* Frees the memory at ptr. If ptr is NULL, does nothing. */
void clargs_free(void *ptr);

#endif /* CLARGS_ALLOC_INTERNAL_H */
