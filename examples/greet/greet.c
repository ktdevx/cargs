/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX example - the program "greet" of the README. Everything after this
 * comment is the code of the README's example, byte for byte: the test
 * example.greet.readme checks it, and runs the command lines of the README
 * and compares their output.
 */

#include <argx/argx.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* Checks a call that can fail; on a failure, goes to the message. */
#define TRY(call)                     \
    do                                \
    {                                 \
        error = (call);               \
        if (error != ARGX_ERROR_NONE) \
        {                             \
            goto define_failed;       \
        }                             \
    } while (0)

/* The library passes the help text to this function, part by part. */
static bool write_to_stream(void *user_data, const char *text, size_t length)
{
    return fwrite(text, 1, length, (FILE *)user_data) == length;
}

/* Writes the help of command to the standard output. */
static int print_help(const argx_command_t *command)
{
    if (argx_help_write(command, NULL, write_to_stream, stdout) != ARGX_ERROR_NONE)
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/* Writes the message of a parse error and the usage line to the standard error stream. */
static void print_parse_error(const argx_result_t *result)
{
    fprintf(stderr, "greet: %s\n", argx_result_error_message(result));
    argx_help_write_usage(argx_result_error_command(result), NULL, write_to_stream, stderr);
    fprintf(stderr, "For more information, try '--help'.\n");
}

int main(int argc, char **argv)
{
    argx_command_t *root = NULL;
    argx_option_t *help = NULL;
    argx_option_t *loud = NULL;
    argx_option_t *count = NULL;
    argx_positional_t *name = NULL;
    argx_result_t *result = NULL;
    argx_error_t error;
    int status = EXIT_FAILURE;

    /* The schema. The descriptions are for the help. */
    TRY(argx_command_create("greet", &root));
    TRY(argx_command_set_description(root, "Greet somebody."));
    TRY(argx_command_add_flag(root, 'h', "help", &help));
    TRY(argx_option_set_description(help, "Print this help."));
    TRY(argx_command_add_flag(root, 'l', "loud", &loud));
    TRY(argx_option_set_description(loud, "Add an exclamation mark."));
    TRY(argx_command_add_value(root, 'n', "count", argx_type_int(), &count));
    TRY(argx_option_set_description(count, "How often to greet."));
    TRY(argx_option_set_value_name(count, "COUNT"));
    TRY(argx_option_set_default(count, "1"));
    TRY(argx_command_add_positional(root, "name", argx_type_string(), ARGX_ARITY_REQUIRED, &name));
    TRY(argx_positional_set_description(name, "Whom to greet."));
    TRY(argx_command_validate(root, NULL, NULL, NULL));

    error = argx_parse(root, argc, argv, &result);
    switch (error)
    {
    case ARGX_ERROR_NONE:
        if (argx_result_flag(result, help))
        {
            status = print_help(root);
        }
        else
        {
            const char *text = NULL;
            int64_t times = 0;
            int64_t i;

            if (argx_result_positional_string(result, name, 0, &text) &&
                argx_result_option_int(result, count, &times))
            {
                for (i = 0; i < times; ++i)
                {
                    printf("hello, %s%s\n", text, argx_result_flag(result, loud) ? "!" : "");
                }
                printf("greeted %" PRId64 " time(s)\n", times);
                status = EXIT_SUCCESS;
            }
        }
        break;

    case ARGX_ERROR_INVALID_COMMAND_LINE:
        /* The result holds what was read before the error: "greet --help" works without a name. */
        if (argx_result_flag(result, help))
        {
            status = print_help(root);
        }
        else
        {
            print_parse_error(result);
        }
        break;

    default:
        /* ARGX_ERROR_OUT_OF_MEMORY, the only code that leaves no result. */
        fprintf(stderr, "greet: %s\n", argx_error_message(error));
        break;
    }

    goto cleanup;

define_failed:
    fprintf(stderr, "greet: cannot define the command line: %s\n", argx_error_message(error));

cleanup:
    /* The result first, then the schema. Destroying NULL does nothing. */
    argx_result_destroy(result);
    argx_command_destroy(root);

    return status;
}
