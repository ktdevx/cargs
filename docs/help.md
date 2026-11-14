# Help and Usage

This document explains how to pass one command of a validated schema and generate the help of that command, or only its usage line. It also describes the rules of the generated text, the help options that control the layout, where the text is written, and what is left at the destination when generation fails.

This document covers generating the help and the usage line, the help options, and the output. Defining and validating a schema is in [Schemas](schema.md). Parsing argv and parse errors are in [Parsing](parse.md). Generation only reads the schema; it does not need a parse result. The library does not print the text anywhere by itself, does not end the process, and reads neither environment variables nor the terminal width. It does not add an option such as `--help` to the schema either.

The detailed description and the preconditions of each function are in the comments of the public header `include/argx/help.h`. The rules for who owns memory are in [Ownership and Lifetime](ownership.md).

Terms used in this document:

| Term | Meaning |
| --- | --- |
| help | The whole text that describes one command. It consists of the usage line, the description of the command, and the lists |
| usage line | The part at the start of the help that begins with `Usage: `. When it is wrapped, it spans several lines |
| list | Each of `Commands:`, `Arguments:` and `Options:` |
| item | One subcommand, positional or option in a list |
| heading | The notation of the names on the left of an item (such as `-o, --output <FILE>`) |
| description | The text on the right of an item. It consists of the description text and the annotations |
| annotation | A phrase made from the definition, such as `[default: 6]`, that follows the description text |
| column | The number of bytes counted from the start of the line. The start of a line is column 0. One byte counts as one column |
| description column | The column where the descriptions of the items start |
| help options | The object that holds the width, the indent and the maximum description column (`argx_help_options_t`) |
| write function | Your function that receives the generated text (`argx_help_write_fn_t`) |

## A minimal example

The example prints the help of `fetch`, a made-up program, to the standard output, and then prints the usage line alone. The example is one translation unit, and it includes only `<argx/argx.h>` and standard headers. You can copy it out and compile it as it is.

```c
#include <argx/argx.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * The write function: writes one part of the text to the stream that
 * user_data points to. The part is not terminated, so it is written with
 * its length.
 */
static bool write_to_stream(void *user_data, const char *text, size_t length)
{
    return fwrite(text, 1, length, (FILE *)user_data) == length;
}

int main(void)
{
    argx_command_t *root = NULL;
    argx_option_t *option = NULL;
    argx_positional_t *positional = NULL;
    argx_error_t error;
    int status = EXIT_FAILURE;

    /*
     * The schema. The help is generated from it, so it must be validated.
     * Each call is checked; on a failure, the code jumps to the message.
     */
    error = argx_command_create("fetch", &root);
    if (error != ARGX_ERROR_NONE)
    {
        goto define_failed;
    }
    error = argx_command_set_description(root, "Download a file.");
    if (error != ARGX_ERROR_NONE)
    {
        goto define_failed;
    }
    error = argx_command_add_flag(root, 'q', "quiet", &option);
    if (error != ARGX_ERROR_NONE)
    {
        goto define_failed;
    }
    error = argx_option_set_description(option, "Print nothing.");
    if (error != ARGX_ERROR_NONE)
    {
        goto define_failed;
    }
    error = argx_command_add_value(root, 'o', "output", argx_type_string(), &option);
    if (error != ARGX_ERROR_NONE)
    {
        goto define_failed;
    }
    error = argx_option_set_description(option, "The file to write.");
    if (error != ARGX_ERROR_NONE)
    {
        goto define_failed;
    }
    error = argx_option_set_value_name(option, "FILE");
    if (error != ARGX_ERROR_NONE)
    {
        goto define_failed;
    }
    error = argx_option_set_required(option, true);
    if (error != ARGX_ERROR_NONE)
    {
        goto define_failed;
    }
    error = argx_command_add_value(root, 'r', "retries", argx_type_int(), &option);
    if (error != ARGX_ERROR_NONE)
    {
        goto define_failed;
    }
    error = argx_option_set_description(option, "How often to try again.");
    if (error != ARGX_ERROR_NONE)
    {
        goto define_failed;
    }
    error = argx_option_set_default(option, "3");
    if (error != ARGX_ERROR_NONE)
    {
        goto define_failed;
    }
    error = argx_command_add_positional(root, "url", argx_type_string(), ARGX_ARITY_REQUIRED,
                                        &positional);
    if (error != ARGX_ERROR_NONE)
    {
        goto define_failed;
    }
    error = argx_positional_set_description(positional, "The address to read.");
    if (error != ARGX_ERROR_NONE)
    {
        goto define_failed;
    }
    error = argx_command_validate(root, NULL, NULL, NULL);
    if (error != ARGX_ERROR_NONE)
    {
        goto define_failed;
    }

    /* The help of the root, with the default settings (NULL). */
    error = argx_help_write(root, NULL, write_to_stream, stdout);
    if (error == ARGX_ERROR_NONE)
    {
        /* The usage line alone: the first line of the help. */
        printf("----\n");
        error = argx_help_write_usage(root, NULL, write_to_stream, stdout);
    }

    if (error != ARGX_ERROR_NONE)
    {
        fprintf(stderr, "fetch: cannot print the help: %s\n", argx_error_message(error));
    }
    else
    {
        status = EXIT_SUCCESS;
    }
    goto cleanup;

define_failed:
    fprintf(stderr, "fetch: cannot define the command line: %s\n", argx_error_message(error));

cleanup:
    /* Destroying NULL does nothing, so this is also right after a failure. */
    argx_command_destroy(root);

    return status;
}
```

Output:

```
Usage: fetch [OPTIONS] --output <FILE> <url>

Download a file.

Arguments:
  <url>                    The address to read.

Options:
  -q, --quiet              Print nothing.
  -o, --output <FILE>      The file to write. [required]
  -r, --retries <integer>  How often to try again. [default: 3]
----
Usage: fetch [OPTIONS] --output <FILE> <url>
```

