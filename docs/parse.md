# Parsing

This document explains how to parse argv with a validated schema, read the values from the result, and destroy the result. It also describes the forms that the user of your program can write on the command line (the command-line syntax), and the parse errors that are returned when the arguments do not match the schema.

This document covers parsing, reading the result, parse errors and destroying the result. Defining and validating a schema is in [Schemas](schema.md). Generating the help text and the usage line is in [Help and Usage](help.md). Parsing prints nothing, does not end the process, and does not read environment variables.

The detailed description and the preconditions of each function are in the comments of the public header `include/clargs/parse.h`. The rules for who owns memory are in [Ownership and Lifetime](ownership.md).

Contents:

- [A complete example](#a-complete-example)
- [Entry points](#entry-points)
- [Command-line syntax](#command-line-syntax)
- [Required checks and default values](#required-checks-and-default-values)
- [Reading the result](#reading-the-result)
- [Parse errors](#parse-errors)
- [Handling `--help`](#handling---help)
- [Wide-character argv](#wide-character-argv)
- [Lifetime rules](#lifetime-rules)
- [Preconditions](#preconditions)

## A complete example

The example uses `pack`, a made-up program. It validates the schema, parses, checks for `--help`, prints parse errors, reads the values and destroys everything. It accepts command lines of this form:

```
pack [-h] [-v]... [--color | --no-color] create [-h] -o FILE [-l LEVEL] file...
pack [-h] [-v]... [--color | --no-color] list [-h] archive [pattern]
```

The example is one translation unit, and it includes only `<clargs/clargs.h>` and standard headers. You can copy it out and compile it as it is.

```c
#include <clargs/clargs.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* The schema of pack, and the handles that the result is read with. The
 * root owns all of them. */
typedef struct pack_schema {
    clargs_command_t *root;
    clargs_command_t *create;
    clargs_command_t *list;
    clargs_option_t *verbose;     /* pack: -v, --verbose; a count */
    clargs_option_t *color;       /* pack: --color, --no-color */
    clargs_option_t *output;      /* create: -o, --output FILE; required */
    clargs_option_t *level;       /* create: -l, --level LEVEL; 6 if not given */
    clargs_positional_t *files;   /* create: file... */
    clargs_positional_t *archive; /* list: archive */
    clargs_positional_t *pattern; /* list: [pattern]; "*" if not given */
} pack_schema_t;

/* Keeps the result of a call in *error and tells whether the call failed. */
static bool failed(clargs_error_t *error, clargs_error_t result)
{
    *error = result;
    return result != CLARGS_ERROR_NONE;
}

/* Adds -h, --help to command. An option is only known where its command is
 * the current command, so every command gets its own. */
static clargs_error_t add_help(clargs_command_t *command)
{
    return clargs_command_add_flag(command, 'h', "help", NULL);
}

/* Defines the schema. When it fails, schema->root is NULL or a schema that
 * can be destroyed. */
static clargs_error_t define_schema(pack_schema_t *schema)
{
    clargs_error_t error;

    schema->root = NULL;

    if (failed(&error, clargs_command_create("pack", &schema->root)) ||
        failed(&error, add_help(schema->root)) ||
        failed(&error, clargs_command_add_count(schema->root, 'v', "verbose", &schema->verbose)) ||
        failed(&error, clargs_command_add_negatable(schema->root, '\0', "color", &schema->color)) ||
        failed(&error, clargs_option_set_initial(schema->color, true))) {
        return error;
    }

    if (failed(&error, clargs_command_add_subcommand(schema->root, "create", &schema->create)) ||
        failed(&error, add_help(schema->create)) ||
        failed(&error, clargs_command_add_value(schema->create, 'o', "output", clargs_type_string(),
                                                &schema->output)) ||
        failed(&error, clargs_option_set_required(schema->output, true)) ||
        failed(&error, clargs_command_add_value(schema->create, 'l', "level", clargs_type_int(),
                                                &schema->level)) ||
        failed(&error, clargs_option_set_default(schema->level, "6")) ||
        failed(&error, clargs_command_add_positional(schema->create, "file", clargs_type_string(),
                                                     CLARGS_ARITY_ONE_OR_MORE, &schema->files))) {
        return error;
    }

    if (failed(&error, clargs_command_add_subcommand(schema->root, "list", &schema->list)) ||
        failed(&error, add_help(schema->list)) ||
        failed(&error, clargs_command_add_positional(schema->list, "archive", clargs_type_string(),
                                                     CLARGS_ARITY_REQUIRED, &schema->archive)) ||
        failed(&error, clargs_command_add_positional(schema->list, "pattern", clargs_type_string(),
                                                     CLARGS_ARITY_OPTIONAL, &schema->pattern)) ||
        failed(&error, clargs_positional_set_default(schema->pattern, "*"))) {
        return error;
    }

    /* A schema must be validated before it is parsed with. */
    return clargs_command_validate(schema->root, NULL, NULL, NULL);
}

/* Tells whether --help or -h was given to one of the commands that the
 * command line selected. This also works on a result with a parse error:
 * it holds what was read before the error. */
static bool help_was_given(const clargs_result_t *result)
{
    size_t i;

    for (i = 0; i < clargs_result_command_count(result); ++i) {
        const clargs_command_t *command = clargs_result_command_at(result, i);
        const clargs_option_t *help = clargs_command_find_long(command, "help", 4, NULL);

        if (help != NULL && clargs_result_flag(result, help)) {
            return true;
        }
    }
    return false;
}

/* Prints the usage of the command that the command line ends in. */
static void print_usage(const pack_schema_t *schema, const clargs_command_t *command)
{
    if (command == schema->create) {
        printf("usage: pack create -o FILE [-l LEVEL] file...\n");
    } else if (command == schema->list) {
        printf("usage: pack list archive [pattern]\n");
    } else {
        printf("usage: pack [-v]... [--color | --no-color] <command> ...\n");
        printf("commands: create, list\n");
    }
}

/* The text of a source, for this example. Later versions may add sources,
 * so the switch has a default label. */
static const char *source_text(clargs_source_t source)
{
    switch (source) {
    case CLARGS_SOURCE_COMMAND_LINE:
        return "command line";
    case CLARGS_SOURCE_DEFAULT:
        return "default";
    default:
        return "none";
    }
}

/* Reads the values of a command line that matched the schema. */
static int run(const pack_schema_t *schema, const clargs_result_t *result)
{
    const clargs_command_t *command = clargs_result_command(result);

    if (command != schema->create && command != schema->list) {
        /* A command line that names no subcommand is not a parse error. */
        fprintf(stderr, "pack: a command is required\n");
        return EXIT_FAILURE;
    }

    /* The options of the root: a count and a negatable flag. */
    printf("verbose: %lu, color: %s\n", (unsigned long)clargs_result_count(result, schema->verbose),
           clargs_result_flag(result, schema->color) ? "on" : "off");

    if (command == schema->create) {
        const char *output = NULL;
        const char *file = NULL;
        int64_t level = 0;
        size_t i;

        /* --output is required and --level has a default value: after a
         * parse that succeeded, both have a value. */
        if (!clargs_result_option_string(result, schema->output, &output) ||
            !clargs_result_option_int(result, schema->level, &level)) {
            return EXIT_FAILURE;
        }
        printf("create %s, level %" PRId64 " (%s)\n", output, level,
               source_text(clargs_result_option_source(result, schema->level)));

        /* A positional that takes any number of arguments: its values in
         * the order of the command line. */
        for (i = 0; i < clargs_result_positional_count(result, schema->files); ++i) {
            if (clargs_result_positional_string(result, schema->files, i, &file)) {
                printf("  %s\n", file);
            }
        }
    } else {
        const char *archive = NULL;
        const char *pattern = NULL;

        if (!clargs_result_positional_string(result, schema->archive, 0, &archive) ||
            !clargs_result_positional_string(result, schema->pattern, 0, &pattern)) {
            return EXIT_FAILURE;
        }
        printf("list %s, pattern %s (%s)\n", archive, pattern,
               source_text(clargs_result_positional_source(result, schema->pattern)));
    }

    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
    pack_schema_t schema;
    clargs_result_t *result = NULL;
    clargs_error_t error;
    int status = EXIT_FAILURE;

    if (failed(&error, define_schema(&schema))) {
        fprintf(stderr, "pack: cannot define the command line: %s\n", clargs_error_message(error));
    } else {
        /* argc and argv go in as main() received them. */
        error = clargs_parse(schema.root, argc, argv, &result);

        if (result == NULL) {
            /* CLARGS_ERROR_OUT_OF_MEMORY: there is no result. */
            fprintf(stderr, "pack: %s\n", clargs_error_message(error));
        } else if (help_was_given(result)) {
            /* --help first: "pack create --help" leaves out what create
             * requires, and that must not hide the help. */
            print_usage(&schema, clargs_result_command(result));
            status = EXIT_SUCCESS;
        } else if (error == CLARGS_ERROR_INVALID_COMMAND_LINE) {
            /* The message is one line with no prefix and no line break. */
            fprintf(stderr, "pack: %s\n", clargs_result_error_message(result));
        } else {
            status = run(&schema, result);
        }

        if (status != EXIT_SUCCESS) {
            fprintf(stderr, "Try 'pack --help' for more information.\n");
        }
    }

    /* The result first, then the schema it was made from. A parse error
     * also leaves a result to destroy. Destroying NULL does nothing. */
    clargs_result_destroy(result);
    clargs_command_destroy(schema.root);

    return status;
}
```

Example runs (the `$` lines are the input, and the lines after them are the output):

```
$ pack -vv create -o out.tar a.txt b.txt
verbose: 2, color: on
create out.tar, level 6 (default)
  a.txt
  b.txt

$ pack --no-color create -l9 --output=out.tar -- -x.txt
verbose: 0, color: off
create out.tar, level 9 (command line)
  -x.txt

$ pack create a.txt
pack: missing required option '--output'
Try 'pack --help' for more information.

$ pack create --help
usage: pack create -o FILE [-l LEVEL] file...
```

Notes on the example:

- What you pass to the parse is the root of a validated schema, and the `argc` and `argv` that `main` received. No conversion or cast is needed.
- There are three possible return values. With `CLARGS_ERROR_NONE` (success) and `CLARGS_ERROR_INVALID_COMMAND_LINE` (a parse error), a result is written to `result`. With `CLARGS_ERROR_OUT_OF_MEMORY`, `NULL` is written.
- Check whether `--help` was given before you look at the parse error. Even from a result with a parse error, you can read what was read before the error ([Handling `--help`](#handling---help)).
- The `print_usage` of the example writes the usage text by hand, to keep the example about parsing. How to generate the help and the usage line from the schema is in [Help and Usage](help.md).
- The message of a parse error is the one line that `clargs_result_error_message` returns. The code that prints it adds a prefix such as the program name, and the line break.
- A value is read by the handle that you received when you defined the schema (such as `schema.output`), not by a name string.
- Destroy the result first, then the schema. You also destroy a result that has a parse error.

## Entry points

```
clargs_error_t clargs_parse(const clargs_command_t *root, int argc, char *const *argv,
                            clargs_result_t **out);
clargs_error_t clargs_parse_wide(const clargs_command_t *root, int argc, wchar_t *const *argv,
                                 clargs_result_t **out);
```

`clargs_parse` parses a `char` argv. `clargs_parse_wide` converts a wide-character argv to UTF-8 and then parses it in the same way ([Wide-character argv](#wide-character-argv)).

| Return value | `*out` | Meaning |
|---|---|---|
| `CLARGS_ERROR_NONE` | A result | Success. The values have been resolved |
| `CLARGS_ERROR_INVALID_COMMAND_LINE` | A result | A parse error. From the result, you can read the details of the error and what was read before the error |
| `CLARGS_ERROR_OUT_OF_MEMORY` | `NULL` | An allocation failed. Nothing is left |

- If `*out` is not `NULL`, you destroy it with `clargs_result_destroy`. **A result is passed to you on a parse error too.** This is an exception to the rule of the other functions, "on failure, the output is `NULL`".
- When the conversion function of a value type returns `CLARGS_ERROR_OUT_OF_MEMORY`, it is also treated as an allocation failure.
- `argv[0]` is the program name, and it is not parsed. If `argc` is 0 or 1, the command line is parsed as one in which nothing was given. `argv[argc]` is not read.
- What you pass as `root` is the root of a validated schema. Passing a schema that has not been validated, or a subcommand, breaks a [precondition](#preconditions), and no error code is returned for it.
- The parse does not change the schema, the value types, the argv array or the argv strings. No state is kept between calls, and the same schema and the same argv always give the same result. While the schema is not changed, several threads can parse with the same schema at the same time.
- Elements are read once each, from the first to the last. The parse stops at the first element that does not match the schema.

## Command-line syntax

These are the forms that the user of your program can write on the command line. An "element" is one string of argv. The "current command" is the root at first, and it becomes a subcommand each time the name of that subcommand appears.

### How elements are classified

The rows are tried from the top.

| Form of the element | Condition | Treatment |
|---|---|---|
| `--` | Before the terminator | The terminator. Everything after it is a positional |
| `--x…` | Before the terminator | A long option. It is split into the name and the value at the first `=` |
| `-` | — | An element that is not an option |
| `-N…`, `-.N…` (N is a digit) | The current command has no short name that is a digit | An element that is not an option |
| `-x…` | None of the above | A bundle of short options |
| Anything else (including the empty string) | — | An element that is not an option |
| Any | After the terminator | A positional (for a command that has subcommands, an unexpected positional error) |

- An element that is not an option is the name of a subcommand if the current command has subcommands, and the value of a positional if it does not. In a validated schema, no command has both subcommands and positionals.
- The terminator `--` does not become a value. Every element after the terminator is a value, whatever its form: `-v`, `--name` and a second `--` too. The terminator stays in effect after the parse switches to a subcommand.
- An element in the form of a negative number (`-1`, `-.5` and so on) is an element that is not an option if the current command has no option whose short name is a digit. If it has even one, every element in the form of a negative number is read as a bundle (in `-.5`, the `.` is an unknown option). To pass a negative number to a positional of such a command, write it after the terminator.
- An element that is taken as the value of an option with a value is not classified. The `-x` of `--name -x` and the `--` of `--name --` are both values.
- Option names and subcommand names are compared as whole names, byte by byte. Uppercase and lowercase letters make different names, and part of a name (a prefix) does not match.
- Options are looked up only in the definition of the current command. The options of the parent command and of subcommands are not looked at ([Subcommands](#subcommands)).

### How options take values

A long option is matched against the long names and the negated names. In a bundle of short options, the characters after the `-` are matched one at a time, from the first, against the short names.

| Form | Kind of option | Behavior |
|---|---|---|
| `--name=value` | With a value | The text right after the `=` is the value (it may be empty) |
| `--name` | With a value | The next element is the value, whatever it contains. If there is none, `MISSING_VALUE` |
| `--name=value`, `--no-name=value` | Flag, negatable flag, count | `UNEXPECTED_VALUE` |
| Characters remain after `-o` | With a value | All of the remaining characters are the value. This ends the reading of the bundle |
| `-o` is the last in the bundle | With a value | The next element is the value, whatever it contains. If there is none, `MISSING_VALUE` |

Forms of options that take no value:

| Form | Kind of option | Behavior |
|---|---|---|
| `--name`, `-n` | Flag | Given |
| `--name`, `-n` | Count | The count goes up by 1 |
| `--name`, `-n` | Negatable flag | On |
| `--no-name` | Negatable flag | Off |

- The name of a long option ends before the first `=`. The value of `--name=a=b` is `a=b`.
- `-abc` is the same as `-a -b -c`. `-vvv` gives a count 3 times. `-abovalue` is the same as `-a -b -ovalue`, and `-abo value` is the same as `-a -b -o value`.
- The value of an option with a value in a bundle is all of the remaining characters. The value of `-o=value` is `=value`; the `=` is not removed.
- Every character after an option that takes no value is matched as one character of the bundle. The second `-` of `-a-b` and the `=` of `-a=1` are characters that cannot be short names, so they are unknown options (the messages are `unknown option '--'` and `unknown option '-='`).
- A value is passed to the value type as it was written, without changes. White space at the start and the end is not removed either. The input that each value type accepts is in [Schemas](schema.md#value-types).

### Repeated options

The same option can be given any number of times, by its short name or by its long name.

| Kind of option | Result of giving it more than once |
|---|---|
| Flag | Stays given |
| Count | The number of times it was given (`-v --verbose -vv` is 4) |
| Negatable flag | The last one given (`--color --no-color` is off; in the reverse order, on) |
| With a value | The last value (`--out a --out b` is `b`) |

The values of an option with a value are converted by the value type one at a time, as they are read. A value that is later replaced is converted too, so `--num x --num 5` is a parse error for the failed conversion of `x`.

### Assigning positionals

In a command that has no subcommands, the elements that are not options are assigned, in the order they appear, to the positionals in the order the positionals were added.

| Arity of the positional | Assignment |
|---|---|
| `CLARGS_ARITY_REQUIRED`, `CLARGS_ARITY_OPTIONAL` | One value is assigned. The next element goes to the next positional |
| `CLARGS_ARITY_ZERO_OR_MORE`, `CLARGS_ARITY_ONE_OR_MORE` | The value is assigned. Every later element is also a value of this positional |

- An element after every positional has been assigned its values, and an element that is not an option in a command that has no positionals, is an unexpected positional parse error (`UNEXPECTED_POSITIONAL`).
- Options can be written before, between or after the values of positionals. The assignment does not change.
- A lone `-` and the empty string become values of positionals. To pass a value that starts with `-`, write it after the terminator `--`.
- A value is converted by the value type of its positional as it is read.

### Subcommands

In a command that has subcommands, an element before the terminator that is not an option is matched against the names of the subcommands.

- The matching subcommand is selected and becomes the current command. The elements after its name are read with the options, positionals and subcommands of that subcommand. There is no limit to the depth of nesting.
- A name that matches none of the subcommands of the current command is an unknown subcommand parse error (`UNKNOWN_SUBCOMMAND`).
- **Write an option after the name of the command that defines it, and before the name of that command's subcommand.** The `-v` of `pack -v create …` is an option of `pack`. The `-v` of `pack create -v …` is looked up as an option of `create`, so if `create` has no `-v`, it is an unknown option. The options of a parent are not inherited by its subcommands.
- When a parent command and a child command have options with the same name, each option belongs to its own command, at its own position.
- If the command line ends without the name of a subcommand, it is not a parse error. `clargs_result_command` returns the command that has subcommands. A program that wants to require a subcommand checks this itself, as `run` in the example does.
- An element after the terminator does not become the name of a subcommand. A command that has subcommands has no positionals, so it is an unexpected positional parse error.
- The options and positionals of subcommands that were not selected can be read as not given ([Rules for reading](#rules-for-reading)).

## Required checks and default values

When the parse has read up to the last element without a parse error, the required checks are done and the default values are applied for the selected commands (the root, and the subcommands whose names appeared). The definition elements of commands that were not selected are not checked even if they are required, and they get no default values.

1. The required checks. If a required option with a value (`clargs_option_set_required`) has no value, or a positional of `CLARGS_ARITY_REQUIRED` or `CLARGS_ARITY_ONE_OR_MORE` has no value at all, it is a parse error for something required that is missing (`MISSING_OPTION`, `MISSING_POSITIONAL`). When two or more are missing, the first one is reported. The order is: commands closer to the root first; within one command, options before positionals, each in the order they were added.
2. Applying the default values. This is done only when nothing is missing. Each option with a value and each positional of `CLARGS_ARITY_OPTIONAL` that was not given and that has a default value gets the default value, converted by its value type. Its source becomes `CLARGS_SOURCE_DEFAULT`.

- An option with a value or an optional positional that was not given and that has no default value has no value (its source is `CLARGS_SOURCE_NONE`). A positional of `CLARGS_ARITY_ZERO_OR_MORE` that was not given has 0 values.
- A flag that was not given is "not given", a count is 0, and a negatable flag is in its initial state (`clargs_option_set_initial`). Their source is `CLARGS_SOURCE_DEFAULT`.
- If the conversion of a default value fails, it is a parse error for a value that failed to convert (`INVALID_VALUE`). Validation of the schema has checked the default values, so this happens only when a user-defined value type gives a different answer for the same input.
- The only sources of a value are the command line and the default values. Environment variables and the like are not read.
- **Default values are not applied to a result that has a parse error.** Options with a value and positionals that were not given have no value, even if they have a default value.

## Reading the result

A result (`clargs_result_t`) is read with the same functions whether the parse succeeded or ended in a parse error. The reading functions take a `const` result, change nothing, allocate nothing and do not fail. Several threads can read one result at the same time.

| What is read | Functions | Value when there is "none" |
|---|---|---|
| Whether the parse succeeded | `clargs_result_error` | Returns `CLARGS_ERROR_NONE` or `CLARGS_ERROR_INVALID_COMMAND_LINE` |
| The sequence of selected commands | `clargs_result_command_count`, `clargs_result_command_at`, `clargs_result_command` | The sequence has at least one command. Index 0 is the root. `clargs_result_command` is the last command |
| Flag, negatable flag | `clargs_result_flag` | `false` (a negatable flag is in its initial state) |
| Count | `clargs_result_count` | 0 |
| The source of the value of an option | `clargs_result_option_source` | `CLARGS_SOURCE_NONE` |
| The value of an option with a value | `clargs_result_option_string`, `_int`, `_float`, `_choice` | Returns `false` and does not change `*out` |
| The buffer of the value of an option with a value | `clargs_result_option_value` | `NULL` |
| The number of values and the source of a positional | `clargs_result_positional_count`, `clargs_result_positional_source` | 0, `CLARGS_SOURCE_NONE` |
| The value of a positional | `clargs_result_positional_string`, `_int`, `_float`, `_choice` | Returns `false` and does not change `*out` |
| The buffer of the value of a positional | `clargs_result_positional_value` | `NULL` |
| The details of a parse error | `clargs_result_error_kind` and others ([Parse errors](#parse-errors)) | `CLARGS_PARSE_ERROR_NONE`, `NULL`, `CLARGS_SOURCE_NONE`, 0, `'\0'` |

- A definition element is given by the very handle of the option or positional of the schema. It may be the handle that an add function wrote, or a handle that a reading or lookup function of the schema returned.
- There is a typed reading function for each kind of value type. The types of the values are `const char *` for `_string`, `int64_t` for `_int`, `double` for `_float`, and `size_t` (the index of the matching choice) for `_choice`.
- `_value` can be used for any kind of value type. The value of a user-defined value type is read with it. The returned pointer points to the buffer that the value type wrote the value to. The buffer holds an object of the C type of the value of the value type (`const char *`, `int64_t`, `double`, `size_t`, or what the conversion function of the user-defined value type wrote).
- The value of a positional is read by its index (from 0, in the order it appeared on the command line). The number of values of a required or optional positional is 0 or 1. The number of values of a positional with a variable number of values is the number of values assigned to it. When the index is equal to or greater than the number of values, `false` or `NULL` is returned as if there were no value. It does not break a precondition.
- A string value points into the copy of the arguments that the result holds, or to the default value of the schema. In a result parsed by the wide-character entry point, it is UTF-8.

### Rules for reading

What the reading functions return, for each state of a definition element.

| State of the definition element | Flag | Count | Negatable flag | Option with a value, positional | Source |
|---|---|---|---|---|---|
| Belongs to a command of the sequence, and was given on the command line | `true` | The number of times | The last one given | The value | `COMMAND_LINE` |
| Belongs to a command of the sequence, was not given, and its default value was applied | — | — | — | The default value | `DEFAULT` |
| Belongs to a command of the sequence, and was not given (other than above) | `false` | 0 | The initial state | No value | `DEFAULT` for flags, counts and negatable flags; `NONE` for options with a value and positionals |
| Belongs to a command that is not in the sequence | `false` | 0 | The initial state | No value | `NONE` |

- The "sequence" is the sequence of selected commands. In the source column, `CLARGS_SOURCE_` is left out.
- In a result with a parse error, the default values have not been applied, so the second row does not occur.
- Reading a definition element of a command that was not selected is neither a failure nor a broken precondition. It reads as in the fourth row.
- Values may be added to the enumeration of sources, `clargs_source_t`, in later versions. When you branch on it with `switch`, write a `default` label.

## Parse errors

When the arguments do not match the schema, the parse returns `CLARGS_ERROR_INVALID_COMMAND_LINE` and records one parse error in the result. Only the first error found, going from the start of argv, is recorded. Missing required items are checked after the last element has been read, so they come after every parse error about an element of the command line.

### Kinds and messages

The kind is the `clargs_parse_error_kind_t` that `clargs_result_error_kind` returns, and the message is returned by `clargs_result_error_message`. In the table below, `CLARGS_PARSE_ERROR_` is left out of the kinds.

| Kind | Wording | Example |
|---|---|---|
| `UNKNOWN_OPTION` | `unknown option '<name>'` | `unknown option '--colour'`, `unknown option '-x'` |
| `UNKNOWN_SUBCOMMAND` | `unknown command '<value>'` | `unknown command 'comit'` |
| `MISSING_VALUE` | `option '<name>' requires a value` | `option '--output' requires a value` |
| `UNEXPECTED_VALUE` | `option '<name>' does not take a value` | `option '--no-color' does not take a value` |
| `INVALID_VALUE` (option) | `invalid value '<value>' for option '<name>' (expected <type>)` | `invalid value 'abc' for option '--jobs' (expected integer)` |
| `INVALID_VALUE` (positional) | `invalid value '<value>' for argument '<name>' (expected <type>)` | `invalid value 'x' for argument 'COUNT' (expected integer)` |
| `INVALID_VALUE` (default value) | The two wordings above, with `invalid value` replaced by `invalid default value` | — |
| `MISSING_OPTION` | `missing required option '<name>'` | `missing required option '--output'` |
| `MISSING_POSITIONAL` | `missing required argument '<name>'` | `missing required argument 'FILE'` |
| `UNEXPECTED_POSITIONAL` | `unexpected argument '<value>'` | `unexpected argument 'extra'` |
| `INVALID_ENCODING` | `invalid character encoding in argument '<value>'` | — |

- The message is one line of English. It has no line break, no prefix such as the program name, and no full stop. You can show it to the user of your program as it is. The names and values in it are inserted as they are, without changes.
- The `<name>` of an option is the prefix (`--` or `-`) joined to the name, in the form that the user of the program wrote. If it was written with the negated name, it is `--no-<name>`; if it is one character of a bundle, it is `-` and that character. A value attached to a long option with `=` is not included (`--bogus=1` gives `unknown option '--bogus'`). For an option that was not written (a missing required option, a default value), it is `--<long name>` if the option has a long name, and `-<short name>` if it does not.
- The wording for a default value is, for example, `invalid default value '64x' for option '--block-size' (expected size)`.
- The `<name>` of a positional is the name of the positional in the schema. `<type>` is the name of the value type (`clargs_type_name`).
- `<value>` is the element itself for an unknown subcommand and an unexpected positional. For a value that failed to convert, it is the value string. For `INVALID_ENCODING`, it is the element in UTF-8, with each unit that cannot be converted replaced by U+FFFD.
- Values may be added to the enumeration of kinds in later versions. When you branch on it with `switch`, write a `default` label.
- The description (`clargs_error_message`) of the parse error code `CLARGS_ERROR_INVALID_COMMAND_LINE` is `"invalid command line"`. Read the message for each kind from the result. `CLARGS_ERROR_INVALID_VALUE`, which the conversion function of a value type returns, is never returned by the parse.

### Parse error details

Besides the message, the following functions read the details of the error. The column names are the function names with `clargs_result_error_` left out.

| Kind | `argument`, `index`, `source` | `value` | `option` / `positional` | `short_name` |
|---|---|---|---|---|
| `UNKNOWN_OPTION` | The element, `COMMAND_LINE` | `NULL` | `NULL` | In a bundle, that character |
| `UNKNOWN_SUBCOMMAND` | The element | The element | `NULL` | `'\0'` |
| `MISSING_VALUE` | The element that the option was written in | `NULL` | The option | In a bundle, that character |
| `UNEXPECTED_VALUE` | The element | The text right after the `=` | The option | `'\0'` |
| `INVALID_VALUE` (command line) | The element that holds the value | The value string | The definition element | For an option with a value in a bundle, that character |
| `INVALID_VALUE` (default value) | `NULL`, 0, `DEFAULT` | The default value string | The definition element | `'\0'` |
| `MISSING_OPTION`, `MISSING_POSITIONAL` | `NULL`, 0, `NONE` | `NULL` | The definition element | `'\0'` |
| `UNEXPECTED_POSITIONAL` | The element | The element | `NULL` | `'\0'` |
| `INVALID_ENCODING` | The element (UTF-8, with U+FFFD as replacements) | The element | `NULL` | `'\0'` |

- In the rows whose first column does not give `source`, `source` is `CLARGS_SOURCE_COMMAND_LINE`.
- `argument` (`clargs_result_error_argument`) is the whole offending element. For an error in a bundle, it is the whole bundle (`-vx`).
- `index` (`clargs_result_error_index`) is the index in the argv that you passed (`argv[0]` is 0). It has a meaning only when `source` (`clargs_result_error_source`) is `CLARGS_SOURCE_COMMAND_LINE`.
- `command` (`clargs_result_error_command`) is the current command when the error was found. For a missing required item and a default value, it is the command that the definition element belongs to. In a result with a parse error, it is never `NULL`.
- `option` (`clargs_result_error_option`) and `positional` (`clargs_result_error_positional`): the "definition element" of the table goes to `option` if it is an option with a value, and to `positional` if it is a positional. The other one is `NULL`.
- For `INVALID_VALUE`, when the value was taken from the next element (`--jobs many`, `-j many`), `argument` and `index` are those of the element of the value.
- `short_name` (`clargs_result_error_short_name`) is the character when one character of a bundle is the cause. A lone short option (`-n x`) is also read as a bundle of one character, so that character (`'n'`) is returned. When the option was written as a long option, it is `'\0'`.
- On a result whose parse succeeded, every function returns the value that means "none" (`CLARGS_PARSE_ERROR_NONE`, `NULL`, `CLARGS_SOURCE_NONE`, 0, `'\0'`).
- The returned strings are valid until the result is destroyed. Do not free them. The command, option and positional belong to the schema.

### The result of a parse error

From a result with a parse error, besides the details of the error, you can read what was read before the error was found, with the same functions as on success.

- You can read the sequence of selected commands, the flags, counts and negatable flags that were given, and the values that were converted successfully. Their source is always `CLARGS_SOURCE_COMMAND_LINE`.
- The elements after the offending element have not been read.
- The default values have not been applied. Options with a value and positionals that were not given have no value. Flags, counts and negatable flags that were not given are "not given", 0, and in their initial state.

### Printing the message

The library prints nothing. Your code prints the message.

```
fprintf(stderr, "pack: %s\n", clargs_result_error_message(result));
```

Adding the usage line, pointing at the position of the offending element, changing the wording for each kind, and so on, are done in your code by reading the [Parse error details](#parse-error-details). An example that generates the usage line of the command where the error was found and prints it with the message is in [Help and Usage](help.md#printing---help-and-errors). The messages are in English only.

## Handling `--help`

The library does not treat `--help` specially. `--help` is a flag that you define in the schema. Your code reads from the result whether it was given, prints the help and exits. The help text can be generated from the schema. The parse neither generates nor prints the help. The generating functions, and an example that reads `--help` and prints the help, are in [Help and Usage](help.md#printing---help-and-errors).

**Check whether `--help` was given before you look at the parse error.** `pack create --help` does not give `--output`, a required option of `create`, so the parse returns a parse error for a missing required option. But from a result with a parse error, you can still read what was read before the error, so `--help` reads as "given". If you look at the error first, a user who asked for help is shown an error message.

The order is the one in `main` of the example.

1. If `*out` is `NULL`, an allocation failed. There is no result.
2. If `--help` was given, print the help and exit. Choose the command whose help is printed with `clargs_result_command` (the last selected command). Passing that command to `clargs_help_write` generates the help of that command.
3. If the return value is `CLARGS_ERROR_INVALID_COMMAND_LINE`, print the message of the parse error.
4. Otherwise, read the values and do the work of the program.

**Define `--help` on subcommands too.** An option is recognized only while the command that defines it is the current command. If you define `--help` only on the root, `pack --help` and `pack --help create` are accepted, but `pack create --help` is an unknown option parse error, because `create` has no `--help`. To accept the form `tool sub --help`, define `--help` on every command, as `add_help` in the example does.

- To find out whether it was given, go through the sequence of selected commands and read the `--help` of each command with `clargs_result_flag` (`help_was_given` in the example). The `--help` of a command that was not selected reads as `false`.
- If an element before `--help` has a parse error, the parse stops there, so `--help` is not read (`pack create --bogus --help` is an unknown option error).

## Wide-character argv

`clargs_parse_wide` parses a wide-character argv, such as the one that `wmain` receives. It is the entry point for receiving, on Windows, arguments that contain non-ASCII characters without losing any of them to the code page.

- **`clargs_parse_wide` is provided on every platform.** It is in the library on Linux and macOS too, not only on Windows.
- How a wide string is read depends on the width of `wchar_t`. It does not depend on the locale.

  | Width of `wchar_t` | Example platforms | How it is read |
  |---|---|---|
  | 16 bits | Windows | UTF-16. A pair of a high and a low surrogate is read as one character |
  | Wider | Linux, macOS | UTF-32 |

- Each element is converted to UTF-8, and then the same parse as `clargs_parse` is done. The result, or the parse error, is the same as when a `char` argv with the same contents written in UTF-8 is passed to `clargs_parse`.
- Every string read from the result is a UTF-8 `char` string. String values, and the offending element, the value and the message of a parse error, are all UTF-8.
- An element that contains a sequence that cannot be converted to UTF-8 is the parse error `CLARGS_PARSE_ERROR_INVALID_ENCODING`. A sequence that cannot be converted is, in UTF-16, a surrogate that is not part of a pair, and in UTF-32, a negative value, a value in the surrogate range (U+D800 to U+DFFF), or a value greater than U+10FFFF.
  - This error is found just before the element is read. This is also true for an element that is taken as the value of an option with a value. If an earlier element has another parse error, that error is returned.
  - The offending element and the value are UTF-8 strings in which each unit that cannot be converted is replaced by U+FFFD.
  - `argv[0]` is not parsed, so it never causes this error.
- `clargs_parse` treats the elements of a `char` argv as byte sequences as they are, without checking or converting their character encoding. Bytes that are invalid in UTF-8 also become values as they are.
- The return values, `*out`, ownership and preconditions are the same as for `clargs_parse`.

An example with `wmain`. `greet` takes one name.

```c
#include <clargs/clargs.h>

#include <stdio.h>
#include <stdlib.h>

int wmain(int argc, wchar_t **argv)
{
    clargs_command_t *root = NULL;
    clargs_positional_t *name = NULL;
    clargs_result_t *result = NULL;
    const char *text = NULL;
    clargs_error_t error;
    int status = EXIT_FAILURE;

    error = clargs_command_create("greet", &root);
    if (error == CLARGS_ERROR_NONE) {
        error = clargs_command_add_positional(root, "name", clargs_type_string(), CLARGS_ARITY_REQUIRED,
                                              &name);
    }
    if (error == CLARGS_ERROR_NONE) {
        error = clargs_command_validate(root, NULL, NULL, NULL);
    }

    if (error != CLARGS_ERROR_NONE) {
        fprintf(stderr, "greet: cannot define the command line: %s\n", clargs_error_message(error));
    } else {
        /* The wide argv goes in as wmain() received it. */
        error = clargs_parse_wide(root, argc, argv, &result);

        if (result == NULL) {
            fprintf(stderr, "greet: %s\n", clargs_error_message(error));
        } else if (error == CLARGS_ERROR_INVALID_COMMAND_LINE) {
            /* The message is UTF-8, like every string of the result. */
            fprintf(stderr, "greet: %s\n", clargs_result_error_message(result));
        } else if (clargs_result_positional_string(result, name, 0, &text)) {
            /* text is UTF-8, whatever the width of wchar_t is. */
            printf("hello, %s\n", text);
            status = EXIT_SUCCESS;
        }
    }

    clargs_result_destroy(result);
    clargs_command_destroy(root);

    return status;
}
```

- `wmain` is an entry point that Windows compilers provide. On other platforms, you can use `clargs_parse_wide`, but there is no `wmain` entry point.
- The example writes the UTF-8 strings to standard output as they are. Making the console display UTF-8 (on Windows, setting the output code page of the console, and so on) is up to the program. The library does not call operating system APIs.

## Lifetime rules

| Object | Owner | How long it is valid |
|---|---|---|
| A result (`clargs_result_t`) | You, who called the parse | Until you destroy it with `clargs_result_destroy`. The same for a result with a parse error |
| Strings and value buffers read from the result | The result | Until the result is destroyed. Do not free or modify them |
| The details and message of a parse error | The result | Until the result is destroyed. Do not free or modify them |
| Commands, options and positionals read from the result | The schema | Until the root of the schema is destroyed |
| The argv array and strings | You | They need to be valid only during the call to the parse |
| The schema and the value types it uses | You | Do not change or destroy them until all of their results have been destroyed |

- **Destroy the result before the schema.** A result borrows the schema and the value types. Destroying the result cleans up the values it holds with the release functions of the value types, so the schema and the value types must be valid until then. The order of destruction is: the result, the schema, the value types, and then your data that the value types borrowed ([Schemas](schema.md#borrowed-value-types-and-the-order-of-destruction)).
- **Do not change the schema while a result exists.** While even one result of the schema is left, do not add definition elements to the schema or set attributes.
- **You may change or free argv after the parse.** The result holds copies of all the strings of argv. After the call to the parse returns, it refers to neither the argv array nor its strings.
- **Strings and values read from the result are valid until the result is destroyed.** A string value points into the copy of the arguments that the result holds, or to the default value of the schema. If you use it after destroying the result, copy it before the result is destroyed.
- **A result is passed to you on a parse error too, and you destroy it.** On an allocation failure (`CLARGS_ERROR_OUT_OF_MEMORY`), `*out` is `NULL`, and there is nothing to destroy. All the memory allocated for that parse, and the converted values, have been cleaned up.
- `clargs_result_destroy(NULL)` does nothing. If you initialize `result` to `NULL`, as the example does, you can write the destruction in the same order on every path.
- Destroying a result neither changes nor destroys the schema and the value types. You can parse with the same schema any number of times. Results are independent of each other and can be destroyed in any order.
- All memory is allocated from the allocator set for the library (`clargs_set_allocator`). Do not set the allocator again while a result is left ([Ownership and Lifetime](ownership.md#5-allocator)).
- The library does not detect the mistakes of not destroying a result before the schema, and of changing the schema while a result exists.

## Preconditions

Breaking a precondition is a programming error. No error code is returned. Preconditions that can be checked are checked with `assert`, and in the Debug configuration the process aborts when one is broken. In the Release configuration they are not checked, and the behavior is undefined ([Ownership and Lifetime](ownership.md#runtime-failures-and-programming-errors)). The preconditions of each function are also written in the comments of the public header `include/clargs/parse.h`, under `Preconditions:`.

What every function has in common:

- Pointer arguments are not `NULL`. The only ones that may be `NULL` are the `result` of `clargs_result_destroy`, and `argv` when `argc` is 0. Checked with `assert`.
- The result has not been destroyed. The schema of the result, and the value types that the schema uses, have been neither destroyed nor changed. Not checked.

The preconditions of each function:

| Function | Preconditions checked with `assert` | Preconditions not checked |
|---|---|---|
| `clargs_parse`, `clargs_parse_wide` | `root` and `out` are not `NULL`. `root` is a root command (not a subcommand). The schema of `root` has been validated (`clargs_command_is_validated`). `argc` is 0 or greater. If `argc` is greater than 0, `argv` is not `NULL`, and `argv[0]` to `argv[argc - 1]` are not `NULL` | The schema and the value types it uses are neither changed nor destroyed until the result is destroyed |
| `clargs_result_destroy` | None | `result` is `NULL` or a result that has not been destroyed. The schema and the value types of the result have been neither destroyed nor changed |
| `clargs_result_error`, `clargs_result_command_count`, `clargs_result_command` | `result` is not `NULL` | The common ones |
| `clargs_result_command_at` | `result` is not `NULL`. `index` is less than `clargs_result_command_count` | The common ones |
| `clargs_result_flag` | `result` and `option` are not `NULL`. `option` belongs to the schema of this result. `option` is a flag or a negatable flag | The common ones |
| `clargs_result_count` | `result` and `option` are not `NULL`. `option` belongs to the schema of this result. `option` is a count | The common ones |
| `clargs_result_option_source` | `result` and `option` are not `NULL`. `option` belongs to the schema of this result (of any kind) | The common ones |
| `clargs_result_option_string`, `_int`, `_float`, `_choice` | `result`, `option` and `out` are not `NULL`. `option` belongs to the schema of this result. `option` is an option with a value. The kind of its value type is `CLARGS_TYPE_STRING`, `CLARGS_TYPE_INT`, `CLARGS_TYPE_FLOAT` and `CLARGS_TYPE_CHOICE` respectively | The common ones |
| `clargs_result_option_value` | `result` and `option` are not `NULL`. `option` belongs to the schema of this result. `option` is an option with a value | The common ones. The returned pointer is read with the C type of the value of the value type |
| `clargs_result_positional_count`, `clargs_result_positional_source` | `result` and `positional` are not `NULL`. `positional` belongs to the schema of this result | The common ones |
| `clargs_result_positional_string`, `_int`, `_float`, `_choice` | `result`, `positional` and `out` are not `NULL`. `positional` belongs to the schema of this result. The kind of its value type is `CLARGS_TYPE_STRING`, `CLARGS_TYPE_INT`, `CLARGS_TYPE_FLOAT` and `CLARGS_TYPE_CHOICE` respectively | The common ones |
| `clargs_result_positional_value` | `result` and `positional` are not `NULL`. `positional` belongs to the schema of this result | The common ones. The returned pointer is read with the C type of the value of the value type |
| `clargs_result_error_kind`, `_message`, `_argument`, `_value`, `_source`, `_index`, `_command`, `_option`, `_positional`, `_short_name` | `result` is not `NULL` | The common ones |

- "Belongs to the schema of this result" means that the root reached by following the parents from the command of the definition element is the root that was passed to the parse. Passing an option or positional of another schema breaks a precondition.
- These do not break a precondition: reading a definition element of a command that was not selected; reading the value of an option with a value or a positional that has no value; reading the value of a positional with an index equal to or greater than the number of values; reading the details of a parse error from a result that succeeded. Each of them returns the value that means "none" ([Reading the result](#reading-the-result)).
- Passing a schema that has not been validated to the parse breaks a precondition. Adding a definition element to a schema or setting an attribute makes the schema not validated again, so validate it again before the parse ([Schemas](schema.md#validation-and-finding-the-cause-of-a-failure)).
- Neither the compiler nor the library detects the mistake of reading the pointer that `_value` returns with a type other than the C type of the value of the value type.
