/*
 * CLine - commands: the tree of the schema, and the helper functions that
 * the sources of the schema share.
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

/* The pointers that the three arrays of a command hold. */
static cline_option_t *option_at(const cline_command_t *command, size_t index)
{
    return *(cline_option_t **)cline_vec_at(&command->options, index);
}

static cline_positional_t *positional_at(const cline_command_t *command, size_t index)
{
    return *(cline_positional_t **)cline_vec_at(&command->positionals, index);
}

static cline_command_t *subcommand_at(const cline_command_t *command, size_t index)
{
    return *(cline_command_t **)cline_vec_at(&command->subcommands, index);
}

/* Whether the string name is exactly the length bytes at text. */
static bool name_equals(const char *name, const char *text, size_t length)
{
    return (strlen(name) == length) && (memcmp(name, text, length) == 0);
}

/* Frees an option and its strings. The value type is borrowed and stays. */
static void free_option(cline_option_t *option)
{
    cline_free(option->long_name);
    cline_free(option->negated_name);
    cline_free(option->description);
    cline_free(option->value_name);
    cline_free(option->default_value);
    cline_free(option);
}

/*
 * Frees a positional and its strings. The value type is borrowed and
 * stays.
 */
static void free_positional(cline_positional_t *positional)
{
    cline_free(positional->name);
    cline_free(positional->description);
    cline_free(positional->default_value);
    cline_free(positional);
}

/*
 * Frees command and everything below it. The depth of the recursion is the
 * depth of the tree.
 */
static void free_command(cline_command_t *command)
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

    cline_vec_destroy(&command->subcommands);
    cline_vec_destroy(&command->options);
    cline_vec_destroy(&command->positionals);
    cline_free(command->description);
    cline_free(command->name);
    cline_free(command);
}

/*
 * Allocates an empty command with a copy of name and the parent parent
 * (NULL for a root). It is not put in the array of the parent. Returns
 * NULL, with nothing left allocated, when an allocation fails.
 */
static cline_command_t *new_command(const char *name, cline_command_t *parent)
{
    cline_command_t *command = (cline_command_t *)cline_malloc(sizeof(*command));

    if (command == NULL)
    {
        return NULL;
    }

    command->name = cline_strdup(name);
    if (command->name == NULL)
    {
        cline_free(command);
        return NULL;
    }

    command->description = NULL;
    command->parent = parent;
    cline_vec_init(&command->options, sizeof(cline_option_t *));
    cline_vec_init(&command->positionals, sizeof(cline_positional_t *));
    cline_vec_init(&command->subcommands, sizeof(cline_command_t *));
    command->validated = false;
    return command;
}

/* ---- Shared helper functions -------------------------------------------- */

cline_command_t *cline_schema_root(cline_command_t *command)
{
    assert(command != NULL);

    while (command->parent != NULL)
    {
        command = command->parent;
    }
    return command;
}

void cline_schema_touch(cline_command_t *command)
{
    assert(command != NULL);

    cline_schema_root(command)->validated = false;
}

cline_error_t cline_schema_replace_string(char **slot, const char *text)
{
    char *copy;

    assert(slot != NULL);
    assert(text != NULL);

    copy = cline_strdup(text);
    if (copy == NULL)
    {
        return CLINE_ERROR_OUT_OF_MEMORY;
    }

    cline_free(*slot);
    *slot = copy;
    return CLINE_ERROR_NONE;
}

/* ---- Creation, destruction and changes ---------------------------------- */

cline_error_t cline_command_create(const char *name, cline_command_t **out)
{
    cline_command_t *root;

    assert(name != NULL);
    assert(out != NULL);

    *out = NULL;

    if (name[0] == '\0')
    {
        return CLINE_ERROR_INVALID_NAME;
    }

    root = new_command(name, NULL);
    if (root == NULL)
    {
        return CLINE_ERROR_OUT_OF_MEMORY;
    }

    *out = root;
    return CLINE_ERROR_NONE;
}

void cline_command_destroy(cline_command_t *root)
{
    if (root == NULL)
    {
        return;
    }

    assert(root->parent == NULL);

    free_command(root);
}

