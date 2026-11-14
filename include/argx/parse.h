/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - parsing: what a command line is read into.
 *
 * This header declares the functions that parse a command line,
 * argx_parse() and argx_parse_wide() for wide strings; the parse result,
 * argx_result_t, with the functions that read and destroy it; and the
 * enumerations that say where a value came from and what kind of error a
 * command line has.
 *
 * Ownership:
 *
 * - A result owns a copy of the arguments it was made from and everything
 *   that was read from them. argx_result_destroy() frees all of it.
 * - A result borrows the schema and the value types the schema uses: do
 *   not change or destroy them before the result is destroyed. The result
 *   never changes or destroys them.
 * - What the functions that read a result return stays valid until the
 *   result is destroyed. The caller does not free it.
 *
 * Errors:
 *
 * - Both parse functions return an argx_error_t. A command line that does
 *   not match the schema gives ARGX_ERROR_INVALID_COMMAND_LINE. Unlike
 *   every other failure, the output is then a result, not NULL, and it
 *   holds the parse error.
 * - A function that only reads takes a const result, changes nothing,
 *   allocates nothing and cannot fail. Several threads may call such
 *   functions on the same result at the same time. Anything a result does
 *   not have reads as "none": false, 0, NULL or ARGX_SOURCE_NONE.
 * - A NULL pointer where none is allowed, a schema that is not validated,
 *   an option or positional from another schema or of the wrong kind, and
 *   a command index that is out of range are programming errors, not
 *   error codes: see each function's preconditions.
 *
 * Preconditions of every function that takes a result, in addition to
 * those each function lists:
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
 * A parse result: what a command line was read into. Its contents are not
 * public.
 */
typedef struct argx_result argx_result_t;

/*
 * Where a value came from, and where a parse error is.
 *
 * Later versions may add sources, such as an environment variable. Write
 * code that can handle a value it does not know: give every switch over
 * argx_source_t a default label.
 */
typedef enum argx_source
{
    ARGX_SOURCE_NONE,        /* There is no value; the error has no position. */
    ARGX_SOURCE_DEFAULT,     /* The schema's default value or initial state. */
    ARGX_SOURCE_COMMAND_LINE /* The command line (argv). */
} argx_source_t;

/*
 * The parse error kind: what is wrong with a command line that does not
 * match the schema.
 *
 * Later versions may add kinds. Write code that can handle a value it does
 * not know: give every switch over argx_parse_error_kind_t a default label.
 */
