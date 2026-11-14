# Guide

This guide walks through a program that uses ArgX, from creating the value types to destroying the result. It follows [examples/pack.c](../examples/pack.c), a complete program for a made-up archiver with two subcommands, `create` and `list`. The program is built and run with the tests, so its code works as shown. The code in this guide is taken from it; each excerpt names the function it comes from.

To build ArgX and use it from your project, see [Installation](install.md) first.

## Overview

Every program goes through the same steps. They are numbered the same way in the comments of `main` in [examples/pack.c](../examples/pack.c).

| Step | In examples/pack.c | Reference |
| --- | --- | --- |
| 1. Create the value types | `create_types`, `convert_size` | [Value types](schema.md#value-types), [User-defined value types](schema.md#user-defined-value-types) |
| 2. Define the schema | `define_schema`, `define_root`, `define_create`, `define_list`, `add_help` | [Commands](schema.md#commands), [Options](schema.md#options), [Positionals](schema.md#positionals) |
| 3. Validate it | `main`, `report_fault` | [Validation](schema.md#validation) |
| 4. Parse argv, and check `--help` first | `main`, `help_was_given` | [Entry points](parse.md#entry-points), [`--help`](parse.md#--help) |
| 5. Print the help or the parse error | `main`, `write_to_stream` | [Generating functions](help.md#generating-functions), [Parse errors](parse.md#parse-errors) |
| 6. Read the values | `run`, `run_create`, `run_list` | [Reading the result](parse.md#reading-the-result) |
| 7. Destroy the result, the schema and the value types | `main` | [Borrowed value types and the order of destruction](schema.md#borrowed-value-types-and-the-order-of-destruction) |

Every function that can fail returns an `argx_error_t`, which is `ARGX_ERROR_NONE` on success. [examples/pack.c](../examples/pack.c) stops at the first failure with the macro `TRY`, which returns the error from the function that uses it.

## 1. Create the value types

The string, integer and float value types are built in: `argx_type_string()`, `argx_type_int()` and `argx_type_float()` return them, and you never create or destroy them. Choice value types and user-defined value types are created by you. A schema borrows its value types, so create them before the schema (`create_types`):

```c
static const char *const formats[] = { "tar", "zip" };

TRY(argx_type_create_choice(formats, sizeof formats / sizeof formats[0], &schema->format));
return argx_type_create("size", sizeof(unsigned long), convert_size, NULL, limit,
                        &schema->size);
```

The value of a choice option is the index of the matching choice, so `formats[index]` gives back the string.

The user-defined value type `size` takes decimal digits with an optional unit, `k` or `m` (`512`, `64k`, `2m`), and writes the number of bytes as an `unsigned long`. Its value refers to nothing, so it has no release function. Its `context` is `limit`, a `size_limit_t` in `main` that holds the largest size it takes and stays valid until the value type is destroyed.

When you write a conversion function:

- Return `ARGX_ERROR_INVALID_VALUE` for an input it does not take. `convert_size` does so for a missing digit, an unknown unit and a size above `limit->max`.
- Give it no side effects. Validation calls it for every default value of its value type, on every validation.

## 2. Define the schema

Create the root command, then add options, positionals and subcommands to it. Each add function writes a handle. You set the attributes of what you added with that handle, and later read its value from the result with it, so keep the handles you will read. [examples/pack.c](../examples/pack.c) keeps them in the structure `pack_schema_t`.

An excerpt of `define_create`:

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

When an add or set function fails, the handle it writes is `NULL`. Stop at the first failure, as `TRY` does, so that a `NULL` handle is never passed to the next call.

**Define `--help` on every command.** The library adds no `--help` of its own, and an option is recognized only in its own command, so `--help` on the root alone makes `pack create --help` an unknown option ([`--help`](parse.md#--help)). `add_help` adds it to each command:

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

Validate the root once the definition is complete. Parsing and help generation take only a validated schema. Pass the three output parameters to learn which definition a failure points to:

```c
error = argx_command_validate(schema.root, &faulty_command, &faulty_option,
                              &faulty_positional);
if (error != ARGX_ERROR_NONE)
{
    report_fault(error, faulty_command, faulty_option, faulty_positional);
    goto cleanup;
}
```

To find the cause of a failure, do what `report_fault` does:

1. Pass the return value to `argx_error_message` to get the kind of error.
2. Pass `*command` to `argx_command_name` to get which command's definition it is. To walk up to the parents, use `argx_command_parent` (`NULL` for the root).
3. If `*option` is not `NULL`, use `argx_option_long_name` (`NULL` if none), `argx_option_short_name` (`'\0'` if none), `argx_option_default`, `argx_option_is_required` and so on to find which option has which error.
4. If `*positional` is not `NULL`, use `argx_positional_name`, `argx_positional_arity`, `argx_positional_default` and so on.
5. If all three are `NULL` and the return value is `ARGX_ERROR_OUT_OF_MEMORY`, it is not an error in the definition. The definition is unchanged, so you can validate again once enough memory is available.

To fix an error, set the attribute again (replace the default value with a correct one, set required to `false`, and so on), then validate again. There are no functions that remove a definition element, so for an error in the sequence of positionals, destroy the schema and build it again.

Use the library built in the Debug configuration during development. It aborts the process when a precondition that can be checked is broken, such as passing a schema that is not validated; the Release configuration does not check them ([Ownership and Lifetime](ownership.md#runtime-failures-and-programming-errors)).

## 4. Parse argv, and check `--help` first

Pass the validated root, and `argc` and `argv` as `main` received them, to `argx_parse`. Then check the outcome in this order:

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

**A parse error also gives you a result.** You read the error from it, and you destroy it like any other result.

**Check whether `--help` was given before you look at the parse error.** `pack create --help` leaves out `--output`, which `create` requires, so it is a parse error, but `--help` still reads as given ([`--help`](parse.md#--help)). If you look at the error first, a user who asked for help is shown an error message instead.

To find out whether `--help` was given, read the `--help` of each selected command (`help_was_given`):

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

The library prints nothing by itself. Your code decides what to generate, for which command, and where to write it:

| To print | Generate for | Usual destination |
| --- | --- | --- |
| The help, for `--help` | `argx_result_command`: the last command that the command line selected. `pack create --help` gives the help of `create` | The standard output |
| The usage line, with the message of a parse error | `argx_result_error_command`: the command that was current when the error was found. It is never `NULL` on a result with a parse error | The standard error stream |

The destination is the `user_data` of the write function; `write_to_stream` writes to the `FILE *` it is given ([Writing to standard output](help.md#writing-to-standard-output)). The usage line already ends with a newline, so do not add one after it.

With the code of step 4, the command line `pack create a.txt`, which leaves out the required `--output` of `create`, prints:

```
pack: missing required option '--output'
Usage: pack create [OPTIONS] --output <FILE> <file>...
Try 'pack --help' for more information.
```

The message of a parse error has no prefix and no line break, so the program adds its own name and the newline. To add more, such as pointing at the offending element, read the [Parse error details](parse.md#parse-error-details).

## 6. Read the values

Read each value from the result with the handle that you received when you defined the schema. An excerpt of `run_create`:

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

- Use the typed function that matches the value type: `_string`, `_int`, `_float` or `_choice`. Read a user-defined value type with `_value`, and read the pointer it returns with the C type that your conversion function wrote.
- `argx_result_command` returns the last selected command, which tells you which subcommand to run. A command line that names no subcommand is not a parse error, so a program that requires one checks this itself, as `run` does.

## 7. Destroy

Destroy the result, then the schema, then the value types ([the order of destruction](schema.md#borrowed-value-types-and-the-order-of-destruction)):

```c
argx_result_destroy(result);
argx_command_destroy(schema.root);
argx_type_destroy(schema.size);
argx_type_destroy(schema.format);
```

Destroying `NULL` does nothing. `main` initializes every pointer to `NULL`, so the same cleanup is right on every path, also after a failure.

## Recipes

### Getting the help as a string

This is the two calls of [Formatting into a buffer](help.md#formatting-into-a-buffer), first for the length and then for the text:

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

Formatting into a buffer also avoids partial text: when `argx_help_write` fails, what was written before the failure stays at the destination.

### Printing a parse error with the usage line from a fixed buffer

`argx_help_format_usage` cannot fail, so its return value is not checked.

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

### Wide-character argv with `wmain`

On Windows, pass the wide argv of `wmain` to `argx_parse_wide` as `wmain` received it ([Wide-character argv](parse.md#wide-character-argv)). Everything read from the result is UTF-8 (`name` is a positional of the schema):

```c
int wmain(int argc, wchar_t **argv)
{
    ...
    error = argx_parse_wide(root, argc, argv, &result);
    ...
    if (argx_result_positional_string(result, name, 0, &text))
    {
        /* text is UTF-8, whatever the width of wchar_t is. */
        printf("hello, %s\n", text);
    }
    ...
}
```

`wmain` is an entry point that Windows compilers provide; other platforms can call `argx_parse_wide`, but have no `wmain`. Writing the UTF-8 strings as they are, as above, does not make the console display them: set the output code page of the console yourself.
