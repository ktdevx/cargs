/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - internal declarations for the string functions.
 */
#ifndef ARGX_STR_H
#define ARGX_STR_H

/*
 * Returns a new copy of str, terminator included, allocated with the
 * current allocator (argx_set_allocator(), <argx/alloc.h>). The name and
 * parameter match the common strdup. Copying an empty string yields a
 * one-byte string.
 *
 * The caller owns the copy and frees it with argx_free()
 * ("alloc_internal.h").
 *
 * Returns NULL when the allocation fails. Nothing is left allocated in
 * that case, and the caller reports it as ARGX_ERROR_OUT_OF_MEMORY. Does
 * not write errno, print anything or end the process.
 *
 * Precondition: str is not NULL. It is checked with assert.
 */
char *argx_strdup(const char *str);

#endif /* ARGX_STR_H */
