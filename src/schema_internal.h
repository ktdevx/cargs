/*
 * CLArgs - internal declarations for the schema.
 *
 * The contents of a command, an option and a positional, and the helper
 * functions that the sources of the schema share. Only the sources of the
 * schema include this header.
 *
 * Each command, option and positional is one allocation of its own. The
 * arrays of a command hold the pointers to them, so a handle stays the same
 * when an array grows. Every string is a copy made with clargs_strdup()
 * and is owned by the structure that holds it. The value types are
 * borrowed: the tree never frees one.
 */
#ifndef CLARGS_SCHEMA_INTERNAL_H
#define CLARGS_SCHEMA_INTERNAL_H

#include <clargs/schema.h>
#include <clargs/type.h>

#include "vec.h"

#include <stdbool.h>

/*
 * A command.
 *
 * - name: the name. Never NULL and never empty.
 * - description: the description, or NULL when none has been set.
 * - parent: the command this one is a subcommand of; NULL for a root.
 * - options: the options, as clargs_option_t *, in the order of the
 *   definition.
 * - positionals: the positionals, as clargs_positional_t *, in the order of
 *   the definition.
 * - subcommands: the subcommands, as clargs_command_t *, in the order of
 *   the definition.
 * - validated: whether the tree has been validated since its last change.
 *   Used in the root only; read and write it through the root.
 */
struct clargs_command
{
    char *name;
    char *description;
    clargs_command_t *parent;
    clargs_vec_t options;
    clargs_vec_t positionals;
    clargs_vec_t subcommands;
    bool validated;
};

/*
 * An option.
 *
 * - command: the command the option belongs to. Never NULL.
 * - kind: the kind.
 * - short_name: the short name, or '\0' when the option has none.
 * - long_name: the long name, or NULL when the option has none.
 * - negated_name: "no-" and the long name, for the kind
 *   CLARGS_OPTION_NEGATABLE; NULL for every other kind.
 * - type: the value type, for the kind CLARGS_OPTION_VALUE; NULL for every
 *   other kind. Borrowed.
 * - description: the description, or NULL when none has been set.
 * - value_name: the display name of the value, or NULL when none has been
 *   set. Only the kind CLARGS_OPTION_VALUE has one.
 * - default_value: the default value, or NULL when none has been set. Only
 *   the kind CLARGS_OPTION_VALUE has one.
 * - required: whether the option is required. Only the kind
 *   CLARGS_OPTION_VALUE can be.
 * - initial: the initial state of the kind CLARGS_OPTION_NEGATABLE; false
 *   for every other kind.
 */
struct clargs_option
{
    clargs_command_t *command;
    clargs_option_kind_t kind;
    char short_name;
    char *long_name;
    char *negated_name;
    const clargs_type_t *type;
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
 *   the arity CLARGS_ARITY_OPTIONAL has one.
 */
struct clargs_positional
{
    clargs_command_t *command;
    char *name;
    clargs_arity_t arity;
    const clargs_type_t *type;
    char *description;
    char *default_value;
};

/*
 * Returns the root of the tree that command belongs to; command itself when
 * it is a root.
 *
 * Precondition: command is not NULL. It is checked with assert.
 */
clargs_command_t *clargs_schema_root(clargs_command_t *command);

/*
 * Marks the tree that command belongs to as not validated. It is called
 * last in a change that succeeded, and never in one that failed.
 *
 * Precondition: command is not NULL. It is checked with assert.
 */
void clargs_schema_touch(clargs_command_t *command);

/*
 * Copies text and puts the copy in *slot, in the place of the string that
 * was there, which is freed. *slot may be NULL before the call.
 *
 * Returns CLARGS_ERROR_NONE, or CLARGS_ERROR_OUT_OF_MEMORY when the copy
 * cannot be allocated; *slot and its string are then as they were.
 *
 * The copy is made before the old string is freed, so text may be the
 * string at *slot or a part of it.
 *
 * Preconditions: slot and text are not NULL. Checked with assert.
 */
clargs_error_t clargs_schema_replace_string(char **slot, const char *text);

#endif /* CLARGS_SCHEMA_INTERNAL_H */
