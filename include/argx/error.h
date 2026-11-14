/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - error codes and their messages.
 *
 * A function that can fail at run time returns an argx_error_t. Only
 * ARGX_ERROR_NONE means success; every other value is a failure. Test for
 * success like this:
 *
 *     if (error == ARGX_ERROR_NONE) { ... }
 *
 * An error code tells only what kind of failure occurred. It carries no
 * position and no subject.
 *
 * Later versions add error codes. Write code that may receive a value it
 * does not know: give a switch over argx_error_t a default label.
 */
#ifndef ARGX_ERROR_H
#define ARGX_ERROR_H

#include <argx/export.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * The error code list. ARGX_ERROR_LIST(X) expands to one X(name, message)
 * for each error code, in value order. The enumeration below and
 * argx_error_message() are generated from it. Use it to generate your own
 * tables.
 *
 * Values follow the list order, starting at 0. A published value never
 * changes and is never reused for another meaning. So add new error codes
 * only at the end: never remove a line, never reorder the lines and never
 * add a line in the middle.
 */
#define ARGX_ERROR_LIST(X) \
    X(ARGX_ERROR_NONE, "success") \
    X(ARGX_ERROR_OUT_OF_MEMORY, "out of memory") \
    X(ARGX_ERROR_INVALID_NAME, "invalid name") \
    X(ARGX_ERROR_NAME_CONFLICT, "name already in use") \
    X(ARGX_ERROR_INVALID_DEFINITION, "inconsistent definition") \
    X(ARGX_ERROR_INVALID_DEFAULT, "default value rejected by its value type") \
    X(ARGX_ERROR_INVALID_VALUE, "invalid value") \
    X(ARGX_ERROR_INVALID_COMMAND_LINE, "invalid command line") \
    X(ARGX_ERROR_WRITE_FAILED, "write failed")

/* The kind of a failure, or ARGX_ERROR_NONE for success. */
typedef enum argx_error
{
#define ARGX_ERROR_ENUM(name, message) name,
    ARGX_ERROR_LIST(ARGX_ERROR_ENUM)
#undef ARGX_ERROR_ENUM
} argx_error_t;

/*
 * Returns the message for an error code: a short English phrase, such as
 * "out of memory".
 *
 * Any value may be passed. The result is never NULL and never empty. For a
 * value that is not in ARGX_ERROR_LIST, it is "unknown error code".
 *
 * The string is valid until the program ends. Do not free it and do not
 * modify it.
 */
ARGX_API const char *argx_error_message(argx_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* ARGX_ERROR_H */
