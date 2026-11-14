/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - parsing: what a command line is read into.
 *
 * This header holds the functions that parse a command line,
 * argx_parse() and argx_parse_wide() for wide strings, the result of a
 * parse, argx_result_t, the functions that read and destroy it, and the
 * enumerations that tell where a value came from and what kind of error a
 * command line has.
 *
 * Ownership:
 *
 * - A result owns a copy of the arguments it was made from and everything
 *   that was read from them. argx_result_destroy() frees all of it.
 * - A result borrows the schema and the value types that the schema uses:
 *   do not change or destroy them before the result is destroyed. The
 *   result never changes or destroys them.
 * - What the functions that read a result return is valid until the result
 *   is destroyed. The caller does not free it.
 *
 * Errors:
 *
 * - The two functions that parse return a argx_error_t. A command line
 *   that does not match the schema is ARGX_ERROR_INVALID_COMMAND_LINE,
 *   and then, other than with every other failure, the output is a result
 *   and not NULL: it holds the parse error.
 * - A function that only reads takes a const result, changes nothing,
 *   allocates nothing and cannot fail. Such functions may be called on one
 *   result from several threads at the same time. What a result does not
 *   have reads as "none": false, 0, NULL or ARGX_SOURCE_NONE.
 * - A NULL pointer where none is allowed, a schema that is not validated,
 *   an option or a positional of another schema or of the wrong kind, and
 *   an index of a command that is out of range are errors of the program,
 *   not error codes: see the preconditions of each function.
 *
 * Preconditions of every function that takes a result, besides those that
 * each function lists:
 *
 * - The result has not been destroyed. Not checked.
 * - The schema that the result was made from, and the value types it uses,
 *   have not been changed or destroyed. Not checked.
 */
#ifndef ARGX_PARSE_H
#define ARGX_PARSE_H

#include <argx/error.h>
#include <argx/export.h>
#include <argx/schema.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * The result of a parse: what a command line was read into. The contents
 * are not public.
 */
typedef struct argx_result argx_result_t;

/*
 * Where a value came from, and where a parse error is.
 *
 * Later versions may add sources, such as an environment variable. Write
 * code that may receive a value it does not know: give a switch over
 * argx_source_t a default label.
 */
typedef enum argx_source
{
    ARGX_SOURCE_NONE,        /* There is no value; the error has no position. */
    ARGX_SOURCE_DEFAULT,     /* The default value or the initial state of the schema. */
    ARGX_SOURCE_COMMAND_LINE /* The command line (argv). */
} argx_source_t;

/*
 * The kind of a parse error: what is wrong with a command line that does
 * not match the schema.
 *
 * Later versions may add kinds. Write code that may receive a value it does
 * not know: give a switch over argx_parse_error_kind_t a default label.
 */
