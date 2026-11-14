/*
 * SPDX-FileCopyrightText: 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - internal declarations for parsing: the contents of a result and
 * the functions that build one.
 *
 * A result (argx_result_t, <argx/parse.h>) holds a copy of the
 * arguments, the commands that the command line selected, one frame of
 * slots for each of those commands, and the parse error, if there is one.
 * It borrows the schema and reads it through <argx/schema.h> only.
 *
 * Only result.c reads and writes the fields of the structures below.
 * parse.c and resolve.c go through the functions declared here and the
 * public functions that read a result. This header is included by result.c,
 * resolve.c, parse.c and the test of the internal functions only.
 *
 * A result can be destroyed with argx_result_destroy() at any point of
 * its construction. That is the one way in which a parse that fails for
 * lack of memory cleans up.
 *
 * Nothing here is synchronized: a result is built by one thread.
 */
#ifndef ARGX_PARSE_INTERNAL_H
#define ARGX_PARSE_INTERNAL_H

#include <argx/error.h>
#include <argx/parse.h>
#include <argx/schema.h>

#include "vec.h"

#include <stdbool.h>
#include <stddef.h>

/* ---- The contents of a result ------------------------------------------- */

/* The slot of one option. */
typedef struct argx_option_slot
{
    argx_source_t source;
    bool on;      /* A flag: whether it was given. A negatable flag: its state. */
    size_t count; /* A count: how many times it was given. */
    void *value;  /* An option that takes a value: the memory of the value; NULL when there is none. */
} argx_option_slot_t;

/* The slot of one positional. */
typedef struct argx_positional_slot
{
    argx_source_t source;
    argx_vec_t values; /* Items of the type void *: the memory of each value. */
} argx_positional_slot_t;

/*
 * One selected command and the slots of its options and positionals, in
 * the order of the command.
 */
typedef struct argx_result_frame
{
    const argx_command_t *command;
    argx_option_slot_t *options;         /* NULL when option_count is 0. */
    size_t option_count;                 /* argx_command_option_count(command) */
    argx_positional_slot_t *positionals; /* NULL when positional_count is 0. */
    size_t positional_count;             /* argx_command_positional_count(command) */
} argx_result_frame_t;

struct argx_result
{
    const argx_command_t *root;
    /*
     * The copy of the arguments: the array and the strings in one allocation.
     * NULL when arg_count is 0.
     */
    char **args;
    size_t arg_count;
    /*
     * The index of the first argument with a sequence that could not be
     * converted; 0 when there is none.
     */
    size_t first_invalid_arg;
    argx_vec_t frames;      /* Items of the type argx_result_frame_t; the first is that of root. */
    argx_error_t error;     /* ARGX_ERROR_NONE or ARGX_ERROR_INVALID_COMMAND_LINE */
    argx_parse_error_kind_t error_kind;
    argx_source_t error_source;
    size_t error_index;
    const char *error_argument; /* An item of args, or NULL. */
    const char *error_value;    /* In args, a default value of the schema, or NULL. */
    const argx_command_t *error_command;
    const argx_option_t *error_option;
    const argx_positional_t *error_positional;
    char error_short_name;
    char *error_message;        /* Owned. NULL when there is none. */
};

/* ---- Creation ----------------------------------------------------------- */

/*
 * Creates a result for the schema of root and writes it to *out. The new
 * result has no parse error, and its selected commands are root alone (as
 * after argx_result_push_command() of root).
 *
 * The argc arguments of argv are copied: the array and all the strings in
 * one allocation. argx_result_create() copies the bytes as they are;
 * argx_result_create_wide() converts each argument to UTF-8 ("utf8.h")
 * and remembers the first argument after argv[0] that has a sequence that
 * cannot be converted. If argc is 0, no copy is allocated. argv and its
 * strings are only read during the call. argv[argc] is not read.
 *
 * The caller owns the result and destroys it with argx_result_destroy().
 * root and its value types are borrowed until then.
 *
 * Returns ARGX_ERROR_NONE on success. Returns ARGX_ERROR_OUT_OF_MEMORY
 * when an allocation fails or the size of the copy does not fit in size_t;
 * *out is NULL then and nothing is left allocated.
 *
 * Preconditions, checked with assert:
 *
 * - root and out are not NULL.
 * - root is a root command: argx_command_parent(root) is NULL.
 * - argc is not negative.
 * - If argc is greater than 0, argv is not NULL and none of argv[0] to
 *   argv[argc - 1] is NULL.
 */
