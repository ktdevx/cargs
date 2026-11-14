/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX test support - the wrap allocator. See wrap_allocator.h.
 */
#include "wrap_allocator.h"

#include <stdlib.h>
#include <string.h>

/* The counts: one wrap allocator per test executable. */
static argx_wrap_counts_t counts;

static void *wrap_malloc(size_t size)
{
    ++counts.malloc_calls;
    counts.malloc_size = size;
    return malloc(size);
}

static void *wrap_realloc(void *ptr, size_t size)
{
    ++counts.realloc_calls;
    counts.realloc_size = size;
    return realloc(ptr, size);
}

static void *wrap_calloc(size_t count, size_t size)
{
    ++counts.calloc_calls;
    return calloc(count, size);
}

static void wrap_free(void *ptr)
{
    ++counts.free_calls;
    free(ptr);
}

argx_allocator_t argx_wrap_allocator_get(void)
{
    argx_allocator_t allocator;

    allocator.malloc_fn = wrap_malloc;
    allocator.realloc_fn = wrap_realloc;
    allocator.calloc_fn = wrap_calloc;
    allocator.free_fn = wrap_free;
    return allocator;
}

void argx_wrap_allocator_reset(void)
{
    memset(&counts, 0, sizeof counts);
}

argx_wrap_counts_t argx_wrap_allocator_counts(void)
{
    return counts;
}

size_t argx_wrap_allocator_calls(void)
{
    return counts.malloc_calls + counts.realloc_calls + counts.calloc_calls + counts.free_calls;
}