typedef enum argx_parse_error_kind
{
    ARGX_PARSE_ERROR_NONE,                  /* There is no parse error. */
    ARGX_PARSE_ERROR_UNKNOWN_OPTION,        /* An option that the command does not have. */
    ARGX_PARSE_ERROR_UNKNOWN_SUBCOMMAND,    /* A subcommand that the command does not have. */
    ARGX_PARSE_ERROR_MISSING_VALUE,         /* An option that takes a value has none. */
    ARGX_PARSE_ERROR_UNEXPECTED_VALUE,      /* A value for an option that takes none. */
    ARGX_PARSE_ERROR_INVALID_VALUE,         /* A value that its value type does not take. */
    ARGX_PARSE_ERROR_MISSING_OPTION,        /* A required option was not given. */
    ARGX_PARSE_ERROR_MISSING_POSITIONAL,    /* A required positional argument was not given. */
    ARGX_PARSE_ERROR_UNEXPECTED_POSITIONAL, /* One positional argument too many. */
    ARGX_PARSE_ERROR_INVALID_ENCODING       /* A wide argument that cannot be converted to UTF-8. */
} argx_parse_error_kind_t;

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Parses the command line argv, which has argc arguments, with the schema
 * of root, and writes a new result to *out. argc and argv can be passed as
 * main() received them.
 *
 * argv[0] is the name of the program and is not parsed. When there is no
 * other argument, also when argc is 0, the command line is one on which
 * nothing was given. The other arguments are read once each, from the
 * first to the last, and matched with the current command: root at first,
 * and then the subcommand that was named last (see below). The scan stops
 * at the first argument that does not match the schema.
 *
 * An argument that starts with "--" and has more characters is a long
 * option. Its name ends before the first '=' and is compared as a whole,
 * byte by byte, with the long names and the negated names of the options
 * of the command: upper and lower case are different, and a part of a name
 * does not match.
 *
 *     --name         a flag is given; a count is one more; a negatable
 *                    flag is on
 *     --no-name      a negatable flag is off
 *     --name=value   an option that takes a value has the value, which is
 *                    everything after the first '=' and may be empty
 *     --name value   an option that takes a value has the next argument as
 *                    its value, whatever that argument looks like
 *
 * An argument that starts with one '-' and has more characters is a bundle
 * of short options. Each character after the '-' is compared, from the
 * first to the last, with the short names of the options of the command;
 * upper and lower case are different. An option that takes a value ends the
 * bundle.
 *
 *     -v             a flag is given; a count is one more; a negatable
 *                    flag is on
 *     -abc           the same as -a -b -c; -vvv gives a count three times
 *     -ovalue        an option that takes a value has the rest of the
 *                    argument as its value, whatever the rest looks like:
 *                    the value of -o=value is "=value"
 *     -o value       an option that takes a value and is the last character
 *                    has the next argument as its value, whatever that
 *                    argument looks like
 *     -abovalue      the same as -a -b -ovalue; -abo value is the same as
 *                    -a -b -o value
 *
 * An argument with the form of a negative number, a '-' and a digit or a
 * '-', a '.' and a digit, such as "-1" or "-.5", is a bundle only when the
 * command has an option whose short name is a digit. For a command that
 * has none, it is not an option. An argument that is taken as the value of
 * an option is never looked at in this way.
 *
 * Every other argument is not an option: one that does not start with a
 * '-', the empty string, and a '-' alone.
 *
 * For a command that has subcommands, an argument that is not an option is
 * the name of a subcommand. It is compared as a whole, byte by byte, with
 * the names of the subcommands of the current command: upper and lower case
 * are different, and a part of a name does not match. The subcommand with
 * that name is selected and becomes the current command: the arguments
 * after its name are read with its options, its positionals and its
 * subcommands, to any depth. A name that is no subcommand of the current
 * command is a parse error.
 *
 *     tool remote add --force name
 *                    selects "remote" of tool and then "add" of "remote";
 *                    --force is an option of "add", and name a value of
 *                    its first positional
 *
 * An option is looked up in the current command only, not in its parent and
 * not in its subcommands: an option of a command is written after the name
 * of that command and before the name of its subcommand. An option that
 * only the parent has is an unknown option after the name of the
 * subcommand. When a command and its subcommand have an option of the same
 * name, each is given where its command is the current command. Whether an
 * argument with the form of a negative number is a bundle depends on the
 * short names of the current command as well.
 *
 * A command line that names no subcommand of a command that has subcommands
 * is not a parse error: the command line ends in that command, which
 * argx_result_command() returns. The options and the positionals of the
 * commands that were not selected read as not given.
 *
 * For a command that has no subcommand, an argument that is not an option
 * is a value of a positional of the command. The arguments are assigned in
 * their order to the positionals in the order in which they were added:
 *
 *     ARGX_ARITY_REQUIRED, ARGX_ARITY_OPTIONAL
 *                    the positional has the argument as its value, and the
 *                    next argument goes to the next positional
 *     ARGX_ARITY_ZERO_OR_MORE, ARGX_ARITY_ONE_OR_MORE
 *                    the positional has the argument as one more value, and
 *                    so has every argument that follows
 *
 * An argument after every positional has its value, or for a command that
 * has no positional, is one argument too many, a parse error. Each value is
 * converted with the value type of its positional when it is read.
 *
 * Options and arguments that are not options can be mixed: an option may
 * stand before, between and after the values of the positionals, and the
 * assignment is the same.
 *
 * The argument "--" alone is the terminator of the options. It is not a
 * value itself. Every argument after the first terminator is an argument
 * that is not an option, whatever it looks like: "-v", "--name" and a
 * second "--" are values then. Such an argument is never the name of a
 * subcommand: for a command that has subcommands, which has no positional,
 * it is one argument too many, a parse error. The terminator stays in
 * effect after a subcommand was selected.
 *
 * An option may be given more than once, by its short name and its long
 * name alike: a flag stays given, a count is the number of times, a
 * negatable flag has the state that was given last, and an option that
 * takes a value has the value that was given last. Each
 * value is converted with the value type of its option when it is read,
 * also a value that is replaced later.
 *
 * When the last argument was read without a parse error, the values are
 * resolved, for the selected commands only: the root and every subcommand
 * that was named. A command that was not selected is neither checked nor
 * given a default value.
 *
 * First, what is required must have been given: an option that takes a
 * value and is required (argx_option_set_required()) must have a value,
 * and a positional of the arity ARGX_ARITY_REQUIRED or
 * ARGX_ARITY_ONE_OR_MORE must have one value at least. One that is
 * missing is a parse error. Of several, the first is reported: the
 * commands from the root on, the options of a command before its
 * positionals, each in the order in which they were added.
 *
 * Then, when nothing is missing, what was not given gets its default
 * value:
 *
 *     an option that takes a value, a positional of the arity
 *     ARGX_ARITY_OPTIONAL
 *                    with a default value: the default value, converted
 *                    with its value type; its source is
 *                    ARGX_SOURCE_DEFAULT
 *                    without one: no value; its source is
 *                    ARGX_SOURCE_NONE
 *     a positional of the arity ARGX_ARITY_ZERO_OR_MORE
 *                    no value: its count is 0
 *     a flag, a count, a negatable flag
 *                    not given, 0, its initial state; the source is
 *                    ARGX_SOURCE_DEFAULT
 *
 * A default value that its value type does not take is a parse error. The
 * validation of the schema checked the default values, so this happens
 * only with a user-defined value type that does not give the same answer
 * every time.
 *
 * The command line and the default values are the only sources of a value:
 * nothing else, such as the environment, is read.
 *
 * What is returned, and what is written to *out:
 *
 *     ARGX_ERROR_NONE                  a result  The command line matches
 *                                                the schema.
 *     ARGX_ERROR_INVALID_COMMAND_LINE  a result  It does not. The result
 *                                                holds the parse error and
 *                                                what was read before it.
 *     ARGX_ERROR_OUT_OF_MEMORY         NULL      An allocation failed, or
 *                                                a value type returned
 *                                                this code. Nothing stays
 *                                                allocated.
 *
 * A parse error is read with argx_result_error_kind() and the functions
 * next to it.
 *
 * The result of a command line with a parse error, whichever it is, holds
 * what was read before the error: the selected commands, the options that
 * were given and the values that were converted, all with the source
 * ARGX_SOURCE_COMMAND_LINE. It holds no default value: an option that
 * takes a value and a positional that were not given have no value, also
 * those with a default value. A flag, a count and a negatable flag that
 * were not given read as not given, 0 and the initial state.
 *
 * Ownership:
 *
 * - The caller owns the result and destroys it with
 *   argx_result_destroy(), also the result of a parse that returned
 *   ARGX_ERROR_INVALID_COMMAND_LINE.
 * - argv and its strings are only read, and the result keeps its own copy
 *   of them: after the call, argv may be changed or freed.
 * - The schema and its value types are only read, and the result borrows
 *   them: destroy the result before the schema, and do not add to the
 *   schema or set anything in it while a result of it exists.
 *
 * Nothing is kept between two calls: the same schema and the same argv give
 * the same result every time.
 *
 * Preconditions:
 *
 * - root and out are not NULL. Checked with assert.
 * - root is a root, not a subcommand. Checked with assert.
 * - The schema is validated: argx_command_validate() succeeded on root
 *   and nothing was added or set since (argx_command_is_validated()).
 *   Checked with assert. A schema that is not validated is not reported
 *   with an error code.
 * - argc is not negative. Checked with assert.
 * - If argc is greater than 0, argv is not NULL and none of argv[0] to
 *   argv[argc - 1] is NULL. Checked with assert. argv[argc] is not read.
 * - The schema and its value types are not changed or destroyed before the
 *   result is destroyed. Not checked.
 */
