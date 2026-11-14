/*
 * CLArgs - options: the addition of the four kinds of options to a command,
 * the attributes of an option, and the reading of an option.
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
 * What a negated name starts with: the negated name of the long name
 * "color" is "no-color".
 */
#define NEGATED_PREFIX "no-"
#define NEGATED_PREFIX_LENGTH (sizeof(NEGATED_PREFIX) - 1)

/*
 * Whether name, which may be NULL, is the negated name of long_name. The
 * negated name is not built: the check comes before every allocation.
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
static bool negated_name_is_in_use(const clargs_command_t *command, const char *long_name)
{
    size_t count = clargs_command_option_count(command);
    size_t index;

    for (index = 0; index < count; ++index)
    {
        const clargs_option_t *option = clargs_command_option_at(command, index);

        if (is_negated_name_of(option->long_name, long_name) ||
            is_negated_name_of(option->negated_name, long_name))
        {
            return true;
        }
    }
    return false;
}

/*
 * Returns a new string, "no-" and long_name, or NULL when it cannot be
 * allocated.
 */
static char *new_negated_name(const char *long_name)
{
    size_t length = strlen(long_name);
    char *negated_name = (char *)clargs_malloc(NEGATED_PREFIX_LENGTH + length + 1);

    if (negated_name == NULL)
    {
        return NULL;
    }

    memcpy(negated_name, NEGATED_PREFIX, NEGATED_PREFIX_LENGTH);
    memcpy(negated_name + NEGATED_PREFIX_LENGTH, long_name, length + 1);
    return negated_name;
}

/*
 * Allocates an option of command with copies of its names. It is not put in
 * the array of the command. Returns NULL, with nothing left allocated, when
 * an allocation fails.
 */
static clargs_option_t *new_option(clargs_command_t *command, clargs_option_kind_t kind, char short_name,
                                   const char *long_name, const clargs_type_t *type)
{
    clargs_option_t *option = (clargs_option_t *)clargs_malloc(sizeof(*option));

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
        option->long_name = clargs_strdup(long_name);
        if (option->long_name == NULL)
        {
            clargs_free(option);
            return NULL;
        }
    }

    if (kind == CLARGS_OPTION_NEGATABLE)
    {
        option->negated_name = new_negated_name(long_name);
        if (option->negated_name == NULL)
        {
            clargs_free(option->long_name);
            clargs_free(option);
            return NULL;
        }
    }

    return option;
}

/* Frees an option that new_option() returned and that is in no array. */
static void free_new_option(clargs_option_t *option)
{
    clargs_free(option->negated_name);
    clargs_free(option->long_name);
    clargs_free(option);
}

/*
 * The addition of the four kinds. type is the value type of an option with a
 * value, and NULL for every other kind.
 */
static clargs_error_t add_option(clargs_command_t *command, clargs_option_kind_t kind, char short_name,
                                 const char *long_name, const clargs_type_t *type, clargs_option_t **out)
{
    clargs_option_t *option;

    assert(command != NULL);

    if (out != NULL)
    {
        *out = NULL;
    }

    /*
     * The checks come before every allocation, so a wrong definition
     * touches nothing. Their order is part of the contract.
     */
    if ((short_name == '\0') && (long_name == NULL))
    {
        return CLARGS_ERROR_INVALID_NAME;
    }
    if ((kind == CLARGS_OPTION_NEGATABLE) && (long_name == NULL))
    {
        return CLARGS_ERROR_INVALID_NAME;
    }
    if ((short_name != '\0') && !clargs_name_is_short(short_name))
    {
        return CLARGS_ERROR_INVALID_NAME;
    }
    if ((long_name != NULL) && !clargs_name_is_long(long_name))
    {
        return CLARGS_ERROR_INVALID_NAME;
    }
    if (clargs_command_find_short(command, short_name) != NULL)
    {
        return CLARGS_ERROR_NAME_CONFLICT;
    }
    if ((long_name != NULL) && (clargs_command_find_long(command, long_name, strlen(long_name), NULL) != NULL))
    {
        return CLARGS_ERROR_NAME_CONFLICT;
    }
    if ((kind == CLARGS_OPTION_NEGATABLE) && negated_name_is_in_use(command, long_name))
    {
        return CLARGS_ERROR_NAME_CONFLICT;
    }

    option = new_option(command, kind, short_name, long_name, type);
    if (option == NULL)
    {
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }

    /*
     * The tree is changed last: a push that fails leaves the array as it
     * was.
     */
    if (clargs_vec_push(&command->options, &option) != CLARGS_ERROR_NONE)
    {
        free_new_option(option);
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }

    clargs_schema_touch(command);

    if (out != NULL)
    {
        *out = option;
    }
    return CLARGS_ERROR_NONE;
}

