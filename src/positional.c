/*
 * CLine - positionals: the addition of a positional to a command, the
 * attributes of a positional, and the reading of a positional.
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
 * Whether command has a positional with the name name. The whole name is
 * compared, byte by byte.
 */
static bool name_is_in_use(const cline_command_t *command, const char *name)
{
    size_t count = cline_command_positional_count(command);
    size_t index;

    for (index = 0; index < count; ++index)
    {
        if (strcmp(cline_command_positional_at(command, index)->name, name) == 0)
        {
            return true;
        }
    }
    return false;
}

/*
 * Allocates a positional of command with a copy of its name. It is not put
 * in the array of the command. Returns NULL, with nothing left allocated,
 * when an allocation fails.
 */
static cline_positional_t *new_positional(cline_command_t *command, const char *name,
                                           const cline_type_t *type, cline_arity_t arity)
{
    cline_positional_t *positional = (cline_positional_t *)cline_malloc(sizeof(*positional));

    if (positional == NULL)
    {
        return NULL;
    }

    positional->command = command;
    positional->arity = arity;
    positional->type = type;
    positional->description = NULL;
    positional->default_value = NULL;

    positional->name = cline_strdup(name);
    if (positional->name == NULL)
    {
        cline_free(positional);
        return NULL;
    }

    return positional;
}

/*
 * Frees a positional that new_positional() returned and that is in no
 * array: it has no string but its name.
 */
static void free_new_positional(cline_positional_t *positional)
{
    cline_free(positional->name);
    cline_free(positional);
}

/* ---- Addition ----------------------------------------------------------- */

cline_error_t cline_command_add_positional(cline_command_t *command, const char *name,
                                             const cline_type_t *type, cline_arity_t arity,
                                             cline_positional_t **out)
{
    cline_positional_t *positional;

    assert(command != NULL);
    assert(name != NULL);
    assert(type != NULL);
    assert((arity == CLINE_ARITY_REQUIRED) || (arity == CLINE_ARITY_OPTIONAL) ||
           (arity == CLINE_ARITY_ZERO_OR_MORE) || (arity == CLINE_ARITY_ONE_OR_MORE));

    if (out != NULL)
    {
        *out = NULL;
    }

    /*
     * The checks come before every allocation, so a wrong definition
     * touches nothing. Their order is part of the contract.
     */
    if (name[0] == '\0')
    {
        return CLINE_ERROR_INVALID_NAME;
    }
    if (cline_command_subcommand_count(command) != 0)
    {
        return CLINE_ERROR_INVALID_DEFINITION;
    }
    if (name_is_in_use(command, name))
    {
        return CLINE_ERROR_NAME_CONFLICT;
    }

    positional = new_positional(command, name, type, arity);
    if (positional == NULL)
    {
        return CLINE_ERROR_OUT_OF_MEMORY;
    }

    /*
     * The tree is changed last: a push that fails leaves the array as it
     * was.
     */
    if (cline_vec_push(&command->positionals, &positional) != CLINE_ERROR_NONE)
    {
        free_new_positional(positional);
        return CLINE_ERROR_OUT_OF_MEMORY;
    }

    cline_schema_touch(command);

    if (out != NULL)
    {
        *out = positional;
    }
    return CLINE_ERROR_NONE;
}

/* ---- Attributes --------------------------------------------------------- */

/*
 * Sets an attribute that is a string: *slot, a member of positional, gets a
 * copy of text.
 */
static cline_error_t set_string(cline_positional_t *positional, char **slot, const char *text)
{
    cline_error_t error = cline_schema_replace_string(slot, text);

    if (error != CLINE_ERROR_NONE)
    {
        return error;
    }

    cline_schema_touch(positional->command);
    return CLINE_ERROR_NONE;
}

cline_error_t cline_positional_set_description(cline_positional_t *positional, const char *text)
{
    assert(positional != NULL);
    assert(text != NULL);

    return set_string(positional, &positional->description, text);
}

cline_error_t cline_positional_set_default(cline_positional_t *positional, const char *value)
{
    assert(positional != NULL);
    assert(value != NULL);

    /*
     * The check of the arity comes before the allocation, so a set on
     * another arity touches nothing.
     */
    if (positional->arity != CLINE_ARITY_OPTIONAL)
    {
        return CLINE_ERROR_INVALID_DEFINITION;
    }

    return set_string(positional, &positional->default_value, value);
}

/* ---- Reading ------------------------------------------------------------ */

const cline_command_t *cline_positional_command(const cline_positional_t *positional)
{
    assert(positional != NULL);

    return positional->command;
}

const char *cline_positional_name(const cline_positional_t *positional)
{
    assert(positional != NULL);

    return positional->name;
}

cline_arity_t cline_positional_arity(const cline_positional_t *positional)
{
    assert(positional != NULL);

    return positional->arity;
}

const cline_type_t *cline_positional_type(const cline_positional_t *positional)
{
    assert(positional != NULL);

    return positional->type;
}

const char *cline_positional_description(const cline_positional_t *positional)
{
    assert(positional != NULL);

    return positional->description;
}

const char *cline_positional_default(const cline_positional_t *positional)
{
    assert(positional != NULL);

    return positional->default_value;
}