ARGX_API argx_error_t argx_parse(const argx_command_t *root, int argc, char *const *argv,
                                 argx_result_t **out);

/*
 * Parses a command line of wide strings, such as the argv of wmain(), with
 * the schema of root: the same as argx_parse(), after each argument was
 * converted to UTF-8. The function is there on every platform.
 *
 * How a wide string is read depends on the width of wchar_t:
 *
 * - 16 bits (Windows): as UTF-16. A high surrogate followed by a low
 *   surrogate is one character.
 * - wider (Linux, macOS): as UTF-32.
 *
 * The conversion does not depend on the locale. What it gives is what
 * argx_parse() gives for the same command line written in UTF-8: the same
 * result, or the same parse error. Every string that is read from the
 * result is UTF-8: the values, and the argument, the value and the message
 * of a parse error. argx_parse() itself does not look at the bytes of its
 * arguments: bytes that are not UTF-8 are passed on as they are.
 *
 * An argument that cannot be converted is the parse error
 * ARGX_PARSE_ERROR_INVALID_ENCODING. That is an argument with a surrogate
 * that is not part of a pair, in UTF-16, or with a value that is negative,
 * in the range of the surrogates (U+D800 to U+DFFF) or above U+10FFFF, in
 * UTF-32. It is found when the argument is about to be read, also when it
 * would be the value of the option before it, so an error of an earlier
 * argument is reported instead. The argument and the value of the error
 * are the argument in UTF-8, with U+FFFD in the place of each unit that
 * could not be converted; the error has no option, no positional and no
 * letter. argv[0] is not parsed, and so it is never this error.
 *
 * What is returned, what is written to *out, the ownership and the
 * preconditions are those of argx_parse(), with argv and its wide
 * strings in the place of argv and its strings.
 */
