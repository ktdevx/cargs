/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX example - the complete program "pack", a made-up archiver with two
 * subcommands. It goes through every step of using ArgX:
 *
 *   1. creates a choice value type and a user-defined value type;
 *   2. defines the schema: every kind of option, positionals, subcommands,
 *      descriptions for the help, and -h, --help on every command;
 *   3. validates it, and tells which definition a failed validation points to;
 *   4. parses argv, and checks --help before the parse error;
 *   5. prints the help of the selected command, or the message and the usage
 *      line of a parse error;
 *   6. reads the values and where they came from;
 *   7. destroys the result, the schema and the value types, in this order.
 *
 * docs/getting-started/tutorial.md walks through this program step by step, and the API
 * pages in docs/references/ (schema.md, types.md, parse.md and help.md) are
 * the reference for each function it calls.
 *
 * It accepts command lines of this form:
 *
 *   pack [-h] [-v]... [-q] [--color | --no-color] create [-h] -o FILE
 *        [-l LEVEL] [--ratio RATIO] [--format tar|zip] [--block-size SIZE] file...
 *   pack [-h] [-v]... [-q] [--color | --no-color] list [-h] archive [pattern]
 *
 * Example runs (the "$" lines are the input, and the lines after them are
 * the output):
 *
 *   $ pack -vv create -o out.tar a.txt b.txt
 *   verbose: 2, color: on
 *   create out.tar, level 6 (default)
 *   format tar, block size 65536 bytes, ratio 0.50
 *     a.txt
 *     b.txt
 *
 *   $ pack --no-color create -l9 --format=zip --block-size 2m -o out.zip -- -x.txt
 *   verbose: 0, color: off
 *   create out.zip, level 9 (command line)
 *   format zip, block size 2097152 bytes, ratio 0.50
 *     -x.txt
 *
 *   $ pack list out.tar
 *   verbose: 0, color: on
 *   list out.tar, pattern * (default)
 *
 *   $ pack create a.txt
 *   pack: missing required option '--output'
 *   Usage: pack create [OPTIONS] --output <FILE> <file>...
 *   Try 'pack --help' for more information.
 *
 *   $ pack
 *   pack: a command is required
 *   Try 'pack --help' for more information.
 *
 * "pack --help" and "pack create --help" print the help of the root and of
 * create.
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
 * The choices of the value type of --format. The value of a choice option is
 * the index of the matching choice in this array.
 */
static const char *const formats[] = { "tar", "zip" };

/* The user data of the value type "size": the largest size it takes. */
typedef struct size_limit
{
    unsigned long max;
} size_limit_t;

/*
 * The schema of pack, and the handles that the result is read with. The root
 * owns the commands, options and positionals; the value types are borrowed
 * and outlive the schema.
 */
typedef struct pack_schema
{
    argx_type_t *format;            /* choice: tar or zip */
    argx_type_t *size;              /* user-defined: 512, 64k, 2m */
    argx_command_t *root;           /* pack */
    argx_command_t *create;         /* pack create */
    argx_command_t *list;           /* pack list */
    argx_option_t *verbose;         /* pack: -v, --verbose; a count */
    argx_option_t *quiet;           /* pack: -q, --quiet; a flag */
    argx_option_t *color;           /* pack: --color, --no-color; on if not given */
    argx_option_t *output;          /* create: -o, --output FILE; required */
    argx_option_t *level;           /* create: -l, --level LEVEL; 6 if not given */
    argx_option_t *ratio;           /* create: --ratio RATIO; 0.5 if not given */
    argx_option_t *archive_format;  /* create: --format tar|zip; tar if not given */
    argx_option_t *block_size;      /* create: --block-size SIZE; 64k if not given */
    argx_positional_t *files;       /* create: file... */
    argx_positional_t *archive;     /* list: archive */
    argx_positional_t *pattern;     /* list: [pattern]; "*" if not given */
} pack_schema_t;

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

/*
 * Creates the value types. The schema borrows them, so they are created
 * before it and destroyed after it.
 */
static argx_error_t create_types(pack_schema_t *schema, size_limit_t *limit)
{
    argx_error_t error;

    TRY(argx_type_create_choice(formats, sizeof formats / sizeof formats[0], &schema->format));
    return argx_type_create("size", sizeof(unsigned long), convert_size, NULL, limit,
                            &schema->size);
}

/*
 * Adds -h, --help to command. The library adds no such option by itself, and
 * an option is only known where its command is the current command, so
 * every command gets its own.
 */
static argx_error_t add_help(argx_command_t *command)
{
    argx_option_t *option = NULL;
    argx_error_t error;

    TRY(argx_command_add_flag(command, 'h', "help", &option));
    return argx_option_set_description(option, "Print this help.");
}

