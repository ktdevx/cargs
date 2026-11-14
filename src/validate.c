/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - validation: the schema checks that need the whole tree, and
 * whether the tree has been validated.
 *
 * Validation reads the tree through the schema's reading functions. The
 * only thing it writes is the root's state. The only memory it allocates
 * is the area a default value is converted into, one at a time, and it
 * frees each area before moving on.
 */
#include "schema_internal.h"

#include "alloc_internal.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * The location of a fault: the command it is in, and the option or
 * positional that has it. Either option or positional is NULL.
 */
typedef struct fault
{
    const argx_command_t *command;
    const argx_option_t *option;
    const argx_positional_t *positional;
} fault_t;

/*
 * Returns the first positional of command that cannot follow the earlier
 * ones, or NULL when the order has no fault. Each positional is checked
 * against two rules, in this order:
 *
 * 1. It needs an argument (REQUIRED, ONE_OR_MORE), and an earlier one can
 *    do without (OPTIONAL, ZERO_OR_MORE).
 * 2. An earlier one takes any number of arguments (ZERO_OR_MORE,
 *    ONE_OR_MORE).
 */
static const argx_positional_t *find_positional_fault(const argx_command_t *command)
{
    const size_t count = argx_command_positional_count(command);
    bool after_optional = false;
    bool after_variadic = false;
    size_t index;

    for (index = 0; index < count; ++index)
    {
        const argx_positional_t *positional = argx_command_positional_at(command, index);
        const argx_arity_t arity = argx_positional_arity(positional);

        if (after_optional && ((arity == ARGX_ARITY_REQUIRED) || (arity == ARGX_ARITY_ONE_OR_MORE)))
        {
            return positional;
        }
        if (after_variadic)
        {
            return positional;
        }

        if ((arity == ARGX_ARITY_OPTIONAL) || (arity == ARGX_ARITY_ZERO_OR_MORE))
        {
            after_optional = true;
        }
        if ((arity == ARGX_ARITY_ZERO_OR_MORE) || (arity == ARGX_ARITY_ONE_OR_MORE))
        {
            after_variadic = true;
        }
    }
    return NULL;
}

/*
 * Checks that the value type accepts text, a default value, by converting
 * it once. The value is written to an area the size of one value of the
 * type and cleaned up if the conversion succeeded, and then the area is
 * freed. When the function returns, nothing of the conversion is left.
 *
 * Returns ARGX_ERROR_NONE when the value type accepts text,
 * ARGX_ERROR_OUT_OF_MEMORY when the area cannot be allocated or the
 * conversion returns that code, and ARGX_ERROR_INVALID_DEFAULT for any
 * other code the conversion returns.
 */
static argx_error_t check_default(const argx_type_t *type, const char *text)
{
    /*
     * Memory from the allocator is aligned for an object of any type, as
     * the conversion needs, and the value size is never 0.
     */
    void *value = argx_malloc(argx_type_value_size(type));
    argx_error_t error;

    if (value == NULL)
    {
        return ARGX_ERROR_OUT_OF_MEMORY;
    }

    error = argx_type_convert(type, text, value);
    if (error == ARGX_ERROR_NONE)
    {
        argx_type_release(type, value);
    }
    argx_free(value);

    if ((error == ARGX_ERROR_NONE) || (error == ARGX_ERROR_OUT_OF_MEMORY))
    {
        return error;
    }
    return ARGX_ERROR_INVALID_DEFAULT;
}

/*
 * Checks the options of command in definition order, each against two
 * rules, in this order:
 *
 * 1. It is not both required and given a default value
 *    (ARGX_ERROR_INVALID_DEFINITION).
 * 2. Its value type accepts its default value, if it has one
 *    (ARGX_ERROR_INVALID_DEFAULT).
 *
 * Stops at the first option that breaks a rule, writes it to *faulty and
 * returns the error code. Returns ARGX_ERROR_OUT_OF_MEMORY, without
 * writing *faulty, when a default value cannot be checked for lack of
 * memory. Returns ARGX_ERROR_NONE, without writing *faulty, when no rule
 * is broken.
 */
