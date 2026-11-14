/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - internal declarations for the schema.
 *
 * The command, option and positional structures, and the helper functions
 * shared by the schema sources. Only the schema sources include this
 * header.
 *
 * Each command, option and positional is a separate allocation. A
 * command's arrays hold pointers to them, so a handle stays the same when
 * an array grows. Every string is a copy made with argx_strdup() and is
 * owned by the structure that holds it. Value types are borrowed: the tree
 * never frees one.
 */
#ifndef ARGX_SCHEMA_INTERNAL_H
#define ARGX_SCHEMA_INTERNAL_H

#include <argx/schema.h>
#include <argx/type.h>

#include "vec.h"

#include <stdbool.h>

/*
 * A command.
 *
 * - name: the name. Never NULL and never empty.
 * - description: the description, or NULL when none has been set.
 * - parent: the command this one is a subcommand of. NULL for a root.
 * - options: the options, as argx_option_t *, in definition order.
 * - positionals: the positionals, as argx_positional_t *, in definition
 *   order.
 * - subcommands: the subcommands, as argx_command_t *, in definition
 *   order.
 * - validated: whether the tree has been validated since its last change.
 *   Used only in the root. Read and write it through the root.
 */
struct argx_command
{
    char *name;
    char *description;
    argx_command_t *parent;
    argx_vec_t options;
    argx_vec_t positionals;
    argx_vec_t subcommands;
    bool validated;
};

/*
 * An option.
 *
 * - command: the command the option belongs to. Never NULL.
 * - kind: the kind.
 * - short_name: the short name, or '\0' when the option has none.
 * - long_name: the long name, or NULL when the option has none.
 * - negated_name: "no-" followed by the long name, for the
 *   ARGX_OPTION_NEGATABLE kind. NULL for every other kind.
 * - type: the value type, for the ARGX_OPTION_VALUE kind. NULL for every
 *   other kind. Borrowed.
 * - description: the description, or NULL when none has been set.
 * - value_name: the value name, or NULL when none has been set. Only the
 *   ARGX_OPTION_VALUE kind has one.
 * - default_value: the default value, or NULL when none has been set. Only
 *   the ARGX_OPTION_VALUE kind has one.
 * - required: whether the option is required. Only the ARGX_OPTION_VALUE
 *   kind can be.
 * - initial: the initial state, for the ARGX_OPTION_NEGATABLE kind. False
 *   for every other kind.
 */
struct argx_option
{
    argx_command_t *command;
    argx_option_kind_t kind;
    char short_name;
    char *long_name;
    char *negated_name;
    const argx_type_t *type;
    char *description;
    char *value_name;
    char *default_value;
    bool required;
    bool initial;
};

/*
 * A positional.
 *
 * - command: the command the positional belongs to. Never NULL.
 * - name: the name. Never NULL and never empty.
 * - arity: how many arguments it takes.
 * - type: the value type. Never NULL. Borrowed.
 * - description: the description, or NULL when none has been set.
 * - default_value: the default value, or NULL when none has been set. Only
 *   the ARGX_ARITY_OPTIONAL arity has one.
 */
struct argx_positional
{
    argx_command_t *command;
    char *name;
    argx_arity_t arity;
    const argx_type_t *type;
    char *description;
    char *default_value;
};

/*
 * Marks the tree that command belongs to as not validated. It is called
 * as the last step of a successful change, and never in a failed one.
 *
 * Precondition: command is not NULL. It is checked with assert.
 */
void argx_schema_touch(argx_command_t *command);

/*
 * Copies text and stores the copy in *slot, replacing and freeing the
 * string that was there. *slot may be NULL before the call.
 *
 * Returns ARGX_ERROR_NONE, or ARGX_ERROR_OUT_OF_MEMORY when the copy
 * cannot be allocated. In that case *slot and its string are unchanged.
 *
 * The copy is made before the old string is freed, so text may be the
 * string at *slot or a part of it.
 *
 * Preconditions: slot and text are not NULL. They are checked with assert.
 */
argx_error_t argx_schema_replace_string(char **slot, const char *text);

#endif /* ARGX_SCHEMA_INTERNAL_H */
