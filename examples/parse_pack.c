/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX example - parses the command line of "pack", a made-up program with
 * two subcommands, checks for --help, prints parse errors, reads the values
 * and destroys everything. Described in docs/parse.md.
 *
 * It accepts command lines of this form:
 *
 *   pack [-h] [-v]... [--color | --no-color] create [-h] -o FILE [-l LEVEL] file...
 *   pack [-h] [-v]... [--color | --no-color] list [-h] archive [pattern]
 *
 * For example, "pack -vv create -o out.tar a.txt b.txt" prints:
 *
 *   verbose: 2, color: on
 *   create out.tar, level 6 (default)
 *     a.txt
 *     b.txt
 */

#include <argx/argx.h>

#include <inttypes.h>
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
 * The schema of pack, and the handles that the result is read with. The
 * root owns all of them.
 */
typedef struct pack_schema
{
    argx_command_t *root;
    argx_command_t *create;
    argx_command_t *list;
    argx_option_t *verbose;     /* pack: -v, --verbose; a count */
    argx_option_t *color;       /* pack: --color, --no-color */
    argx_option_t *output;      /* create: -o, --output FILE; required */
    argx_option_t *level;       /* create: -l, --level LEVEL; 6 if not given */
    argx_positional_t *files;   /* create: file... */
    argx_positional_t *archive; /* list: archive */
    argx_positional_t *pattern; /* list: [pattern]; "*" if not given */
} pack_schema_t;

/*
 * Adds -h, --help to command. An option is only known where its command is
 * the current command, so every command gets its own.
 */
static argx_error_t add_help(argx_command_t *command)
{
    return argx_command_add_flag(command, 'h', "help", NULL);
}

/*
 * Defines and validates the schema. When it fails, schema->root is NULL or
 * a schema that can be destroyed.
 */
