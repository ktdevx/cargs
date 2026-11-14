# Help and Usage

API reference of `include/argx/help.h`. The library generates the help of a command, or only its usage line, from a validated schema, and hands it to your write function or buffer. What the text looks like is in [Help Text Layout](help-layout.md).

## Functions

| Functions | Section |
| --- | --- |
| `argx_help_write`, `argx_help_write_usage`, `argx_help_format`, `argx_help_format_usage` | [Generating functions](#generating-functions) |
| `argx_help_options_create`, `argx_help_options_destroy`, `argx_help_options_set_width`, `argx_help_options_set_indent`, `argx_help_options_set_max_column` | [Help options](#help-options) |

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

- `command` is any command of a validated schema. `options` may be `NULL` for the defaults.
- Writing and formatting produce the same bytes, and the usage line equals the first line of the help.
- Generation changes nothing and returns nothing to free.

## Help options

| To do this | Function | Notes |
| --- | --- | --- |
| Create | `argx_help_options_create` | With the defaults. May return `ARGX_ERROR_OUT_OF_MEMORY` |
| Destroy | `argx_help_options_destroy` | `NULL` does nothing |
| Set the width | `argx_help_options_set_width` | Cannot fail |
| Set the indent | `argx_help_options_set_indent` | Cannot fail |
| Set the maximum description column | `argx_help_options_set_max_column` | Cannot fail |

| Setting | Meaning | Default |
| --- | --- | --- |
| Width | The line width wrapping aims at; 0 for no wrapping. The only setting the usage line uses | 80 |
| Indent | Blanks before each list item | 2 |
| Maximum description column | The upper limit of the [description column](help-layout.md#description-column) | 32 |

- Values above 65535 are treated as 65535.
- One object may serve any number of generations, concurrently while nobody changes it.
- The library never reads the terminal width; set it yourself.
- Later versions may add settings.

```c
/* Prints the help of command to the standard output, 40 columns wide. */
static argx_error_t print_narrow_help(const argx_command_t *command)
{
    argx_help_options_t *options = NULL;
    argx_error_t error;

    error = argx_help_options_create(&options);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    argx_help_options_set_width(options, 40);
    error = argx_help_write(command, options, write_to_stream, stdout);
    argx_help_options_destroy(options);
    return error;
}
```

For `pack create`, half the width, 20, becomes the maximum description column:

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

### The write function contract

```
typedef bool (*argx_help_write_fn_t)(void *user_data, const char *text, size_t length);
```

- The text arrives in order, in parts of unspecified size and boundaries. Do not assume a part is a line or a word.
- `user_data` is passed through unchanged and may be `NULL`.
- `length` is at least 1. **`text` is not null-terminated** and is valid only during the call.
- Return `true` on success. After `false`, the write function is not called again and the generation returns `ARGX_ERROR_WRITE_FAILED`.
- Do not change or destroy the schema from inside the write function.

### Writing to standard output

```c
static bool write_to_stream(void *user_data, const char *text, size_t length)
{
    return fwrite(text, 1, length, (FILE *)user_data) == length;
}
```

```c
error = argx_help_write(root, NULL, write_to_stream, stdout);
```

- Do not pass `text` to `fputs()` or `printf("%s")`.
- The function is called many times; a `FILE *` buffers the parts.
- **On Windows, a text-mode stream writes each `\n` as `\r\n`.** Use binary mode to keep `\n`.

### Formatting into a buffer

| `size` | What is written into `buffer` |
| --- | --- |
| 0 | Nothing. `buffer` may be `NULL` |
| 1 or more | Up to `size - 1` bytes of the text, then a null character |

- `*length`, if `length` is not `NULL`, receives the length of the whole text.
- **Truncation is not a failure.** The text was cut when `*length >= size`, possibly mid-line.

To get the whole text, call with `size` 0 for the length, then with `*length + 1` bytes ([Guide](../guides/guide.md#getting-the-help-as-a-string)).

## On failure

| Return value | Meaning | Returned by |
| --- | --- | --- |
| `ARGX_ERROR_NONE` | Success, truncation included | All |
| `ARGX_ERROR_WRITE_FAILED` | The write function returned `false` | `argx_help_write`, `argx_help_write_usage` |
| `ARGX_ERROR_OUT_OF_MEMORY` | An allocation failed | `argx_help_write`, `argx_help_format`, `argx_help_options_create` |

**Text already written stays at the destination.**

| Function | Failure | What is left |
| --- | --- | --- |
| `argx_help_write`, `argx_help_write_usage` | Write failure | The parts accepted so far, possibly mid-line |
| `argx_help_write` | Allocation failure | The lines before the item that needed the allocation |
| `argx_help_format` | Allocation failure | An empty string and `*length` of 0 if `size` is not 0; bytes after the null are unspecified |
| `argx_help_options_create` | Allocation failure | `*out` is `NULL` |

- The usage line allocates nothing, so `argx_help_format_usage` never fails.
- A failure changes nothing and frees everything; you can generate again.

## Lifetime rules

| Thing | Owner | Valid for |
| --- | --- | --- |
| Help options | You | Until destroyed, independently of the schema |
| `text` given to the write function | The library | Only during that call |
| `buffer` | You | Only during the call |
| The schema and its value types | You | Unchanged during the call |

- Generation keeps no reference to anything after it returns.
- Do not set the allocator while help options exist ([rule 5](ownership.md#5-allocator)).
- Threads may generate concurrently with the same schema and options while nobody changes them, each with its own buffer.

## Preconditions

([Programming errors](ownership.md#runtime-failures-and-programming-errors).)

| Function | Checked with `assert` | Not checked |
| --- | --- | --- |
| `argx_help_options_create` | `out` is not `NULL` | — |
| `argx_help_options_destroy` | — | `options` is `NULL` or not destroyed, and not in use by another thread |
| `argx_help_options_set_*` | `options` is not `NULL` | `options` is not destroyed, and not in use by another thread |
| `argx_help_write`, `argx_help_write_usage` | `command` and `write_fn` are not `NULL`; the schema is validated | The write function does not change the schema |
| `argx_help_format`, `argx_help_format_usage` | `command` is not `NULL`; the schema is validated; `buffer` is not `NULL` if `size` is not 0 | `buffer` has at least `size` bytes |

For every generation, not checked: `options` is `NULL` or valid and unchanged during the call, and the schema and its value types are unchanged.

Arguments that may be `NULL`: `options` of the generating functions and of `argx_help_options_destroy`, `user_data`, `length`, and `buffer` when `size` is 0.
