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

/*
 * Read an option that takes no value.
 *
 * clargs_result_flag() returns whether a flag was given, and whether a
 * negatable flag is on: the state that was given last or, when it was not
 * given, its initial state.
 * clargs_result_count() returns how many times a count was given, by its
 * short name and its long name together.
 *
 * An option of a command that the command line did not select reads as not
 * given: false, 0, and the initial state of a negatable flag.
 *
 * Preconditions:
 *
 * - result and option are not NULL. Checked with assert.
 * - option belongs to the schema that result was made from. Checked with
 *   assert.
 * - The option of clargs_result_flag() is a flag or a negatable flag,
 *   CLARGS_OPTION_FLAG or CLARGS_OPTION_NEGATABLE. Checked with assert.
 * - The option of clargs_result_count() is a count, CLARGS_OPTION_COUNT.
 *   Checked with assert.
 */
CLARGS_API bool clargs_result_flag(const clargs_result_t *result, const clargs_option_t *option);
CLARGS_API size_t clargs_result_count(const clargs_result_t *result, const clargs_option_t *option);

/*
 * Returns where what result holds for option came from. option may be of
 * any kind.
 *
 * - CLARGS_SOURCE_COMMAND_LINE: option was given on the command line.
 * - CLARGS_SOURCE_DEFAULT: option was not given, and what result holds
 *   comes from the schema: the default value of an option that takes a
 *   value, the initial state of a negatable flag, "not given" for a flag
 *   and 0 for a count.
 * - CLARGS_SOURCE_NONE: an option that takes a value has none, or option
 *   belongs to a command that the command line did not select.
 *
 * Preconditions:
 *
 * - result and option are not NULL. Checked with assert.
 * - option belongs to the schema that result was made from. Checked with
 *   assert.
 */
CLARGS_API clargs_source_t clargs_result_option_source(const clargs_result_t *result,
                                                       const clargs_option_t *option);

/*
 * Read the value of an option that takes a value.
 *
 * clargs_result_option_string(), _int(), _float() and _choice() read the
 * value of a value type of the kind CLARGS_TYPE_STRING, CLARGS_TYPE_INT,
 * CLARGS_TYPE_FLOAT and CLARGS_TYPE_CHOICE. If option has a value, they
 * write it to *out and return true. If it has none, they return false and
 * leave *out as it was.
 *
 * clargs_result_option_value() works for a value type of every kind, the
 * user-defined ones included. It returns a pointer to the area that the
 * value type wrote the value to: an object of the type of its values (const
 * char *, int64_t, double, size_t, or what the conversion function of a
 * user-defined value type writes; see <clargs/type.h>). It returns NULL if
 * option has no value.
 *
 * The value is the one that was given last on the command line or, when
 * option was not given, its default value. option has no value when it was
 * not given and has no default value, when the command line has a parse
 * error before a default value is applied, and when option belongs to a
 * command that the command line did not select.
 *
 * A string and the area are valid until result is destroyed. Do not free
 * or modify them.
 *
 * Preconditions:
 *
 * - result, option and out are not NULL. Checked with assert.
 * - option belongs to the schema that result was made from. Checked with
 *   assert.
 * - option takes a value, CLARGS_OPTION_VALUE. Checked with assert.
 * - The value type of the option of clargs_result_option_string() is of the
 *   kind CLARGS_TYPE_STRING, that of _int() CLARGS_TYPE_INT, that of
 *   _float() CLARGS_TYPE_FLOAT and that of _choice() CLARGS_TYPE_CHOICE.
 *   Checked with assert.
 */
CLARGS_API bool clargs_result_option_string(const clargs_result_t *result, const clargs_option_t *option,
                                            const char **out);
CLARGS_API bool clargs_result_option_int(const clargs_result_t *result, const clargs_option_t *option,
                                         int64_t *out);
CLARGS_API bool clargs_result_option_float(const clargs_result_t *result, const clargs_option_t *option,
                                           double *out);
CLARGS_API bool clargs_result_option_choice(const clargs_result_t *result, const clargs_option_t *option,
                                            size_t *out);
CLARGS_API const void *clargs_result_option_value(const clargs_result_t *result,
                                                  const clargs_option_t *option);

/*
 * Read the values of a positional.
 *
 * clargs_result_positional_count() returns how many values positional has:
 * 0 or 1 for a positional that takes one argument at most, and the number
 * of the arguments that were assigned to it for one that takes any number.
 * clargs_result_positional_source() returns where the values came from:
 * CLARGS_SOURCE_COMMAND_LINE, CLARGS_SOURCE_DEFAULT for the default value,
 * or CLARGS_SOURCE_NONE when positional has no value.
 *
 * The other functions read the value at index, counted from 0 in the order
 * of the command line, as the functions of an option with a value do:
 * clargs_result_positional_string(), _int(), _float() and _choice() write
 * the value to *out and return true, and clargs_result_positional_value()
 * returns a pointer to the area that holds the value, for a value type of
 * every kind. If there is no value at index, that is, if index is not less
 * than clargs_result_positional_count(), they return false and leave *out
 * as it was, or return NULL. That is not a violation of a precondition.
 *
 * A positional of a command that the command line did not select has no
 * value: its count is 0 and its source is CLARGS_SOURCE_NONE.
 *
 * A string and the area are valid until result is destroyed. Do not free
 * or modify them.
 *
 * Preconditions:
 *
 * - result, positional and out are not NULL. Checked with assert.
 * - positional belongs to the schema that result was made from. Checked
 *   with assert.
 * - The value type of the positional of clargs_result_positional_string()
 *   is of the kind CLARGS_TYPE_STRING, that of _int() CLARGS_TYPE_INT, that
 *   of _float() CLARGS_TYPE_FLOAT and that of _choice() CLARGS_TYPE_CHOICE.
 *   Checked with assert.
 */
CLARGS_API size_t clargs_result_positional_count(const clargs_result_t *result,
                                                 const clargs_positional_t *positional);
CLARGS_API clargs_source_t clargs_result_positional_source(const clargs_result_t *result,
                                                           const clargs_positional_t *positional);
CLARGS_API bool clargs_result_positional_string(const clargs_result_t *result,
                                                const clargs_positional_t *positional, size_t index,
                                                const char **out);
CLARGS_API bool clargs_result_positional_int(const clargs_result_t *result,
                                             const clargs_positional_t *positional, size_t index,
                                             int64_t *out);
CLARGS_API bool clargs_result_positional_float(const clargs_result_t *result,
                                               const clargs_positional_t *positional, size_t index,
                                               double *out);
CLARGS_API bool clargs_result_positional_choice(const clargs_result_t *result,
                                                const clargs_positional_t *positional, size_t index,
                                                size_t *out);
CLARGS_API const void *clargs_result_positional_value(const clargs_result_t *result,
                                                      const clargs_positional_t *positional, size_t index);

#ifdef __cplusplus
}
#endif

#endif /* CLARGS_PARSE_H */
