/*
 * CLine - validation: the checks of a schema that need the whole tree, and
 * whether the tree has been validated.
 *
 * The validation reads the tree through the reading functions of the
 * schema. The only thing it writes is the state of the root. The only
 * memory it allocates is the area that a default value is converted to, one
 * at a time, and it frees each before it goes on.
 */
#include "schema_internal.h"

#include "alloc_internal.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * Where a fault is: the command it is in, and the option or the positional
 * that has it. One of option and positional is NULL.
 */
typedef struct fault
{
    const cline_command_t *command;
    const cline_option_t *option;
    const cline_positional_t *positional;
} fault_t;

/*
 * Returns the first positional of command that cannot follow the ones
 * before it, or NULL when the order has no fault. A positional is checked
 * against two rules, in this order:
 *
 * 1. It needs an argument (REQUIRED, ONE_OR_MORE), and one before it can
 *    do without (OPTIONAL, ZERO_OR_MORE).
 * 2. One before it takes any number of arguments (ZERO_OR_MORE,
 *    ONE_OR_MORE).
 */
static const cline_positional_t *find_positional_fault(const cline_command_t *command)
{
    const size_t count = cline_command_positional_count(command);
    bool after_optional = false;
    bool after_variadic = false;
    size_t index;

    for (index = 0; index < count; ++index)
    {
        const cline_positional_t *positional = cline_command_positional_at(command, index);
        const cline_arity_t arity = cline_positional_arity(positional);

        if (after_optional && ((arity == CLINE_ARITY_REQUIRED) || (arity == CLINE_ARITY_ONE_OR_MORE)))
        {
            return positional;
        }
        if (after_variadic)
        {
            return positional;
        }

        if ((arity == CLINE_ARITY_OPTIONAL) || (arity == CLINE_ARITY_ZERO_OR_MORE))
        {
            after_optional = true;
        }
        if ((arity == CLINE_ARITY_ZERO_OR_MORE) || (arity == CLINE_ARITY_ONE_OR_MORE))
        {
            after_variadic = true;
        }
    }
    return NULL;
}

/*
 * Checks that the value type takes text, a default value, by converting it
 * once. The value is written to an area of the size of the values of the
 * type, cleaned up if the conversion succeeded, and the area is freed: when
 * the function returns, nothing of the conversion is left.
 *
 * Returns CLINE_ERROR_NONE when the value type takes text,
 * CLINE_ERROR_OUT_OF_MEMORY when the area cannot be allocated or the
 * conversion returns that code, and CLINE_ERROR_INVALID_DEFAULT for every
 * other code of the conversion.
 */
static cline_error_t check_default(const cline_type_t *type, const char *text)
{
    /*
     * Memory of the allocator is aligned for an object of any type, as the
     * conversion needs it, and the size of a value is never 0.
     */
    void *value = cline_malloc(cline_type_value_size(type));
    cline_error_t error;

    if (value == NULL)
    {
        return CLINE_ERROR_OUT_OF_MEMORY;
    }

    error = cline_type_convert(type, text, value);
    if (error == CLINE_ERROR_NONE)
    {
        cline_type_release(type, value);
    }
    cline_free(value);

    if ((error == CLINE_ERROR_NONE) || (error == CLINE_ERROR_OUT_OF_MEMORY))
    {
        return error;
    }
    return CLINE_ERROR_INVALID_DEFAULT;
}

/*
 * Checks the options of command in the order of their definition, each
 * with two rules, in this order:
 *
 * 1. It is not both required and given a default value
 *    (CLINE_ERROR_INVALID_DEFINITION).
 * 2. Its value type takes its default value, if it has one
 *    (CLINE_ERROR_INVALID_DEFAULT).
 *
 * Stops at the first option that breaks a rule, writes it to *faulty and
 * returns the error code. Returns CLINE_ERROR_OUT_OF_MEMORY, with *faulty
 * not written, when a default value cannot be checked for a lack of memory,
 * and CLINE_ERROR_NONE, with *faulty not written, when no rule is broken.
 */
static cline_error_t check_options(const cline_command_t *command, const cline_option_t **faulty)
{
    const size_t count = cline_command_option_count(command);
    size_t index;

    for (index = 0; index < count; ++index)
    {
        const cline_option_t *option = cline_command_option_at(command, index);
        const char *text = cline_option_default(option);
        cline_error_t error;

        /* Only an option that takes a value has a default value. */
        if (text == NULL)
        {
            continue;
        }

        if (cline_option_is_required(option))
        {
            *faulty = option;
            return CLINE_ERROR_INVALID_DEFINITION;
        }

        error = check_default(cline_option_type(option), text);
        if (error == CLINE_ERROR_INVALID_DEFAULT)
        {
            *faulty = option;
        }
        if (error != CLINE_ERROR_NONE)
        {
            return error;
        }
    }
    return CLINE_ERROR_NONE;
}