ARGX_API argx_error_t argx_parse_wide(const argx_command_t *root, int argc, wchar_t *const *argv,
                                      argx_result_t **out);

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
ARGX_API void argx_result_destroy(argx_result_t *result);

/*
 * Returns whether the command line matched the schema:
 *
 * - ARGX_ERROR_NONE: it did.
 * - ARGX_ERROR_INVALID_COMMAND_LINE: it did not; result holds a parse
 *   error.
 *
 * Preconditions:
 *
 * - result is not NULL. Checked with assert.
 */
ARGX_API argx_error_t argx_result_error(const argx_result_t *result);

/*
 * Read the commands that the command line selected, in their order: the
 * root command at index 0, then the subcommand that was named on the
 * command line, then its subcommand, and so on. Each command but the first
 * is a subcommand of the one before it.
 *
 * argx_result_command_count() returns their number, which is at least 1.
 * argx_result_command_at() returns the command at index, counted from 0.
 * argx_result_command() returns the last of them: the command that the
 * command line ends in.
 *
 * The commands belong to the schema. Never NULL.
 *
 * Preconditions:
 *
 * - result is not NULL. Checked with assert.
 * - index < argx_result_command_count(result). Checked with assert.
 */
ARGX_API size_t argx_result_command_count(const argx_result_t *result);
ARGX_API const argx_command_t *argx_result_command_at(const argx_result_t *result, size_t index);
ARGX_API const argx_command_t *argx_result_command(const argx_result_t *result);

/*
 * Read an option that takes no value.
 *
 * argx_result_flag() returns whether a flag was given, and whether a
 * negatable flag is on: the state that was given last or, when it was not
 * given, its initial state.
 * argx_result_count() returns how many times a count was given, by its
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
 * - The option of argx_result_flag() is a flag or a negatable flag,
 *   ARGX_OPTION_FLAG or ARGX_OPTION_NEGATABLE. Checked with assert.
 * - The option of argx_result_count() is a count, ARGX_OPTION_COUNT.
 *   Checked with assert.
 */
ARGX_API bool argx_result_flag(const argx_result_t *result, const argx_option_t *option);
ARGX_API size_t argx_result_count(const argx_result_t *result, const argx_option_t *option);

