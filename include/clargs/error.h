/*
 * CLArgs - error codes and their messages.
 *
 * A function that can fail at run time returns a clargs_error_t. Only
 * CLARGS_ERROR_NONE means success; every other value is a failure. Test for
 * success like this:
 *
 *     if (error == CLARGS_ERROR_NONE) { ... }
 *
 * An error code tells the kind of the failure only. It carries no position
 * and no subject.
 *
 * Later versions add error codes. Write code that may receive a value it
 * does not know: give a switch over clargs_error_t a default label.
 */
#ifndef CLARGS_ERROR_H
#define CLARGS_ERROR_H

#include <clargs/export.h>

/*
 * The list of the error codes: CLARGS_ERROR_LIST(X) expands to one
 * X(name, message) for each error code, in the order of their values. The
 * enumeration below and clargs_error_message() are generated from it. Use it
 * to generate tables of your own.
 *
 * The values are given in the order of the list, starting at 0, and a value
 * that has been published never changes and is never used for another
 * meaning. So add new error codes at the end only: never remove a line,
 * never reorder the lines and never add a line in the middle.
 */
#define CLARGS_ERROR_LIST(X) \
    X(CLARGS_ERROR_NONE, "success") \
    X(CLARGS_ERROR_OUT_OF_MEMORY, "out of memory") \
    X(CLARGS_ERROR_INVALID_NAME, "invalid name") \
    X(CLARGS_ERROR_NAME_CONFLICT, "name already in use") \
    X(CLARGS_ERROR_INVALID_DEFINITION, "inconsistent definition") \
    X(CLARGS_ERROR_INVALID_DEFAULT, "default value rejected by its value type") \
    X(CLARGS_ERROR_INVALID_VALUE, "invalid value") \
    X(CLARGS_ERROR_INVALID_COMMAND_LINE, "invalid command line") \
    X(CLARGS_ERROR_WRITE_FAILED, "write failed")

/* The kind of a failure, or CLARGS_ERROR_NONE for success. */
typedef enum clargs_error
{
#define CLARGS_ERROR_ENUM(name, message) name,
    CLARGS_ERROR_LIST(CLARGS_ERROR_ENUM)
#undef CLARGS_ERROR_ENUM
} clargs_error_t;

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Returns the message of an error code: a short English phrase, such as
 * "out of memory".
 *
 * Any value may be passed. The result is never NULL and never empty. For a
 * value that is not in CLARGS_ERROR_LIST, it is "unknown error code".
 *
 * The string is valid until the program ends. Do not free it and do not
 * modify it.
 */
CLARGS_API const char *clargs_error_message(clargs_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* CLARGS_ERROR_H */
