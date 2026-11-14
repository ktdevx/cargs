/*
 * CLArgs - validation: the checks of a schema that need the whole tree, and
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

/*
 * Checks that the value type takes text, a default value, by converting it
 * once. The value is written to an area of the size of the values of the
 * type, cleaned up if the conversion succeeded, and the area is freed: when
 * the function returns, nothing of the conversion is left.
 *
 * Returns CLARGS_ERROR_NONE when the value type takes text,
 * CLARGS_ERROR_OUT_OF_MEMORY when the area cannot be allocated or the
 * conversion returns that code, and CLARGS_ERROR_INVALID_DEFAULT for every
 * other code of the conversion.
 */
static clargs_error_t check_default(const clargs_type_t *type, const char *text)
{
    /*
     * Memory of the allocator is aligned for an object of any type, as the
     * conversion needs it, and the size of a value is never 0.
     */
    void *value = clargs_malloc(clargs_type_value_size(type));
    clargs_error_t error;

    if (value == NULL) {
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }

    error = clargs_type_convert(type, text, value);
    if (error == CLARGS_ERROR_NONE) {
        clargs_type_release(type, value);
    }
    clargs_free(value);

    if (error == CLARGS_ERROR_NONE || error == CLARGS_ERROR_OUT_OF_MEMORY) {
        return error;
    }
    return CLARGS_ERROR_INVALID_DEFAULT;
}

/*
 * Checks the options of command in the order of their definition, each
 * with two rules, in this order:
 *
 * 1. It is not both required and given a default value
 *    (CLARGS_ERROR_INVALID_DEFINITION).
 * 2. Its value type takes its default value, if it has one
 *    (CLARGS_ERROR_INVALID_DEFAULT).
 *
 * Stops at the first option that breaks a rule, writes it to *faulty and
 * returns the error code. Returns CLARGS_ERROR_OUT_OF_MEMORY, with *faulty
 * not written, when a default value cannot be checked for a lack of memory,
 * and CLARGS_ERROR_NONE, with *faulty not written, when no rule is broken.
 */
static clargs_error_t check_options(const clargs_command_t *command, const clargs_option_t **faulty)
{
    const size_t count = clargs_command_option_count(command);
    size_t index;

    for (index = 0; index < count; ++index) {
        const clargs_option_t *option = clargs_command_option_at(command, index);
        const char *text = clargs_option_default(option);
        clargs_error_t error;

        /* Only an option that takes a value has a default value. */
        if (text == NULL) {
            continue;
        }

        if (clargs_option_is_required(option)) {
            *faulty = option;
            return CLARGS_ERROR_INVALID_DEFINITION;
        }

        error = check_default(clargs_option_type(option), text);
        if (error == CLARGS_ERROR_INVALID_DEFAULT) {
            *faulty = option;
        }
        if (error != CLARGS_ERROR_NONE) {
            return error;
        }
    }
    return CLARGS_ERROR_NONE;
}

/*
 * Checks, in the order of the definition, that the value type of every
 * positional of command that has a default value takes it. Stops at the
 * first that does not, writes it to *faulty and returns
 * CLARGS_ERROR_INVALID_DEFAULT. Returns CLARGS_ERROR_OUT_OF_MEMORY, with
 * *faulty not written, when a default value cannot be checked for a lack of
 * memory, and CLARGS_ERROR_NONE, with *faulty not written, when every
 * default value is taken.
 */
static clargs_error_t check_positional_defaults(const clargs_command_t *command,
                                                const clargs_positional_t **faulty)
{
    const size_t count = clargs_command_positional_count(command);
    size_t index;

    for (index = 0; index < count; ++index) {
        const clargs_positional_t *positional = clargs_command_positional_at(command, index);
        const char *text = clargs_positional_default(positional);
        clargs_error_t error;

        if (text == NULL) {
            continue;
        }

        error = check_default(clargs_positional_type(positional), text);
        if (error == CLARGS_ERROR_INVALID_DEFAULT) {
            *faulty = positional;
        }
        if (error != CLARGS_ERROR_NONE) {
            return error;
        }
    }
    return CLARGS_ERROR_NONE;
}

/*
 * Checks command and then its subcommands, in the order of their
 * definition, each with everything below it. Within one command: the order
 * of the positionals, then the options, then the default values of the
 * positionals. Stops at the first fault, writes where it is to *fault and
 * returns its error code. Returns CLARGS_ERROR_NONE, with *fault not
 * written, when there is no fault, and CLARGS_ERROR_OUT_OF_MEMORY, with
 * *fault not written, when a check fails for a lack of memory.
 *
 * The order of the checks is part of the contract of
 * clargs_command_validate(). The depth of the recursion is the depth of
 * the tree.
 */
static clargs_error_t check_command(const clargs_command_t *command, fault_t *fault)
{
    const clargs_positional_t *positional = find_positional_fault(command);
    const clargs_option_t *option = NULL;
    clargs_error_t error;
    size_t count;
    size_t index;

    if (positional != NULL) {
        fault->command = command;
        fault->option = NULL;
        fault->positional = positional;
        return CLARGS_ERROR_INVALID_DEFINITION;
    }

    error = check_options(command, &option);
    if (option != NULL) {
        fault->command = command;
        fault->option = option;
        fault->positional = NULL;
    }
    if (error != CLARGS_ERROR_NONE) {
        return error;
    }

    error = check_positional_defaults(command, &positional);
    if (positional != NULL) {
        fault->command = command;
        fault->option = NULL;
        fault->positional = positional;
    }
    if (error != CLARGS_ERROR_NONE) {
        return error;
    }

    count = clargs_command_subcommand_count(command);
    for (index = 0; index < count; ++index) {
        error = check_command(clargs_command_subcommand_at(command, index), fault);
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

    /*
     * The state changes only here: a validation that fails leaves it as it
     * was.
     */
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
