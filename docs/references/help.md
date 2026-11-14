# Help and Usage

The library generates the help of a command, or only its usage line, from a validated schema. You choose where the text goes: a write function of your own, or a buffer. This document covers the generating functions, the help options, the output and the failures.

Generation only reads the schema; it needs no parse result. This document is the API reference of `include/argx/help.h`; the comments of the header describe each function in detail, and how a program uses them is in the [Guide](../guides/guide.md). What the text looks like is specified in [Help Text Layout](help-layout.md). Defining a schema is in [Schemas](schema.md), parsing in [Parsing](parse.md), and the rules for memory in [Ownership and Lifetime](ownership.md).

## Functions

| Functions | Section |
| --- | --- |
| `argx_help_write`, `argx_help_write_usage`, `argx_help_format`, `argx_help_format_usage` | [Generating functions](#generating-functions) |
| `argx_help_options_create`, `argx_help_options_destroy`, `argx_help_options_set_width`, `argx_help_options_set_indent`, `argx_help_options_set_max_column` | [Help options](#help-options) |

## Overview

Every generation takes the same four things:

1. **A command** of a validated schema: the root or a subcommand at any depth.
2. **Help options** that set the width, the indent and the maximum description column. `NULL` means the defaults ([Help options](#help-options)).
3. **A destination**: a write function that receives the text in parts ([The write function contract](#the-write-function-contract)), or a buffer of yours ([Formatting into a buffer](#formatting-into-a-buffer)).
4. **The return value**, which tells you whether the text was written in full ([On failure](#on-failure)).

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

- `command` may be the root or a subcommand at any depth. Its schema must be validated ([Preconditions](#preconditions)).
- `options` are the help options. `NULL` means every setting has its default value ([Help options](#help-options)).
- The functions that format into a buffer produce the same bytes as the functions that pass the text to a write function.
- The usage line that `argx_help_write_usage` generates is the same text as the first line of the help, for the same command and help options.
- Generation changes neither the schema nor the help options, and returns nothing for you to free.
- The library prints nothing by itself and adds no `--help` option.
- Return values, and what is left on failure, are in [On failure](#on-failure).

## Help options

The help options (`argx_help_options_t`) hold the settings of the layout. You create them, pass them to the generating functions, and destroy them.

| To do this | Function | Notes |
| --- | --- | --- |
| Create | `argx_help_options_create` | Every setting has its default value. On an allocation failure, returns `ARGX_ERROR_OUT_OF_MEMORY` and sets `*out` to `NULL` |
| Destroy | `argx_help_options_destroy` | `NULL` does nothing |
| Set the width | `argx_help_options_set_width` | Cannot fail |
| Set the indent | `argx_help_options_set_indent` | Cannot fail |
| Set the maximum description column | `argx_help_options_set_max_column` | Cannot fail |

| Setting | Meaning | Default |
| --- | --- | --- |
| Width | The line width that wrapping aims at. 0 means no limit: nothing is wrapped | 80 |
| Indent | The blanks before each item of a list | 2 |
| Maximum description column | The upper limit of the description column. Half of the width can lower it, and "indent + 2" can raise it past what you set ([Description column](help-layout.md#description-column)) | 32 |

- Every setting is at most 65535. A greater value is treated as 65535; the set functions reject nothing.
- `NULL` help options give the same text as newly created ones.
- The usage line uses only the width.
- Generation does not change the help options. One object can serve any number of generations, from any schemas.
- Several threads may generate with the same help options at once while nobody changes them. While a set function or the destroy function runs, no other thread may use them.
- The library reads neither the terminal width nor environment variables. To fit the terminal, find its width yourself and set it.
- The contents are not public. Later versions may add settings and their set functions.

The next function prints a help 40 columns wide (`write_to_stream` is the one in [Writing to standard output](#writing-to-standard-output)):

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

For `pack create` (the example command of [Help Text Layout](help-layout.md)), half of the width, 20, becomes the maximum description column, so the descriptions of `--output` and `--level` move to the next line:

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

## Output

The generating functions hand the text to a write function, or write it into your buffer. The library does not use `<stdio.h>` and writes nowhere you did not specify; to write to a `FILE *`, use a write function that calls `fwrite()`, like `write_to_stream` in [Writing to standard output](#writing-to-standard-output).

### The write function contract

```
typedef bool (*argx_help_write_fn_t)(void *user_data, const char *text, size_t length);
```

The library passes the text from the start, in order, in several parts.

- `user_data` is the pointer given to the generating function, passed on unchanged and never read by the library. It may be `NULL`.
- `text` is the start of one part, and `length` its number of bytes, always 1 or more. **No terminating null character follows.** Read exactly `length` bytes.
- Joining the parts in the order of the calls gives the whole text. How many parts there are, and where they split, is unspecified and may change between versions: do not assume a part is a line or a word.
- `text` is valid only during the call. Copy it if you need it later.
- Return `true` when the part was written and `false` when it was not. After `false`, the write function is not called again for that generation, and the generating function returns `ARGX_ERROR_WRITE_FAILED`.
- Do not change or destroy the schema from inside the write function.

### Writing to standard output

A write function that calls `fwrite()` is all a `FILE *` needs. Pass the stream as `user_data`:

```c
/* Writes one part of the text to the stream that user_data points to. */
static bool write_to_stream(void *user_data, const char *text, size_t length)
{
    return fwrite(text, 1, length, (FILE *)user_data) == length;
}
```

```c
error = argx_help_write(root, NULL, write_to_stream, stdout);
```

- When `fwrite()` writes fewer than `length` bytes, it returns `false`, and the generation ends with `ARGX_ERROR_WRITE_FAILED`.
- `text` is not terminated, so do not pass it to `fputs()` or `printf("%s")`.
- The write function is called many times, in units of words and blanks. A `FILE *` buffers them; for a destination without buffering, collect the parts in the write function.
- **On Windows, a stream in text mode converts newlines.** The library passes only `\n`, and `stdout` in text mode writes each as `\r\n`. That is right for a terminal. To compare the output exactly, or to keep `\n` in a file, use a stream opened in binary mode (`b` in the mode of `fopen()`).

### Formatting into a buffer

`argx_help_format` and `argx_help_format_usage` write the text as a string into your array, with no write function.

| `size` | What is written into `buffer` |
| --- | --- |
| 0 | Nothing. `buffer` may be `NULL` |
| 1 or more | As much of the text as fits (at most `size - 1` bytes), then a terminating null character. Nothing is written outside the array |

- When `length` is not `NULL`, `*length` receives the length of the whole text (without the terminating null character), whether it fit or not.
- **Truncation is not a failure: it returns `ARGX_ERROR_NONE`.** The text was truncated when `*length` is `size` or more. A truncated text may end in the middle of a line.

To get the whole text, call twice: first with `size` 0 and `buffer` `NULL` for the length, then with an array of `*length + 1` bytes.

Code that gets the whole help as a string this way, and code that prints the usage line from a fixed-size array, is in the [Guide](../guides/guide.md#getting-the-help-as-a-string).

## On failure

Two failures can happen at run time, a write failure and an allocation failure. The returned `argx_error_t` reports them.

| Return value | Meaning | Returned by |
| --- | --- | --- |
| `ARGX_ERROR_NONE` | Success. Truncation into a buffer is a success too | All |
| `ARGX_ERROR_WRITE_FAILED` | The write function returned `false` | `argx_help_write`, `argx_help_write_usage` |
| `ARGX_ERROR_OUT_OF_MEMORY` | An allocation failed | `argx_help_write`, `argx_help_format`, `argx_help_options_create` |

**The text passed to the write function before a failure stays at the destination.** The library cannot take it back.

| Function | Failure | What is left at the destination |
| --- | --- | --- |
| `argx_help_write`, `argx_help_write_usage` | Write failure | The parts for which the write function returned `true`: the text from the start up to some point, possibly in the middle of a line |
| `argx_help_write` | Allocation failure | The text up to the line of the item (an option or a positional) that needed the allocation, ending with the newline of the line before |
| `argx_help_format` | Allocation failure | If `size` is not 0, `buffer` holds the empty string and `*length` is 0. If `size` is 0, nothing is written into `buffer` |
| `argx_help_options_create` | Allocation failure | `*out` is `NULL` |

- After a write failure, the write function is not called again for that generation.
- When `argx_help_format` fails to allocate, the bytes of the array after the terminating null character are unspecified and may hold part of the text. Nothing is written outside the array.
- Generating the usage line alone allocates nothing, so `argx_help_write_usage` and `argx_help_format_usage` never return `ARGX_ERROR_OUT_OF_MEMORY`, and `argx_help_format_usage` cannot fail.
- `argx_help_format` never returns `ARGX_ERROR_WRITE_FAILED`.
- A failure changes neither the schema nor the help options, and all memory of the generation is freed before the function returns. You can generate again with the same schema and help options.
- A `NULL` argument, or a schema that is not validated, is not a failure but a programming error ([Preconditions](#preconditions)).

## Lifetime rules

| Thing | Owner | Valid for |
| --- | --- | --- |
| Help options (`argx_help_options_t`) | You, who created them | Until `argx_help_options_destroy`. They are independent of the schema; either may be destroyed first |
| `text` passed to the write function | The library or the schema | Only during that call. Do not free or change it |
| Buffer (`buffer`) | You | Only during the call of the generation. The library does not refer to it afterwards |
| The schema and the value types it uses | You | Must not be changed or destroyed during the call of the generation |

- The generating functions borrow the schema and the help options only during the call. They change neither, and after returning they keep no reference to them, to the write function, to its `user_data` or to the buffer.
- Nothing that the generating functions return is for you to free. No function returns a string allocated by the library.
- Memory for a generation, and the help options object, come from the allocator set for the library (`argx_set_allocator`). Generation frees its memory before returning. Do not set the allocator again while a help options object exists ([Ownership and Lifetime](ownership.md#5-allocator)).
- While the schema and the help options are not being changed, several threads may generate with them at once. Give each call its own buffer. Whether your write function can be called concurrently is up to it.

## Preconditions

Breaking a precondition is a programming error: no error code is returned ([Ownership and Lifetime](ownership.md#runtime-failures-and-programming-errors)).

| Function | Checked with `assert` | Not checked |
| --- | --- | --- |
| `argx_help_options_create` | `out` is not `NULL` | None |
| `argx_help_options_destroy` | None | `options` is `NULL` or help options not yet destroyed. No other thread is using them |
| `argx_help_options_set_width`, `argx_help_options_set_indent`, `argx_help_options_set_max_column` | `options` is not `NULL` | `options` is not destroyed. No other thread is using them during the call |
| `argx_help_write`, `argx_help_write_usage` | `command` is not `NULL`. Its schema is validated (`argx_command_is_validated`). `write_fn` is not `NULL` | The common ones below. The write function neither changes nor destroys the schema |
| `argx_help_format`, `argx_help_format_usage` | `command` is not `NULL`. Its schema is validated (`argx_command_is_validated`). If `size` is not 0, `buffer` is not `NULL` | The common ones below. `buffer` points to an array of at least `size` bytes |

Common to every generation, not checked:

- `options` is `NULL` or help options not yet destroyed, and no other thread changes them during the call.
- The schema and the value types it uses are neither changed nor destroyed during the call.

Arguments that may be `NULL`:

- `options` of the generating functions (the defaults are used).
- `user_data` of `argx_help_write` and `argx_help_write_usage`.
- `length` of `argx_help_format` and `argx_help_format_usage` (the length is not reported), and `buffer` when `size` is 0.
- `options` of `argx_help_options_destroy`.

Generating from a schema that is not validated breaks a precondition. Adding a definition or setting an attribute makes the schema unvalidated again, so validate it again before generating. For a subcommand, it is still the root that you validate ([Schemas](schema.md#validation)).