/*
 * Checks, in the order of the definition, that the value type of every
 * positional of command that has a default value takes it. Stops at the
 * first that does not, writes it to *faulty and returns
 * CLINE_ERROR_INVALID_DEFAULT. Returns CLINE_ERROR_OUT_OF_MEMORY, with
 * *faulty not written, when a default value cannot be checked for a lack of
 * memory, and CLINE_ERROR_NONE, with *faulty not written, when every
 * default value is taken.
 */
static cline_error_t check_positional_defaults(const cline_command_t *command,
                                                const cline_positional_t **faulty)
{
    const size_t count = cline_command_positional_count(command);
    size_t index;

    for (index = 0; index < count; ++index)
    {
        const cline_positional_t *positional = cline_command_positional_at(command, index);
        const char *text = cline_positional_default(positional);
        cline_error_t error;

        if (text == NULL)
        {
            continue;
        }

        error = check_default(cline_positional_type(positional), text);
        if (error == CLINE_ERROR_INVALID_DEFAULT)
        {
            *faulty = positional;
        }
        if (error != CLINE_ERROR_NONE)
        {
            return error;
        }
    }
    return CLINE_ERROR_NONE;
}

/*
 * Checks command and then its subcommands, in the order of their
 * definition, each with everything below it. Within one command: the order
 * of the positionals, then the options, then the default values of the
 * positionals. Stops at the first fault, writes where it is to *fault and
 * returns its error code. Returns CLINE_ERROR_NONE, with *fault not
 * written, when there is no fault, and CLINE_ERROR_OUT_OF_MEMORY, with
 * *fault not written, when a check fails for a lack of memory.
 *
 * The order of the checks is part of the contract of
 * cline_command_validate(). The depth of the recursion is the depth of
 * the tree.
 */
static cline_error_t check_command(const cline_command_t *command, fault_t *fault)
{
    const cline_positional_t *positional = find_positional_fault(command);
    const cline_option_t *option = NULL;
    cline_error_t error;
    size_t count;
    size_t index;

    if (positional != NULL)
    {
        fault->command = command;
        fault->option = NULL;
        fault->positional = positional;
        return CLINE_ERROR_INVALID_DEFINITION;
    }

    error = check_options(command, &option);
    if (option != NULL)
    {
        fault->command = command;
        fault->option = option;
        fault->positional = NULL;
    }
    if (error != CLINE_ERROR_NONE)
    {
        return error;
    }

    error = check_positional_defaults(command, &positional);
    if (positional != NULL)
    {
        fault->command = command;
        fault->option = NULL;
        fault->positional = positional;
    }
    if (error != CLINE_ERROR_NONE)
    {
        return error;
    }

    count = cline_command_subcommand_count(command);
    for (index = 0; index < count; ++index)
    {
        error = check_command(cline_command_subcommand_at(command, index), fault);
        if (error != CLINE_ERROR_NONE)
        {
            return error;
        }
    }
    return CLINE_ERROR_NONE;
}

cline_error_t cline_command_validate(cline_command_t *root, const cline_command_t **command,
                                       const cline_option_t **option,
                                       const cline_positional_t **positional)
{
    fault_t fault;
    cline_error_t error;

    assert(root != NULL);
    assert(root->parent == NULL);

    fault.command = NULL;
    fault.option = NULL;
    fault.positional = NULL;

    error = check_command(root, &fault);

    /*
     * The state changes only here: a validation that fails leaves it as it
     * was.
     */
    if (error == CLINE_ERROR_NONE)
    {
        root->validated = true;
    }

    /* Every output that is given is written on every path. */
    if (command != NULL)
    {
        *command = fault.command;
    }
    if (option != NULL)
    {
        *option = fault.option;
    }
    if (positional != NULL)
    {
        *positional = fault.positional;
    }
    return error;
}

bool cline_command_is_validated(const cline_command_t *command)
{
    assert(command != NULL);

    /* The state is kept in the root. */
    while (command->parent != NULL)
    {
        command = command->parent;
    }
    return command->validated;
}