argx_error_t argx_result_create(const argx_command_t *root, int argc, char *const *argv,
                                argx_result_t **out);
argx_error_t argx_result_create_wide(const argx_command_t *root, int argc, wchar_t *const *argv,
                                     argx_result_t **out);

/*
 * Returns the copy of the argument at index, the index it had in argv. The
 * string is valid until result is destroyed.
 *
 * Preconditions, checked with assert: result is not NULL; index is less
 * than the argc that result was created with.
 */
const char *argx_result_arg(const argx_result_t *result, size_t index);

/*
 * Returns the index of the first argument that has a sequence that could
 * not be converted to UTF-8, which is 1 or more, or 0 when there is none.
 * It is always 0 for a result of argx_result_create().
 *
 * Preconditions, checked with assert: result is not NULL.
 */
size_t argx_result_first_invalid_arg(const argx_result_t *result);

/* ---- Recording ---------------------------------------------------------- */

/*
 * Adds command to the end of the selected commands of result, with one
 * slot for each of its options and positionals. The slots of a command
 * that has no option, or no positional, take no allocation.
 *
 * The slots start as "not given":
 *
 *     a flag                  false, ARGX_SOURCE_DEFAULT
 *     a count                 0, ARGX_SOURCE_DEFAULT
 *     a negatable flag        its initial state, ARGX_SOURCE_DEFAULT
 *     an option with a value  no value, ARGX_SOURCE_NONE
 *     a positional            no value, ARGX_SOURCE_NONE
 *
 * Returns ARGX_ERROR_NONE on success. Returns ARGX_ERROR_OUT_OF_MEMORY
 * when an allocation fails; result is then as it was before the call.
 *
 * Preconditions, checked with assert:
 *
 * - result and command are not NULL.
 * - command is a subcommand of the last selected command of result, or
 *   the root that result was created with when result has no selected
 *   command yet.
 */
argx_error_t argx_result_push_command(argx_result_t *result, const argx_command_t *command);

/*
 * Record that an option was given on the command line; its source becomes
 * ARGX_SOURCE_COMMAND_LINE.
 *
 * argx_result_record_flag() makes a flag "given".
 * argx_result_record_state() replaces the state of a negatable flag.
 * argx_result_record_count() adds 1 to a count.
 *
 * Preconditions, checked with assert:
 *
 * - result and option are not NULL.
 * - option belongs to a selected command of result.
 * - The option of argx_result_record_flag() is a flag, that of
 *   argx_result_record_state() a negatable flag and that of
 *   argx_result_record_count() a count.
 */
void argx_result_record_flag(argx_result_t *result, const argx_option_t *option);
void argx_result_record_state(argx_result_t *result, const argx_option_t *option, bool on);
void argx_result_record_count(argx_result_t *result, const argx_option_t *option);

/*
 * Where a parse error is: for a record of a value, where the value is,
 * for the parse error of a value that its value type does not take. The
 * strings are only read during the call.
 */
typedef struct argx_result_where
{
    /*
     * ARGX_SOURCE_COMMAND_LINE or ARGX_SOURCE_DEFAULT for a value;
     * ARGX_SOURCE_NONE for an error that has no position.
     */
    argx_source_t source;
    /*
     * The index of the argument that is wrong, or that holds the value; 0 when
     * source is not ARGX_SOURCE_COMMAND_LINE.
     */
    size_t index;
    char short_name;         /* The letter that the error is about in a bundle; otherwise '\0'. */
    /*
     * For the message: the prefix of the name as it was written, "--", "-" or
     * "". NULL is read as "".
     */
    const char *name_prefix;
    /*
     * For the message: the name as it was written. May be NULL for a kind
     * whose message has no name ("message.h").
     */
    const char *name;
    size_t name_length;      /* The length of name, which need not end at a terminator. */
} argx_result_where_t;