Points of the example:

- Generation takes four things: a command of a validated schema, the help options, a write function, and the data passed to the write function. If the help options are `NULL`, the default values are used.
- The library only passes the text to the write function. The write function decides where it goes. The `write_to_stream` of the example writes to a `FILE *` with `fwrite()` ([Writing to standard output](#writing-to-standard-output)).
- `argx_help_write` generates the help, and `argx_help_write_usage` generates the usage line alone. The `----` line is written by the example program.
- The text of the usage line alone is byte for byte the same as the usage line of the help for the same command and help options.
- The text is made from the definitions of the schema. `<integer>` is the name of the value type of `--retries`, which has no value name set. `[required]` and `[default: 3]` are annotations made from the required and default value definitions.
- Generation changes neither the schema nor the help options. Nothing that the generating functions return is freed by you.

## Printing `--help` and errors

The library does not treat `--help` specially. `--help` is a flag that you define in the schema. Your code reads from the result whether it was given, generates and prints the help, and ends the program. How to read `--help`, and why to check it before parse errors, is in [Parsing](parse.md#handling---help).

The next example uses `pack`, a made-up program. When `--help` is given, it prints the help to the standard output. On a parse error, it prints the message with the usage line to the standard error stream. You can compile this one as it is, too.

```c
#include <argx/argx.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* The schema of pack. The root owns all of it. */
typedef struct pack_schema
{
    argx_command_t *root;
    argx_command_t *create;
    argx_command_t *list;
} pack_schema_t;

/*
 * Adds -h, --help to command. The library adds no such option by itself,
 * and an option is only known where its command is the current command, so
 * every command gets its own.
 */
static argx_error_t add_help(argx_command_t *command)
{
    argx_option_t *option = NULL;
    argx_error_t error;

    error = argx_command_add_flag(command, 'h', "help", &option);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }
    return argx_option_set_description(option, "Print this help.");
}

/*
 * Defines the schema. When it fails, schema->root is NULL or a schema that
 * can be destroyed.
 */
static argx_error_t define_schema(pack_schema_t *schema)
{
    argx_option_t *option = NULL;
    argx_positional_t *positional = NULL;
    argx_error_t error;

    schema->root = NULL;

    /* The root: pack [-h] [-v] [--[no-]color] [COMMAND]. */
    error = argx_command_create("pack", &schema->root);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }
    error = argx_command_set_description(schema->root, "Pack files into an archive.");
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }
    error = add_help(schema->root);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }
    error = argx_command_add_count(schema->root, 'v', "verbose", &option);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }
    error = argx_option_set_description(option, "Print more; can be repeated.");
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }
    error = argx_command_add_negatable(schema->root, '\0', "color", &option);
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

    /* The subcommand create. */
    error = argx_command_add_subcommand(schema->root, "create", &schema->create);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }
    error = argx_command_set_description(schema->create, "Create an archive.");
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }
    error = add_help(schema->create);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }
    error = argx_command_add_value(schema->create, 'o', "output", argx_type_string(), &option);
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
    error = argx_command_add_value(schema->create, 'l', "level", argx_type_int(), &option);
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
    error = argx_command_add_positional(schema->create, "file", argx_type_string(),
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

    /* The subcommand list. */
    error = argx_command_add_subcommand(schema->root, "list", &schema->list);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }
    error = argx_command_set_description(schema->list, "List the files of an archive.");
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }
    error = add_help(schema->list);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }
    error = argx_command_add_positional(schema->list, "archive", argx_type_string(),
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
    error = argx_command_add_positional(schema->list, "pattern", argx_type_string(),
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

    /*
     * A schema must be validated before it is parsed with, and before a
     * help is generated from it.
     */
    return argx_command_validate(schema->root, NULL, NULL, NULL);
}

/*
 * Tells whether --help or -h was given to one of the commands that the
 * command line selected. This also works on a result with a parse error:
 * it holds what was read before the error.
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
 * user_data points to.
 */
static bool write_to_stream(void *user_data, const char *text, size_t length)
{
    return fwrite(text, 1, length, (FILE *)user_data) == length;
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
         * requires, and that must not hide the help. The help is the one
         * of the command that the command line ends in. Ending the
         * program is up to this code: the library never exits.
         */
        if (argx_help_write(argx_result_command(result), NULL, write_to_stream, stdout) ==
            ARGX_ERROR_NONE)
        {
            status = EXIT_SUCCESS;
        }
    }
    else if (error == ARGX_ERROR_INVALID_COMMAND_LINE)
    {
        /*
         * The message, then the usage line of the command in which the
         * error was found, both to the standard error stream.
         */
        fprintf(stderr, "pack: %s\n", argx_result_error_message(result));
        argx_help_write_usage(argx_result_error_command(result), NULL, write_to_stream,
                              stderr);
        fprintf(stderr, "Try 'pack --help' for more information.\n");
    }
    else
    {
        /* The values are read here; see docs/parse.md. */
        printf("pack: nothing to show in this example\n");
        status = EXIT_SUCCESS;
    }

cleanup:
    /*
     * The result first, then the schema it was made from. Destroying NULL
     * does nothing, so this is also right after a failure.
     */
    argx_result_destroy(result);
    argx_command_destroy(schema.root);

    return status;
}
```

Example runs (a `$` line is the input, and the lines after it are the output):

```
$ pack --help
Usage: pack [OPTIONS] [COMMAND]

Pack files into an archive.

Commands:
  create            Create an archive.
  list              List the files of an archive.

Options:
  -h, --help        Print this help.
  -v, --verbose...  Print more; can be repeated.
      --[no-]color  Color the output. [default: on]

$ pack create --help
Usage: pack create [OPTIONS] --output <FILE> <file>...

Create an archive.

Arguments:
  <file>...            The files to pack.

Options:
  -h, --help           Print this help.
  -o, --output <FILE>  The archive to write. [required]
  -l, --level <LEVEL>  The compression level. [default: 6]

$ pack create a.txt
pack: missing required option '--output'
Usage: pack create [OPTIONS] --output <FILE> <file>...
Try 'pack --help' for more information.

$ pack list a b c
pack: unexpected argument 'c'
Usage: pack list [OPTIONS] <archive> [pattern]
Try 'pack --help' for more information.
```

Points of the example:

- You define `--help` on every command yourself (`add_help`). The library never adds it. The `Options:` list of the help shows only the options in the schema, and `--help` appears in the same form as the other options.
- The command whose help is printed is `argx_result_command` (the last command that the command line selected). `pack create --help` gives the help of `create`.
- The command of the usage line printed with a parse error is `argx_result_error_command` (the current command when the error was found). On a result with a parse error, it is not `NULL`.
- The text of the usage line ends with a newline. Do not add a newline when you print it after the message.
- The library does not end the process. Your code decides the exit status and returns from `main`.
- You change the destination with the data passed to the write function. The example writes the help to `stdout` and the usage line to `stderr`.

## Generating functions

```
argx_error_t argx_help_write(const argx_command_t *command,
                             const argx_help_options_t *options,
                             argx_help_write_fn_t write_fn, void *user_data);
argx_error_t argx_help_write_usage(const argx_command_t *command,
                                   const argx_help_options_t *options,
                                   argx_help_write_fn_t write_fn, void *user_data);
argx_error_t argx_help_format(const argx_command_t *command,
                              const argx_help_options_t *options,
                              char *buffer, size_t size, size_t *length);
argx_error_t argx_help_format_usage(const argx_command_t *command,
                                    const argx_help_options_t *options,
                                    char *buffer, size_t size, size_t *length);
```

| Function | Generates | Destination |
| --- | --- | --- |
| `argx_help_write` | The help | A write function |
| `argx_help_write_usage` | The usage line | A write function |
| `argx_help_format` | The help | Your buffer |
| `argx_help_format_usage` | The usage line | Your buffer |

- `command` may be the root command or a subcommand at any depth. The schema of that command must be validated ([Preconditions](#preconditions)).
- `options` are the help options. If you pass `NULL`, every setting has its default value ([Help options](#help-options)).
- The same command and help options always give the same text, byte for byte, however many times you generate it and in any environment. The text does not depend on the locale, environment variables or the terminal.
- The text of the functions that format into a buffer is the same as the text of the functions that pass it to a write function ([Formatting into a buffer](#formatting-into-a-buffer)).
- The return values, and what is left on failure, are in [On failure](#on-failure).

## Text layout rules

The text is decided by the definitions of the schema and the help options, through the following rules only. You can write tests that compare the generated text, byte for byte, with the string you expect.

The rules in this section are written for a schema whose names (the name of the root command, the names of the positionals, the value names, the names of user-defined value types), default values and choices do not start with a blank, do not end with a blank, and contain no newline. Descriptions are assumed to avoid the forms described in [Notes on writing descriptions](#notes-on-writing-descriptions) (starting with a newline or with a line of blanks only). For other schemas, some statements of this section do not hold. The main differences are in [Limits](#limits) and its notes.

What holds for every text:

- The only line separator that the library adds is `\n`. It does not add `\r`. The text ends with `\n`.
- The library adds no blanks at the end of a line. A `\r` in a string of the schema, and trailing blanks of a name, are written as they are. When the name of the root command starts with a newline, the blank of `Usage: ` is left at the end of the line ([Limits](#limits)).
- The fixed phrases (`Usage:`, `Commands:`, `[OPTIONS]`, `[default: …]` and so on) are in English.
- The strings of the schema (names, descriptions, value names, default values, choices) are written without converting any characters. Upper and lower case are not changed either. Names (including the names of value types) and value names are written byte for byte. Descriptions and annotations (including default values and choices) follow the rules of [Wrapping](#wrapping), so blanks at the end of a line and blanks at the wrapping points are not written. A newline at the end of a description is not written, except when annotations follow ([Annotations](#annotations)). In the run of blanks and newlines at the end of the description of a command, an option or a positional (after the last byte that is neither a blank nor a newline), the blanks are ignored and only the newlines are kept (`"foo\n  "` gives the same text as `"foo\n"`, and `"foo\n \n"` the same as `"foo\n\n"`).

### Structure

The help consists of the following parts, in this order.

| Order | Part | Appears when | Content |
| --- | --- | --- | --- |
| 1 | Usage line | Always | As in the rules of [Usage line](#usage-line) |
| 2 | Description of the command | A description is set | Written from the start of the line. A newline at the end is not written. Wrapped at the width ([Wrapping](#wrapping)) |
| 3 | `Commands:` | There is at least one subcommand | The direct subcommands, one item each. The heading is the name, and the description is the first line of the description text (up to the first newline) |
| 4 | `Arguments:` | There is at least one positional | The positionals, one item each. The description is the whole description text and the annotations |
| 5 | `Options:` | There is at least one option | The options, one item each. The description is the whole description text and the annotations |

- A part whose condition is not met is not written, including its name line.
- One empty line is placed between parts. There is no empty line at the start or at the end of the text. Descriptions that start with a newline or a line of blanks only are the exception ([Notes on writing descriptions](#notes-on-writing-descriptions)).
- A list starts with a line that holds only its name (such as `Commands:`), followed by its items in the order they were defined.
- A list shows only what is defined on the command itself. The options of the parent command, the options and positionals of subcommands, and the subcommands of subcommands are not shown. No option that is not in the schema is added.
- A command cannot have both positionals and subcommands ([Schemas](schema.md#commands)). So one help shows at most two lists: either `Commands:` and `Options:`, or `Arguments:` and `Options:`.
- A description that has only blanks and newlines is treated as not set ([Notes on writing descriptions](#notes-on-writing-descriptions)).

The help of `pack` (the output of the example in [Printing `--help` and errors](#printing---help-and-errors)) consists of the usage line, the description, `Commands:` and `Options:`. The help of `pack create` consists of the usage line, the description, `Arguments:` and `Options:`.

The help of a command `tool` that has no description, no options, no positionals and no subcommands is the usage line alone.

```
Usage: tool
```

The description of a subcommand is only the first line of its description text. The next help is that of a command with `create`, whose description has two lines, and `check`, which has no description.

```
Usage: tool [COMMAND]

Commands:
  create  Create an archive.
  check
```

The help of `create` itself shows the whole description.

```
Usage: tool create

Create an archive.
The files are packed in the order of the command line.
```

### Usage line

The usage line consists of the following items, in this order.

| Order | Item | Appears when | Notation |
| --- | --- | --- | --- |
| 1 | The command path | Always | `Usage: ` followed by the names from the root to the command, separated by one blank |
| 2 | `[OPTIONS]` | There is at least one option that is not required | `[OPTIONS]`. Flags, negatable flags, counts and options with a value that are not required are the options that are not required |
| 3 | Required options | One for each required option with a value, in the order they were defined | `--long <value name>` if it has a long name, otherwise `-s <value name>` |
| 4 | Positionals | One for each positional, in the order they were defined | One of the four forms in the table below |
| 5 | `[COMMAND]` | There is at least one subcommand | `[COMMAND]` |

The notation of a positional depends on its arity. The headings of the `Arguments:` list use the same notation.

| Arity | Notation |
| --- | --- |
| `ARGX_ARITY_REQUIRED` | `<name>` |
| `ARGX_ARITY_OPTIONAL` | `[name]` |
| `ARGX_ARITY_ZERO_OR_MORE` | `[name]...` |
| `ARGX_ARITY_ONE_OR_MORE` | `<name>...` |

The name shown when no value name is set is described in [Headings](#headings).

The usage line of a command with the required options `--output` (value name `FILE`) and `-j` (an integer, with neither a long name nor a value name), and the positionals `source` (required), `target` (optional) and `extra` (zero or more):

```
Usage: tool --output <FILE> -j <integer> <source> [target] [extra]...
```

The usage line of a command with the flag `-q` and the positional `file` (one or more):

```
Usage: tool [OPTIONS] <file>...
```

The usage line of `add`, a subcommand of `remote`, which is itself a subcommand of `tool`, and the usage line of `remote`:

```
Usage: tool remote add [OPTIONS] <name>
Usage: tool remote [COMMAND]
```

Wrapping rules:

- From the second item on, one blank is placed between an item and the one before it.
- However, when the width is not 0 and "the current column + 1 + the width of the item" is greater than the width, a newline and 7 blanks are placed instead of the blank. 7 columns is the width of `Usage: `, so the continuation lines start at the same column as the start of the command path.
- An item is never split. It is not split at a blank in a value name either.
- An item wider than the width goes on a line of its own and is written past the width.
- The command path is written right after `Usage: ` even when it is wider than the width. The path itself is never wrapped.
- When the width is 0, the line is not wrapped.
- The width of an item is its number of bytes.

The usage line of `pack create` is one line at the default width (80). At the width 40, it looks as follows. Putting `--output <FILE>` after `[OPTIONS]` would reach column 44, so the line is wrapped there.

```
Usage: pack create [OPTIONS]
       --output <FILE> <file>...
```

The next usage line is that of a command with a required option whose value name is `FILE OR DIRECTORY`, at the width 20. The item on the second line is 28 columns wide, which is wider than the width, but it is written without being split.

```
Usage: tool
       --output <FILE OR DIRECTORY>
       <source>
```

### Headings

The heading of a subcommand is its name. The heading of a positional is the same as the notation of the positional in the [Usage line](#usage-line). The heading of an option consists of a names part and a continuation that depends on the kind.

The names part:

| Names of the option | Notation |
| --- | --- |
| A short name and a long name | `-s, --long` |
| A short name only | `-s` |
| A long name only | 4 blanks and `--long`. The long name lines up with the long names of options that have a short name |

The continuation for each kind:

| Kind | Notation | Example |
| --- | --- | --- |
| Flag | The names part only | `-h, --help` |
| Negatable flag | The long name is written as `--[no-]long` | `--[no-]color` |
| Count | `...` right after the names part | `-v, --verbose...` |
| Option with a value | After the names part, one blank, `<`, the value name and `>` | `-o, --output <FILE>` |

For an option with a value that has no value name set (`argx_option_set_value_name`), the name of the value type is shown in place of the value name. The names of the built-in value types are `string`, `integer` and `float`. The name of a choice value type is `choice`, and the name of a user-defined value type is the name passed when the value type was created. The same applies to the required options in the usage line.

A help that shows every notation:

```
Usage: tool [OPTIONS]

Options:
  -h, --help           Both names
  -q                   A short name only
      --dry-run        A long name only
      --[no-]color     A negatable flag
  -c, --[no-]check     A negatable flag with a short name
  -v, --verbose...     A count
  -d...                A count with a short name only
  -o, --output <FILE>  A value with a value name
  -j <integer>         A value without one: the name of the type
      --ratio <float>  A float
      --name <string>  A string
      --mode <choice>  A choice [choices: fast, small]
```

- If you define `--help` as a flag, it appears in the same form as the other flags.
- The width of a heading is its number of bytes.

### Annotations

Annotations follow the description of an option or a positional, in the following order. Subcommands have no annotations.

| Order | Annotation | Appears when |
| --- | --- | --- |
| 1 | `[choices: a, b, c]` | An option with a value or a positional whose value type is a choice. The choices are listed in the order they were defined, separated by `, ` |
| 2 | `[default: value]` | An option with a value or a positional that has a default value set. The value is the string that was set, unchanged. However, annotations are wrapped too, so blanks at the end of a line and blanks at the wrapping points inside the value are not written |
| 2 | `[default: on]` | A negatable flag whose initial state is on. When it is off, there is no annotation |
| 3 | `[required]` | A required option with a value. It does not appear on positionals: whether a positional is required can be seen from the notation of its heading |

- One blank is placed between the description and an annotation, and between annotations.
- On an item that has no description set, the annotations start at the description column.
- When the description ends with a newline, the annotations start at the description column of the next line, with no blank before them. You can use this to put the annotations on a line of their own. When the description ends with two or more newlines, the newlines from the second one on become empty lines between the description and the annotations. On an item with no annotations, a newline at the end of the description is not written.
- Annotations are wrapped in the same way as descriptions. They are also wrapped at the blanks inside an annotation ([Wrapping](#wrapping)).
- An option that is required and has a default value is an error in validation ([Schemas](schema.md#errors-found-by-validation)). So `[default: …]` and `[required]` never appear on the same item.

In the next help, `--kind` has no description. The description of `--title` ends with a newline (`"The title of the archive.\n"`). The initial state of `--color` is on, and that of `--sort` is off.

```
Usage: tool [OPTIONS] --output <FILE> --kind <choice> <format> [level]

Arguments:
  <format>             The format of the archive. [choices: tar, zip, cpio]
  [level]              The compression level. [default: 6]

Options:
  -m, --mode <MODE>    What to prefer. [choices: fast, small] [default: fast]
  -o, --output <FILE>  The archive to write. [required]
  -k, --kind <choice>  [choices: fast, small] [required]
      --[no-]color     Color the output. [default: on]
      --[no-]sort      Sort the names.
  -t, --title <TEXT>   The title of the archive.
                       [default: untitled]
```

### Description column

- An item starts with as many blanks as the indent, followed by the heading. The indent is a help option (2 by default).
- The description starts at the description column. There is one description column per help, shared by all the lists of that help.
- An item with no description (neither a description text nor annotations) is a line with only the heading.

The description column is decided as follows.

1. Decide the maximum. The maximum is the smaller of the "maximum description column" help option (32 by default) and half of the width (rounded down). When the width is 0, half of the width is not considered.
2. If the maximum is less than "indent + 2", the maximum becomes "indent + 2".
3. The description column is "indent + the width of the widest heading in that help + 2". If this is greater than the maximum, the description column is the maximum.

The form of the line of an item depends on the width of its heading.

| Condition | Form of the line |
| --- | --- |
| "indent + width of the heading + 2" is not greater than the description column | The description starts at the description column on the same line as the heading. There are 2 or more blanks between the heading and the description |
| "indent + width of the heading + 2" is greater than the description column | The heading is on a line of its own, and the description starts at the description column of the next line |

In the help of `pack create` (the output of the example in [Printing `--help` and errors](#printing---help-and-errors)), the widest headings are `-o, --output <FILE>` and `-l, --level <LEVEL>`, 19 columns each. The description column is 2 + 19 + 2 = 23, which does not exceed the maximum 32, so the descriptions of both `Arguments:` and `Options:` start at column 23.

In the next help, with the default help options, the widest heading is 41 columns. The description column becomes the maximum, 32. The heading of `--exactly-two-blanks-fit` is 28 columns, and 2 + 28 + 2 = 32, so its description is on the same line. The description of `--one-blank-is-not-enough`, which is one column wider, is on the next line. The last option has no description, so its line holds only the heading.

```
Usage: tool [OPTIONS]

Options:
  -v, --verbose...              Say more
      --a-rather-long-name-of-option <FILE>
                                Write to FILE
      --exactly-two-blanks-fit  On the line of the heading
      --one-blank-is-not-enough
                                On the next line
      --a-long-name-without-a-description
```

At the width 40, the maximum is half of the width, 20.

```
Usage: tool [OPTIONS]

Options:
  -o, --output <FILE>
                    The archive to
                    write.
      --compression-level <LEVEL>
                    The compression
                    level.
```

### Wrapping

The description of the command and the descriptions of the items are wrapped so that they do not exceed the width. The description of the command starts at column 0, the description of an item starts at the description column, and the continuation lines start at the same column.

- A word is a run of bytes other than the blank (0x20) and the newline (0x0A). Other bytes, such as tabs, are treated as part of a word.
- Lines are wrapped only at the blanks between words. A word is never split.
- When placing a word would exceed the width, the line is wrapped before that word. The blanks at the wrapping point are not written.
- The first word of a line is written as it is, even when it exceeds the width. A word that does not fit between the description column and the width is written past the width.
- A newline in a description always starts a new line. The continuation starts at the same column. Consecutive newlines give empty lines.
- Consecutive blanks in the middle of a line, and blanks right after a newline, are written with the same count. With blanks right after a newline, you can write indented lines.
- Blanks at the end of a line are not written.
- When the width is 0, lines are broken only at the newlines in the description.

A help at the width 60. The description of the command has an empty line (consecutive newlines). The description of `--exclude` contains newlines and blanks at the start of lines. The description of `--url` has a word that does not fit in the width.

```
Usage: tool [OPTIONS] --output <FILE>

Pack the files that are named on the command line into one
archive, in the order in which they are named.

An archive that exists is replaced.

Options:
  -o, --output <FILE>      The archive to write. A file of
                           this name that exists is replaced
                           without a question. [required]
  -x, --exclude <PATTERN>  Leave out the files that match:
                             *.o    one kind of file
                             build/ a directory
  -u, --url                See
                           https://example.com/a/very/long/address/that/has/no/blank/in/it/at/all
                           for more.
```

## Help options

The help options (`argx_help_options_t`) are an object that holds the values that decide the layout of the text. You create them, pass them to the generating functions, and destroy them.

| To do this | Function | Notes |
| --- | --- | --- |
| Create | `argx_help_options_create` | Every setting has its default value. When the allocation fails, it returns `ARGX_ERROR_OUT_OF_MEMORY` and `*out` is `NULL` |
| Destroy | `argx_help_options_destroy` | `NULL` does nothing |
| Set the width | `argx_help_options_set_width` | Cannot fail |
| Set the indent | `argx_help_options_set_indent` | Cannot fail |
| Set the maximum description column | `argx_help_options_set_max_column` | Cannot fail |

| Setting | Meaning | Default value |
| --- | --- | --- |
| Width | The number of columns of one line that wrapping is based on. 0 means "no limit", and nothing is wrapped | 80 |
| Indent | The number of blanks placed before the items of a list | 2 |
| Maximum description column | The column that the description column does not exceed. However, when the width is not 0 and half of the width (rounded down) is smaller, that half is the maximum. Then, if the maximum is less than "indent + 2", "indent + 2" is the maximum. Because "indent + 2" takes priority, in that case the description column can exceed the maximum you set or half of the width ([Description column](#description-column)) | 32 |

- The limit of every setting is 65535. A value greater than 65535 is treated as 65535. The set functions do not reject values.
- If you pass `NULL` as the `options` of a generating function, every setting has its default value. The text is the same as with help options just created.
- The help options also apply to the usage line. The usage line uses only the width.
- Generation does not change the help options. You can use one help options object for any number of generations, and for generations from different schemas.
- Help options that are not being changed can be passed to generation from several threads at the same time. While a set function or the destroy function is being called, no other thread may use those help options.
- The library looks at neither the terminal width nor environment variables. To match the width of the terminal, find the width yourself and set it as the width.
- The contents of the help options are not public. Later versions may add settings and their set functions.

The next function prints the help to the standard output at the width 40 (`write_to_stream` is the one from the example in [Printing `--help` and errors](#printing---help-and-errors)).

```c
/* Prints the help of command to the standard output, 40 columns wide. */
static argx_error_t print_narrow_help(const argx_command_t *command)
{
    argx_help_options_t *options = NULL;
    argx_error_t error;

    /* The settings start with their default values: 80, 2 and 32. */
    error = argx_help_options_create(&options);
    if (error != ARGX_ERROR_NONE)
    {
        /* ARGX_ERROR_OUT_OF_MEMORY: options is NULL. */
        return error;
    }

    /* The functions that set a value cannot fail. */
    argx_help_options_set_width(options, 40);

    error = argx_help_write(command, options, write_to_stream, stdout);

    /* The settings belong to the caller. */
    argx_help_options_destroy(options);
    return error;
}
```

The output when `pack create` is passed:

```
Usage: pack create [OPTIONS]
       --output <FILE> <file>...

Create an archive.

Arguments:
  <file>...         The files to pack.

Options:
  -h, --help        Print this help.
  -o, --output <FILE>
                    The archive to
                    write. [required]
  -l, --level <LEVEL>
                    The compression
                    level. [default: 6]
```

Half of the width, 20, becomes the maximum description column, and the descriptions of `--output` and `--level` are on the next line.

## Output

The generating functions pass the text to a write function, or write it into your buffer. The library does not use `<stdio.h>`, and writes nothing anywhere you did not specify. There is no function that writes directly to a `FILE *`. To write to a `FILE *`, write a write function that calls `fwrite()` ([Writing to standard output](#writing-to-standard-output)).

### The write function contract

```
typedef bool (*argx_help_write_fn_t)(void *user_data, const char *text, size_t length);
```

The library passes the text to the write function from the start, in order, split into several parts.

- `user_data` is the pointer passed to the generating function, passed on unchanged. The library does not read what it points to. It may be `NULL`.
- `text` points to the start of one part of the text, and `length` is its number of bytes. **No terminating null character follows.** Read exactly `length` bytes.
- `length` is 1 or greater. The function is never called with the length 0.
- The calls are made in order from the start of the text. Joining the parts in the order of the calls gives the whole text.
- How many parts the text is split into, and where, is not specified. It may change between versions. Do not assume that a part is one line or one word.
- `text` is valid only during that call. If you use it later, copy it inside the call.
- Return `true` if the part was written, and `false` if it could not be written. When it returns `false`, generation stops there, and the write function is not called again for that generation. The generating function returns `ARGX_ERROR_WRITE_FAILED`.
- Inside the write function, do not change or destroy the schema passed to the generation.

### Writing to standard output

A write function that writes to a `FILE *` only calls `fwrite()`. Pass the stream as `user_data`.

```c
/*
 * The write function: writes one part of the text to the stream that
 * user_data points to. The part is not terminated, so it is written with
 * its length.
 */
static bool write_to_stream(void *user_data, const char *text, size_t length)
{
    return fwrite(text, 1, length, (FILE *)user_data) == length;
}
```

```
error = argx_help_write(root, NULL, write_to_stream, stdout);
```

- If `fwrite()` wrote fewer bytes than `length`, it returns `false`. Generation stops, and `ARGX_ERROR_WRITE_FAILED` is returned.
- `text` is not terminated, so it cannot be passed to `fputs()` or `printf("%s")`.
- The write function is called many times, in units of words and blanks. A `FILE *` is buffered by the standard library, so this is not a problem. When you write to a destination that is not buffered, collect the parts in the write function.
- **On a stream in text mode, newlines are converted on Windows.** The only newline that the library passes is `\n`. `stdout` is usually in text mode, and on Windows each `\n` written comes out as `\r\n`. For output to a terminal, this is correct. When you compare the output byte for byte, or keep `\n` as it is in a file, write to a stream opened in binary mode (add `b` to the mode of `fopen()`).

### Formatting into a buffer

`argx_help_format` and `argx_help_format_usage` write the text as a string into an array that you provide. No write function is needed.

| `size` | What is written into `buffer` |
| --- | --- |
| 0 | Nothing. `buffer` may be `NULL` |
| 1 or greater | As much of the text from the start as fits (at most `size - 1` bytes), followed by a terminating null character. Nothing is written outside the array |

- If `length` is not `NULL`, `*length` receives the length of the whole text (not counting the terminating null character), whether or not it fit.
- A text that does not fit is truncated. **Truncation is not a failure: `ARGX_ERROR_NONE` is returned.** If `*length` is `size` or greater, the text was truncated. A truncated text may end in the middle of a line.
- The text is byte for byte the same as the text that `argx_help_write` and `argx_help_write_usage` pass to a write function for the same command and help options.

To get the whole text, call twice. The first call passes `size` 0 and `buffer` `NULL`, and gets the length. The second call passes an array of `*length + 1` bytes.

```c
/*
 * Returns the help of command as a string that the caller frees with
 * free(), or NULL when it cannot be made.
 */
static char *help_string(const argx_command_t *command)
{
    char *text = NULL;
    size_t length = 0;

    /*
     * First the length: with the size 0 nothing is written, and the buffer
     * may be NULL.
     */
    if (argx_help_format(command, NULL, NULL, 0, &length) != ARGX_ERROR_NONE)
    {
        return NULL;
    }

    text = malloc(length + 1);
    if (text == NULL)
    {
        return NULL;
    }

    /*
     * Then the text. The schema and the settings are the same as in the
     * first call, so the text has the same length and fits.
     */
    if (argx_help_format(command, NULL, text, length + 1, NULL) != ARGX_ERROR_NONE)
    {
        free(text);
        return NULL;
    }
    return text;
}
```

The next example receives the usage line in a fixed-size array. `argx_help_format_usage` cannot fail, so the return value is not checked.

```c
/* Prints an error message with the usage line of command behind it. */
static void print_error(const argx_command_t *command, const char *message)
{
    char usage[256];
    size_t length = 0;

    /* The usage line cannot fail. It is cut when it does not fit. */
    argx_help_format_usage(command, NULL, usage, sizeof(usage), &length);

    if (length >= sizeof(usage))
    {
        /* Cut: the array holds the first sizeof(usage) - 1 bytes. */
        fprintf(stderr, "pack: %s\n", message);
    }
    else
    {
        /* The text ends with a newline of its own. */
        fprintf(stderr, "pack: %s\n%s", message, usage);
    }
}
```

## Limits

- **Widths are counted in bytes.** One byte counts as one column, so a description that contains non-ASCII characters (in UTF-8, one character is 2 bytes or more) is wrapped before its visible width reaches the width. A heading with a name that contains non-ASCII characters counts as wider than it looks, and the description column moves to the right. The library does not look up the display width of characters.

  The next help is at the width 50. The description of `--quiet` is in Greek, where each letter is 2 bytes in UTF-8. It fits in 50 columns visually, but it does not fit when counted in bytes, so it is wrapped.

  ```
  Usage: tool [OPTIONS]

  Options:
    -q, --quiet    Μην εκτυπώνεις
                   τίποτα
    -v, --verbose  Print more than usual
  ```

- **A newline in a name breaks the columns.** The name of a positional, a value name, the name of the root command and the name of a user-defined value type can be defined with strings that contain newlines. The help writes them unconverted, newlines included. The width of a heading or of an item of the usage line is then counted as the number of bytes of the whole, newlines included. In the output, however, a newline starts a new line, and the rest starts at the start of the next line. So the blanks up to the description column and the wrapping points of the usage line no longer match what you see. Do not put newlines in names and value names.

  The next help is that of a command with a positional named `"first\nsecond"`. The heading `<first\nsecond>` counts as 14 columns, and the description column is 18. In the output, `second>` starts at the start of the line, so the blanks before the description look too few.

  ```
  Usage: tool [OPTIONS] <first
  second>

  Arguments:
    <first
  second>           A name with a newline

  Options:
    -q, --quiet     Print nothing
  ```

- **Blanks in names and `\r` in strings are written as they are.** Blanks at the end of a name, and blanks right before a newline in a name, are written as they are, so a line can end with a blank (the usage line of a root command named `"tool "` with no options, positionals or subcommands is `Usage: tool `, ending with a blank). When the name of the root command starts with a newline, the blank of `Usage: ` is left at the end of the line (if the name is `"\ntool"`, the first line is `Usage: ` and ends with a blank). A `\r` in a string of the schema (names, descriptions, value names, default values, choices) is also written as it is.
- Tabs and control characters also count as one column per byte.
- The fixed phrases are in English only. There is no way to change their language, to add colors, or to reorder the parts of the help.
- There is no way to hide an option from the list, or to put options into groups.
- The help of a subcommand does not show the options of its parent command.

### Notes on writing descriptions

- A description that has no bytes other than the blank (0x20) and the newline (0x0A) is treated as not set. The empty string, `"   "` and `" \n"` all mean "no description". This is the same for the descriptions of commands, options and positionals. For the description of a subcommand in the `Commands:` list, only the first line of its description text is considered. If the first line has only blanks, the list shows a line with only the heading, even when more lines follow.
- **Do not start a description with a newline, or with a line of blanks only (a line that has one or more blanks and no other bytes).** In the description of a command (in the help of that command itself) and in the descriptions of options and positionals, a leading newline and a leading line of blanks only are not removed. So the text may get extra empty lines, and the position where a description or annotations start may change. "One empty line between parts" in [Structure](#structure) may no longer hold. For example, the help of a command whose description is `"\nPack files."` is the following.

  ```
  Usage: tool [OPTIONS]


  Pack files.

  Options:
    -q, --quiet    Print nothing.
    -v, --verbose  Print more.
  ```

  The text for such descriptions may change in later versions.

## On failure

There are two runtime failures: a write failure and an allocation failure. Both are reported by the returned `argx_error_t`.

| Return value | Meaning | Returned by |
| --- | --- | --- |
| `ARGX_ERROR_NONE` | Success. For the functions that format into a buffer, truncation is also a success | All |
| `ARGX_ERROR_WRITE_FAILED` | The write function returned `false` | `argx_help_write`, `argx_help_write_usage` |
| `ARGX_ERROR_OUT_OF_MEMORY` | An allocation failed | `argx_help_write`, `argx_help_format`, `argx_help_options_create` |

**When generation fails, the text passed to the write function before the failure stays at the destination.** The library cannot take back text it has passed.

| Function | Failure | What is left at the destination |
| --- | --- | --- |
| `argx_help_write`, `argx_help_write_usage` | Write failure | The parts received by the calls in which the write function returned `true`. This is the text from the start up to some position. It may end in the middle of a line |
| `argx_help_write` | Allocation failure | The text from the start up to the line of the item (an option or a positional) that needed the allocation. It ends with the newline of the line before |
| `argx_help_format` | Allocation failure | If `size` is not 0, `buffer` holds the empty string. `*length` is 0. If `size` is 0, nothing is written into `buffer` |
| `argx_help_options_create` | Allocation failure | `*out` is `NULL` |

- After a write failure, the write function is not called again for that generation.
- When `argx_help_format` fails to allocate, `buffer` holds the empty string, but the contents of the bytes of the array after the terminating null character are unspecified. Part of the text generated before the failure may be left there. Nothing is written outside the array.
- Generating the usage line alone (`argx_help_write_usage`, `argx_help_format_usage`) allocates no memory. It never returns `ARGX_ERROR_OUT_OF_MEMORY`. `argx_help_format_usage` cannot fail.
- `argx_help_format` does not return `ARGX_ERROR_WRITE_FAILED`.
- A failure changes neither the schema nor the help options. All memory allocated for the generation is freed before the function returns, on success and on failure. After a failure, you can generate again with the same schema and help options.
- If you do not want partial text left at the destination, format into a buffer with `argx_help_format`, check that it succeeded, and then write it out.
- A `NULL` argument, or a schema that is not validated, is not a failure but a programming error ([Preconditions](#preconditions)).

## Lifetime rules

| Thing | Owner | Valid for |
| --- | --- | --- |
| Help options (`argx_help_options_t`) | You, who created them | Until destroyed with `argx_help_options_destroy`. They are independent of the schema, and either may be destroyed first |
| `text` passed to the write function | The library or the schema | Only during that call. You neither free nor change it |
| Buffer (`buffer`) | You | It needs to be valid only during the call of the generation. The library does not refer to it after the call returns |
| The schema and the value types it uses | You | Neither changed nor destroyed during the call of the generation |

- The generating functions borrow the schema and the help options only during the call. They change neither of them, and do not refer to them after returning.
- Nothing that the generating functions return is freed by you. No function returns a string allocated by the library.
- Memory for the generation is allocated from the allocator set for the library (`argx_set_allocator`), and freed before the function returns. The help options object is also allocated from the same allocator. Do not set the allocator again while a help options object still exists ([Ownership and Lifetime](ownership.md#5-allocator)).
- While the schema and the help options are not being changed, generation with the same schema and help options can be called from several threads at the same time. Pass a different buffer to each call. Whether the write function works correctly when called at the same time depends on your write function.

## Preconditions

Breaking a precondition is a programming error. No error code is returned. Preconditions that can be checked are checked with `assert`, and in the Debug configuration the process aborts when one is broken. In the Release configuration they are not checked, and the behavior is undefined ([Ownership and Lifetime](ownership.md#runtime-failures-and-programming-errors)). The preconditions of each function are also written in the comments of the public header `include/argx/help.h`, under `Preconditions:`.

| Function | Preconditions checked with `assert` | Preconditions not checked |
| --- | --- | --- |
| `argx_help_options_create` | `out` is not `NULL` | None |
| `argx_help_options_destroy` | None | `options` is `NULL` or help options that have not been destroyed. No other thread is using those help options |
| `argx_help_options_set_width`, `argx_help_options_set_indent`, `argx_help_options_set_max_column` | `options` is not `NULL` | `options` has not been destroyed. During the call, no other thread is using those help options |
| `argx_help_write`, `argx_help_write_usage` | `command` is not `NULL`. The schema of `command` has been validated (`argx_command_is_validated`). `write_fn` is not `NULL` | The ones common to generation. The write function neither changes nor destroys the schema |
| `argx_help_format`, `argx_help_format_usage` | `command` is not `NULL`. The schema of `command` has been validated (`argx_command_is_validated`). If `size` is not 0, `buffer` is not `NULL` | The ones common to generation. `buffer` points to an array of at least `size` bytes |

The preconditions common to generation, not checked:

- `options` is `NULL` or help options that have not been destroyed. During the call, no other thread is changing those help options.
- During the call, the schema and the value types it uses are neither changed nor destroyed.

Arguments that may be `NULL`:

- The `options` of the generating functions (the default values are used).
- The `user_data` of `argx_help_write` and `argx_help_write_usage`.
- The `length` of `argx_help_format` and `argx_help_format_usage` (the length is not received), and `buffer` when `size` is 0.
- The `options` of `argx_help_options_destroy`.

Passing a schema that is not validated to generation violates a precondition. Adding a definition element to the schema or setting an attribute makes the schema unvalidated again, so validate it again before generating. When you pass a subcommand, it is still its root that you validate ([Schemas](schema.md#validation-and-finding-the-cause-of-a-failure)).
