/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - internal declarations for parsing: the contents of a result and
 * the functions that build one.
 *
 * A result (argx_result_t, <argx/parse.h>) holds a copy of the
 * arguments, the commands that the command line selected, one frame of
 * slots for each of those commands, and the parse error, if any. It
 * borrows the schema and reads it only through <argx/schema.h>.
 *
 * Only result.c reads and writes the fields of the structures below.
 * parse.c and resolve.c go through the functions declared here and the
 * public functions that read a result. Only result.c, resolve.c, parse.c
 * and the test of the internal functions include this header.
 *
 * A result can be destroyed with argx_result_destroy() at any point
 * during its construction. That is the only way a parse that runs out of
 * memory cleans up.
 *
 * Nothing here is synchronized: one thread builds a result.
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

/* The slot for one option. */
typedef struct argx_option_slot
{
    argx_source_t source;
    bool on;      /* A flag: whether it was given. A negatable flag: its state. */
    size_t count; /* A count: how many times it was given. */
    void *value;  /* An option that takes a value: the value's memory, or NULL if none. */
} argx_option_slot_t;

/* The slot for one positional. */
typedef struct argx_positional_slot
{
    argx_source_t source;
    argx_vec_t values; /* Items of type void *: each value's memory. */
} argx_positional_slot_t;

/*
 * One selected command and the slots for its options and positionals, in
 * the command's order.
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
     * converted, or 0 if there is none.
     */
    size_t first_invalid_arg;
    argx_vec_t frames;      /* Items of type argx_result_frame_t; the first is root's frame. */
    argx_error_t error;     /* ARGX_ERROR_NONE or ARGX_ERROR_INVALID_COMMAND_LINE */
    argx_parse_error_kind_t error_kind;
    argx_source_t error_source;
    size_t error_index;
    const char *error_argument; /* An item of args, or NULL. */
    const char *error_value;    /* Points into args or to a schema default value, or NULL. */
    const argx_command_t *error_command;
    const argx_option_t *error_option;
    const argx_positional_t *error_positional;
    char error_short_name;
    char *error_message;        /* Owned. NULL when there is none. */
};

/* ---- Creation ----------------------------------------------------------- */

/*
 * Creates a result for root's schema and writes it to *out. The new result
 * has no parse error, and root is its only selected command (as after
 * argx_result_push_command() of root).
 *
 * The argc arguments in argv are copied, the array and all the strings in
 * one allocation. argx_result_create() copies the bytes unchanged;
 * argx_result_create_wide() converts each argument to UTF-8 ("utf8.h")
 * and remembers the first argument after argv[0] that contains a sequence
 * that cannot be converted. If argc is 0, no copy is allocated. argv and
 * its strings are only read during the call. argv[argc] is not read.
 *
 * The caller owns the result and destroys it with argx_result_destroy().
 * root and its value types are borrowed until then.
 *
 * Returns ARGX_ERROR_NONE on success. Returns ARGX_ERROR_OUT_OF_MEMORY
 * when an allocation fails or the size of the copy does not fit in size_t;
 * *out is then NULL and nothing is left allocated.
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
 * Returns the copy of the argument at index, which is its index in argv.
 * The string remains valid until result is destroyed.
 *
 * Preconditions, checked with assert: result is not NULL; index is less
 * than the argc that result was created with.
 */
const char *argx_result_arg(const argx_result_t *result, size_t index);

/*
 * Returns the index (1 or more) of the first argument that contains a
 * sequence that could not be converted to UTF-8, or 0 when there is none.
 * It is always 0 for a result from argx_result_create().
 *
 * Preconditions, checked with assert: result is not NULL.
 */
size_t argx_result_first_invalid_arg(const argx_result_t *result);

/* ---- Recording ---------------------------------------------------------- */

/*
 * Appends command to result's selected commands, with one slot for each
 * of its options and positionals. A command without options, or without
 * positionals, needs no allocation for those slots.
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
 * - command is a subcommand of result's last selected command or, when
 *   result has no selected command yet, the root that result was created
 *   with.
 */
argx_error_t argx_result_push_command(argx_result_t *result, const argx_command_t *command);

/*
 * Records that an option was given on the command line; its source
 * becomes ARGX_SOURCE_COMMAND_LINE.
 *
 * argx_result_record_flag() marks a flag as "given".
 * argx_result_record_state() replaces a negatable flag's state.
 * argx_result_record_count() adds 1 to a count.
 *
 * Preconditions, checked with assert:
 *
 * - result and option are not NULL.
 * - option belongs to a selected command of result.
 * - option is a flag for argx_result_record_flag(), a negatable flag for
 *   argx_result_record_state() and a count for
 *   argx_result_record_count().
 */
void argx_result_record_flag(argx_result_t *result, const argx_option_t *option);
void argx_result_record_state(argx_result_t *result, const argx_option_t *option, bool on);
void argx_result_record_count(argx_result_t *result, const argx_option_t *option);

