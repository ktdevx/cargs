/*
 * CLArgs - commands: the tree of the schema, and the helper functions that
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
static clargs_option_t *option_at(const clargs_command_t *command, size_t index)
{
    return *(clargs_option_t **)clargs_vec_at(&command->options, index);
}

static clargs_positional_t *positional_at(const clargs_command_t *command, size_t index)
{
    return *(clargs_positional_t **)clargs_vec_at(&command->positionals, index);
}

static clargs_command_t *subcommand_at(const clargs_command_t *command, size_t index)
{
    return *(clargs_command_t **)clargs_vec_at(&command->subcommands, index);
}

/* Whether the string name is exactly the length bytes at text. */
static bool name_equals(const char *name, const char *text, size_t length)
{
    return (strlen(name) == length) && (memcmp(name, text, length) == 0);
}

/* Frees an option and its strings. The value type is borrowed and stays. */
static void free_option(clargs_option_t *option)
{
    clargs_free(option->long_name);
    clargs_free(option->negated_name);
    clargs_free(option->description);
    clargs_free(option->value_name);
    clargs_free(option->default_value);
    clargs_free(option);
}

/*
 * Frees a positional and its strings. The value type is borrowed and
 * stays.
 */
static void free_positional(clargs_positional_t *positional)
{
    clargs_free(positional->name);
    clargs_free(positional->description);
    clargs_free(positional->default_value);
    clargs_free(positional);
}

/*
 * Frees command and everything below it. The depth of the recursion is the
 * depth of the tree.
 */
static void free_command(clargs_command_t *command)
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

    clargs_vec_destroy(&command->subcommands);
    clargs_vec_destroy(&command->options);
    clargs_vec_destroy(&command->positionals);
    clargs_free(command->description);
    clargs_free(command->name);
    clargs_free(command);
}

/*
 * Allocates an empty command with a copy of name and the parent parent
 * (NULL for a root). It is not put in the array of the parent. Returns
 * NULL, with nothing left allocated, when an allocation fails.
 */
static clargs_command_t *new_command(const char *name, clargs_command_t *parent)
{
    clargs_command_t *command = (clargs_command_t *)clargs_malloc(sizeof(*command));

    if (command == NULL)
    {
        return NULL;
    }

    command->name = clargs_strdup(name);
    if (command->name == NULL)
    {
        clargs_free(command);
        return NULL;
    }

    command->description = NULL;
    command->parent = parent;
    clargs_vec_init(&command->options, sizeof(clargs_option_t *));
    clargs_vec_init(&command->positionals, sizeof(clargs_positional_t *));
    clargs_vec_init(&command->subcommands, sizeof(clargs_command_t *));
    command->validated = false;
    return command;
}

/* ---- Shared helper functions -------------------------------------------- */

clargs_command_t *clargs_schema_root(clargs_command_t *command)
{
    assert(command != NULL);

    while (command->parent != NULL)
    {
        command = command->parent;
    }
    return command;
}

void clargs_schema_touch(clargs_command_t *command)
{
    assert(command != NULL);

    clargs_schema_root(command)->validated = false;
}

clargs_error_t clargs_schema_replace_string(char **slot, const char *text)
{
    char *copy;

    assert(slot != NULL);
    assert(text != NULL);

    copy = clargs_strdup(text);
    if (copy == NULL)
    {
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }

    clargs_free(*slot);
    *slot = copy;
    return CLARGS_ERROR_NONE;
}

/* ---- Creation, destruction and changes ---------------------------------- */

clargs_error_t clargs_command_create(const char *name, clargs_command_t **out)
{
    clargs_command_t *root;

    assert(name != NULL);
    assert(out != NULL);

    *out = NULL;

    if (name[0] == '\0')
    {
        return CLARGS_ERROR_INVALID_NAME;
    }

    root = new_command(name, NULL);
    if (root == NULL)
    {
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }

    *out = root;
    return CLARGS_ERROR_NONE;
}

