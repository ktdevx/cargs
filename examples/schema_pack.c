/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX example - defines, validates and destroys the schema of "pack", a
 * made-up program with two subcommands, every kind of option, a choice value
 * type and a user-defined value type. Described in docs/schema.md.
 *
 * It accepts command lines of this form:
 *
 *   pack [-v]... [-q] [--color | --no-color] create -o FILE [-l LEVEL]
 *        [--ratio RATIO] [--format tar|zip] [--block-size SIZE] file...
 *   pack [-v]... [-q] [--color | --no-color] list archive [pattern]
 *
 * If the validation succeeds, it prints "the schema of pack is valid" and
 * exits with status 0.
 */

#include <argx/argx.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* The user data of the value type "size": the largest size it takes. */
typedef struct size_limit
{
    unsigned long max;
} size_limit_t;

/*
 * The conversion function of the value type "size". Takes decimal digits
 * with an optional unit, 'k' or 'm' ("512", "64k", "2m"), and writes the
 * number of bytes as an unsigned long.
 *
 * It returns ARGX_ERROR_INVALID_VALUE for an input that it does not take,
 * and it has no side effect: the validation of a schema calls it for every
 * default value of this type.
 */
static argx_error_t convert_size(const char *text, void *value, void *context)
{
    const size_limit_t *limit = context;
    const char *p = text;
    unsigned long bytes = 0;
    unsigned long unit = 1;

    if (*p < '0' || *p > '9')
    {
        return ARGX_ERROR_INVALID_VALUE;
    }

    for (; *p >= '0' && *p <= '9'; ++p)
    {
        unsigned long digit = (unsigned long)(*p - '0');

        if (bytes > (limit->max - digit) / 10)
        {
            return ARGX_ERROR_INVALID_VALUE;
        }

        bytes = bytes * 10 + digit;
    }

    if (*p == 'k')
    {
        unit = 1024UL;
        ++p;
    }
    else if (*p == 'm')
    {
        unit = 1024UL * 1024UL;
        ++p;
    }

    if (*p != '\0' || bytes > limit->max / unit)
    {
        return ARGX_ERROR_INVALID_VALUE;
    }

    *(unsigned long *)value = bytes * unit;

    return ARGX_ERROR_NONE;
}

/* The options of the root: they stand in front of the subcommand. */
static argx_error_t define_common_options(argx_command_t *root)
{
    argx_option_t *option;
    argx_error_t error;

    /* A count: -v, -vv, --verbose. */
    error = argx_command_add_count(root, 'v', "verbose", &option);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    error = argx_option_set_description(option, "Print more; can be repeated.");
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    /* A flag: -q, --quiet. */
    error = argx_command_add_flag(root, 'q', "quiet", &option);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    error = argx_option_set_description(option, "Print nothing.");
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    /*
     * A negatable flag without a short name: --color and --no-color. It is
     * on when neither is given.
     */
    error = argx_command_add_negatable(root, '\0', "color", &option);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    error = argx_option_set_description(option, "Color the output.");
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    error = argx_option_set_initial(option, true);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    return ARGX_ERROR_NONE;
}

/* The subcommand "create": options that take a value, and its positional. */
static argx_error_t define_create(argx_command_t *root, const argx_type_t *format,
                                  const argx_type_t *size)
{
    argx_command_t *create;
    argx_option_t *option;
    argx_positional_t *positional;
    argx_error_t error;

    error = argx_command_add_subcommand(root, "create", &create);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    error = argx_command_set_description(create, "Create an archive.");
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    /* A required option with the built-in value type string. */
    error = argx_command_add_value(create, 'o', "output", argx_type_string(), &option);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    error = argx_option_set_description(option, "The archive to write.");
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    error = argx_option_set_value_name(option, "FILE");
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    error = argx_option_set_required(option, true);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    /*
     * The built-in value type integer, with a default value. A default
     * value is a string, written as on a command line.
     */
    error = argx_command_add_value(create, 'l', "level", argx_type_int(), &option);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    error = argx_option_set_description(option, "The compression level.");
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    error = argx_option_set_value_name(option, "LEVEL");
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    error = argx_option_set_default(option, "6");
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    /* The built-in value type float; only a long name. */
    error = argx_command_add_value(create, '\0', "ratio", argx_type_float(), &option);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    error = argx_option_set_description(option, "Stop below this ratio.");
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    error = argx_option_set_value_name(option, "RATIO");
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    error = argx_option_set_default(option, "0.5");
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    /* A choice. */
    error = argx_command_add_value(create, '\0', "format", format, &option);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    error = argx_option_set_description(option, "The format of the archive.");
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    error = argx_option_set_default(option, "tar");
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    /* A user-defined value type. */
    error = argx_command_add_value(create, '\0', "block-size", size, &option);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    error = argx_option_set_description(option, "The size of a block.");
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    error = argx_option_set_value_name(option, "SIZE");
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    error = argx_option_set_default(option, "64k");
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    /* One or more files. */
    error = argx_command_add_positional(create, "file", argx_type_string(),
                                        ARGX_ARITY_ONE_OR_MORE, &positional);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    error = argx_positional_set_description(positional, "The files to pack.");
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    return ARGX_ERROR_NONE;
}