/*
 * Returns where what result holds for option came from. option may be of
 * any kind.
 *
 * - ARGX_SOURCE_COMMAND_LINE: option was given on the command line.
 * - ARGX_SOURCE_DEFAULT: option was not given, and what result holds
 *   comes from the schema: the default value of an option that takes a
 *   value, the initial state of a negatable flag, "not given" for a flag
 *   and 0 for a count.
 * - ARGX_SOURCE_NONE: an option that takes a value has none, or option
 *   belongs to a command that the command line did not select.
 *
 * Preconditions:
 *
 * - result and option are not NULL. Checked with assert.
 * - option belongs to the schema that result was made from. Checked with
 *   assert.
 */
ARGX_API argx_source_t argx_result_option_source(const argx_result_t *result,
                                                 const argx_option_t *option);

/*
 * Read the value of an option that takes a value.
 *
 * argx_result_option_string(), _int(), _float() and _choice() read the
 * value of a value type of the kind ARGX_TYPE_STRING, ARGX_TYPE_INT,
 * ARGX_TYPE_FLOAT and ARGX_TYPE_CHOICE. If option has a value, they
 * write it to *out and return true. If it has none, they return false and
 * leave *out as it was.
 *
 * argx_result_option_value() works for a value type of every kind, the
 * user-defined ones included. It returns a pointer to the area that the
 * value type wrote the value to: an object of the type of its values (const
 * char *, int64_t, double, size_t, or what the conversion function of a
 * user-defined value type writes; see <argx/type.h>). It returns NULL if
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
 * - option takes a value, ARGX_OPTION_VALUE. Checked with assert.
 * - The value type of the option of argx_result_option_string() is of the
 *   kind ARGX_TYPE_STRING, that of _int() ARGX_TYPE_INT, that of
 *   _float() ARGX_TYPE_FLOAT and that of _choice() ARGX_TYPE_CHOICE.
 *   Checked with assert.
 */
ARGX_API bool argx_result_option_string(const argx_result_t *result, const argx_option_t *option,
                                        const char **out);
ARGX_API bool argx_result_option_int(const argx_result_t *result, const argx_option_t *option,
                                     int64_t *out);
ARGX_API bool argx_result_option_float(const argx_result_t *result, const argx_option_t *option,
                                       double *out);
ARGX_API bool argx_result_option_choice(const argx_result_t *result, const argx_option_t *option,
                                        size_t *out);
ARGX_API const void *argx_result_option_value(const argx_result_t *result,
                                              const argx_option_t *option);

/*
 * Read the values of a positional.
 *
 * argx_result_positional_count() returns how many values positional has:
 * 0 or 1 for a positional that takes one argument at most, and the number
 * of the arguments that were assigned to it for one that takes any number.
 * argx_result_positional_source() returns where the values came from:
 * ARGX_SOURCE_COMMAND_LINE, ARGX_SOURCE_DEFAULT for the default value,
 * or ARGX_SOURCE_NONE when positional has no value.
 *
 * The other functions read the value at index, counted from 0 in the order
 * of the command line, as the functions of an option with a value do:
 * argx_result_positional_string(), _int(), _float() and _choice() write
 * the value to *out and return true, and argx_result_positional_value()
 * returns a pointer to the area that holds the value, for a value type of
 * every kind. If there is no value at index, that is, if index is not less
 * than argx_result_positional_count(), they return false and leave *out
 * as it was, or return NULL. That is not a violation of a precondition.
 *
 * A positional of a command that the command line did not select has no
 * value: its count is 0 and its source is ARGX_SOURCE_NONE.
 *
 * A string and the area are valid until result is destroyed. Do not free
 * or modify them.
 *
 * Preconditions:
 *
 * - result, positional and out are not NULL. Checked with assert.
 * - positional belongs to the schema that result was made from. Checked
 *   with assert.
 * - The value type of the positional of argx_result_positional_string()
 *   is of the kind ARGX_TYPE_STRING, that of _int() ARGX_TYPE_INT, that
 *   of _float() ARGX_TYPE_FLOAT and that of _choice() ARGX_TYPE_CHOICE.
 *   Checked with assert.
 */
ARGX_API size_t argx_result_positional_count(const argx_result_t *result,
                                             const argx_positional_t *positional);
ARGX_API argx_source_t argx_result_positional_source(const argx_result_t *result,
                                                     const argx_positional_t *positional);
ARGX_API bool argx_result_positional_string(const argx_result_t *result,
                                            const argx_positional_t *positional, size_t index,
                                            const char **out);