/* The root: its options stand in front of the subcommand. */
static argx_error_t define_root(pack_schema_t *schema)
{
    argx_error_t error;

    TRY(argx_command_create("pack", &schema->root));
    TRY(argx_command_set_description(schema->root, "Pack files into an archive."));
    TRY(add_help(schema->root));

    /* A count: -v, -vv, --verbose. */
    TRY(argx_command_add_count(schema->root, 'v', "verbose", &schema->verbose));
    TRY(argx_option_set_description(schema->verbose, "Print more; can be repeated."));

    /* A flag: -q, --quiet. */
    TRY(argx_command_add_flag(schema->root, 'q', "quiet", &schema->quiet));
    TRY(argx_option_set_description(schema->quiet, "Print nothing."));

    /* A negatable flag that is on unless --no-color is given. */
    TRY(argx_command_add_negatable(schema->root, '\0', "color", &schema->color));
    TRY(argx_option_set_description(schema->color, "Color the output."));
    return argx_option_set_initial(schema->color, true);
}

/* The subcommand create: options with a value of each kind of value type. */
static argx_error_t define_create(pack_schema_t *schema)
{
    argx_command_t *create = NULL;
    argx_error_t error;

    TRY(argx_command_add_subcommand(schema->root, "create", &schema->create));
    create = schema->create;
    TRY(argx_command_set_description(create, "Create an archive."));
    TRY(add_help(create));

    /* A required string. */
    TRY(argx_command_add_value(create, 'o', "output", argx_type_string(), &schema->output));
    TRY(argx_option_set_description(schema->output, "The archive to write."));
    TRY(argx_option_set_value_name(schema->output, "FILE"));
    TRY(argx_option_set_required(schema->output, true));

    /* An integer with a default value. */
    TRY(argx_command_add_value(create, 'l', "level", argx_type_int(), &schema->level));
    TRY(argx_option_set_description(schema->level, "The compression level."));
    TRY(argx_option_set_value_name(schema->level, "LEVEL"));
    TRY(argx_option_set_default(schema->level, "6"));

    /* A float, with a long name only. */
    TRY(argx_command_add_value(create, '\0', "ratio", argx_type_float(), &schema->ratio));
    TRY(argx_option_set_description(schema->ratio, "Stop below this ratio."));
    TRY(argx_option_set_value_name(schema->ratio, "RATIO"));
    TRY(argx_option_set_default(schema->ratio, "0.5"));

    /*
     * A choice. Without a value name, the help shows the name of the value
     * type, <choice>, and lists the choices in an annotation.
     */
    TRY(argx_command_add_value(create, '\0', "format", schema->format, &schema->archive_format));
    TRY(argx_option_set_description(schema->archive_format, "The format of the archive."));
    TRY(argx_option_set_default(schema->archive_format, "tar"));

    /* The user-defined value type. Validation converts its default value. */
    TRY(argx_command_add_value(create, '\0', "block-size", schema->size, &schema->block_size));
    TRY(argx_option_set_description(schema->block_size, "The size of a block."));
    TRY(argx_option_set_value_name(schema->block_size, "SIZE"));
    TRY(argx_option_set_default(schema->block_size, "64k"));

    /* A positional that takes one argument or more. */
    TRY(argx_command_add_positional(create, "file", argx_type_string(), ARGX_ARITY_ONE_OR_MORE,
                                    &schema->files));
    return argx_positional_set_description(schema->files, "The files to pack.");
}

/* The subcommand list: a required and an optional positional. */
static argx_error_t define_list(pack_schema_t *schema)
{
    argx_error_t error;

    TRY(argx_command_add_subcommand(schema->root, "list", &schema->list));
    TRY(argx_command_set_description(schema->list, "List the files of an archive."));
    TRY(add_help(schema->list));

    TRY(argx_command_add_positional(schema->list, "archive", argx_type_string(),
                                    ARGX_ARITY_REQUIRED, &schema->archive));
    TRY(argx_positional_set_description(schema->archive, "The archive to read."));

    TRY(argx_command_add_positional(schema->list, "pattern", argx_type_string(),
                                    ARGX_ARITY_OPTIONAL, &schema->pattern));
    TRY(argx_positional_set_description(schema->pattern, "The files to list."));
    return argx_positional_set_default(schema->pattern, "*");
}

/*
 * Defines the schema. When an add or set function fails, the schema is as it
 * was before that call, and the handle it writes is NULL; TRY stops there, so
 * a NULL handle is never passed on. The schema can still be destroyed.
 */
static argx_error_t define_schema(pack_schema_t *schema)
{
    argx_error_t error;

    TRY(define_root(schema));
    TRY(define_create(schema));
    return define_list(schema);
}

/* Tells which definition a failed validation points to. */
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

/*
 * Tells whether --help or -h was given to one of the selected commands. This
 * also works on a result with a parse error: it holds what was read before
 * the error.
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
 * user_data points to. The part is not terminated, so it is written with its
 * length.
 */
static bool write_to_stream(void *user_data, const char *text, size_t length)
{
    return fwrite(text, 1, length, (FILE *)user_data) == length;
}