/* ---- Addition ----------------------------------------------------------- */

clargs_error_t clargs_command_add_flag(clargs_command_t *command, char short_name, const char *long_name,
                                       clargs_option_t **out)
{
    return add_option(command, CLARGS_OPTION_FLAG, short_name, long_name, NULL, out);
}

clargs_error_t clargs_command_add_negatable(clargs_command_t *command, char short_name,
                                            const char *long_name, clargs_option_t **out)
{
    return add_option(command, CLARGS_OPTION_NEGATABLE, short_name, long_name, NULL, out);
}

clargs_error_t clargs_command_add_count(clargs_command_t *command, char short_name, const char *long_name,
                                        clargs_option_t **out)
{
    return add_option(command, CLARGS_OPTION_COUNT, short_name, long_name, NULL, out);
}

clargs_error_t clargs_command_add_value(clargs_command_t *command, char short_name, const char *long_name,
                                        const clargs_type_t *type, clargs_option_t **out)
{
    assert(type != NULL);

    return add_option(command, CLARGS_OPTION_VALUE, short_name, long_name, type, out);
}

/* ---- Attributes --------------------------------------------------------- */

/*
 * Sets an attribute that is a string: *slot, a member of option, gets a
 * copy of text. only_value tells whether the attribute is one that only an
 * option with a value has. The check of the kind comes before the
 * allocation, so a set on another kind touches nothing.
 */
static clargs_error_t set_string(clargs_option_t *option, char **slot, const char *text, bool only_value)
{
    clargs_error_t error;

    if (only_value && (option->kind != CLARGS_OPTION_VALUE))
    {
        return CLARGS_ERROR_INVALID_DEFINITION;
    }

    error = clargs_schema_replace_string(slot, text);
    if (error != CLARGS_ERROR_NONE)
    {
        return error;
    }

    clargs_schema_touch(option->command);
    return CLARGS_ERROR_NONE;
}

clargs_error_t clargs_option_set_description(clargs_option_t *option, const char *text)
{
    assert(option != NULL);
    assert(text != NULL);

    return set_string(option, &option->description, text, false);
}

clargs_error_t clargs_option_set_value_name(clargs_option_t *option, const char *name)
{
    assert(option != NULL);
    assert(name != NULL);

    return set_string(option, &option->value_name, name, true);
}

clargs_error_t clargs_option_set_required(clargs_option_t *option, bool required)
{
    assert(option != NULL);

    if (option->kind != CLARGS_OPTION_VALUE)
    {
        return CLARGS_ERROR_INVALID_DEFINITION;
    }

    option->required = required;
    clargs_schema_touch(option->command);
    return CLARGS_ERROR_NONE;
}

clargs_error_t clargs_option_set_default(clargs_option_t *option, const char *value)
{
    assert(option != NULL);
    assert(value != NULL);

    return set_string(option, &option->default_value, value, true);
}

clargs_error_t clargs_option_set_initial(clargs_option_t *option, bool on)
{
    assert(option != NULL);

    if (option->kind != CLARGS_OPTION_NEGATABLE)
    {
        return CLARGS_ERROR_INVALID_DEFINITION;
    }

    option->initial = on;
    clargs_schema_touch(option->command);
    return CLARGS_ERROR_NONE;
}

/* ---- Reading ------------------------------------------------------------ */

const clargs_command_t *clargs_option_command(const clargs_option_t *option)
{
    assert(option != NULL);

    return option->command;
}

clargs_option_kind_t clargs_option_kind(const clargs_option_t *option)
{
    assert(option != NULL);

    return option->kind;
}

char clargs_option_short_name(const clargs_option_t *option)
{
    assert(option != NULL);

    return option->short_name;
}

const char *clargs_option_long_name(const clargs_option_t *option)
{
    assert(option != NULL);

    return option->long_name;
}

const char *clargs_option_negated_name(const clargs_option_t *option)
{
    assert(option != NULL);

    return option->negated_name;
}

const clargs_type_t *clargs_option_type(const clargs_option_t *option)
{
    assert(option != NULL);

    return option->type;
}

const char *clargs_option_description(const clargs_option_t *option)
{
    assert(option != NULL);

    return option->description;
}

const char *clargs_option_value_name(const clargs_option_t *option)
{
    assert(option != NULL);

    return option->value_name;
}

bool clargs_option_is_required(const clargs_option_t *option)
{
    assert(option != NULL);

    return option->required;
}

const char *clargs_option_default(const clargs_option_t *option)
{
    assert(option != NULL);

    return option->default_value;
}

bool clargs_option_initial(const clargs_option_t *option)
{
    assert(option != NULL);

    return option->initial;
}
