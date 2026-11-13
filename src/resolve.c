/*
 * CLArgs - resolving: what is done with a result after its arguments were
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
 * <clargs/parse.h>, and the schema through <clargs/schema.h>, and it writes
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
typedef struct element_name {
    char short_name;
    const char *prefix;
    const char *text;
    size_t length;
} element_name_t;

static void name_option(element_name_t *name, const clargs_option_t *option)
{
    const char *long_name = clargs_option_long_name(option);

    name->short_name = clargs_option_short_name(option);
    if (long_name != NULL) {
        name->prefix = "--";
        name->text = long_name;
        name->length = strlen(long_name);
    } else {
        name->prefix = "-";
        name->text = &name->short_name;
        name->length = 1;
    }
}

static void name_positional(element_name_t *name, const clargs_positional_t *positional)
{
    name->short_name = '\0';
    name->prefix = "";
    name->text = clargs_positional_name(positional);
    name->length = strlen(name->text);
}

/* Fills in where for something that is not on the command line: no index
 * and no letter of a bundle. name must outlive the use of where. */
static void locate(clargs_result_where_t *where, clargs_source_t source, const element_name_t *name)
{
    where->source = source;
    where->index = 0;
    where->short_name = '\0';
    where->name_prefix = name->prefix;
    where->name = name->text;
    where->name_length = name->length;
}

/* Records that option or positional, one of which is NULL, is required by
 * command and was not given. Returns what clargs_result_fail() returns. */
static clargs_error_t fail_missing(clargs_result_t *result, const clargs_command_t *command,
                                   const clargs_option_t *option, const clargs_positional_t *positional)
{
    clargs_result_failure_t failure;
    element_name_t name;

    if (option != NULL) {
        name_option(&name, option);
        failure.kind = CLARGS_PARSE_ERROR_MISSING_OPTION;
    } else {
        name_positional(&name, positional);
        failure.kind = CLARGS_PARSE_ERROR_MISSING_POSITIONAL;
    }
    locate(&failure.where, CLARGS_SOURCE_NONE, &name);
    failure.value = NULL;
    failure.command = command;
    failure.option = option;
    failure.positional = positional;
    return clargs_result_fail(result, &failure);
}

/*
 * The first pass over command: records the first option, or else the first
 * positional, that is required and has no value. Returns CLARGS_ERROR_NONE
 * when nothing is missing, and otherwise what clargs_result_fail() returns.
 */
static clargs_error_t check_required(clargs_result_t *result, const clargs_command_t *command)
{
    const size_t option_count = clargs_command_option_count(command);
    const size_t positional_count = clargs_command_positional_count(command);
    size_t index;

    for (index = 0; index < option_count; index++) {
        const clargs_option_t *option = clargs_command_option_at(command, index);

        if (clargs_option_kind(option) == CLARGS_OPTION_VALUE && clargs_option_is_required(option) &&
            clargs_result_option_source(result, option) == CLARGS_SOURCE_NONE) {
            return fail_missing(result, command, option, NULL);
        }
    }
    for (index = 0; index < positional_count; index++) {
        const clargs_positional_t *positional = clargs_command_positional_at(command, index);
        const clargs_arity_t arity = clargs_positional_arity(positional);

        if ((arity == CLARGS_ARITY_REQUIRED || arity == CLARGS_ARITY_ONE_OR_MORE) &&
            clargs_result_positional_count(result, positional) == 0) {
            return fail_missing(result, command, NULL, positional);
        }
    }
    return CLARGS_ERROR_NONE;
}

/*
 * The second pass over command: gives every option that takes a value, and
 * every positional, that has no value and has a default value that value,
 * with the source CLARGS_SOURCE_DEFAULT. Returns CLARGS_ERROR_NONE, or what
 * the record of the first default value that failed returned.
 */
static clargs_error_t apply_defaults(clargs_result_t *result, const clargs_command_t *command)
{
    const size_t option_count = clargs_command_option_count(command);
    const size_t positional_count = clargs_command_positional_count(command);
    clargs_result_where_t where;
    element_name_t name;
    size_t index;

    for (index = 0; index < option_count; index++) {
        const clargs_option_t *option = clargs_command_option_at(command, index);
        const char *value;
        clargs_error_t error;

        if (clargs_option_kind(option) != CLARGS_OPTION_VALUE ||
            clargs_result_option_source(result, option) != CLARGS_SOURCE_NONE) {
            continue;
        }
        value = clargs_option_default(option);
        if (value == NULL) {
            continue;
        }
        name_option(&name, option);
        locate(&where, CLARGS_SOURCE_DEFAULT, &name);
        error = clargs_result_record_option_value(result, option, value, &where);
        if (error != CLARGS_ERROR_NONE) {
            return error;
        }
    }
    for (index = 0; index < positional_count; index++) {
        const clargs_positional_t *positional = clargs_command_positional_at(command, index);
        const char *value;
        clargs_error_t error;

        if (clargs_result_positional_source(result, positional) != CLARGS_SOURCE_NONE) {
            continue;
        }
        value = clargs_positional_default(positional);
        if (value == NULL) {
            continue;
        }
        name_positional(&name, positional);
        locate(&where, CLARGS_SOURCE_DEFAULT, &name);
        error = clargs_result_record_positional_value(result, positional, value, &where);
        if (error != CLARGS_ERROR_NONE) {
            return error;
        }
    }
    return CLARGS_ERROR_NONE;
}

clargs_error_t clargs_resolve(clargs_result_t *result)
{
    size_t command_count;
    size_t index;

    assert(result != NULL);

    command_count = clargs_result_command_count(result);
    for (index = 0; index < command_count; index++) {
        const clargs_error_t error = check_required(result, clargs_result_command_at(result, index));

        if (error != CLARGS_ERROR_NONE) {
            return error;
        }
    }
    for (index = 0; index < command_count; index++) {
        const clargs_error_t error = apply_defaults(result, clargs_result_command_at(result, index));

        if (error != CLARGS_ERROR_NONE) {
            /* A result with a parse error holds no default value. A result
             * that ran out of memory is destroyed by the caller. */
            if (error != CLARGS_ERROR_OUT_OF_MEMORY) {
                clargs_result_discard_defaults(result);
            }
            return error;
        }
    }
    return CLARGS_ERROR_NONE;
}
