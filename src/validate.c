/*
 * CLArgs - validation: the checks of a schema that need the whole tree, and
 * whether the tree has been validated.
 *
 * The validation reads the tree through the reading functions of the
 * schema. The only thing it writes is the state of the root.
 */
#include "schema_internal.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

/* Where a fault is: the command it is in, and the option or the positional
 * that has it. One of option and positional is NULL. */
typedef struct fault {
    const clargs_command_t *command;
    const clargs_option_t *option;
    const clargs_positional_t *positional;
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
static const clargs_positional_t *find_positional_fault(const clargs_command_t *command)
{
    const size_t count = clargs_command_positional_count(command);
    bool after_optional = false;
    bool after_variadic = false;
    size_t index;

    for (index = 0; index < count; ++index) {
        const clargs_positional_t *positional = clargs_command_positional_at(command, index);
        const clargs_arity_t arity = clargs_positional_arity(positional);

        if (after_optional && (arity == CLARGS_ARITY_REQUIRED || arity == CLARGS_ARITY_ONE_OR_MORE)) {
            return positional;
        }
        if (after_variadic) {
            return positional;
        }

        if (arity == CLARGS_ARITY_OPTIONAL || arity == CLARGS_ARITY_ZERO_OR_MORE) {
            after_optional = true;
        }
        if (arity == CLARGS_ARITY_ZERO_OR_MORE || arity == CLARGS_ARITY_ONE_OR_MORE) {
            after_variadic = true;
        }
    }
    return NULL;
}

/* Returns the first option of command that is required and has a default
 * value, or NULL when there is none. */
static const clargs_option_t *find_option_fault(const clargs_command_t *command)
{
    const size_t count = clargs_command_option_count(command);
    size_t index;

    for (index = 0; index < count; ++index) {
        const clargs_option_t *option = clargs_command_option_at(command, index);

        if (clargs_option_is_required(option) && clargs_option_default(option) != NULL) {
            return option;
        }
    }
    return NULL;
}

/*
 * Checks command and then its subcommands, in the order of their
 * definition, each with everything below it. Stops at the first fault,
 * writes where it is to *fault and returns its error code. Returns
 * CLARGS_ERROR_NONE, with *fault not written, when there is no fault.
 *
 * The order of the checks is part of the contract of
 * clargs_command_validate(). The depth of the recursion is the depth of
 * the tree.
 */
static clargs_error_t check_command(const clargs_command_t *command, fault_t *fault)
{
    const clargs_positional_t *positional = find_positional_fault(command);
    const clargs_option_t *option;
    size_t count;
    size_t index;

    if (positional != NULL) {
        fault->command = command;
        fault->option = NULL;
        fault->positional = positional;
        return CLARGS_ERROR_INVALID_DEFINITION;
    }

    option = find_option_fault(command);
    if (option != NULL) {
        fault->command = command;
        fault->option = option;
        fault->positional = NULL;
        return CLARGS_ERROR_INVALID_DEFINITION;
    }

    count = clargs_command_subcommand_count(command);
    for (index = 0; index < count; ++index) {
        clargs_error_t error = check_command(clargs_command_subcommand_at(command, index), fault);

        if (error != CLARGS_ERROR_NONE) {
            return error;
        }
    }
    return CLARGS_ERROR_NONE;
}

clargs_error_t clargs_command_validate(clargs_command_t *root, const clargs_command_t **command,
                                       const clargs_option_t **option,
                                       const clargs_positional_t **positional)
{
    fault_t fault;
    clargs_error_t error;

    assert(root != NULL);
    assert(root->parent == NULL);

    fault.command = NULL;
    fault.option = NULL;
    fault.positional = NULL;

    error = check_command(root, &fault);

    /* The state changes only here: a validation that fails leaves it as it
     * was. */
    if (error == CLARGS_ERROR_NONE) {
        root->validated = true;
    }

    /* Every output that is given is written on every path. */
    if (command != NULL) {
        *command = fault.command;
    }
    if (option != NULL) {
        *option = fault.option;
    }
    if (positional != NULL) {
        *positional = fault.positional;
    }
    return error;
}

bool clargs_command_is_validated(const clargs_command_t *command)
{
    assert(command != NULL);

    /* The state is kept in the root. */
    while (command->parent != NULL) {
        command = command->parent;
    }
    return command->validated;
}
