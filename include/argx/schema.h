/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - the schema: the commands, options and positionals that a program
 * takes on its command line.
 *
 * A schema is a tree of commands. The root command stands for the program;
 * each command can have subcommands, to any depth. A command also has
 * options and positionals. The schema only tells what the command line
 * looks like: it knows nothing about an argv.
 *
 * Ownership:
 *
 * - The root command owns the whole tree: every subcommand, option and
 *   positional, and every string in it. Calling argx_command_destroy() on
 *   the root frees all of it; nothing else in the tree is destroyed on its
 *   own.
 * - Every string passed in is copied. The caller may change or free its
 *   string after the call.
 * - Returned handles and strings are valid until the root is destroyed,
 *   with one exception. A string read from an attribute that can be set
 *   again (a command's description; an option's description, value name
 *   and default value; a positional's description and default value) is
 *   valid until the root is destroyed or that attribute is set again,
 *   whichever comes first. Setting an attribute again frees its old
 *   string. Names are never replaced. The caller does not free what is
 *   returned. A handle does not change when more is added to the tree.
 *
 * Errors:
 *
 * - A function that adds or sets something returns an argx_error_t. When
 *   it fails, the schema is exactly as it was before the call, and any
 *   output is NULL.
 * - A read-only function takes a const handle, changes nothing and cannot
 *   fail. While the schema is not being changed, such functions may be
 *   called from several threads at the same time. Changing one schema from
 *   several threads at the same time is not supported.
 * - A NULL pointer where none is allowed and an out-of-range index are
 *   programming errors, not error codes: see each function's
 *   preconditions.
 *
 * Names are compared byte by byte, so they are case-sensitive.
 */
#ifndef ARGX_SCHEMA_H
#define ARGX_SCHEMA_H

#include <argx/error.h>
#include <argx/export.h>
#include <argx/type.h>

#include <stdbool.h>
#include <stddef.h>

/*
 * A command: the root of a schema, or a subcommand. The contents are not
 * public.
 */
typedef struct argx_command argx_command_t;

/* An option of a command. The contents are not public. */
typedef struct argx_option argx_option_t;

/* A positional of a command. The contents are not public. */
typedef struct argx_positional argx_positional_t;

/*
 * The kind of an option.
 *
 * Later versions may add kinds. Write code that may receive a value it does
 * not know: give a switch over argx_option_kind_t a default label.
 */
typedef enum argx_option_kind
{
    ARGX_OPTION_FLAG,      /* Whether it was given. */
    ARGX_OPTION_NEGATABLE, /* Switched with --name and --no-name. */
    ARGX_OPTION_COUNT,     /* How many times it was given. */
    ARGX_OPTION_VALUE      /* Takes one value. */
} argx_option_kind_t;

/*
 * How many arguments a positional takes.
 *
 * Later versions may add kinds. Write code that may receive a value it does
 * not know: give a switch over argx_arity_t a default label.
 */
typedef enum argx_arity
{
    ARGX_ARITY_REQUIRED,     /* Exactly one. */
    ARGX_ARITY_OPTIONAL,     /* None or one. */
    ARGX_ARITY_ZERO_OR_MORE, /* None or more. */
    ARGX_ARITY_ONE_OR_MORE   /* One or more. */
} argx_arity_t;

#ifdef __cplusplus
extern "C"
{
#endif

/* ---- Commands ----------------------------------------------------------- */

/*
 * Creates a root command named name and writes it to *out. The new command
 * is empty: it has no description, options, positionals or subcommands.
 *
 * The name is copied. Any non-empty string is a valid root name; the
 * naming rule for subcommands does not apply to it.
 *
 * The caller owns the root and destroys it with argx_command_destroy().
 *
 * Returns ARGX_ERROR_NONE on success. On failure, nothing is created,
 * nothing stays allocated, and *out is NULL:
 *
 * - ARGX_ERROR_INVALID_NAME: name is the empty string.
 * - ARGX_ERROR_OUT_OF_MEMORY: an allocation failed.
 *
 * Preconditions:
 *
 * - name is not NULL. Checked with assert.
 * - out is not NULL. Checked with assert.
 */
ARGX_API argx_error_t argx_command_create(const char *name, argx_command_t **out);

/*
 * Destroys a root command and everything below it: its subcommands to any
 * depth, the options, the positionals and all the strings. Afterwards,
 * every handle and string read from the tree is invalid. Does nothing for
 * NULL.
 *
 * The value types the tree uses are not destroyed; they belong to the
 * caller.
 *
 * The recursion depth equals the tree depth.
 *
 * Preconditions:
 *
 * - root is NULL, or a root created by argx_command_create() that has not
 *   been destroyed. Never pass a subcommand: it is destroyed with its
 *   root. That root is not a subcommand is checked with assert.
 */
ARGX_API void argx_command_destroy(argx_command_t *root);

/*
 * Adds a subcommand named name to command, after its existing
 * subcommands, and writes it to *out if out is not NULL. The new
 * subcommand is empty, like a new root, and can have subcommands of its
 * own: there is no depth limit.
 *
 * The name is copied. It has at least one character; the first is an ASCII
 * letter or digit, and the rest are ASCII letters, digits, '-' or '_'.
 *
 * The subcommand belongs to the tree and is destroyed with the root.
 *
 * Returns ARGX_ERROR_NONE on success. On failure, the tree is as it was
 * before the call, nothing from the call stays allocated, and *out is
 * NULL. The conditions are checked in this order:
 *
 * - ARGX_ERROR_INVALID_NAME: name breaks the naming rule. The empty string
 *   breaks it.
 * - ARGX_ERROR_INVALID_DEFINITION: command has a positional. A command
 *   has positionals or subcommands, not both.
 * - ARGX_ERROR_NAME_CONFLICT: command already has a subcommand with that
 *   name. The same name can be used in another command.
 * - ARGX_ERROR_OUT_OF_MEMORY: an allocation failed.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 * - name is not NULL. Checked with assert.
 * - out may be NULL, when the handle is not needed.
 */
ARGX_API argx_error_t argx_command_add_subcommand(argx_command_t *command, const char *name,
                                                  argx_command_t **out);

/*
 * Sets command's description to a copy of text, replacing any earlier
 * description. The empty string can be set; it is not the same as no
 * description.
 *
 * Returns ARGX_ERROR_NONE on success, and ARGX_ERROR_OUT_OF_MEMORY when
 * the allocation fails; the command then keeps the description it had.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 * - text is not NULL. Checked with assert.
 */
ARGX_API argx_error_t argx_command_set_description(argx_command_t *command, const char *text);

/*
 * Returns the name of command. Never NULL and never empty.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 */
ARGX_API const char *argx_command_name(const argx_command_t *command);

/*
 * Returns the description of command, or NULL when none has been set.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 */
ARGX_API const char *argx_command_description(const argx_command_t *command);

/*
 * Returns the command that command is a subcommand of, or NULL when command
 * is a root.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 */
ARGX_API const argx_command_t *argx_command_parent(const argx_command_t *command);

/*
 * Return the number of options, positionals and subcommands of command
 * itself (not of its subcommands), and the one at index, counting from 0.
 * They are in the order they were added.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 * - For the functions that take an index: index is less than the matching
 *   count. Checked with assert.
 */
ARGX_API size_t argx_command_option_count(const argx_command_t *command);
ARGX_API const argx_option_t *argx_command_option_at(const argx_command_t *command, size_t index);
ARGX_API size_t argx_command_positional_count(const argx_command_t *command);
ARGX_API const argx_positional_t *argx_command_positional_at(const argx_command_t *command,
                                                             size_t index);
ARGX_API size_t argx_command_subcommand_count(const argx_command_t *command);
ARGX_API const argx_command_t *argx_command_subcommand_at(const argx_command_t *command,
                                                          size_t index);

/*
 * Returns the option of command whose short name is name, or NULL when
 * there is none. Only command's own options are searched, not those of
 * its parent or its subcommands. The match is case-sensitive. For '\0',
 * which is not a name, the result is NULL.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 */
ARGX_API const argx_option_t *argx_command_find_short(const argx_command_t *command, char name);

/*
 * Returns the option of command whose long name, or negated name ("no-"
 * followed by the long name, for a negatable flag), equals the first
 * length bytes of name, or NULL when there is none. The string at name
 * need not end after length bytes, so a name can be looked up in place
 * inside a longer string such as "name=value". The whole name is compared
 * byte by byte: the match is case-sensitive, and neither part of a name
 * nor a longer string matches.
 *
 * Only command's own options are searched, not those of its parent or its
 * subcommands.
 *
 * When an option is found and negated is not NULL, *negated is false if
 * the long name matched and true if the negated name matched. When no
 * option is found, *negated is not written.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 * - name is not NULL and points to at least length bytes. That it is not
 *   NULL is checked with assert.
 * - negated may be NULL.
 */
ARGX_API const argx_option_t *argx_command_find_long(const argx_command_t *command,
                                                     const char *name, size_t length,
                                                     bool *negated);

/*
 * Returns the subcommand of command named name, or NULL when there is
 * none. Only command's direct subcommands are searched, not their
 * subcommands. The whole name is compared byte by byte, so the match is
 * case-sensitive.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 * - name is not NULL. Checked with assert.
 */
ARGX_API const argx_command_t *argx_command_find_subcommand(const argx_command_t *command,
                                                            const char *name);

/* ---- Options ------------------------------------------------------------ */

/*
 * Add an option to command, after its existing options, and write it to
 * *out if out is not NULL. There is one function per option kind:
 *
 * - argx_command_add_flag(): a flag, ARGX_OPTION_FLAG.
 * - argx_command_add_negatable(): a negatable flag,
 *   ARGX_OPTION_NEGATABLE. Its negated name, "no-" followed by the long
 *   name, is built here; the long name "color" gives the negated name
 *   "no-color".
 * - argx_command_add_count(): a count, ARGX_OPTION_COUNT.
 * - argx_command_add_value(): an option that takes one value,
 *   ARGX_OPTION_VALUE. It is the only kind with a value type, type. The
 *   value type is borrowed, not copied: the caller keeps it valid until the
 *   tree's root is destroyed, and destroys it afterwards.
 *
 * An option has a short name, a long name, or both. short_name is '\0' for
 * an option without a short name, and long_name is NULL for an option
 * without a long name. A short name is one ASCII letter or digit. A long
 * name has at least two characters; the first is an ASCII letter or digit,
 * and the rest are ASCII letters, digits, '-' or '_'. The long name is
 * copied. A negatable flag always has a long name.
 *
 * The option belongs to the tree and is destroyed with the root.
 *
 * Return ARGX_ERROR_NONE on success. On failure, the tree is as it was
 * before the call, nothing from the call stays allocated, and *out is
 * NULL. The conditions are checked in this order:
 *
 * - ARGX_ERROR_INVALID_NAME: there is neither a short name nor a long
 *   name.
 * - ARGX_ERROR_INVALID_NAME: a negatable flag has no long name.
 * - ARGX_ERROR_INVALID_NAME: the short name or the long name breaks the
 *   naming rule; the empty string is not a long name.
 * - ARGX_ERROR_NAME_CONFLICT: command already has an option with the same
 *   short name.
 * - ARGX_ERROR_NAME_CONFLICT: command has an option whose long name or
 *   negated name equals the long name.
 * - ARGX_ERROR_NAME_CONFLICT: for a negatable flag, command has an option
 *   whose long name or negated name equals the negated name.
 * - ARGX_ERROR_OUT_OF_MEMORY: an allocation failed.
 *
 * The same names can be used in other commands, including command's
 * parent and its subcommands.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 * - short_name may be '\0' and long_name may be NULL, as described above.
 * - For argx_command_add_value(): type is not NULL (checked with assert)
 *   and is a value type that has not been destroyed.
 * - out may be NULL, when the handle is not needed.
 */
ARGX_API argx_error_t argx_command_add_flag(argx_command_t *command, char short_name,
                                            const char *long_name, argx_option_t **out);
ARGX_API argx_error_t argx_command_add_negatable(argx_command_t *command, char short_name,
                                                 const char *long_name, argx_option_t **out);
ARGX_API argx_error_t argx_command_add_count(argx_command_t *command, char short_name,
                                             const char *long_name, argx_option_t **out);
ARGX_API argx_error_t argx_command_add_value(argx_command_t *command, char short_name,
                                             const char *long_name, const argx_type_t *type,
                                             argx_option_t **out);

/*
 * Returns the command that option belongs to. Never NULL.
 *
 * Preconditions:
 *
 * - option is not NULL. Checked with assert.
 */
ARGX_API const argx_command_t *argx_option_command(const argx_option_t *option);

/*
 * Returns the kind of option.
 *
 * Preconditions:
 *
 * - option is not NULL. Checked with assert.
 */
ARGX_API argx_option_kind_t argx_option_kind(const argx_option_t *option);

/*
 * Returns the short name of option, or '\0' when it has none.
 *
 * Preconditions:
 *
 * - option is not NULL. Checked with assert.
 */
ARGX_API char argx_option_short_name(const argx_option_t *option);

/*
 * Returns the long name of option, or NULL when it has none.
 *
 * Preconditions:
 *
 * - option is not NULL. Checked with assert.
 */
ARGX_API const char *argx_option_long_name(const argx_option_t *option);

/*
 * Returns option's negated name ("no-" followed by its long name) when
 * option is a negatable flag, and NULL for every other kind.
 *
 * Preconditions:
 *
 * - option is not NULL. Checked with assert.
 */
ARGX_API const char *argx_option_negated_name(const argx_option_t *option);

/*
 * Returns option's value type when option takes a value, and NULL for
 * every other kind. It is the value type passed to
 * argx_command_add_value(); it belongs to the caller of that function.
 *
 * Preconditions:
 *
 * - option is not NULL. Checked with assert.
 */
ARGX_API const argx_type_t *argx_option_type(const argx_option_t *option);

/*
 * Set an attribute of option. An option has the attributes its kind
 * takes. Every attribute is optional, and each is set separately, in any
 * order, after the option has been added:
 *
 * - argx_option_set_description(): the description. Every kind has one.
 * - argx_option_set_value_name(): the value name, the name the help
 *   shows for the value, such as "FILE". Only for an option that takes a
 *   value, ARGX_OPTION_VALUE.
 * - argx_option_set_required(): whether the option must be given. Only
 *   for an option that takes a value.
 * - argx_option_set_default(): the default value, as a string in the form
 *   the value takes on a command line. Only for an option that takes a
 *   value. The string is stored as is; whether the option's value
 *   type takes it is not checked here.
 * - argx_option_set_initial(): the initial state of a negatable flag,
 *   ARGX_OPTION_NEGATABLE, which is its state when it is not given: true
 *   for on and false for off. Only for a negatable flag.
 *
 * Strings are copied. The empty string can be set; it is not the same as
 * an unset attribute. Setting an attribute again replaces its earlier
 * value. Once set, an attribute cannot be unset.
 *
 * Return ARGX_ERROR_NONE on success. On failure, the option and the rest
 * of the tree are as they were before the call, the attribute keeps its
 * previous value, and nothing from the call stays allocated. The
 * conditions are checked in this order:
 *
 * - ARGX_ERROR_INVALID_DEFINITION: option's kind does not take the
 *   attribute, whatever the value.
 * - ARGX_ERROR_OUT_OF_MEMORY: for a string, allocating the copy failed.
 *
 * Preconditions:
 *
 * - option is not NULL (checked with assert) and is a handle written by
 *   one of the functions that add an option.
 * - text, name and value are not NULL. Checked with assert.
 */
ARGX_API argx_error_t argx_option_set_description(argx_option_t *option, const char *text);
ARGX_API argx_error_t argx_option_set_value_name(argx_option_t *option, const char *name);
ARGX_API argx_error_t argx_option_set_required(argx_option_t *option, bool required);
ARGX_API argx_error_t argx_option_set_default(argx_option_t *option, const char *value);
ARGX_API argx_error_t argx_option_set_initial(argx_option_t *option, bool on);

/*
 * Returns the description of option, or NULL when none has been set.
 *
 * Preconditions:
 *
 * - option is not NULL. Checked with assert.
 */
ARGX_API const char *argx_option_description(const argx_option_t *option);

/*
 * Returns option's value name, or NULL when none has been set. It is
 * always NULL for an option that takes no value.
 *
 * Preconditions:
 *
 * - option is not NULL. Checked with assert.
 */
ARGX_API const char *argx_option_value_name(const argx_option_t *option);

/*
 * Returns whether option is required. It is false until set, and always
 * false for an option that takes no value.
 *
 * Preconditions:
 *
 * - option is not NULL. Checked with assert.
 */
ARGX_API bool argx_option_is_required(const argx_option_t *option);

/*
 * Returns option's default value (the string that was set), or NULL when
 * none has been set. It is always NULL for an option that takes no value.
 *
 * Preconditions:
 *
 * - option is not NULL. Checked with assert.
 */
ARGX_API const char *argx_option_default(const argx_option_t *option);

/*
 * Returns option's initial state: true for on, false for off. It is off
 * until set, and always false for an option that is not a negatable flag.
 *
 * Preconditions:
 *
 * - option is not NULL. Checked with assert.
 */
ARGX_API bool argx_option_initial(const argx_option_t *option);

/* ---- Positionals -------------------------------------------------------- */

/*
 * Adds a positional named name, with value type type and arity arity, to
 * command, after its existing positionals, and writes it to *out if out is
 * not NULL. A command's positionals take the command line arguments in the
 * order they were added.
 *
 * The name is used for display and to tell positionals apart; it never
 * appears on a command line. Any non-empty string is a valid name: there
 * is no character rule. The name is copied.
 *
 * The value type is borrowed, not copied: the caller keeps it valid until
 * the tree's root is destroyed, and destroys it afterwards.
 *
 * The positional belongs to the tree and is destroyed with the root.
 *
 * Returns ARGX_ERROR_NONE on success. On failure, the tree is as it was
 * before the call, nothing from the call stays allocated, and *out is
 * NULL. The conditions are checked in this order:
 *
 * - ARGX_ERROR_INVALID_NAME: name is the empty string.
 * - ARGX_ERROR_INVALID_DEFINITION: command has a subcommand. A command
 *   has positionals or subcommands, not both. (It can have options with
 *   either.)
 * - ARGX_ERROR_NAME_CONFLICT: command already has a positional with that
 *   name. The same name can be used in another command.
 * - ARGX_ERROR_OUT_OF_MEMORY: an allocation failed.
 *
 * This call does not check whether the arities of a command's positionals
 * come in an order a command line can follow (nothing required after an
 * optional one, nothing after one that takes any number). Tree validation
 * checks it.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 * - name is not NULL. Checked with assert.
 * - type is not NULL (checked with assert) and is a value type that has not
 *   been destroyed.
 * - arity is one of the values of argx_arity_t. Checked with assert.
 * - out may be NULL, when the handle is not needed.
 */
ARGX_API argx_error_t argx_command_add_positional(argx_command_t *command, const char *name,
                                                  const argx_type_t *type, argx_arity_t arity,
                                                  argx_positional_t **out);

/*
 * Set an attribute of positional. Both attributes are optional, and each
 * is set separately, in any order, after the positional has been added:
 *
 * - argx_positional_set_description(): the description. Every arity has
 *   one.
 * - argx_positional_set_default(): the default value, as a string in the
 *   form the value takes on a command line. Only for a positional that
 *   takes zero or one argument, ARGX_ARITY_OPTIONAL. The string is stored
 *   as is; whether the positional's value type takes it is not checked
 *   here.
 *
 * The string is copied. The empty string can be set; it is not the same as
 * an unset attribute. Setting an attribute again replaces its earlier
 * value. Once set, an attribute cannot be unset.
 *
 * Return ARGX_ERROR_NONE on success. On failure, the positional and the
 * rest of the tree are as they were before the call, the attribute keeps
 * its previous value, and nothing from the call stays allocated. The
 * conditions are checked in this order:
 *
 * - ARGX_ERROR_INVALID_DEFINITION: for the default value, positional's
 *   arity is not ARGX_ARITY_OPTIONAL, whatever the value.
 * - ARGX_ERROR_OUT_OF_MEMORY: allocating the copy failed.
 *
 * Preconditions:
 *
 * - positional is not NULL (checked with assert) and is a handle written
 *   by argx_command_add_positional().
 * - text and value are not NULL. Checked with assert.
 */
ARGX_API argx_error_t argx_positional_set_description(argx_positional_t *positional,
                                                      const char *text);
ARGX_API argx_error_t argx_positional_set_default(argx_positional_t *positional,
                                                  const char *value);

/*
 * Returns the command that positional belongs to. Never NULL.
 *
 * Preconditions:
 *
 * - positional is not NULL. Checked with assert.
 */
ARGX_API const argx_command_t *argx_positional_command(const argx_positional_t *positional);

/*
 * Returns the name of positional. Never NULL and never empty.
 *
 * Preconditions:
 *
 * - positional is not NULL. Checked with assert.
 */
ARGX_API const char *argx_positional_name(const argx_positional_t *positional);

/*
 * Returns the arity of positional: how many arguments it takes.
 *
 * Preconditions:
 *
 * - positional is not NULL. Checked with assert.
 */
ARGX_API argx_arity_t argx_positional_arity(const argx_positional_t *positional);

/*
 * Returns positional's value type. Never NULL. It is the value type passed
 * to argx_command_add_positional(); it belongs to the caller of that
 * function.
 *
 * Preconditions:
 *
 * - positional is not NULL. Checked with assert.
 */
ARGX_API const argx_type_t *argx_positional_type(const argx_positional_t *positional);

/*
 * Returns the description of positional, or NULL when none has been set.
 *
 * Preconditions:
 *
 * - positional is not NULL. Checked with assert.
 */
ARGX_API const char *argx_positional_description(const argx_positional_t *positional);

/*
 * Returns positional's default value (the string that was set), or NULL
 * when none has been set. It is always NULL for a positional whose arity
 * is not ARGX_ARITY_OPTIONAL.
 *
 * Preconditions:
 *
 * - positional is not NULL. Checked with assert.
 */
ARGX_API const char *argx_positional_default(const argx_positional_t *positional);

/* ---- Validation --------------------------------------------------------- */

/*
 * Validates root's tree. It checks root and every subcommand below it for
 * what the add and set functions cannot check, because each check depends
 * on more than one definition. A tree that passes is marked as validated.
 *
 * Commands are checked from the root down: a command first, then its
 * subcommands in the order they were added, each with everything below it
 * before the next. Within one command, the checks run in this order:
 *
 * 1. The positionals, in the order they were added. A positional is
 *    faulty when
 *    - it needs an argument (ARGX_ARITY_REQUIRED,
 *      ARGX_ARITY_ONE_OR_MORE) and an earlier positional can do without
 *      one (ARGX_ARITY_OPTIONAL, ARGX_ARITY_ZERO_OR_MORE), or
 *    - an earlier positional takes any number of arguments
 *      (ARGX_ARITY_ZERO_OR_MORE, ARGX_ARITY_ONE_OR_MORE).
 * 2. The options, in the order they were added, running both checks on
 *    each option before the next. An option is faulty when
 *    - it is required and has a default value
 *      (ARGX_ERROR_INVALID_DEFINITION), or
 *    - its value type does not take its default value
 *      (ARGX_ERROR_INVALID_DEFAULT).
 * 3. The positionals' default values, in the order the positionals were
 *    added. A positional is faulty when its value type does not take its
 *    default value (ARGX_ERROR_INVALID_DEFAULT).
 *
 * Validation stops at the first fault. The order is fixed, so the same
 * definition always gives the same result.
 *
 * A default value is checked by converting it. For each default value,
 * validation allocates an area for the value, calls argx_type_convert()
 * with the option's or positional's value type, calls argx_type_release()
 * if the conversion succeeded, and frees the area. So a user-defined value
 * type's conversion and cleanup functions are called during validation,
 * once for each default value of that type that is reached, in every
 * validation. The values are cleaned up before validation returns; none is
 * kept. Any conversion failure other than ARGX_ERROR_OUT_OF_MEMORY,
 * whatever its code, counts as a default value the value type does not
 * take.
 *
 * Returns ARGX_ERROR_NONE when the tree has no fault. The tree is then
 * validated, and NULL is written to *command, *option and *positional.
 *
 * Returns ARGX_ERROR_INVALID_DEFINITION or ARGX_ERROR_INVALID_DEFAULT
 * for a fault, as listed with the checks above. The command that owns the
 * faulty option or positional is written to *command. The faulty option
 * is written to *option, or the faulty positional to *positional, and NULL
 * to the other one. The handles belong to the tree: they are valid until
 * the root is destroyed and can be read with the reading functions. The
 * tree stays unvalidated.
 *
 * Returns ARGX_ERROR_OUT_OF_MEMORY when the area for a value cannot be
 * allocated or a conversion returns that code. NULL is written to
 * *command, *option and *positional, nothing stays allocated, and the
 * tree's state is as it was before the call.
 *
 * Validation never changes a definition: whatever it returns, the
 * commands, options, positionals and their attributes are as they were
 * before the call. The recursion depth equals the tree depth.
 *
 * Preconditions:
 *
 * - root is not NULL and is a root, not a subcommand. Both are checked with
 *   assert.
 * - command, option and positional may each be NULL when that part of the
 *   fault location is not needed. Every call writes each output that is
 *   not NULL.
 */
ARGX_API argx_error_t argx_command_validate(argx_command_t *root,
                                            const argx_command_t **command,
                                            const argx_option_t **option,
                                            const argx_positional_t **positional);

/*
 * Returns whether command's tree is validated: whether
 * argx_command_validate() succeeded on its root and nothing has been added
 * to or set in the tree since. Any command in the tree can be passed; the
 * result is the same for all of them.
 *
 * A new root is not validated. Every successful addition or set, anywhere
 * in the tree, makes the tree unvalidated again, even when it sets the
 * value that was already there. A failed one leaves the state as it was.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 */
ARGX_API bool argx_command_is_validated(const argx_command_t *command);

#ifdef __cplusplus
}
#endif

#endif /* ARGX_SCHEMA_H */