/*
 * Convert text with the value type of option, or of positional, and keep
 * the value in result, with the source where->source. text is passed to
 * the value type as it is; it points into the arguments of result or to a
 * default value of the schema.
 *
 * A second value of an option replaces the first, which is released. A
 * value of a positional is added after the values it has.
 *
 * What argx_type_convert() returns decides the outcome:
 *
 * - ARGX_ERROR_NONE: the value is kept. Returns ARGX_ERROR_NONE.
 * - ARGX_ERROR_OUT_OF_MEMORY: returns ARGX_ERROR_OUT_OF_MEMORY.
 * - Anything else: records the parse error
 *   ARGX_PARSE_ERROR_INVALID_VALUE at where, as argx_result_fail()
 *   does, and returns what argx_result_fail() returns. The value of the
 *   error is text, its option or positional is the one given, its command
 *   is the command of that, and the type of the message is the name of
 *   the value type.
 *
 * Also returns ARGX_ERROR_OUT_OF_MEMORY when an allocation of result
 * fails. After a failure, the values that result held are still there.
 * After ARGX_ERROR_OUT_OF_MEMORY, no parse error is recorded either.
 *
 * Preconditions, checked with assert:
 *
 * - result, option or positional, text and where are not NULL.
 * - option takes a value.
 * - option or positional belongs to a selected command of result.
 * - What argx_result_fail() requires of where, when the conversion
 *   fails.
 */
argx_error_t argx_result_record_option_value(argx_result_t *result, const argx_option_t *option,
                                             const char *text, const argx_result_where_t *where);
argx_error_t argx_result_record_positional_value(argx_result_t *result,
                                                 const argx_positional_t *positional,
                                                 const char *text, const argx_result_where_t *where);

/*
 * Takes back every default value that was applied: releases each value of
 * an option and of a positional whose source is ARGX_SOURCE_DEFAULT and
 * puts the slot back to "no value", ARGX_SOURCE_NONE. Flags, counts and
 * negatable flags are not touched.
 *
 * Preconditions, checked with assert: result is not NULL.
 */
void argx_result_discard_defaults(argx_result_t *result);

/*
 * A parse error to record. The strings are only read during the call,
 * except value, which the result keeps a pointer to.
 */
typedef struct argx_result_failure
{
    argx_parse_error_kind_t kind;
    argx_result_where_t where;
    /*
     * In the arguments of the result, a default value of the
     * schema, or NULL.
     */
    const char *value;
    const argx_command_t *command;
    const argx_option_t *option;         /* May be NULL. */
    const argx_positional_t *positional; /* May be NULL. */
} argx_result_failure_t;

/*
 * Records the parse error failure in result and builds its message
 * ("message.h"). The result then reports
 * ARGX_ERROR_INVALID_COMMAND_LINE, and the functions of <argx/parse.h>
 * that read the parse error return the fields of failure. The argument of
 * the error is the argument of result at failure->where.index when
 * failure->where.source is ARGX_SOURCE_COMMAND_LINE, and NULL otherwise.
 *
 * The message is built from the name of failure->where and from
 * failure->value. For ARGX_PARSE_ERROR_INVALID_VALUE, its type is the
 * name of the value type of failure->positional or, when that is NULL, of
 * failure->option.
 *
 * Returns ARGX_ERROR_INVALID_COMMAND_LINE when the error was recorded.
 * Returns ARGX_ERROR_OUT_OF_MEMORY when the allocation of the message
 * fails; nothing is recorded then: result is as it was before the call and
 * still reports ARGX_ERROR_NONE.
 *
 * Preconditions, checked with assert unless stated otherwise:
 *
 * - result and failure are not NULL.
 * - failure->kind is not ARGX_PARSE_ERROR_NONE.
 * - failure->command is not NULL.
 * - result has no parse error: an error is recorded once for a result at
 *   most.
 * - If failure->where.source is ARGX_SOURCE_COMMAND_LINE,
 *   failure->where.index is less than the argc that result was created
 *   with.
 * - For ARGX_PARSE_ERROR_INVALID_VALUE, failure->option or
 *   failure->positional is not NULL.
 * - The strings that the wording of the kind uses are not NULL; see
 *   argx_message_build().
 */
argx_error_t argx_result_fail(argx_result_t *result, const argx_result_failure_t *failure);

/* ---- Resolving ---------------------------------------------------------- */

/*
 * Resolves the values of a result whose arguments were all read without a
 * parse error: checks that what is required was given, and then applies
 * the default values. Only the selected commands are looked at.
 *
 * Returns ARGX_ERROR_NONE on success, ARGX_ERROR_INVALID_COMMAND_LINE
 * when a parse error was recorded (no default value is left applied then),
 * and ARGX_ERROR_OUT_OF_MEMORY when an allocation fails.
 *
 * Preconditions: result is not NULL (checked with assert) and has no parse
 * error.
 */
argx_error_t argx_resolve(argx_result_t *result);

#endif /* ARGX_PARSE_INTERNAL_H */