/*
 * The text of a source, for this example. Later versions may add sources, so
 * the switch has a default label.
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

/* Reads the values of create. */
static int run_create(const pack_schema_t *schema, const argx_result_t *result)
{
    const char *output = NULL;
    const char *file = NULL;
    int64_t level = 0;
    double ratio = 0.0;
    size_t format = 0;
    const unsigned long *block_size = NULL;
    size_t i;

    /*
     * --output is required, and the other options have a default value: after
     * a parse that succeeded, all of them have a value.
     */
    if (!argx_result_option_string(result, schema->output, &output) ||
        !argx_result_option_int(result, schema->level, &level) ||
        !argx_result_option_float(result, schema->ratio, &ratio) ||
        !argx_result_option_choice(result, schema->archive_format, &format))
    {
        return EXIT_FAILURE;
    }

    /*
     * The value of a user-defined value type is read from the buffer that its
     * conversion function wrote.
     */
    block_size = argx_result_option_value(result, schema->block_size);
    if (block_size == NULL)
    {
        return EXIT_FAILURE;
    }

    printf("create %s, level %" PRId64 " (%s)\n", output, level,
           source_text(argx_result_option_source(result, schema->level)));
    printf("format %s, block size %lu bytes, ratio %.2f\n", formats[format], *block_size, ratio);

    /*
     * A positional that takes any number of arguments: its values in the
     * order of the command line.
     */
    for (i = 0; i < argx_result_positional_count(result, schema->files); ++i)
    {
        if (argx_result_positional_string(result, schema->files, i, &file))
        {
            printf("  %s\n", file);
        }
    }

    return EXIT_SUCCESS;
}

/* Reads the values of list. */
static int run_list(const pack_schema_t *schema, const argx_result_t *result)
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
    return EXIT_SUCCESS;
}

/* Reads the values of a command line that matched the schema. */
static int run(const pack_schema_t *schema, const argx_result_t *result)
{
    const argx_command_t *command = argx_result_command(result);

    if (command != schema->create && command != schema->list)
    {
        /* A command line that names no subcommand is not a parse error. */
        fprintf(stderr, "pack: a command is required\n");
        fprintf(stderr, "Try 'pack --help' for more information.\n");
        return EXIT_FAILURE;
    }

    if (argx_result_flag(result, schema->quiet))
    {
        return EXIT_SUCCESS;
    }

    /* The options of the root: a count and a negatable flag. */
    printf("verbose: %lu, color: %s\n", (unsigned long)argx_result_count(result, schema->verbose),
           argx_result_flag(result, schema->color) ? "on" : "off");

    if (command == schema->create)
    {
        return run_create(schema, result);
    }
    return run_list(schema, result);
}

int main(int argc, char **argv)
{
    pack_schema_t schema = { 0 };
    size_limit_t limit = { 1024UL * 1024UL * 1024UL };
    const argx_command_t *faulty_command = NULL;
    const argx_option_t *faulty_option = NULL;
    const argx_positional_t *faulty_positional = NULL;
    argx_result_t *result = NULL;
    argx_error_t error;
    int status = EXIT_FAILURE;

    /* 1, 2. The value types first, then the schema that borrows them. */
    error = create_types(&schema, &limit);
    if (error == ARGX_ERROR_NONE)
    {
        error = define_schema(&schema);
    }
    if (error != ARGX_ERROR_NONE)
    {
        fprintf(stderr, "pack: cannot define the command line: %s\n", argx_error_message(error));
        goto cleanup;
    }

    /*
     * 3. A schema must be validated before it is parsed with, and before a
     * help is generated from it.
     */
    error = argx_command_validate(schema.root, &faulty_command, &faulty_option,
                                  &faulty_positional);
    if (error != ARGX_ERROR_NONE)
    {
        report_fault(error, faulty_command, faulty_option, faulty_positional);
        goto cleanup;
    }

    /* 4. argc and argv go in as main() received them. */
    error = argx_parse(schema.root, argc, argv, &result);

    if (result == NULL)
    {
        /* ARGX_ERROR_OUT_OF_MEMORY: there is no result. */
        fprintf(stderr, "pack: %s\n", argx_error_message(error));
    }
    else if (help_was_given(result))
    {
        /*
         * 5. --help first: "pack create --help" leaves out what create
         * requires, and that must not hide the help. The help is of the last
         * selected command, to the standard output.
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
         * 5. The message, then the usage line of the command in which the
         * error was found, both to the standard error stream. The message has
         * no prefix and no line break; the usage line ends with a newline.
         */
        fprintf(stderr, "pack: %s\n", argx_result_error_message(result));
        argx_help_write_usage(argx_result_error_command(result), NULL, write_to_stream, stderr);
        fprintf(stderr, "Try 'pack --help' for more information.\n");
    }
    else
    {
        /* 6. */
        status = run(&schema, result);
    }

cleanup:
    /*
     * 7. The result first, then the schema it was made from, then the value
     * types the schema borrowed. A parse error also leaves a result to
     * destroy. Destroying NULL does nothing, so this is also right after a
     * failure. The built-in value types are never destroyed.
     */
    argx_result_destroy(result);
    argx_command_destroy(schema.root);
    argx_type_destroy(schema.size);
    argx_type_destroy(schema.format);

    return status;
}
