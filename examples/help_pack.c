/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX example - generates the help of "pack", a made-up program with two
 * subcommands: the help of the selected command for --help, and the usage
 * line of the command in which a parse error was found. Described in
 * docs/help.md.
 *
 * It accepts command lines of this form:
 *
 *   pack [-h] [-v]... [--color | --no-color] create [-h] -o FILE [-l LEVEL] file...
 *   pack [-h] [-v]... [--color | --no-color] list [-h] archive [pattern]
 *
 * For example, "pack create a.txt" prints to the standard error stream:
 *
 *   pack: missing required option '--output'
 *   Usage: pack create [OPTIONS] --output <FILE> <file>...
 *   Try 'pack --help' for more information.
 */

#include <argx/argx.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* Stops at the first call that fails. */
#define TRY(call)                     \
    do                                \
    {                                 \
        error = (call);               \
        if (error != ARGX_ERROR_NONE) \
        {                             \
            return error;             \
        }                             \
    } while (0)

/*
 * Adds -h, --help to command. The library adds no such option by itself,
 * and an option is only known where its command is the current command, so
 * every command gets its own.
 */
static argx_error_t add_help(argx_command_t *command)
{
    argx_option_t *option = NULL;
    argx_error_t error;

    TRY(argx_command_add_flag(command, 'h', "help", &option));
    return argx_option_set_description(option, "Print this help.");
}

/*
 * Defines and validates the schema. When it fails, *root is NULL or a
 * schema that can be destroyed.
 */
static argx_error_t define_schema(argx_command_t **root)
{
    argx_command_t *create = NULL;
    argx_command_t *list = NULL;
    argx_option_t *option = NULL;
    argx_positional_t *positional = NULL;
    argx_error_t error;

    *root = NULL;

    /* The root: pack [-h] [-v]... [--color | --no-color] COMMAND. */
    TRY(argx_command_create("pack", root));
    TRY(argx_command_set_description(*root, "Pack files into an archive."));
    TRY(add_help(*root));
    TRY(argx_command_add_count(*root, 'v', "verbose", &option));
    TRY(argx_option_set_description(option, "Print more; can be repeated."));
    TRY(argx_command_add_negatable(*root, '\0', "color", &option));
    TRY(argx_option_set_description(option, "Color the output."));
    TRY(argx_option_set_initial(option, true));

    /* The subcommand create. */
    TRY(argx_command_add_subcommand(*root, "create", &create));
    TRY(argx_command_set_description(create, "Create an archive."));
    TRY(add_help(create));
    TRY(argx_command_add_value(create, 'o', "output", argx_type_string(), &option));
    TRY(argx_option_set_description(option, "The archive to write."));
    TRY(argx_option_set_value_name(option, "FILE"));
    TRY(argx_option_set_required(option, true));
    TRY(argx_command_add_value(create, 'l', "level", argx_type_int(), &option));
    TRY(argx_option_set_description(option, "The compression level."));
    TRY(argx_option_set_value_name(option, "LEVEL"));
    TRY(argx_option_set_default(option, "6"));
    TRY(argx_command_add_positional(create, "file", argx_type_string(), ARGX_ARITY_ONE_OR_MORE,
                                    &positional));
    TRY(argx_positional_set_description(positional, "The files to pack."));

    /* The subcommand list. */
    TRY(argx_command_add_subcommand(*root, "list", &list));
    TRY(argx_command_set_description(list, "List the files of an archive."));
    TRY(add_help(list));
    TRY(argx_command_add_positional(list, "archive", argx_type_string(), ARGX_ARITY_REQUIRED,
                                    &positional));
    TRY(argx_positional_set_description(positional, "The archive to read."));
    TRY(argx_command_add_positional(list, "pattern", argx_type_string(), ARGX_ARITY_OPTIONAL,
                                    &positional));
    TRY(argx_positional_set_description(positional, "The files to list."));
    TRY(argx_positional_set_default(positional, "*"));

    /*
     * A schema must be validated before it is parsed with, and before a
     * help is generated from it.
     */
    return argx_command_validate(*root, NULL, NULL, NULL);
}

/*
 * Tells whether --help or -h was given to one of the selected commands.
 * This also works on a result with a parse error: it holds what was read
 * before the error.
 */
static bool help_was_given(const argx_result_t *result)
{
    size_t i;

    for (i = 0; i < argx_result_command_count(result); ++i)
    {
        const argx_command_t *command = argx_result_command_at(result, i);
        const argx_option_t *help = argx_command_find_long(command, "help", 4, NULL);

        if (help != NULL && argx_result_flag(result, help))
        {
            return true;
        }
    }
    return false;
}

/*
 * The write function: writes one part of the text to the stream that
 * user_data points to. The part is not terminated, so it is written with
 * its length.
 */
static bool write_to_stream(void *user_data, const char *text, size_t length)
{
    return fwrite(text, 1, length, (FILE *)user_data) == length;
}

int main(int argc, char **argv)
{
    argx_command_t *root = NULL;
    argx_result_t *result = NULL;
    argx_error_t error;
    int status = EXIT_FAILURE;

    error = define_schema(&root);
    if (error != ARGX_ERROR_NONE)
    {
        fprintf(stderr, "pack: cannot define the command line: %s\n", argx_error_message(error));
        goto cleanup;
    }

    error = argx_parse(root, argc, argv, &result);

    if (result == NULL)
    {
        /* ARGX_ERROR_OUT_OF_MEMORY: there is no result. */
        fprintf(stderr, "pack: %s\n", argx_error_message(error));
    }
    else if (help_was_given(result))
    {
        /*
         * The help of the command that the command line ends in, to the
         * standard output. Ending the program is up to this code.
         */
        error = argx_help_write(argx_result_command(result), NULL, write_to_stream, stdout);
        if (error == ARGX_ERROR_NONE)
        {
            status = EXIT_SUCCESS;
        }
        else
        {
            fprintf(stderr, "pack: cannot print the help: %s\n", argx_error_message(error));
        }
    }
    else if (error == ARGX_ERROR_INVALID_COMMAND_LINE)
    {
        /*
         * The message, then the usage line of the command in which the
         * error was found, both to the standard error stream. The usage
         * line ends with a newline of its own.
         */
        fprintf(stderr, "pack: %s\n", argx_result_error_message(result));
        argx_help_write_usage(argx_result_error_command(result), NULL, write_to_stream, stderr);
        fprintf(stderr, "Try 'pack --help' for more information.\n");
    }
    else
    {
        /* The values are read here, as in examples/parse_pack.c. */
        printf("pack: nothing to do in this example\n");
        status = EXIT_SUCCESS;
    }

cleanup:
    /*
     * The result first, then the schema it was made from. Destroying NULL
     * does nothing, so this is also right after a failure.
     */
    argx_result_destroy(result);
    argx_command_destroy(root);

    return status;
}