/* The subcommand "list": a required positional, then an optional one. */
static argx_error_t define_list(argx_command_t *root)
{
    argx_command_t *list;
    argx_positional_t *positional;
    argx_error_t error;

    error = argx_command_add_subcommand(root, "list", &list);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    error = argx_command_set_description(list, "List the files of an archive.");
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    error = argx_command_add_positional(list, "archive", argx_type_string(),
                                        ARGX_ARITY_REQUIRED, &positional);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    error = argx_positional_set_description(positional, "The archive to read.");
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    /* Only an optional positional can have a default value. */
    error = argx_command_add_positional(list, "pattern", argx_type_string(),
                                        ARGX_ARITY_OPTIONAL, &positional);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    error = argx_positional_set_description(positional, "The files to list.");
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    error = argx_positional_set_default(positional, "*");
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    return ARGX_ERROR_NONE;
}

/* Tells which definition the validation found a fault in. */
static void report_fault(argx_error_t error, const argx_command_t *command,
                         const argx_option_t *option, const argx_positional_t *positional)
{
    fprintf(stderr, "pack: the schema is not valid: %s\n", argx_error_message(error));

    if (command != NULL)
    {
        fprintf(stderr, "  in the command \"%s\"\n", argx_command_name(command));
    }

    if (option != NULL)
    {
        if (argx_option_long_name(option) != NULL)
        {
            fprintf(stderr, "  at the option --%s\n", argx_option_long_name(option));
        }
        else
        {
            fprintf(stderr, "  at the option -%c\n", argx_option_short_name(option));
        }

        if (argx_option_default(option) != NULL)
        {
            fprintf(stderr, "  whose default value is \"%s\"\n", argx_option_default(option));
        }
    }

    if (positional != NULL)
    {
        fprintf(stderr, "  at the positional \"%s\"\n", argx_positional_name(positional));
        if (argx_positional_default(positional) != NULL)
        {
            fprintf(stderr, "  whose default value is \"%s\"\n",
                    argx_positional_default(positional));
        }
    }
}

int main(void)
{
    static const char *const formats[] = { "tar", "zip" };
    size_limit_t limit = { 1024UL * 1024UL * 1024UL };

    argx_type_t *format = NULL;
    argx_type_t *size = NULL;
    argx_command_t *root = NULL;
    const argx_command_t *faulty_command;
    const argx_option_t *faulty_option;
    const argx_positional_t *faulty_positional;
    argx_error_t error;
    int status = EXIT_FAILURE;

    /*
     * The value types come first: the schema borrows them. Each call is
     * checked; on a failure, the code jumps to the message.
     */
    error = argx_type_create_choice(formats, 2, &format);
    if (error != ARGX_ERROR_NONE)
    {
        goto define_failed;
    }

    error = argx_type_create("size", sizeof(unsigned long), convert_size, NULL, &limit, &size);
    if (error != ARGX_ERROR_NONE)
    {
        goto define_failed;
    }

    error = argx_command_create("pack", &root);
    if (error != ARGX_ERROR_NONE)
    {
        goto define_failed;
    }

    error = argx_command_set_description(root, "Pack files into an archive.");
    if (error != ARGX_ERROR_NONE)
    {
        goto define_failed;
    }

    error = define_common_options(root);
    if (error != ARGX_ERROR_NONE)
    {
        goto define_failed;
    }

    error = define_create(root, format, size);
    if (error != ARGX_ERROR_NONE)
    {
        goto define_failed;
    }

    error = define_list(root);
    if (error != ARGX_ERROR_NONE)
    {
        goto define_failed;
    }

    error = argx_command_validate(root, &faulty_command, &faulty_option, &faulty_positional);
    if (error != ARGX_ERROR_NONE)
    {
        report_fault(error, faulty_command, faulty_option, faulty_positional);
        goto cleanup;
    }

    if (argx_command_is_validated(root))
    {
        printf("the schema of %s is valid\n", argx_command_name(root));
        status = EXIT_SUCCESS;
    }

    goto cleanup;

define_failed:
    /*
     * An addition or a set failed. The schema is as it was before the call
     * that failed, and it can still be destroyed.
     */
    fprintf(stderr, "pack: cannot define the command line: %s\n", argx_error_message(error));

cleanup:
    /*
     * The schema first, then the value types it borrowed. Destroying NULL
     * does nothing, so this is also right after a failure.
     */
    argx_command_destroy(root);
    argx_type_destroy(size);
    argx_type_destroy(format);

    return status;
}
