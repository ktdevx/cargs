/*
 * SPDX-FileCopyrightText: 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - internal declarations for the string functions.
 */
#ifndef ARGX_STR_H
#define ARGX_STR_H

/*
 * Returns a new copy of the string str, with its terminator, in memory of
 * the allocator that is set (argx_set_allocator(), <argx/alloc.h>). The
 * name and the parameter match the common strdup. The copy of an empty
 * string is a string of one byte.
 *
 * The caller owns the copy and frees it with argx_free()
 * ("alloc_internal.h").
 *
 * Returns NULL when the allocation fails; nothing is left allocated then,
 * and the caller reports it as ARGX_ERROR_OUT_OF_MEMORY. Does not write
 * errno, print anything or end the process.
 *
 * Precondition: str is not NULL. It is checked with assert.
 */
char *argx_strdup(const char *str);

#endif /* ARGX_STR_H */
