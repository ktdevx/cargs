/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - options: adding the four option kinds to a command, setting
 * option attributes, and reading options.
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

/*
 * The prefix of a negated name. For example, the negated name of the long
 * name "color" is "no-color".
 */
#define NEGATED_PREFIX "no-"
#define NEGATED_PREFIX_LENGTH (sizeof(NEGATED_PREFIX) - 1)

/*
 * Whether name, which may be NULL, is the negated name of long_name. The
 * negated name is not built, because the check comes before any
 * allocation.
 */
static bool is_negated_name_of(const char *name, const char *long_name)
{
    return (name != NULL) && (strncmp(name, NEGATED_PREFIX, NEGATED_PREFIX_LENGTH) == 0) &&
           (strcmp(name + NEGATED_PREFIX_LENGTH, long_name) == 0);
}

/*
 * Whether command has an option whose long name or negated name is the
 * negated name of long_name.
 */
static bool negated_name_is_in_use(const argx_command_t *command, const char *long_name)
{
    size_t count = argx_command_option_count(command);
    size_t index;

    for (index = 0; index < count; ++index)
    {
        const argx_option_t *option = argx_command_option_at(command, index);

        if (is_negated_name_of(option->long_name, long_name) ||
            is_negated_name_of(option->negated_name, long_name))
        {
            return true;
        }
    }
    return false;
}

/*
 * Returns a new string, "no-" followed by long_name, or NULL when it
 * cannot be allocated.
 */
static char *new_negated_name(const char *long_name)
{
    size_t length = strlen(long_name);
    char *negated_name = (char *)argx_malloc(NEGATED_PREFIX_LENGTH + length + 1);

    if (negated_name == NULL)
    {
        return NULL;
    }

    memcpy(negated_name, NEGATED_PREFIX, NEGATED_PREFIX_LENGTH);
    memcpy(negated_name + NEGATED_PREFIX_LENGTH, long_name, length + 1);
    return negated_name;
}

/*
 * Allocates an option of command with copies of its names. It is not added
 * to the command's array. Returns NULL, with nothing left allocated, when
 * an allocation fails.
 */
static argx_option_t *new_option(argx_command_t *command, argx_option_kind_t kind, char short_name,
                                 const char *long_name, const argx_type_t *type)
{
    argx_option_t *option = (argx_option_t *)argx_malloc(sizeof(*option));

    if (option == NULL)
    {
        return NULL;
    }

    option->command = command;
    option->kind = kind;
    option->short_name = short_name;
    option->long_name = NULL;
    option->negated_name = NULL;
    option->type = type;
    option->description = NULL;
    option->value_name = NULL;
    option->default_value = NULL;
    option->required = false;
    option->initial = false;

    if (long_name != NULL)
    {
        option->long_name = argx_strdup(long_name);
        if (option->long_name == NULL)
        {
            argx_free(option);
            return NULL;
        }
    }

    if (kind == ARGX_OPTION_NEGATABLE)
    {
        option->negated_name = new_negated_name(long_name);
        if (option->negated_name == NULL)
        {
            argx_free(option->long_name);
            argx_free(option);
            return NULL;
        }
    }

    return option;
}

/* Frees an option returned by new_option() that is not in any array. */
static void free_new_option(argx_option_t *option)
{
    argx_free(option->negated_name);
    argx_free(option->long_name);
    argx_free(option);
}

/*
 * Adds an option of any of the four kinds. type is the value type of an
 * option with a value, and NULL for every other kind.
 */
static argx_error_t add_option(argx_command_t *command, argx_option_kind_t kind, char short_name,
                               const char *long_name, const argx_type_t *type, argx_option_t **out)
{
    argx_option_t *option;

    assert(command != NULL);

    if (out != NULL)
    {
        *out = NULL;
    }

    /*
     * The checks come before any allocation, so an invalid definition
     * changes nothing. Their order is part of the contract.
     */
    if ((short_name == '\0') && (long_name == NULL))
    {
        return ARGX_ERROR_INVALID_NAME;
    }
    if ((kind == ARGX_OPTION_NEGATABLE) && (long_name == NULL))
    {
        return ARGX_ERROR_INVALID_NAME;
    }
    if ((short_name != '\0') && !argx_name_is_short(short_name))
    {
        return ARGX_ERROR_INVALID_NAME;
    }
    if ((long_name != NULL) && !argx_name_is_long(long_name))
    {
        return ARGX_ERROR_INVALID_NAME;
    }
    if (argx_command_find_short(command, short_name) != NULL)
    {
        return ARGX_ERROR_NAME_CONFLICT;
    }
    if ((long_name != NULL) && (argx_command_find_long(command, long_name, strlen(long_name), NULL) != NULL))
    {
        return ARGX_ERROR_NAME_CONFLICT;
    }
    if ((kind == ARGX_OPTION_NEGATABLE) && negated_name_is_in_use(command, long_name))
    {
        return ARGX_ERROR_NAME_CONFLICT;
    }

    option = new_option(command, kind, short_name, long_name, type);
    if (option == NULL)
    {
        return ARGX_ERROR_OUT_OF_MEMORY;
    }

    /*
     * The tree is changed last, and a failed push leaves the array
     * unchanged.
     */
    if (argx_vec_push(&command->options, &option) != ARGX_ERROR_NONE)
    {
        free_new_option(option);
        return ARGX_ERROR_OUT_OF_MEMORY;
    }

    argx_schema_touch(command);

    if (out != NULL)
    {
        *out = option;
    }
    return ARGX_ERROR_NONE;
}

