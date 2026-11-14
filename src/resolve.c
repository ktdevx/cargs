/*
 * ArgX - resolving: what is done with a result after its arguments were
 * all read without a parse error.
 *
 * Two passes go over the selected commands of the result, from the root to
 * the last, and for each command over its options and then its positionals,
 * each in the order in which they were added. The first pass checks that
 * what is required was given and records the first that is missing. Only
 * when nothing is missing, the second pass gives the options and the
 * positionals that have no value their default values.
 *
 * This source does not read the arguments. It reads the selected commands
 * and the state of a result through the public functions of
 * <argx/parse.h>, and the schema through <argx/schema.h>, and it writes
 * through the functions of "parse_internal.h". A command that was not
 * selected is never looked at.
 *
 * The command line and the default values are the only sources of a value.
 * A later source, such as the environment, is one more pass before the
 * first, over what has no value yet.
 */
#include "parse_internal.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

/*
 * How an option or a positional is named in a message that is not about an
 * argument: an option by "--" and its long name or, when it has none, by
 * "-" and its short name, and a positional by its name alone. The strings
 * are those of the schema, except the short name, which is kept here.
 */
typedef struct element_name
{
    char short_name;
    const char *prefix;
    const char *text;
    size_t length;
} element_name_t;

static void name_option(element_name_t *name, const argx_option_t *option)
{
    const char *long_name = argx_option_long_name(option);

    name->short_name = argx_option_short_name(option);
    if (long_name != NULL)
    {
        name->prefix = "--";
        name->text = long_name;
        name->length = strlen(long_name);
    }
    else
    {
        name->prefix = "-";
        name->text = &name->short_name;
        name->length = 1;
    }
}

static void name_positional(element_name_t *name, const argx_positional_t *positional)
{
    name->short_name = '\0';
    name->prefix = "";
    name->text = argx_positional_name(positional);
    name->length = strlen(name->text);
}

/*
 * Fills in where for something that is not on the command line: no index
 * and no letter of a bundle. name must outlive the use of where.
 */
static void locate(argx_result_where_t *where, argx_source_t source, const element_name_t *name)
{
    where->source = source;
    where->index = 0;
    where->short_name = '\0';
    where->name_prefix = name->prefix;
    where->name = name->text;
    where->name_length = name->length;
}

/*
 * Records that option or positional, one of which is NULL, is required by
 * command and was not given. Returns what argx_result_fail() returns.
 */
static argx_error_t fail_missing(argx_result_t *result, const argx_command_t *command,
                                 const argx_option_t *option, const argx_positional_t *positional)
{
    argx_result_failure_t failure;
    element_name_t name;

    if (option != NULL)
    {
        name_option(&name, option);
        failure.kind = ARGX_PARSE_ERROR_MISSING_OPTION;
    }
    else
    {
        name_positional(&name, positional);
        failure.kind = ARGX_PARSE_ERROR_MISSING_POSITIONAL;
    }
    locate(&failure.where, ARGX_SOURCE_NONE, &name);
    failure.value = NULL;
    failure.command = command;
    failure.option = option;
    failure.positional = positional;
    return argx_result_fail(result, &failure);
}

/*
 * The first pass over command: records the first option, or else the first
 * positional, that is required and has no value. Returns ARGX_ERROR_NONE
 * when nothing is missing, and otherwise what argx_result_fail() returns.
 */
static argx_error_t check_required(argx_result_t *result, const argx_command_t *command)
{
    const size_t option_count = argx_command_option_count(command);
    const size_t positional_count = argx_command_positional_count(command);
    size_t index;

    for (index = 0; index < option_count; index++)
    {
        const argx_option_t *option = argx_command_option_at(command, index);

        if ((argx_option_kind(option) == ARGX_OPTION_VALUE) && argx_option_is_required(option) &&
            (argx_result_option_source(result, option) == ARGX_SOURCE_NONE))
        {
            return fail_missing(result, command, option, NULL);
        }
    }
    for (index = 0; index < positional_count; index++)
    {
        const argx_positional_t *positional = argx_command_positional_at(command, index);
        const argx_arity_t arity = argx_positional_arity(positional);

        if (((arity == ARGX_ARITY_REQUIRED) || (arity == ARGX_ARITY_ONE_OR_MORE)) &&
            (argx_result_positional_count(result, positional) == 0))
        {
            return fail_missing(result, command, NULL, positional);
        }
    }
    return ARGX_ERROR_NONE;
}

/*
 * The second pass over command: gives every option that takes a value, and
 * every positional, that has no value and has a default value that value,
 * with the source ARGX_SOURCE_DEFAULT. Returns ARGX_ERROR_NONE, or what
 * the record of the first default value that failed returned.
 */
static argx_error_t apply_defaults(argx_result_t *result, const argx_command_t *command)
{
    const size_t option_count = argx_command_option_count(command);
    const size_t positional_count = argx_command_positional_count(command);
    argx_result_where_t where;
    element_name_t name;
    size_t index;

    for (index = 0; index < option_count; index++)
    {
        const argx_option_t *option = argx_command_option_at(command, index);
        const char *value;
        argx_error_t error;

        if ((argx_option_kind(option) != ARGX_OPTION_VALUE) ||
            (argx_result_option_source(result, option) != ARGX_SOURCE_NONE))
        {
            continue;
        }
        value = argx_option_default(option);
        if (value == NULL)
        {
            continue;
        }
        name_option(&name, option);
        locate(&where, ARGX_SOURCE_DEFAULT, &name);
        error = argx_result_record_option_value(result, option, value, &where);
        if (error != ARGX_ERROR_NONE)
        {
            return error;
        }
    }
    for (index = 0; index < positional_count; index++)
    {
        const argx_positional_t *positional = argx_command_positional_at(command, index);
        const char *value;
        argx_error_t error;

        if (argx_result_positional_source(result, positional) != ARGX_SOURCE_NONE)
        {
            continue;
        }
        value = argx_positional_default(positional);
        if (value == NULL)
        {
            continue;
        }
        name_positional(&name, positional);
        locate(&where, ARGX_SOURCE_DEFAULT, &name);
        error = argx_result_record_positional_value(result, positional, value, &where);
        if (error != ARGX_ERROR_NONE)
        {
            return error;
        }
    }
    return ARGX_ERROR_NONE;
}

argx_error_t argx_resolve(argx_result_t *result)
{
    size_t command_count;
    size_t index;

    assert(result != NULL);

    command_count = argx_result_command_count(result);
    for (index = 0; index < command_count; index++)
    {
        const argx_error_t error = check_required(result, argx_result_command_at(result, index));

        if (error != ARGX_ERROR_NONE)
        {
            return error;
        }
    }
    for (index = 0; index < command_count; index++)
    {
        const argx_error_t error = apply_defaults(result, argx_result_command_at(result, index));

        if (error != ARGX_ERROR_NONE)
        {
            /*
             * A result with a parse error holds no default value. A result
             * that ran out of memory is destroyed by the caller.
             */
            if (error != ARGX_ERROR_OUT_OF_MEMORY)
            {
                argx_result_discard_defaults(result);
            }
            return error;
        }
    }
    return ARGX_ERROR_NONE;
}
