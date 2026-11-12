/*
 * CLArgs - parsing: what a command line is read into.
 *
 * This header holds the enumerations that tell where a value came from and
 * what kind of error a command line has.
 */
#ifndef CLARGS_PARSE_H
#define CLARGS_PARSE_H

#include <clargs/error.h>
#include <clargs/export.h>

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

#endif /* CLARGS_PARSE_H */