ARGX_API bool argx_result_positional_int(const argx_result_t *result,
                                         const argx_positional_t *positional, size_t index,
                                         int64_t *out);
ARGX_API bool argx_result_positional_float(const argx_result_t *result,
                                           const argx_positional_t *positional, size_t index,
                                           double *out);
ARGX_API bool argx_result_positional_choice(const argx_result_t *result,
                                            const argx_positional_t *positional, size_t index,
                                            size_t *out);
ARGX_API const void *argx_result_positional_value(const argx_result_t *result,
                                                  const argx_positional_t *positional, size_t index);

/*
 * Read the parse error of a result: what is wrong with a command line that
 * did not match the schema, that is, when argx_result_error() returns
 * ARGX_ERROR_INVALID_COMMAND_LINE. A result holds one parse error at
 * most, the first that was found.
 *
 * argx_result_error_kind() returns the kind of the error.
 * argx_result_error_message() returns one line of English that can be
 * shown to the user of the program as it is: it has no line break, no
 * prefix such as the name of the program, and no full stop.
 * argx_result_error_argument() returns the argument that is wrong, as a
 * whole, such as "--level=x" or the bundle "-vx".
 * argx_result_error_value() returns the value that is wrong: the part of
 * the argument that is the value, or the default value of the schema.
 * argx_result_error_source() and argx_result_error_index() return where
 * the error is: for ARGX_SOURCE_COMMAND_LINE, the index is that of the
 * argument in the argv that was parsed (argv[0] has the index 0); for
 * ARGX_SOURCE_DEFAULT, the error is in a default value of the schema and
 * the index is 0.
 * argx_result_error_command() returns the command that was being read
 * when the error was found or, for an error of a required or a default
 * value, the command of the option or the positional.
 * argx_result_error_option() and argx_result_error_positional() return
 * the option or the positional that the error is about.
 * argx_result_error_short_name() returns the letter that the error is
 * about when it is one letter of a bundle of short options, such as 'x' of
 * "-vx".
 *
 * An error does not have all of these. What it does not have reads as
 * "none": NULL for a string, an option, a positional and a command,
 * ARGX_SOURCE_NONE with the index 0 for an error that has no position
 * (something required that was not given), and '\0' for the letter. Which
 * ones an error has depends on its kind:
 *
 *     kind                    argument  value  option or positional
 *     UNKNOWN_OPTION          yes       no     no
 *     UNKNOWN_SUBCOMMAND      yes       yes    no
 *     MISSING_VALUE           yes       no     the option
 *     UNEXPECTED_VALUE        yes       yes    the option
 *     INVALID_VALUE           yes (*)   yes    the one that takes the value
 *     MISSING_OPTION          no        no     the option
 *     MISSING_POSITIONAL      no        no     the positional
 *     UNEXPECTED_POSITIONAL   yes       yes    no
 *     INVALID_ENCODING        yes       yes    no
 *
 *     (*) Not when the value is a default value of the schema.
 *
 * A result that has no parse error returns "none" from all of them:
 * ARGX_PARSE_ERROR_NONE, NULL, ARGX_SOURCE_NONE, 0 and '\0'.
 *
 * The strings are valid until result is destroyed. Do not free or modify
 * them. The command, the option and the positional belong to the schema.
 *
 * Preconditions:
 *
 * - result is not NULL. Checked with assert.
 */
ARGX_API argx_parse_error_kind_t argx_result_error_kind(const argx_result_t *result);
ARGX_API const char *argx_result_error_message(const argx_result_t *result);
ARGX_API const char *argx_result_error_argument(const argx_result_t *result);
ARGX_API const char *argx_result_error_value(const argx_result_t *result);
ARGX_API argx_source_t argx_result_error_source(const argx_result_t *result);
ARGX_API size_t argx_result_error_index(const argx_result_t *result);
ARGX_API const argx_command_t *argx_result_error_command(const argx_result_t *result);
ARGX_API const argx_option_t *argx_result_error_option(const argx_result_t *result);
ARGX_API const argx_positional_t *argx_result_error_positional(const argx_result_t *result);
ARGX_API char argx_result_error_short_name(const argx_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* ARGX_PARSE_H */