static argx_error_t check_options(const argx_command_t *command, const argx_option_t **faulty)
{
    const size_t count = argx_command_option_count(command);
    size_t index;

    for (index = 0; index < count; ++index)
    {
        const argx_option_t *option = argx_command_option_at(command, index);
        const char *text = argx_option_default(option);
        argx_error_t error;

        /* Only an option that takes a value has a default value. */
        if (text == NULL)
        {
            continue;
        }

        if (argx_option_is_required(option))
        {
            *faulty = option;
            return ARGX_ERROR_INVALID_DEFINITION;
        }

        error = check_default(argx_option_type(option), text);
        if (error == ARGX_ERROR_INVALID_DEFAULT)
        {
            *faulty = option;
        }
        if (error != ARGX_ERROR_NONE)
        {
            return error;
        }
    }
    return ARGX_ERROR_NONE;
}

/*
 * For every positional of command that has a default value, checks in
 * definition order that its value type accepts that value. Stops at the
 * first one that does not, writes it to *faulty and returns
 * ARGX_ERROR_INVALID_DEFAULT. Returns ARGX_ERROR_OUT_OF_MEMORY, without
 * writing *faulty, when a default value cannot be checked for lack of
 * memory. Returns ARGX_ERROR_NONE, without writing *faulty, when every
 * default value is accepted.
 */
static argx_error_t check_positional_defaults(const argx_command_t *command,
                                              const argx_positional_t **faulty)
{
    const size_t count = argx_command_positional_count(command);
    size_t index;

    for (index = 0; index < count; ++index)
    {
        const argx_positional_t *positional = argx_command_positional_at(command, index);
        const char *text = argx_positional_default(positional);
        argx_error_t error;

        if (text == NULL)
        {
            continue;
        }

        error = check_default(argx_positional_type(positional), text);
        if (error == ARGX_ERROR_INVALID_DEFAULT)
        {
            *faulty = positional;
        }
        if (error != ARGX_ERROR_NONE)
        {
            return error;
        }
    }
    return ARGX_ERROR_NONE;
}

/*
 * Checks command and then its subcommands in definition order, each
 * together with everything below it. Within one command, it checks the
 * positional order, then the options, then the positional default values.
 * Stops at the first fault, writes its location to *fault and returns its
 * error code. Returns ARGX_ERROR_NONE, without writing *fault, when there
 * is no fault. Returns ARGX_ERROR_OUT_OF_MEMORY, without writing *fault,
 * when a check fails for lack of memory.
 *
 * The check order is part of the argx_command_validate() contract. The
 * recursion depth equals the tree depth.
 */
static argx_error_t check_command(const argx_command_t *command, fault_t *fault)
{
    const argx_positional_t *positional = find_positional_fault(command);
    const argx_option_t *option = NULL;
    argx_error_t error;
    size_t count;
    size_t index;

    if (positional != NULL)
    {
        fault->command = command;
        fault->option = NULL;
        fault->positional = positional;
        return ARGX_ERROR_INVALID_DEFINITION;
    }

    error = check_options(command, &option);
    if (option != NULL)
    {
        fault->command = command;
        fault->option = option;
        fault->positional = NULL;
    }
    if (error != ARGX_ERROR_NONE)
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
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    count = argx_command_subcommand_count(command);
    for (index = 0; index < count; ++index)
    {
        error = check_command(argx_command_subcommand_at(command, index), fault);
        if (error != ARGX_ERROR_NONE)
        {
            return error;
        }
    }
    return ARGX_ERROR_NONE;
}

argx_error_t argx_command_validate(argx_command_t *root, const argx_command_t **command,
                                   const argx_option_t **option,
                                   const argx_positional_t **positional)
{
    fault_t fault;
    argx_error_t error;

    assert(root != NULL);
    assert(root->parent == NULL);

    fault.command = NULL;
    fault.option = NULL;
    fault.positional = NULL;

    error = check_command(root, &fault);

    /*
     * The state changes only here, so a failed validation leaves it
     * unchanged.
     */
    if (error == ARGX_ERROR_NONE)
    {
        root->validated = true;
    }

    /* Every output pointer given is written on every path. */
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

bool argx_command_is_validated(const argx_command_t *command)
{
    assert(command != NULL);

    /* The state is kept in the root. */
    while (command->parent != NULL)
    {
        command = command->parent;
    }
    return command->validated;
}