/* ---- Addition ----------------------------------------------------------- */

argx_error_t argx_command_add_flag(argx_command_t *command, char short_name, const char *long_name,
                                   argx_option_t **out)
{
    return add_option(command, ARGX_OPTION_FLAG, short_name, long_name, NULL, out);
}

argx_error_t argx_command_add_negatable(argx_command_t *command, char short_name,
                                        const char *long_name, argx_option_t **out)
{
    return add_option(command, ARGX_OPTION_NEGATABLE, short_name, long_name, NULL, out);
}

argx_error_t argx_command_add_count(argx_command_t *command, char short_name, const char *long_name,
                                    argx_option_t **out)
{
    return add_option(command, ARGX_OPTION_COUNT, short_name, long_name, NULL, out);
}

argx_error_t argx_command_add_value(argx_command_t *command, char short_name, const char *long_name,
                                    const argx_type_t *type, argx_option_t **out)
{
    assert(type != NULL);

    return add_option(command, ARGX_OPTION_VALUE, short_name, long_name, type, out);
}

/* ---- Attributes --------------------------------------------------------- */

/*
 * Sets a string attribute: *slot, a member of option, receives a copy of
 * text. only_value tells whether only an option with a value has this
 * attribute. The kind check comes before the allocation, so setting it on
 * another kind changes nothing.
 */
static argx_error_t set_string(argx_option_t *option, char **slot, const char *text, bool only_value)
{
    argx_error_t error;

    if (only_value && (option->kind != ARGX_OPTION_VALUE))
    {
        return ARGX_ERROR_INVALID_DEFINITION;
    }

    error = argx_schema_replace_string(slot, text);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    argx_schema_touch(option->command);
    return ARGX_ERROR_NONE;
}

argx_error_t argx_option_set_description(argx_option_t *option, const char *text)
{
    assert(option != NULL);
    assert(text != NULL);

    return set_string(option, &option->description, text, false);
}

argx_error_t argx_option_set_value_name(argx_option_t *option, const char *name)
{
    assert(option != NULL);
    assert(name != NULL);

    return set_string(option, &option->value_name, name, true);
}

argx_error_t argx_option_set_required(argx_option_t *option, bool required)
{
    assert(option != NULL);

    if (option->kind != ARGX_OPTION_VALUE)
    {
        return ARGX_ERROR_INVALID_DEFINITION;
    }

    option->required = required;
    argx_schema_touch(option->command);
    return ARGX_ERROR_NONE;
}

argx_error_t argx_option_set_default(argx_option_t *option, const char *value)
{
    assert(option != NULL);
    assert(value != NULL);

    return set_string(option, &option->default_value, value, true);
}

argx_error_t argx_option_set_initial(argx_option_t *option, bool on)
{
    assert(option != NULL);

    if (option->kind != ARGX_OPTION_NEGATABLE)
    {
        return ARGX_ERROR_INVALID_DEFINITION;
    }

    option->initial = on;
    argx_schema_touch(option->command);
    return ARGX_ERROR_NONE;
}

/* ---- Reading ------------------------------------------------------------ */

const argx_command_t *argx_option_command(const argx_option_t *option)
{
    assert(option != NULL);

    return option->command;
}

argx_option_kind_t argx_option_kind(const argx_option_t *option)
{
    assert(option != NULL);

    return option->kind;
}

char argx_option_short_name(const argx_option_t *option)
{
    assert(option != NULL);

    return option->short_name;
}

const char *argx_option_long_name(const argx_option_t *option)
{
    assert(option != NULL);

    return option->long_name;
}

const char *argx_option_negated_name(const argx_option_t *option)
{
    assert(option != NULL);

    return option->negated_name;
}

const argx_type_t *argx_option_type(const argx_option_t *option)
{
    assert(option != NULL);

    return option->type;
}

const char *argx_option_description(const argx_option_t *option)
{
    assert(option != NULL);

    return option->description;
}

const char *argx_option_value_name(const argx_option_t *option)
{
    assert(option != NULL);

    return option->value_name;
}

bool argx_option_is_required(const argx_option_t *option)
{
    assert(option != NULL);

    return option->required;
}

const char *argx_option_default(const argx_option_t *option)
{
    assert(option != NULL);

    return option->default_value;
}

bool argx_option_initial(const argx_option_t *option)
{
    assert(option != NULL);

    return option->initial;
}
