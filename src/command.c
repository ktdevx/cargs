/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - commands: the schema tree, and the helper functions shared by the
 * schema sources.
 */
#include "schema_internal.h"

#include "alloc_internal.h"
#include "name.h"
#include "str.h"
#include "vec.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* The pointers held in a command's three arrays. */
static argx_option_t *option_at(const argx_command_t *command, size_t index)
{
    return *(argx_option_t **)argx_vec_at(&command->options, index);
}

static argx_positional_t *positional_at(const argx_command_t *command, size_t index)
{
    return *(argx_positional_t **)argx_vec_at(&command->positionals, index);
}

static argx_command_t *subcommand_at(const argx_command_t *command, size_t index)
{
    return *(argx_command_t **)argx_vec_at(&command->subcommands, index);
}

/* Whether the string name equals exactly the length bytes at text. */
static bool name_equals(const char *name, const char *text, size_t length)
{
    return (strlen(name) == length) && (memcmp(name, text, length) == 0);
}

/* Frees an option and its strings. The value type is borrowed and stays. */
static void free_option(argx_option_t *option)
{
    argx_free(option->long_name);
    argx_free(option->negated_name);
    argx_free(option->description);
    argx_free(option->value_name);
    argx_free(option->default_value);
    argx_free(option);
}

/*
 * Frees a positional and its strings. The value type is borrowed and
 * stays.
 */
static void free_positional(argx_positional_t *positional)
{
    argx_free(positional->name);
    argx_free(positional->description);
    argx_free(positional->default_value);
    argx_free(positional);
}

/*
 * Frees command and everything below it. The recursion depth equals the
 * tree depth.
 */
static void free_command(argx_command_t *command)
{
    size_t index;

    for (index = 0; index < command->subcommands.count; ++index)
    {
        free_command(subcommand_at(command, index));
    }
    for (index = 0; index < command->options.count; ++index)
    {
        free_option(option_at(command, index));
    }
    for (index = 0; index < command->positionals.count; ++index)
    {
        free_positional(positional_at(command, index));
    }

    argx_vec_destroy(&command->subcommands);
    argx_vec_destroy(&command->options);
    argx_vec_destroy(&command->positionals);
    argx_free(command->description);
    argx_free(command->name);
    argx_free(command);
}

/*
 * Allocates an empty command with a copy of name and with parent as its
 * parent (NULL for a root). It is not added to the parent's array.
 * Returns NULL, with nothing left allocated, when an allocation fails.
 */
static argx_command_t *new_command(const char *name, argx_command_t *parent)
{
    argx_command_t *command = (argx_command_t *)argx_malloc(sizeof(*command));

    if (command == NULL)
    {
        return NULL;
    }

    command->name = argx_strdup(name);
    if (command->name == NULL)
    {
        argx_free(command);
        return NULL;
    }

    command->description = NULL;
    command->parent = parent;
    argx_vec_init(&command->options, sizeof(argx_option_t *));
    argx_vec_init(&command->positionals, sizeof(argx_positional_t *));
    argx_vec_init(&command->subcommands, sizeof(argx_command_t *));
    command->validated = false;
    return command;
}

/*
 * Returns the root of the tree that command belongs to, or command itself
 * when it is a root. command is not NULL.
 */
static argx_command_t *root_of(argx_command_t *command)
{
    while (command->parent != NULL)
    {
        command = command->parent;
    }
    return command;
}

/* ---- Shared helper functions -------------------------------------------- */

void argx_schema_touch(argx_command_t *command)
{
    assert(command != NULL);

    root_of(command)->validated = false;
}

argx_error_t argx_schema_replace_string(char **slot, const char *text)
{
    char *copy;

    assert(slot != NULL);
    assert(text != NULL);

    copy = argx_strdup(text);
    if (copy == NULL)
    {
        return ARGX_ERROR_OUT_OF_MEMORY;
    }

    argx_free(*slot);
    *slot = copy;
    return ARGX_ERROR_NONE;
}

/* ---- Creation, destruction and changes ---------------------------------- */

argx_error_t argx_command_create(const char *name, argx_command_t **out)
{
    argx_command_t *root;

    assert(name != NULL);
    assert(out != NULL);

    *out = NULL;

    if (name[0] == '\0')
    {
        return ARGX_ERROR_INVALID_NAME;
    }

    root = new_command(name, NULL);
    if (root == NULL)
    {
        return ARGX_ERROR_OUT_OF_MEMORY;
    }

    *out = root;
    return ARGX_ERROR_NONE;
}

void argx_command_destroy(argx_command_t *root)
{
    if (root == NULL)
    {
        return;
    }

    assert(root->parent == NULL);

    free_command(root);
}

