/*
 * CLArgs - positionals: the addition of a positional to a command, the
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

/* Whether command has a positional with the name name. The whole name is
 * compared, byte by byte. */
static bool name_is_in_use(const clargs_command_t *command, const char *name)
{
    size_t count = clargs_command_positional_count(command);
    size_t index;

    for (index = 0; index < count; ++index) {
        if (strcmp(clargs_command_positional_at(command, index)->name, name) == 0) {
            return true;
        }
    }
    return false;
}

/* Allocates a positional of command with a copy of its name. It is not put
 * in the array of the command. Returns NULL, with nothing left allocated,
 * when an allocation fails. */
static clargs_positional_t *new_positional(clargs_command_t *command, const char *name,
                                           const clargs_type_t *type, clargs_arity_t arity)
{
    clargs_positional_t *positional = (clargs_positional_t *)clargs_malloc(sizeof(*positional));

    if (positional == NULL) {
        return NULL;
    }

    positional->command = command;
    positional->arity = arity;
    positional->type = type;
    positional->description = NULL;
    positional->default_value = NULL;

    positional->name = clargs_strdup(name);
    if (positional->name == NULL) {
        clargs_free(positional);
        return NULL;
    }

    return positional;
}

/* Frees a positional that new_positional() returned and that is in no
 * array: it has no string but its name. */
static void free_new_positional(clargs_positional_t *positional)
{
    clargs_free(positional->name);
    clargs_free(positional);
}

/* ---- Addition ----------------------------------------------------------- */

clargs_error_t clargs_command_add_positional(clargs_command_t *command, const char *name,
                                             const clargs_type_t *type, clargs_arity_t arity,
                                             clargs_positional_t **out)
{
    clargs_positional_t *positional;

    assert(command != NULL);
    assert(name != NULL);
    assert(type != NULL);
    assert(arity == CLARGS_ARITY_REQUIRED || arity == CLARGS_ARITY_OPTIONAL ||
           arity == CLARGS_ARITY_ZERO_OR_MORE || arity == CLARGS_ARITY_ONE_OR_MORE);

    if (out != NULL) {
        *out = NULL;
    }

    /* The checks come before every allocation, so a wrong definition
     * touches nothing. Their order is part of the contract. */
    if (name[0] == '\0') {
        return CLARGS_ERROR_INVALID_NAME;
    }
    if (clargs_command_subcommand_count(command) != 0) {
        return CLARGS_ERROR_INVALID_DEFINITION;
    }
    if (name_is_in_use(command, name)) {
        return CLARGS_ERROR_NAME_CONFLICT;
    }

    positional = new_positional(command, name, type, arity);
    if (positional == NULL) {
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }

    /* The tree is changed last: a push that fails leaves the array as it
     * was. */
    if (clargs_vec_push(&command->positionals, &positional) != CLARGS_ERROR_NONE) {
        free_new_positional(positional);
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }

    clargs_schema_touch(command);

    if (out != NULL) {
        *out = positional;
    }
    return CLARGS_ERROR_NONE;
}

/* ---- Attributes --------------------------------------------------------- */

/* Sets an attribute that is a string: *slot, a member of positional, gets a
 * copy of text. */
static clargs_error_t set_string(clargs_positional_t *positional, char **slot, const char *text)
{
    clargs_error_t error = clargs_schema_replace_string(slot, text);

    if (error != CLARGS_ERROR_NONE) {
        return error;
    }

    clargs_schema_touch(positional->command);
    return CLARGS_ERROR_NONE;
}

clargs_error_t clargs_positional_set_description(clargs_positional_t *positional, const char *text)
{
    assert(positional != NULL);
    assert(text != NULL);

    return set_string(positional, &positional->description, text);
}

clargs_error_t clargs_positional_set_default(clargs_positional_t *positional, const char *value)
{
    assert(positional != NULL);
    assert(value != NULL);

    /* The check of the arity comes before the allocation, so a set on
     * another arity touches nothing. */
    if (positional->arity != CLARGS_ARITY_OPTIONAL) {
        return CLARGS_ERROR_INVALID_DEFINITION;
    }

    return set_string(positional, &positional->default_value, value);
}

/* ---- Reading ------------------------------------------------------------ */

const clargs_command_t *clargs_positional_command(const clargs_positional_t *positional)
{
    assert(positional != NULL);

    return positional->command;
}

const char *clargs_positional_name(const clargs_positional_t *positional)
{
    assert(positional != NULL);

    return positional->name;
}

clargs_arity_t clargs_positional_arity(const clargs_positional_t *positional)
{
    assert(positional != NULL);

    return positional->arity;
}

const clargs_type_t *clargs_positional_type(const clargs_positional_t *positional)
{
    assert(positional != NULL);

    return positional->type;
}

const char *clargs_positional_description(const clargs_positional_t *positional)
{
    assert(positional != NULL);

    return positional->description;
}

const char *clargs_positional_default(const clargs_positional_t *positional)
{
    assert(positional != NULL);

    return positional->default_value;
}