static argx_error_t define_schema(pack_schema_t *schema)
{
    argx_error_t error;

    schema->root = NULL;

    /* The root: pack [-h] [-v]... [--color | --no-color] COMMAND. */
    TRY(argx_command_create("pack", &schema->root));
    TRY(add_help(schema->root));
    TRY(argx_command_add_count(schema->root, 'v', "verbose", &schema->verbose));
    TRY(argx_command_add_negatable(schema->root, '\0', "color", &schema->color));
    TRY(argx_option_set_initial(schema->color, true));

    /* The subcommand create. */
    TRY(argx_command_add_subcommand(schema->root, "create", &schema->create));
    TRY(add_help(schema->create));
    TRY(argx_command_add_value(schema->create, 'o', "output", argx_type_string(),
                               &schema->output));
    TRY(argx_option_set_required(schema->output, true));
    TRY(argx_command_add_value(schema->create, 'l', "level", argx_type_int(), &schema->level));
    TRY(argx_option_set_default(schema->level, "6"));
    TRY(argx_command_add_positional(schema->create, "file", argx_type_string(),
                                    ARGX_ARITY_ONE_OR_MORE, &schema->files));

    /* The subcommand list. */
    TRY(argx_command_add_subcommand(schema->root, "list", &schema->list));
    TRY(add_help(schema->list));
    TRY(argx_command_add_positional(schema->list, "archive", argx_type_string(),
                                    ARGX_ARITY_REQUIRED, &schema->archive));
    TRY(argx_command_add_positional(schema->list, "pattern", argx_type_string(),
                                    ARGX_ARITY_OPTIONAL, &schema->pattern));
    TRY(argx_positional_set_default(schema->pattern, "*"));

    /* A schema must be validated before it is parsed with. */
    return argx_command_validate(schema->root, NULL, NULL, NULL);
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
 * Prints the usage of the last selected command. It is written by hand to
 * keep the example about parsing; docs/help.md generates it from the schema.
 */
static void print_usage(const pack_schema_t *schema, const argx_command_t *command)
{
    if (command == schema->create)
    {
        printf("usage: pack create -o FILE [-l LEVEL] file...\n");
    }
    else if (command == schema->list)
    {
        printf("usage: pack list archive [pattern]\n");
    }
    else
    {
        printf("usage: pack [-v]... [--color | --no-color] <command> ...\n");
        printf("commands: create, list\n");
    }
}

/*
 * The text of a source, for this example. Later versions may add sources,
 * so the switch has a default label.
 */
static const char *source_text(argx_source_t source)
{
    switch (source)
    {
    case ARGX_SOURCE_COMMAND_LINE:
        return "command line";
    case ARGX_SOURCE_DEFAULT:
        return "default";
    default:
        return "none";
    }
}

/* Reads the values of a command line that matched the schema. */
static int run(const pack_schema_t *schema, const argx_result_t *result)
{
    const argx_command_t *command = argx_result_command(result);

    if (command != schema->create && command != schema->list)
    {
        /* A command line that names no subcommand is not a parse error. */
        fprintf(stderr, "pack: a command is required\n");
        return EXIT_FAILURE;
    }

    /* The options of the root: a count and a negatable flag. */
    printf("verbose: %lu, color: %s\n", (unsigned long)argx_result_count(result, schema->verbose),
           argx_result_flag(result, schema->color) ? "on" : "off");

    if (command == schema->create)
    {
        const char *output = NULL;
        const char *file = NULL;
        int64_t level = 0;
        size_t i;

        /*
         * --output is required and --level has a default value: after a
         * parse that succeeded, both have a value.
         */
        if (!argx_result_option_string(result, schema->output, &output) ||
            !argx_result_option_int(result, schema->level, &level))
        {
            return EXIT_FAILURE;
        }
        printf("create %s, level %" PRId64 " (%s)\n", output, level,
               source_text(argx_result_option_source(result, schema->level)));

        /*
         * A positional that takes any number of arguments: its values in
         * the order of the command line.
         */
        for (i = 0; i < argx_result_positional_count(result, schema->files); ++i)
        {
            if (argx_result_positional_string(result, schema->files, i, &file))
            {
                printf("  %s\n", file);
            }
        }
    }
    else
    {
        const char *archive = NULL;
        const char *pattern = NULL;

        if (!argx_result_positional_string(result, schema->archive, 0, &archive) ||
            !argx_result_positional_string(result, schema->pattern, 0, &pattern))
        {
            return EXIT_FAILURE;
        }
        printf("list %s, pattern %s (%s)\n", archive, pattern,
               source_text(argx_result_positional_source(result, schema->pattern)));
    }

    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
    pack_schema_t schema;
    argx_result_t *result = NULL;
    argx_error_t error;
    int status = EXIT_FAILURE;

    error = define_schema(&schema);
    if (error != ARGX_ERROR_NONE)
    {
        fprintf(stderr, "pack: cannot define the command line: %s\n", argx_error_message(error));
        goto cleanup;
    }

    /* argc and argv go in as main() received them. */
    error = argx_parse(schema.root, argc, argv, &result);

    if (result == NULL)
    {
        /* ARGX_ERROR_OUT_OF_MEMORY: there is no result. */
        fprintf(stderr, "pack: %s\n", argx_error_message(error));
    }
    else if (help_was_given(result))
    {
        /*
         * --help first: "pack create --help" leaves out what create
         * requires, and that must not hide the help.
         */
        print_usage(&schema, argx_result_command(result));
        status = EXIT_SUCCESS;
    }
    else if (error == ARGX_ERROR_INVALID_COMMAND_LINE)
    {
        /* The message is one line with no prefix and no line break. */
        fprintf(stderr, "pack: %s\n", argx_result_error_message(result));
    }
    else
    {
        status = run(&schema, result);
    }

    if (status != EXIT_SUCCESS)
    {
        fprintf(stderr, "Try 'pack --help' for more information.\n");
    }

cleanup:
    /*
     * The result first, then the schema it was made from. A parse error
     * also leaves a result to destroy. Destroying NULL does nothing, so
     * this is also right after a failure.
     */
    argx_result_destroy(result);
    argx_command_destroy(schema.root);

    return status;
}