argx_error_t argx_command_add_subcommand(argx_command_t *command, const char *name,
                                         argx_command_t **out)
{
    argx_command_t *subcommand;

    assert(command != NULL);
    assert(name != NULL);

    if (out != NULL)
    {
        *out = NULL;
    }

    /*
     * The checks come before any allocation, so an invalid definition
     * changes nothing.
     */
    if (!argx_name_is_subcommand(name))
    {
        return ARGX_ERROR_INVALID_NAME;
    }
    if (command->positionals.count != 0)
    {
        return ARGX_ERROR_INVALID_DEFINITION;
    }
    if (argx_command_find_subcommand(command, name) != NULL)
    {
        return ARGX_ERROR_NAME_CONFLICT;
    }

    subcommand = new_command(name, command);
    if (subcommand == NULL)
    {
        return ARGX_ERROR_OUT_OF_MEMORY;
    }

    /*
     * The tree is changed last, and a failed push leaves the array
     * unchanged.
     */
    if (argx_vec_push(&command->subcommands, &subcommand) != ARGX_ERROR_NONE)
    {
        free_command(subcommand);
        return ARGX_ERROR_OUT_OF_MEMORY;
    }

    argx_schema_touch(command);

    if (out != NULL)
    {
        *out = subcommand;
    }
    return ARGX_ERROR_NONE;
}

argx_error_t argx_command_set_description(argx_command_t *command, const char *text)
{
    argx_error_t error;

    assert(command != NULL);
    assert(text != NULL);

    error = argx_schema_replace_string(&command->description, text);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    argx_schema_touch(command);
    return ARGX_ERROR_NONE;
}

/* ---- Reading ------------------------------------------------------------ */

const char *argx_command_name(const argx_command_t *command)
{
    assert(command != NULL);

    return command->name;
}

const char *argx_command_description(const argx_command_t *command)
{
    assert(command != NULL);

    return command->description;
}

const argx_command_t *argx_command_parent(const argx_command_t *command)
{
    assert(command != NULL);

    return command->parent;
}

size_t argx_command_option_count(const argx_command_t *command)
{
    assert(command != NULL);

    return command->options.count;
}

const argx_option_t *argx_command_option_at(const argx_command_t *command, size_t index)
{
    assert(command != NULL);
    assert(index < command->options.count);

    return option_at(command, index);
}

size_t argx_command_positional_count(const argx_command_t *command)
{
    assert(command != NULL);

    return command->positionals.count;
}

const argx_positional_t *argx_command_positional_at(const argx_command_t *command, size_t index)
{
    assert(command != NULL);
    assert(index < command->positionals.count);

    return positional_at(command, index);
}

size_t argx_command_subcommand_count(const argx_command_t *command)
{
    assert(command != NULL);

    return command->subcommands.count;
}

const argx_command_t *argx_command_subcommand_at(const argx_command_t *command, size_t index)
{
    assert(command != NULL);
    assert(index < command->subcommands.count);

    return subcommand_at(command, index);
}

/* ---- Lookup ------------------------------------------------------------- */

const argx_option_t *argx_command_find_short(const argx_command_t *command, char name)
{
    size_t index;

    assert(command != NULL);

    /*
     * '\0' marks an option that has no short name. It is not a name
     * itself.
     */
    if (name == '\0')
    {
        return NULL;
    }

    for (index = 0; index < command->options.count; ++index)
    {
        const argx_option_t *option = option_at(command, index);

        if (option->short_name == name)
        {
            return option;
        }
    }
    return NULL;
}

const argx_option_t *argx_command_find_long(const argx_command_t *command, const char *name,
                                            size_t length, bool *negated)
{
    size_t index;

    assert(command != NULL);
    assert(name != NULL);

    for (index = 0; index < command->options.count; ++index)
    {
        const argx_option_t *option = option_at(command, index);
        bool is_long = (option->long_name != NULL) && name_equals(option->long_name, name, length);

        if (is_long || ((option->negated_name != NULL) && name_equals(option->negated_name, name, length)))
        {
            if (negated != NULL)
            {
                *negated = !is_long;
            }
            return option;
        }
    }
    return NULL;
}

const argx_command_t *argx_command_find_subcommand(const argx_command_t *command, const char *name)
{
    size_t index;

    assert(command != NULL);
    assert(name != NULL);

    for (index = 0; index < command->subcommands.count; ++index)
    {
        const argx_command_t *subcommand = subcommand_at(command, index);

        if (strcmp(subcommand->name, name) == 0)
        {
            return subcommand;
        }
    }
    return NULL;
}
