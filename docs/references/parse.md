# Parsing

API reference of `include/argx/parse.h`. Parsing reads argv with a validated schema and gives a result to read the values from. The accepted command lines are in [Command-Line Syntax](syntax.md).

## Functions

| Functions | Section |
| --- | --- |
| `argx_parse`, `argx_parse_wide` | [Entry points](#entry-points) |
| `argx_result_error`, `argx_result_command_count`, `argx_result_command_at`, `argx_result_command` | [Reading the result](#reading-the-result) |
| `argx_result_flag`, `argx_result_count`, `argx_result_option_source`, `argx_result_option_string`, `argx_result_option_int`, `argx_result_option_float`, `argx_result_option_choice`, `argx_result_option_value` | [Reading the result](#reading-the-result) |
| `argx_result_positional_count`, `argx_result_positional_source`, `argx_result_positional_string`, `argx_result_positional_int`, `argx_result_positional_float`, `argx_result_positional_choice`, `argx_result_positional_value` | [Reading the result](#reading-the-result) |
| `argx_result_error_kind`, `argx_result_error_message` | [Kinds and messages](#kinds-and-messages) |
| `argx_result_error_argument`, `argx_result_error_value`, `argx_result_error_source`, `argx_result_error_index`, `argx_result_error_command`, `argx_result_error_option`, `argx_result_error_positional`, `argx_result_error_short_name` | [Parse error details](#parse-error-details) |
| `argx_result_destroy` | [Lifetime rules](#lifetime-rules) |

## Entry points

```
argx_error_t argx_parse(const argx_command_t *root, int argc, char *const *argv,
                        argx_result_t **out);
argx_error_t argx_parse_wide(const argx_command_t *root, int argc, wchar_t *const *argv,
                             argx_result_t **out);
```

| Return value | `*out` | Meaning |
| --- | --- | --- |
| `ARGX_ERROR_NONE` | A result | Success |
| `ARGX_ERROR_INVALID_COMMAND_LINE` | A result | A parse error, held in the result with what was read before it |
| `ARGX_ERROR_OUT_OF_MEMORY` | `NULL` | An allocation failed, including one reported by a conversion function |

- **Destroy any non-`NULL` `*out`, parse errors included.**
- Pass `argc` and `argv` as `main` received them. `argv[0]` is not parsed, and `argv[argc]` is not read.
- Parsing changes nothing it is given and keeps no state: the same input always gives the same result, and threads may parse with one schema concurrently.
- Elements are read once, in order, and parsing stops at the first one that does not match.

## Reading the result

The reading functions work the same after success or a parse error. They take a `const` result, never fail, allocate nothing, and may run concurrently.

The **selected commands** are the root and each subcommand named on the command line, in order. Only their options and positionals can have values.

| What is read | Functions | When there is none |
| --- | --- | --- |
| Success or parse error | `argx_result_error` | — |
| The selected commands | `argx_result_command_count`, `argx_result_command_at`, `argx_result_command` (the last) | Always at least the root, at index 0 |
| Flag, negatable flag | `argx_result_flag` | `false`, or the initial state |
| Count | `argx_result_count` | 0 |
| Source of an option | `argx_result_option_source` | `ARGX_SOURCE_NONE` |
| Option value | `argx_result_option_string`, `_int`, `_float`, `_choice` | `false`, `*out` unchanged |
| Option value buffer | `argx_result_option_value` | `NULL` |
| Positional count and source | `argx_result_positional_count`, `argx_result_positional_source` | 0, `ARGX_SOURCE_NONE` |
| Positional value | `argx_result_positional_string`, `_int`, `_float`, `_choice` | `false`, `*out` unchanged |
| Positional value buffer | `argx_result_positional_value` | `NULL` |
| Parse error details | [Parse errors](#parse-errors) | `ARGX_PARSE_ERROR_NONE`, `NULL`, `ARGX_SOURCE_NONE`, 0, `'\0'` |

- Options and positionals are given by their schema handles.
- The typed functions give `const char *`, `int64_t`, `double` and `size_t` (the choice index). `_value` works for any value type, including user-defined ones, and points to the buffer holding the value.
- Positional values are indexed from 0 in command-line order. An index past the count reads as no value.
- Strings point into the result's copy of argv or to the schema's default, and are UTF-8 after `argx_parse_wide`.

### Rules for reading

Sources are written without `ARGX_SOURCE_`.

| State | Flag | Count | Negatable flag | Option with a value, positional | Source |
| --- | --- | --- | --- | --- | --- |
| Given | `true` | The number of times | The last one given | The value | `COMMAND_LINE` |
| Not given, default applied | — | — | — | The default | `DEFAULT` |
| Not given, no default | `false` | 0 | The initial state | No value | `DEFAULT` for flags and counts; `NONE` otherwise |
| Command not selected | `false` | 0 | The initial state | No value | `NONE` |

New values may be added to `argx_source_t`.

## Parse errors

The result records the first parse error from the start of argv. Missing required items are checked only after the last element, so errors about elements come first.

### Kinds and messages

Kinds are written without `ARGX_PARSE_ERROR_`.

| Kind | Wording | Example |
| --- | --- | --- |
| `UNKNOWN_OPTION` | `unknown option '<name>'` | `unknown option '--colour'` |
| `UNKNOWN_SUBCOMMAND` | `unknown command '<value>'` | `unknown command 'comit'` |
| `MISSING_VALUE` | `option '<name>' requires a value` | `option '--output' requires a value` |
| `UNEXPECTED_VALUE` | `option '<name>' does not take a value` | `option '--no-color' does not take a value` |
| `INVALID_VALUE` (option) | `invalid value '<value>' for option '<name>' (expected <type>)` | `invalid value 'abc' for option '--jobs' (expected integer)` |
| `INVALID_VALUE` (positional) | `invalid value '<value>' for argument '<name>' (expected <type>)` | `invalid value 'x' for argument 'COUNT' (expected integer)` |
| `INVALID_VALUE` (default) | As above, with `invalid default value` | `invalid default value '64x' for option '--block-size' (expected size)` |
| `MISSING_OPTION` | `missing required option '<name>'` | `missing required option '--output'` |
| `MISSING_POSITIONAL` | `missing required argument '<name>'` | `missing required argument 'FILE'` |
| `UNEXPECTED_POSITIONAL` | `unexpected argument '<value>'` | `unexpected argument 'extra'` |
| `INVALID_ENCODING` | `invalid character encoding in argument '<value>'` | — |

- A message is one English line with no prefix, newline or full stop. Names and values are inserted unchanged.
- An option's `<name>` is as the user wrote it (`--name`, `--no-name`, or `-c` for a character of a bundle), without any `=value`. For an option not written (missing, or a default), it is `--<long name>`, or `-<short name>` without one.
- A positional's `<name>` is its schema name; `<type>` is `argx_type_name`.
- `<value>` is the element for an unknown subcommand or unexpected positional, the value string for a failed conversion, and for `INVALID_ENCODING` the element with U+FFFD replacements.
- New kinds may be added; give a `switch` a `default` label.
- Parsing never returns `ARGX_ERROR_INVALID_VALUE`.

### Parse error details

Columns are the functions without `argx_result_error_`; sources are without `ARGX_SOURCE_`.

| Kind | `argument` | `source` | `value` | `option` / `positional` | `short_name` |
| --- | --- | --- | --- | --- | --- |
| `UNKNOWN_OPTION` | The element | `COMMAND_LINE` | `NULL` | `NULL` | In a bundle, the character |
| `UNKNOWN_SUBCOMMAND` | The element | `COMMAND_LINE` | The element | `NULL` | `'\0'` |
| `MISSING_VALUE` | The option's element | `COMMAND_LINE` | `NULL` | The option | In a bundle, the character |
| `UNEXPECTED_VALUE` | The element | `COMMAND_LINE` | The text after `=` | The option | `'\0'` |
| `INVALID_VALUE` (command line) | The element holding the value | `COMMAND_LINE` | The value | The option or positional | In a bundle, the character |
| `INVALID_VALUE` (default) | `NULL` | `DEFAULT` | The default | The option or positional | `'\0'` |
| `MISSING_OPTION`, `MISSING_POSITIONAL` | `NULL` | `NONE` | `NULL` | The option or positional | `'\0'` |
| `UNEXPECTED_POSITIONAL` | The element | `COMMAND_LINE` | The element | `NULL` | `'\0'` |
| `INVALID_ENCODING` | The element, with U+FFFD | `COMMAND_LINE` | The element | `NULL` | `'\0'` |

- `argument` is the whole element, the whole bundle for a bundle error, or the separate value element (`--jobs many`).
- `index` is the position of `argument` in argv, or 0 unless `source` is `COMMAND_LINE`.
- `command` is the current command when the error was found, or the owner of a missing or default item. Never `NULL` on a parse error.
- Exactly one of `option` and `positional` is set when the error concerns one.
- `short_name` is set for any short option, including a lone `-n`, and `'\0'` for long options.
- Strings live until the result is destroyed; handles belong to the schema.

### The result of a parse error

The result holds what was read before the error, with the source `COMMAND_LINE`: the selected commands, given flags and counts, and successfully converted values. Later elements are not read, and **defaults are not applied**.

## `--help`

`--help` is an ordinary flag defined in the schema and read with `argx_result_flag`. Parsing never generates or prints help ([Help and Usage](help.md)).

- With `--help` only on the root, `pack create --help` is an unknown option ([Subcommands](syntax.md#subcommands)).
- `pack create --help` without the required `--output` is a parse error, but `--help` reads as given since it was read first.
- `pack create --bogus --help` stops at `--bogus`, so `--help` is not read.
- Pass `argx_result_command` to `argx_help_write` for the help of the last selected command.

## Wide-character argv

`argx_parse_wide`, available on every platform, converts each element to UTF-8 and parses it as `argx_parse` would. On Windows, it receives non-ASCII arguments from `wmain` without code-page loss ([Guide](../guides/guide.md#wide-character-argv-with-wmain)).

| Width of `wchar_t` | Example | Read as |
| --- | --- | --- |
| 16 bits | Windows | UTF-16 |
| Wider | Linux, macOS | UTF-32 |

- All strings read from the result, including parse error strings, are UTF-8.
- An unpaired surrogate (UTF-16), or a negative, surrogate or above-U+10FFFF value (UTF-32), is `INVALID_ENCODING`. It is found just before the element is read, so an earlier parse error wins; `argv[0]` never causes it.
- `argx_parse` treats `char` elements as bytes, without checking their encoding.
- Return values, ownership and preconditions match `argx_parse`. Displaying UTF-8 on the console is up to the program.

## Lifetime rules

| Object | Owner | Valid |
| --- | --- | --- |
| A result | You | Until `argx_result_destroy`, parse errors included |
| Strings, buffers and error details read from it | The result | Until it is destroyed. Do not free or modify |
| Commands, options and positionals read from it | The schema | Until the root is destroyed |
| argv | You | Only during the call |
| The schema and its value types | You | Unchanged until all their results are destroyed |

- **Destroy results before the schema and value types,** whose release functions clean up the values ([order of destruction](types.md#borrowed-value-types-and-the-order-of-destruction)).
- **Do not change the schema while a result exists.**
- `argx_result_destroy(NULL)` does nothing. Results are independent and may be destroyed in any order.
- Do not set the allocator while a result exists ([rule 5](ownership.md#5-allocator)).
- Wrong orders and changes are not detected.

## Preconditions

([Programming errors](ownership.md#runtime-failures-and-programming-errors).) For every function:

- Pointer arguments are not `NULL`, except the `result` of `argx_result_destroy` and `argv` when `argc` is 0. Checked.
- An `option` or `positional` belongs to the schema of the result. Checked.
- The result, its schema and value types are not destroyed or changed. Not checked.

| Function | Checked with `assert` | Not checked |
| --- | --- | --- |
| `argx_parse`, `argx_parse_wide` | `root` is a validated root; `argc >= 0`; `argv[0]` to `argv[argc - 1]` are not `NULL` | The schema and value types stay unchanged until the result is destroyed |
| `argx_result_destroy` | — | `result` is `NULL` or not yet destroyed |
| `argx_result_command_at` | `index` is less than the count | — |
| `argx_result_flag` | `option` is a flag or negatable flag | — |
| `argx_result_count` | `option` is a count | — |
| `argx_result_option_string`, `_int`, `_float`, `_choice` | `option` has a value type of the matching kind | — |
| `argx_result_option_value` | `option` is an option with a value | The pointer is read with the right C type |
| `argx_result_positional_string`, `_int`, `_float`, `_choice` | `positional` has a value type of the matching kind | — |
| `argx_result_positional_value` | — | The pointer is read with the right C type |

Reading an unselected command's items, a missing value, a positional index past the count, or error details of a successful result is not a violation; it reads as none.
