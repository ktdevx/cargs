# Tutorial

A walk through [examples/pack.c](../../examples/pack.c), a complete program for a made-up archiver with the subcommands `create` and `list`. Each excerpt names the function it comes from. To build ArgX, see [Installation](../getting-started/installation.md).

## Concepts

- A **schema** is a tree of **commands** describing the command lines your program accepts: the root, and subcommands such as `create` ([Schemas](../references/schema.md)).
- An **option** is a named argument such as `-v` or `--output FILE`: a flag, a negatable flag, a count, or an option with a value ([Options](../references/schema.md#options)).
- A **positional** is an unnamed argument, such as the files of `pack create`. Its arity says how many values it takes ([Positionals](../references/schema.md#positionals)).
- A **value type** turns a string into a value: a string, an integer, a float, a choice, or a type of your own ([Value Types](../references/types.md)).
- A **handle** is the pointer an add function writes. You set attributes and read values with it, and never destroy it on its own.
- A **result** is what parsing gives you: the selected commands, the values, and any parse error ([Parsing](../references/parse.md)).

## Overview

The steps are numbered the same way in the comments of `main`.

| Step | In examples/pack.c |
| --- | --- |
| 1. Create the value types | `create_types`, `convert_size` |
| 2. Define the schema | `define_schema`, `define_root`, `define_create`, `define_list`, `add_help` |
| 3. Validate it | `main`, `report_fault` |
| 4. Parse argv, and check `--help` first | `main`, `help_was_given` |
| 5. Print the help or the parse error | `main`, `write_to_stream` |
| 6. Read the values | `run`, `run_create`, `run_list` |
| 7. Destroy everything | `main` |

Functions that can fail return an `argx_error_t`, `ARGX_ERROR_NONE` on success. The example stops at the first failure with the macro `TRY`.

## 1. Create the value types

String, integer and float types are built in (`argx_type_string()` and so on) and never created or destroyed. Create choice and user-defined types yourself, before the schema, which borrows them (`create_types`):

```c
static const char *const formats[] = { "tar", "zip" };

TRY(argx_type_create_choice(formats, sizeof formats / sizeof formats[0], &schema->format));
return argx_type_create("size", sizeof(unsigned long), convert_size, NULL, limit,
                        &schema->size);
```

The value of a choice is the index of the match, so `formats[index]` gives back the string. The user-defined type `size` takes `512`, `64k` or `2m` and writes the bytes as an `unsigned long`; its `context` `limit` holds the largest size it accepts.

A conversion function returns `ARGX_ERROR_INVALID_VALUE` for input it rejects, and has no side effects: validation calls it for every default value.

## 2. Define the schema

Create the root command, then add options, positionals and subcommands. Keep the handles you will read later; the example keeps them in `pack_schema_t`. From `define_create`:

```c
TRY(argx_command_add_subcommand(schema->root, "create", &schema->create));
create = schema->create;
TRY(argx_command_set_description(create, "Create an archive."));
TRY(add_help(create));

/* A required string. */
TRY(argx_command_add_value(create, 'o', "output", argx_type_string(), &schema->output));
TRY(argx_option_set_description(schema->output, "The archive to write."));
TRY(argx_option_set_value_name(schema->output, "FILE"));
TRY(argx_option_set_required(schema->output, true));
```

A failed add function writes a `NULL` handle, so stop at the first failure.

**Define `--help` on every command.** The library adds none, and an option is recognized only on its own command ([`--help`](../references/parse.md#--help)). `add_help`:

```c
static argx_error_t add_help(argx_command_t *command)
{
    argx_option_t *option = NULL;
    argx_error_t error;

    TRY(argx_command_add_flag(command, 'h', "help", &option));
    return argx_option_set_description(option, "Print this help.");
}
```

## 3. Validate the schema

Parsing and help take only a validated schema. The output parameters point to the faulty definition:

```c
error = argx_command_validate(schema.root, &faulty_command, &faulty_option,
                              &faulty_positional);
if (error != ARGX_ERROR_NONE)
{
    report_fault(error, faulty_command, faulty_option, faulty_positional);
    goto cleanup;
}
```

`report_fault` prints `argx_error_message(error)` with the names of the faulty command and option or positional, read with functions such as `argx_command_name` and `argx_option_long_name` ([Reading a schema](../references/schema.md#reading-a-schema)). `ARGX_ERROR_OUT_OF_MEMORY` with all three `NULL` is not a definition error; validate again later.

Develop with the Debug build: it aborts on a broken precondition, such as an unvalidated schema.

## 4. Parse argv, and check `--help` first

```c
error = argx_parse(schema.root, argc, argv, &result);

if (result == NULL)
{
    /* ARGX_ERROR_OUT_OF_MEMORY: there is no result. */
    fprintf(stderr, "pack: %s\n", argx_error_message(error));
}
else if (help_was_given(result))
{
    /* The help of the command that the command line ends in. */
    error = argx_help_write(argx_result_command(result), NULL, write_to_stream, stdout);
    ...
}
else if (error == ARGX_ERROR_INVALID_COMMAND_LINE)
{
    /* The message, then the usage line of the command where the error was found. */
    fprintf(stderr, "pack: %s\n", argx_result_error_message(result));
    argx_help_write_usage(argx_result_error_command(result), NULL, write_to_stream, stderr);
    fprintf(stderr, "Try 'pack --help' for more information.\n");
}
else
{
    status = run(&schema, result);
}
```

- **A parse error also gives a result,** which you destroy like any other.
- **Check `--help` before the parse error.** `pack create --help` omits the required `--output`, so it is a parse error, but `--help` still reads as given.

`help_was_given` reads the `--help` of each selected command:

```c
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
```

## 5. Print the help or the parse error

The library prints nothing; your code chooses what and where.

| To print | Generate for | Usually to |
| --- | --- | --- |
| The help | `argx_result_command`, the last selected command | Standard output |
| The usage line after a parse error | `argx_result_error_command`, the command where the error was found | Standard error |

The destination is the `user_data` of the write function; `write_to_stream` takes a `FILE *` ([Writing to standard output](../references/help.md#writing-to-standard-output)). For `pack create a.txt`, the code of step 4 prints:

```
pack: missing required option '--output'
Usage: pack create [OPTIONS] --output <FILE> <file>...
Try 'pack --help' for more information.
```

The message has no prefix or newline, and the usage line ends with one. For more detail, see [Parse error details](../references/parse.md#parse-error-details).

## 6. Read the values

Read each value with its handle. From `run_create`:

```c
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
```

- Use the function that matches the value type: `_string`, `_int`, `_float`, `_choice`, or `_value` for a user-defined type.
- `argx_result_command` tells which subcommand to run. A missing subcommand is not a parse error, so `run` checks it.

## 7. Destroy

Destroy the result, then the schema, then the value types ([order of destruction](../references/types.md#borrowed-value-types-and-the-order-of-destruction)). Destroying `NULL` does nothing, so one cleanup serves every path.

```c
argx_result_destroy(result);
argx_command_destroy(schema.root);
argx_type_destroy(schema.size);
argx_type_destroy(schema.format);
```

## What to read next

[How-to](how-to.md) for specific tasks, the [FAQ](faq.md), [Design](design.md), and the references in the [documentation index](../README.md#references).
