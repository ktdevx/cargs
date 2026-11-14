/*
 * CLine - internal declarations for the string functions.
 */
#ifndef CLINE_STR_H
#define CLINE_STR_H

/*
 * Returns a new copy of the string str, with its terminator, in memory of
 * the allocator that is set (cline_set_allocator(), <cline/alloc.h>). The
 * name and the parameter match the common strdup. The copy of an empty
 * string is a string of one byte.
 *
 * The caller owns the copy and frees it with cline_free()
 * ("alloc_internal.h").
 *
 * Returns NULL when the allocation fails; nothing is left allocated then,
 * and the caller reports it as CLINE_ERROR_OUT_OF_MEMORY. Does not write
 * errno, print anything or end the process.
 *
 * Precondition: str is not NULL. It is checked with assert.
 */
char *cline_strdup(const char *str);

#endif /* CLINE_STR_H */