void clargs_command_destroy(clargs_command_t *root)
{
    if (root == NULL)
    {
        return;
    }

    assert(root->parent == NULL);

    free_command(root);
}

clargs_error_t clargs_command_add_subcommand(clargs_command_t *command, const char *name,
                                             clargs_command_t **out)
{
    clargs_command_t *subcommand;

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
    if (!clargs_name_is_subcommand(name))
    {
        return CLARGS_ERROR_INVALID_NAME;
    }
    if (command->positionals.count != 0)
    {
        return CLARGS_ERROR_INVALID_DEFINITION;
    }
    if (clargs_command_find_subcommand(command, name) != NULL)
    {
        return CLARGS_ERROR_NAME_CONFLICT;
    }

    subcommand = new_command(name, command);
    if (subcommand == NULL)
    {
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }

    /*
     * The tree is changed last: a push that fails leaves the array as it
     * was.
     */
    if (clargs_vec_push(&command->subcommands, &subcommand) != CLARGS_ERROR_NONE)
    {
        free_command(subcommand);
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }

    clargs_schema_touch(command);

    if (out != NULL)
    {
        *out = subcommand;
    }
    return CLARGS_ERROR_NONE;
}

clargs_error_t clargs_command_set_description(clargs_command_t *command, const char *text)
{
    clargs_error_t error;

    assert(command != NULL);
    assert(text != NULL);

    error = clargs_schema_replace_string(&command->description, text);
    if (error != CLARGS_ERROR_NONE)
    {
        return error;
    }

    clargs_schema_touch(command);
    return CLARGS_ERROR_NONE;
}

/* ---- Reading ------------------------------------------------------------ */

const char *clargs_command_name(const clargs_command_t *command)
{
    assert(command != NULL);

    return command->name;
}

const char *clargs_command_description(const clargs_command_t *command)
{
    assert(command != NULL);

    return command->description;
}

const clargs_command_t *clargs_command_parent(const clargs_command_t *command)
{
    assert(command != NULL);

    return command->parent;
}

size_t clargs_command_option_count(const clargs_command_t *command)
{
    assert(command != NULL);

    return command->options.count;
}

const clargs_option_t *clargs_command_option_at(const clargs_command_t *command, size_t index)
{
    assert(command != NULL);
    assert(index < command->options.count);

    return option_at(command, index);
}

size_t clargs_command_positional_count(const clargs_command_t *command)
{
    assert(command != NULL);

    return command->positionals.count;
}

const clargs_positional_t *clargs_command_positional_at(const clargs_command_t *command, size_t index)
{
    assert(command != NULL);
    assert(index < command->positionals.count);

    return positional_at(command, index);
}

size_t clargs_command_subcommand_count(const clargs_command_t *command)
{
    assert(command != NULL);

    return command->subcommands.count;
}

const clargs_command_t *clargs_command_subcommand_at(const clargs_command_t *command, size_t index)
{
    assert(command != NULL);
    assert(index < command->subcommands.count);

    return subcommand_at(command, index);
}

/* ---- Lookup ------------------------------------------------------------- */

const clargs_option_t *clargs_command_find_short(const clargs_command_t *command, char name)
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
        const clargs_option_t *option = option_at(command, index);

        if (option->short_name == name)
        {
            return option;
        }
    }
    return NULL;
}

const clargs_option_t *clargs_command_find_long(const clargs_command_t *command, const char *name,
                                                size_t length, bool *negated)
{
    size_t index;

    assert(command != NULL);
    assert(name != NULL);

    for (index = 0; index < command->options.count; ++index)
    {
        const clargs_option_t *option = option_at(command, index);
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

const clargs_command_t *clargs_command_find_subcommand(const clargs_command_t *command, const char *name)
{
    size_t index;

    assert(command != NULL);
    assert(name != NULL);

    for (index = 0; index < command->subcommands.count; ++index)
    {
        const clargs_command_t *subcommand = subcommand_at(command, index);

        if (strcmp(subcommand->name, name) == 0)
        {
            return subcommand;
        }
    }
    return NULL;
}