/*
 * The location of a parse error. When a value is recorded, it is where
 * the value is, for the parse error recorded if the value type does not
 * take the value. The strings are only read during the call.
 */
typedef struct argx_result_where
{
    /*
     * ARGX_SOURCE_COMMAND_LINE or ARGX_SOURCE_DEFAULT for a value;
     * ARGX_SOURCE_NONE for an error that has no position.
     */
    argx_source_t source;
    /*
     * The index of the argument that is wrong or that holds the value; 0
     * when source is not ARGX_SOURCE_COMMAND_LINE.
     */
    size_t index;
    char short_name;         /* In a bundle, the letter the error is about; otherwise '\0'. */
    /*
     * For the message: the name's prefix as written ("--", "-" or "").
     * NULL is read as "".
     */
    const char *name_prefix;
    /*
     * For the message: the name as written. May be NULL for a kind whose
     * message has no name ("message.h").
     */
    const char *name;
    size_t name_length;      /* The length of name, which need not be null-terminated. */
} argx_result_where_t;

/*
 * Converts text with the value type of option or positional and keeps the
 * value in result, with the source where->source. text is passed to the
 * value type unchanged; it points into result's arguments or to a default
 * value in the schema.
 *
 * A second value for an option replaces the first, which is released. A
 * value for a positional is appended after its existing values.
 *
 * The outcome depends on what argx_type_convert() returns:
 *
 * - ARGX_ERROR_NONE: the value is kept. Returns ARGX_ERROR_NONE.
 * - ARGX_ERROR_OUT_OF_MEMORY: returns ARGX_ERROR_OUT_OF_MEMORY.
 * - Anything else: records the parse error
 *   ARGX_PARSE_ERROR_INVALID_VALUE at where, as argx_result_fail()
 *   does, and returns what argx_result_fail() returns. The error's value
 *   is text, its option or positional is the one given, its command is
 *   the command that option or positional belongs to, and the message's
 *   type is the value type's name.
 *
 * Also returns ARGX_ERROR_OUT_OF_MEMORY when an allocation for result
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
 * Takes back every default value that was applied: releases each value
 * of an option or positional whose source is ARGX_SOURCE_DEFAULT and
 * resets its slot to "no value", ARGX_SOURCE_NONE. Flags, counts and
 * negatable flags are left untouched.
 *
 * Preconditions, checked with assert: result is not NULL.
 */
void argx_result_discard_defaults(argx_result_t *result);

/*
 * A parse error to record. The strings are only read during the call,
 * except value, to which the result keeps a pointer.
 */
typedef struct argx_result_failure
{
    argx_parse_error_kind_t kind;
    argx_result_where_t where;
    /*
     * Points into the result's arguments or to a schema default
     * value, or NULL.
     */
    const char *value;
    const argx_command_t *command;
    const argx_option_t *option;         /* May be NULL. */
    const argx_positional_t *positional; /* May be NULL. */
} argx_result_failure_t;

/*
 * Records the parse error failure in result and builds its message
 * ("message.h"). The result then reports ARGX_ERROR_INVALID_COMMAND_LINE,
 * and the <argx/parse.h> functions that read the parse error return the
 * fields of failure. The error's argument is result's argument at
 * failure->where.index when failure->where.source is
 * ARGX_SOURCE_COMMAND_LINE, and NULL otherwise.
 *
 * The message is built from the name in failure->where and from
 * failure->value. For ARGX_PARSE_ERROR_INVALID_VALUE, the message's type
 * is the name of the value type of failure->positional or, when that is
 * NULL, of failure->option.
 *
 * Returns ARGX_ERROR_INVALID_COMMAND_LINE when the error was recorded.
 * Returns ARGX_ERROR_OUT_OF_MEMORY when allocating the message fails. In
 * that case nothing is recorded: result is as it was before the call and
 * still reports ARGX_ERROR_NONE.
 *
 * Preconditions, checked with assert unless stated otherwise:
 *
 * - result and failure are not NULL.
 * - failure->kind is not ARGX_PARSE_ERROR_NONE.
 * - failure->command is not NULL.
 * - result has no parse error yet: a result records at most one error.
 * - If failure->where.source is ARGX_SOURCE_COMMAND_LINE,
 *   failure->where.index is less than the argc that result was created
 *   with.
 * - For ARGX_PARSE_ERROR_INVALID_VALUE, failure->option or
 *   failure->positional is not NULL.
 * - The strings that the kind's wording uses are not NULL; see
 *   argx_message_build().
 */
argx_error_t argx_result_fail(argx_result_t *result, const argx_result_failure_t *failure);

/* ---- Resolving ---------------------------------------------------------- */

/*
 * Resolves the values of a result whose arguments were all read without a
 * parse error: checks that everything required was given, then applies
 * the default values. Only the selected commands are examined.
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
