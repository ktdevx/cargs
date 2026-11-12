/*
 * CLArgs - parsing: what a command line is read into.
 *
 * This header holds the result of a parse, clargs_result_t, the functions
 * that read and destroy it, and the enumerations that tell where a value
 * came from and what kind of error a command line has.
 *
 * Ownership:
 *
 * - A result owns a copy of the arguments it was made from and everything
 *   that was read from them. clargs_result_destroy() frees all of it.
 * - A result borrows the schema and the value types that the schema uses:
 *   do not change or destroy them before the result is destroyed. The
 *   result never changes or destroys them.
 * - What the functions that read a result return is valid until the result
 *   is destroyed. The caller does not free it.
 *
 * A function that only reads takes a const result, changes nothing and
 * cannot fail. Such functions may be called on one result from several
 * threads at the same time.
 */
#ifndef CLARGS_PARSE_H
#define CLARGS_PARSE_H

#include <clargs/error.h>
#include <clargs/export.h>
#include <clargs/schema.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The result of a parse: what a command line was read into. The contents
 * are not public. */
typedef struct clargs_result clargs_result_t;

/*
 * Where a value came from, and where a parse error is.
 *
 * Later versions may add sources, such as an environment variable. Write
 * code that may receive a value it does not know: give a switch over
 * clargs_source_t a default label.
 */
typedef enum clargs_source {
    CLARGS_SOURCE_NONE,        /* There is no value; the error has no position. */
    CLARGS_SOURCE_DEFAULT,     /* The default value or the initial state of the schema. */
    CLARGS_SOURCE_COMMAND_LINE /* The command line (argv). */
} clargs_source_t;

/*
 * The kind of a parse error: what is wrong with a command line that does
 * not match the schema.
 *
 * Later versions may add kinds. Write code that may receive a value it does
 * not know: give a switch over clargs_parse_error_kind_t a default label.
 */
typedef enum clargs_parse_error_kind {
    CLARGS_PARSE_ERROR_NONE,                  /* There is no parse error. */
    CLARGS_PARSE_ERROR_UNKNOWN_OPTION,        /* An option that the command does not have. */
    CLARGS_PARSE_ERROR_UNKNOWN_SUBCOMMAND,    /* A subcommand that the command does not have. */
    CLARGS_PARSE_ERROR_MISSING_VALUE,         /* An option that takes a value has none. */
    CLARGS_PARSE_ERROR_UNEXPECTED_VALUE,      /* A value for an option that takes none. */
    CLARGS_PARSE_ERROR_INVALID_VALUE,         /* A value that its value type does not take. */
    CLARGS_PARSE_ERROR_MISSING_OPTION,        /* A required option was not given. */
    CLARGS_PARSE_ERROR_MISSING_POSITIONAL,    /* A required positional argument was not given. */
    CLARGS_PARSE_ERROR_UNEXPECTED_POSITIONAL, /* One positional argument too many. */
    CLARGS_PARSE_ERROR_INVALID_ENCODING       /* A wide argument that cannot be converted to UTF-8. */
} clargs_parse_error_kind_t;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Destroys result: frees the copy of the arguments and everything else the
 * result holds. The schema and its value types are neither changed nor
 * destroyed. Every string and every handle to a value that was read from
 * result is invalid afterwards.
 *
 * If result is NULL, does nothing.
 *
 * Preconditions:
 *
 * - result is NULL or a result that has not been destroyed. Not checked.
 * - The schema that result was made from, and the value types it uses,
 *   still exist and have not been changed. Not checked.
 */
CLARGS_API void clargs_result_destroy(clargs_result_t *result);

/*
 * Returns whether the command line matched the schema:
 *
 * - CLARGS_ERROR_NONE: it did.
 * - CLARGS_ERROR_INVALID_COMMAND_LINE: it did not; result holds a parse
 *   error.
 *
 * Preconditions:
 *
 * - result is not NULL. Checked with assert.
 */
CLARGS_API clargs_error_t clargs_result_error(const clargs_result_t *result);

/*
 * Read the commands that the command line selected, in their order: the
 * root command at index 0, then the subcommand that was named on the
 * command line, then its subcommand, and so on. Each command but the first
 * is a subcommand of the one before it.
 *
 * clargs_result_command_count() returns their number, which is at least 1.
 * clargs_result_command_at() returns the command at index, counted from 0.
 * clargs_result_command() returns the last of them: the command that the
 * command line ends in.
 *
 * The commands belong to the schema. Never NULL.
 *
 * Preconditions:
 *
 * - result is not NULL. Checked with assert.
 * - index < clargs_result_command_count(result). Checked with assert.
 */
CLARGS_API size_t clargs_result_command_count(const clargs_result_t *result);
CLARGS_API const clargs_command_t *clargs_result_command_at(const clargs_result_t *result, size_t index);
CLARGS_API const clargs_command_t *clargs_result_command(const clargs_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* CLARGS_PARSE_H */
