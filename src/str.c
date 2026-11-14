/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - the internal string functions.
 */
#include "str.h"

#include "alloc_internal.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

char *argx_strdup(const char *str)
{
    size_t size;
    char *copy;

    assert(str != NULL);

    /*
     * The terminator is copied along with the string. The sum cannot
     * overflow, because the string and its terminator form an existing
     * object.
     */
    size = strlen(str) + 1;
    copy = argx_malloc(size);
    if (copy == NULL)
    {
        return NULL;
    }

    memcpy(copy, str, size);

    return copy;
}