cline_error_t cline_command_add_subcommand(cline_command_t *command, const char *name,
                                             cline_command_t **out)
{
    cline_command_t *subcommand;

    assert(command != NULL);
    assert(name != NULL);

    if (out != NULL)
    {
        *out = NULL;
    }

    /*
     * The checks come before every allocation, so a wrong definition
     * touches nothing.
     */
    if (!cline_name_is_subcommand(name))
    {
        return CLINE_ERROR_INVALID_NAME;
    }
    if (command->positionals.count != 0)
    {
        return CLINE_ERROR_INVALID_DEFINITION;
    }
    if (cline_command_find_subcommand(command, name) != NULL)
    {
        return CLINE_ERROR_NAME_CONFLICT;
    }

    subcommand = new_command(name, command);
    if (subcommand == NULL)
    {
        return CLINE_ERROR_OUT_OF_MEMORY;
    }

    /*
     * The tree is changed last: a push that fails leaves the array as it
     * was.
     */
    if (cline_vec_push(&command->subcommands, &subcommand) != CLINE_ERROR_NONE)
    {
        free_command(subcommand);
        return CLINE_ERROR_OUT_OF_MEMORY;
    }

    cline_schema_touch(command);

    if (out != NULL)
    {
        *out = subcommand;
    }
    return CLINE_ERROR_NONE;
}

cline_error_t cline_command_set_description(cline_command_t *command, const char *text)
{
    cline_error_t error;

    assert(command != NULL);
    assert(text != NULL);

    error = cline_schema_replace_string(&command->description, text);
    if (error != CLINE_ERROR_NONE)
    {
        return error;
    }

    cline_schema_touch(command);
    return CLINE_ERROR_NONE;
}

/* ---- Reading ------------------------------------------------------------ */

const char *cline_command_name(const cline_command_t *command)
{
    assert(command != NULL);

    return command->name;
}

const char *cline_command_description(const cline_command_t *command)
{
    assert(command != NULL);

    return command->description;
}

const cline_command_t *cline_command_parent(const cline_command_t *command)
{
    assert(command != NULL);

    return command->parent;
}

size_t cline_command_option_count(const cline_command_t *command)
{
    assert(command != NULL);

    return command->options.count;
}

const cline_option_t *cline_command_option_at(const cline_command_t *command, size_t index)
{
    assert(command != NULL);
    assert(index < command->options.count);

    return option_at(command, index);
}

size_t cline_command_positional_count(const cline_command_t *command)
{
    assert(command != NULL);

    return command->positionals.count;
}

const cline_positional_t *cline_command_positional_at(const cline_command_t *command, size_t index)
{
    assert(command != NULL);
    assert(index < command->positionals.count);

    return positional_at(command, index);
}

size_t cline_command_subcommand_count(const cline_command_t *command)
{
    assert(command != NULL);

    return command->subcommands.count;
}

const cline_command_t *cline_command_subcommand_at(const cline_command_t *command, size_t index)
{
    assert(command != NULL);
    assert(index < command->subcommands.count);

    return subcommand_at(command, index);
}

/* ---- Lookup ------------------------------------------------------------- */

const cline_option_t *cline_command_find_short(const cline_command_t *command, char name)
{
    size_t index;

    assert(command != NULL);

    /*
     * '\0' is how an option without a short name is stored; it is not a
     * name.
     */
    if (name == '\0')
    {
        return NULL;
    }

    for (index = 0; index < command->options.count; ++index)
    {
        const cline_option_t *option = option_at(command, index);

        if (option->short_name == name)
        {
            return option;
        }
    }
    return NULL;
}

const cline_option_t *cline_command_find_long(const cline_command_t *command, const char *name,
                                                size_t length, bool *negated)
{
    size_t index;

    assert(command != NULL);
    assert(name != NULL);

    for (index = 0; index < command->options.count; ++index)
    {
        const cline_option_t *option = option_at(command, index);
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

const cline_command_t *cline_command_find_subcommand(const cline_command_t *command, const char *name)
{
    size_t index;

    assert(command != NULL);
    assert(name != NULL);

    for (index = 0; index < command->subcommands.count; ++index)
    {
        const cline_command_t *subcommand = subcommand_at(command, index);

        if (strcmp(subcommand->name, name) == 0)
        {
            return subcommand;
        }
    }
    return NULL;
}
