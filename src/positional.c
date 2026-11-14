/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - positionals: adding a positional to a command, setting positional
 * attributes, and reading positionals.
 */
#include "schema_internal.h"

#include "alloc_internal.h"
#include "str.h"
#include "vec.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/*
 * Whether command has a positional named name. The whole name is
 * compared, byte by byte.
 */
static bool name_is_in_use(const argx_command_t *command, const char *name)
{
    size_t count = argx_command_positional_count(command);
    size_t index;

    for (index = 0; index < count; ++index)
    {
        if (strcmp(argx_command_positional_at(command, index)->name, name) == 0)
        {
            return true;
        }
    }
    return false;
}

/*
 * Allocates a positional of command with a copy of its name. It is not
 * added to the command's array. Returns NULL, with nothing left allocated,
 * when an allocation fails.
 */
static argx_positional_t *new_positional(argx_command_t *command, const char *name,
                                         const argx_type_t *type, argx_arity_t arity)
{
    argx_positional_t *positional = (argx_positional_t *)argx_malloc(sizeof(*positional));

    if (positional == NULL)
    {
        return NULL;
    }

    positional->command = command;
    positional->arity = arity;
    positional->type = type;
    positional->description = NULL;
    positional->default_value = NULL;

    positional->name = argx_strdup(name);
    if (positional->name == NULL)
    {
        argx_free(positional);
        return NULL;
    }

    return positional;
}

/*
 * Frees a positional returned by new_positional() that is not in any
 * array. Its name is the only string it has.
 */
static void free_new_positional(argx_positional_t *positional)
{
    argx_free(positional->name);
    argx_free(positional);
}

/* ---- Addition ----------------------------------------------------------- */

argx_error_t argx_command_add_positional(argx_command_t *command, const char *name,
                                         const argx_type_t *type, argx_arity_t arity,
                                         argx_positional_t **out)
{
    argx_positional_t *positional;

    assert(command != NULL);
    assert(name != NULL);
    assert(type != NULL);
    assert((arity == ARGX_ARITY_REQUIRED) || (arity == ARGX_ARITY_OPTIONAL) ||
           (arity == ARGX_ARITY_ZERO_OR_MORE) || (arity == ARGX_ARITY_ONE_OR_MORE));

    if (out != NULL)
    {
        *out = NULL;
    }

    /*
     * The checks come before any allocation, so an invalid definition
     * changes nothing. Their order is part of the contract.
     */
    if (name[0] == '\0')
    {
        return ARGX_ERROR_INVALID_NAME;
    }
    if (argx_command_subcommand_count(command) != 0)
    {
        return ARGX_ERROR_INVALID_DEFINITION;
    }
    if (name_is_in_use(command, name))
    {
        return ARGX_ERROR_NAME_CONFLICT;
    }

    positional = new_positional(command, name, type, arity);
    if (positional == NULL)
    {
        return ARGX_ERROR_OUT_OF_MEMORY;
    }

    /*
     * The tree is changed last, and a failed push leaves the array
     * unchanged.
     */
    if (argx_vec_push(&command->positionals, &positional) != ARGX_ERROR_NONE)
    {
        free_new_positional(positional);
        return ARGX_ERROR_OUT_OF_MEMORY;
    }

    argx_schema_touch(command);

    if (out != NULL)
    {
        *out = positional;
    }
    return ARGX_ERROR_NONE;
}

/* ---- Attributes --------------------------------------------------------- */

/*
 * Sets a string attribute: *slot, a member of positional, receives a copy
 * of text.
 */
static argx_error_t set_string(argx_positional_t *positional, char **slot, const char *text)
{
    argx_error_t error = argx_schema_replace_string(slot, text);

    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    argx_schema_touch(positional->command);
    return ARGX_ERROR_NONE;
}

argx_error_t argx_positional_set_description(argx_positional_t *positional, const char *text)
{
    assert(positional != NULL);
    assert(text != NULL);

    return set_string(positional, &positional->description, text);
}

argx_error_t argx_positional_set_default(argx_positional_t *positional, const char *value)
{
    assert(positional != NULL);
    assert(value != NULL);

    /*
     * The arity check comes before the allocation, so setting a default on
     * another arity changes nothing.
     */
    if (positional->arity != ARGX_ARITY_OPTIONAL)
    {
        return ARGX_ERROR_INVALID_DEFINITION;
    }

    return set_string(positional, &positional->default_value, value);
}

/* ---- Reading ------------------------------------------------------------ */

const argx_command_t *argx_positional_command(const argx_positional_t *positional)
{
    assert(positional != NULL);

    return positional->command;
}

const char *argx_positional_name(const argx_positional_t *positional)
{
    assert(positional != NULL);

    return positional->name;
}

argx_arity_t argx_positional_arity(const argx_positional_t *positional)
{
    assert(positional != NULL);

    return positional->arity;
}

const argx_type_t *argx_positional_type(const argx_positional_t *positional)
{
    assert(positional != NULL);

    return positional->type;
}

const char *argx_positional_description(const argx_positional_t *positional)
{
    assert(positional != NULL);

    return positional->description;
}

const char *argx_positional_default(const argx_positional_t *positional)
{
    assert(positional != NULL);

    return positional->default_value;
}