typedef enum argx_parse_error_kind
{
    ARGX_PARSE_ERROR_NONE,                  /* There is no parse error. */
    ARGX_PARSE_ERROR_UNKNOWN_OPTION,        /* An option that the command does not have. */
    ARGX_PARSE_ERROR_UNKNOWN_SUBCOMMAND,    /* A subcommand that the command does not have. */
    ARGX_PARSE_ERROR_MISSING_VALUE,         /* An option that takes a value has none. */
    ARGX_PARSE_ERROR_UNEXPECTED_VALUE,      /* A value for an option that takes none. */
    ARGX_PARSE_ERROR_INVALID_VALUE,         /* A value that its value type rejects. */
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
 * Parses the command line argv, which has argc arguments, with root's
 * schema, and writes a new result to *out. argc and argv can be passed as
 * main() received them.
 *
 * argv[0] is the program name and is not parsed. When there are no other
 * arguments, including when argc is 0, nothing was given on the command
 * line.
 * The other arguments are read once each, from first to last, and matched
 * against the current command: root at first, then the subcommand named
 * last (see below). The scan stops at the first argument that does not
 * match the schema.
 *
 * An argument that starts with "--" and has more characters is a long
 * option. Its name ends before the first '='. The name is compared as a
 * whole, byte by byte, with the long names and negated names of the
 * command's options: the comparison is case-sensitive, and part of a name
 * does not match.
 *
 *     --name         a flag is given; a count goes up by one; a negatable
 *                    flag is on
 *     --no-name      a negatable flag is off
 *     --name=value   an option that takes a value gets the value, which is
 *                    everything after the first '=' and may be empty
 *     --name value   an option that takes a value gets the next argument
 *                    as its value, whatever that argument looks like
 *
 * An argument that starts with a single '-' and has more characters is a
 * bundle of short options. Each character after the '-' is compared, from
 * first to last, with the short names of the command's options; the
 * comparison is case-sensitive. An option that takes a value ends the
 * bundle.
 *
 *     -v             a flag is given; a count goes up by one; a negatable
 *                    flag is on
 *     -abc           the same as -a -b -c; -vvv gives a count three times
 *     -ovalue        an option that takes a value gets the rest of the
 *                    argument as its value, whatever the rest looks like:
 *                    the value of -o=value is "=value"
 *     -o value       an option that takes a value and is the last
 *                    character gets the next argument as its value,
 *                    whatever that argument looks like
 *     -abovalue      the same as -a -b -ovalue; -abo value is the same as
 *                    -a -b -o value
 *
 * An argument in the form of a negative number, that is, a '-' and a
 * digit, or a '-', a '.' and a digit, such as "-1" or "-.5", is a bundle
 * only when the command has an option whose short name is a digit.
 * Otherwise it is not an option. An argument that is taken as an option's
 * value is never checked this way.
 *
 * All other arguments are not options: one that does not start with '-',
 * the empty string, and a lone '-'.
 *
 * For a command that has subcommands, an argument that is not an option is
 * a subcommand name. It is compared as a whole, byte by byte, with the
 * names of the current command's subcommands: the comparison is
 * case-sensitive, and part of a name does not match. The subcommand with
 * that name is selected and becomes the current command. The arguments
 * after its name are read against its options, its positionals and its
 * subcommands, to any depth. A name that is not a subcommand of the
 * current command is a parse error.
 *
 *     tool remote add --force name
 *                    selects "remote" under tool, then "add" under
 *                    "remote"; --force is an option of "add", and name is
 *                    a value of its first positional
 *
 * Options are looked up in the current command only, not in its parent
 * and not in its subcommands: a command's options are written after that
 * command's name and before its subcommand's name. An option that only the
 * parent has is an unknown option after the subcommand name. When a
 * command and its subcommand both have an option with the same name, each
 * is given where its own command is the current command. The same goes for
 * negative numbers: whether an argument in the form of a negative number
 * is a bundle depends on the current command's short names.
 *
 * A command line that names no subcommand of a command that has
 * subcommands is not a parse error: the command line ends in that command,
 * and argx_result_command() returns it. The options and positionals of the
 * commands that were not selected read as not given.
 *
 * For a command that has no subcommands, an argument that is not an option
 * is a value of one of the command's positionals. The arguments are
 * assigned, in order, to the positionals in the order they were added:
 *
 *     ARGX_ARITY_REQUIRED, ARGX_ARITY_OPTIONAL
 *                    the positional gets the argument as its value, and
 *                    the next argument goes to the next positional
 *     ARGX_ARITY_ZERO_OR_MORE, ARGX_ARITY_ONE_OR_MORE
 *                    the argument, and every argument after it, is added
 *                    to the positional as one more value
 *
 * An argument that comes after every positional has its value, or any
 * such argument for a command with no positionals, is one argument too
 * many: a parse error. Each value is converted with its positional's value
 * type when it is read.
 *
 * Options and arguments that are not options can be mixed: an option may
 * come before, between or after the positional values, and the assignment
 * stays the same.
 *
 * The argument "--" alone is the option terminator. It is not a value
 * itself. Every argument after the first terminator is not an option,
 * whatever it looks like: "-v", "--name" and a second "--" are then
 * values. Such an argument is never a subcommand name. A command that has
 * subcommands has no positionals, so for such a command the argument is
 * one argument too many, a parse error. The terminator stays in effect
 * after a subcommand is selected.
 *
 * An option may be given more than once, by its short name and its long
 * name alike: a flag stays given, a count is the number of times it was
 * given, a negatable flag has the state given last, and an option that
 * takes a value has the value given last. Each value is converted with its
 * option's value type when it is read, including a value that is replaced
 * later.
 *
 * When the last argument has been read without a parse error, the values
 * are resolved for the selected commands only: the root and every
 * subcommand that was named. A command that was not selected is neither
 * checked nor given default values.
 *
 * First, everything required must have been given: an option that takes a
 * value and is required (argx_option_set_required()) must have a value,
 * and a positional with arity ARGX_ARITY_REQUIRED or
 * ARGX_ARITY_ONE_OR_MORE must have at least one value. A missing one is a
 * parse error. If several are missing, the first is reported: commands
 * from the root down, a command's options before its positionals, and
 * each in the order they were added.
 *
 * Then, if nothing is missing, everything that was not given gets its
 * default value:
 *
 *     an option that takes a value, a positional with arity
 *     ARGX_ARITY_OPTIONAL
 *                    with a default value: the default value, converted
 *                    with its value type; its source is
 *                    ARGX_SOURCE_DEFAULT
 *                    without one: no value; its source is
 *                    ARGX_SOURCE_NONE
 *     a positional with arity ARGX_ARITY_ZERO_OR_MORE
 *                    no value: its count is 0
 *     a flag, a count, a negatable flag
 *                    not given, 0, its initial state; the source is
 *                    ARGX_SOURCE_DEFAULT
 *
 * A default value that its value type does not accept is a parse error.
 * Schema validation already checked the default values, so this happens
 * only with a user-defined value type that does not give the same answer
 * every time.
 *
 * The command line and the default values are the only sources of values:
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
 * Read a parse error with argx_result_error_kind() and the functions
 * declared next to it.
 *
 * When a command line has a parse error, of any kind, its result holds
 * what was read before the error: the selected commands, the options that
 * were given and the values that were converted, all with the source
 * ARGX_SOURCE_COMMAND_LINE. It holds no default values: an option that
 * takes a value and a positional that were not given have no value, even
 * if they have a default value. A flag, a count and a negatable flag that
 * were not given read as not given, 0 and the initial state.
 *
 * Ownership:
 *
 * - The caller owns the result and destroys it with
 *   argx_result_destroy(). This includes the result of a parse that
 *   returned ARGX_ERROR_INVALID_COMMAND_LINE.
 * - argv and its strings are only read, and the result keeps its own copy
 *   of them: after the call, argv may be changed or freed.
 * - The schema and its value types are only read, and the result borrows
 *   them: destroy the result before the schema, and do not add to the
 *   schema or set anything in it while a result made from it exists.
 *
 * Nothing is kept between calls: the same schema and the same argv always
 * give the same result.
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
 * Parses a command line of wide strings, such as wmain()'s argv, with
 * root's schema: the same as argx_parse(), after each argument has been
 * converted to UTF-8. The function is available on every platform.
 *
 * How a wide string is read depends on the width of wchar_t:
 *
 * - 16 bits (Windows): as UTF-16. A high surrogate followed by a low
 *   surrogate is one character.
 * - wider (Linux, macOS): as UTF-32.
 *
 * The conversion does not depend on the locale. It gives what argx_parse()
 * gives for the same command line written in UTF-8: the same result, or
 * the same parse error. Every string read from the result is UTF-8: the
 * values, and a parse error's argument, value and message. argx_parse()
 * itself does not inspect the bytes of its arguments: bytes that are not
 * UTF-8 are passed on unchanged.
 *
 * An argument that cannot be converted is the parse error
 * ARGX_PARSE_ERROR_INVALID_ENCODING. In UTF-16, that is an argument with a
 * surrogate that is not part of a pair. In UTF-32, it is an argument with
 * a value that is negative, in the surrogate range (U+D800 to U+DFFF) or
 * above U+10FFFF. The error is found when the argument is about to be
 * read, even when the argument would be the value of the option before
 * it, so an error in an earlier argument is reported instead. The error's
 * argument and value are the argument in UTF-8, with U+FFFD in place of
 * each unit that could not be converted. The error has no option, no
 * positional and no letter. argv[0] is not parsed, so it never causes this
 * error.
 *
 * The return value, what is written to *out, the ownership and the
 * preconditions are those of argx_parse(), with argv and its wide strings
 * in place of argv and its strings.
 */
ARGX_API argx_error_t argx_parse_wide(const argx_command_t *root, int argc, wchar_t *const *argv,
                                      argx_result_t **out);

/*
 * Destroys result: frees the copy of the arguments and everything else the
 * result holds. The schema and its value types are neither changed nor
 * destroyed. Every string and every handle to a value that was read from
 * result becomes invalid.
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
 * Read the commands that the command line selected, in order: the root
 * command at index 0, then the subcommand named on the command line, then
 * its subcommand, and so on. Each command except the first is a subcommand
 * of the one before it.
 *
 * argx_result_command_count() returns how many there are, at least 1.
 * argx_result_command_at() returns the command at index, counted from 0.
 * argx_result_command() returns the last one: the command that the
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
 * negatable flag is on: the state given last or, if it was not given, its
 * initial state.
 * argx_result_count() returns how many times a count was given, by its
 * short name and its long name together.
 *
 * An option of a command that the command line did not select reads as
 * not given: false, 0, and a negatable flag's initial state.
 *
 * Preconditions:
 *
 * - result and option are not NULL. Checked with assert.
 * - option belongs to the schema that result was made from. Checked with
 *   assert.
 * - The option passed to argx_result_flag() is a flag or a negatable
 *   flag, ARGX_OPTION_FLAG or ARGX_OPTION_NEGATABLE. Checked with assert.
 * - The option passed to argx_result_count() is a count,
 *   ARGX_OPTION_COUNT. Checked with assert.
 */
ARGX_API bool argx_result_flag(const argx_result_t *result, const argx_option_t *option);
ARGX_API size_t argx_result_count(const argx_result_t *result, const argx_option_t *option);

/*
 * Returns the source of what result holds for option. option may be of
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
 * argx_result_option_string(), _int(), _float() and _choice() read a value
 * whose value type is of the kind ARGX_TYPE_STRING, ARGX_TYPE_INT,
 * ARGX_TYPE_FLOAT and ARGX_TYPE_CHOICE respectively. If option has a
 * value, they write it to *out and return true. If it has none, they
 * return false and leave *out unchanged.
 *
 * argx_result_option_value() works for value types of every kind,
 * including user-defined ones. It returns a pointer to the area that the
 * value type wrote the value to: an object of the C type of its values
 * (const char *, int64_t, double, size_t, or whatever a user-defined value
 * type's conversion function writes; see <argx/type.h>). It returns NULL
 * if option has no value.
 *
 * The value is the one given last on the command line or, if option was
 * not given, its default value. option has no value when it was not given
 * and has no default value, when the command line has a parse error before
 * default values are applied, and when option belongs to a command that
 * the command line did not select.
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
 * - The value type of the option passed to argx_result_option_string() is
 *   of the kind ARGX_TYPE_STRING; for _int() it is ARGX_TYPE_INT, for
 *   _float() ARGX_TYPE_FLOAT and for _choice() ARGX_TYPE_CHOICE.
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
 * 0 or 1 for a positional that takes at most one argument, and the number
 * of arguments assigned to it for one that takes any number.
 * argx_result_positional_source() returns where the values came from:
 * ARGX_SOURCE_COMMAND_LINE, ARGX_SOURCE_DEFAULT for the default value,
 * or ARGX_SOURCE_NONE when positional has no value.
 *
 * The other functions read the value at index, counted from 0 in
 * command-line order, the same way the option functions read an option's
 * value: argx_result_positional_string(), _int(), _float() and _choice()
 * write the value to *out and return true, and
 * argx_result_positional_value() returns a pointer to the area that holds
 * the value, for value types of every kind. If there is no value at index,
 * that is, if index is not less than argx_result_positional_count(), they
 * return false and leave *out unchanged, or return NULL. That is not a
 * precondition violation.
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
 * - The value type of the positional passed to
 *   argx_result_positional_string() is of the kind ARGX_TYPE_STRING; for
 *   _int() it is ARGX_TYPE_INT, for _float() ARGX_TYPE_FLOAT and for
 *   _choice() ARGX_TYPE_CHOICE. Checked with assert.
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
 * Read a result's parse error: what is wrong with a command line that did
 * not match the schema, that is, when argx_result_error() returns
 * ARGX_ERROR_INVALID_COMMAND_LINE. A result holds at most one parse error,
 * the first one found.
 *
 * argx_result_error_kind() returns the error kind.
 * argx_result_error_message() returns one line of English that can be
 * shown to the program's user as is: it has no line break, no prefix such
 * as the program name, and no full stop.
 * argx_result_error_argument() returns the whole argument that is wrong,
 * such as "--level=x" or the bundle "-vx".
 * argx_result_error_value() returns the value that is wrong: the part of
 * the argument that is the value, or the schema's default value.
 * argx_result_error_source() and argx_result_error_index() return where
 * the error is. For ARGX_SOURCE_COMMAND_LINE, the index is the argument's
 * index in the argv that was parsed (argv[0] has index 0). For
 * ARGX_SOURCE_DEFAULT, the error is in a default value of the schema, and
 * the index is 0.
 * argx_result_error_command() returns the command being read when the
 * error was found or, for an error about a required or default value, the
 * command of the option or positional.
 * argx_result_error_option() and argx_result_error_positional() return
 * the option or positional that the error is about.
 * argx_result_error_short_name() returns the letter that the error is
 * about when it is one letter of a bundle of short options, such as 'x' in
 * "-vx".
 *
 * An error does not have all of these. Anything it does not have reads as
 * "none": NULL for a string, an option, a positional and a command;
 * ARGX_SOURCE_NONE with index 0 for an error that has no position
 * (something required that was not given); and '\0' for the letter. Which
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
 * A result without a parse error returns "none" from all of them:
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
