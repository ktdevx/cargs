# Parsing

Parsing reads argv with a validated schema and gives you a result, from which you read the values. This document covers parsing, reading the result, parse errors and destroying the result, and the command-line syntax that the users of your program can write. Parsing prints nothing, does not end the process, and does not read environment variables.

Defining and validating a schema is in [Schemas](schema.md), and generating the help and the usage line is in [Help and Usage](help.md). The rules for who owns memory are in [Ownership and Lifetime](ownership.md). The detailed description and the preconditions of each function are in the comments of the public header `include/argx/parse.h`. This document is a reference. How a program parses, checks `--help`, prints parse errors and reads the values, step by step, is in the [Guide](guide.md), which follows the complete program [examples/pack.c](../examples/pack.c).

## Overview

- `argx_parse` and `argx_parse_wide` read argv with the root of a validated schema and write a result ([Entry points](#entry-points)).
- A result is written also when the command line has a parse error. Only on an allocation failure is there no result ([Entry points](#entry-points), [Parse errors](#parse-errors)).
- Values are read from the result with the handles of the schema ([Reading the result](#reading-the-result)).
- The library does not treat `--help` specially ([`--help`](#--help)).
- A result borrows the schema: it is destroyed before the schema, and the schema is not changed while it exists ([Lifetime rules](#lifetime-rules)).

## Entry points

```
argx_error_t argx_parse(const argx_command_t *root, int argc, char *const *argv,
                        argx_result_t **out);
argx_error_t argx_parse_wide(const argx_command_t *root, int argc, wchar_t *const *argv,
                             argx_result_t **out);
```

`argx_parse` parses a `char` argv. `argx_parse_wide` converts a wide-character argv to UTF-8 and then parses it in the same way ([Wide-character argv](#wide-character-argv)).

| Return value | `*out` | Meaning |
| --- | --- | --- |
| `ARGX_ERROR_NONE` | A result | Success. The values have been resolved |
| `ARGX_ERROR_INVALID_COMMAND_LINE` | A result | A parse error. The result holds the details of the error and what was read before it |
| `ARGX_ERROR_OUT_OF_MEMORY` | `NULL` | An allocation failed. Nothing is left |

- **If `*out` is not `NULL`, destroy it with `argx_result_destroy`, on a parse error too.** This is an exception to the rule of the other functions, "on failure, the output is `NULL`".
- When the conversion function of a value type returns `ARGX_ERROR_OUT_OF_MEMORY`, it is also treated as an allocation failure.
- `argc` and `argv` go in as `main` received them, with no conversion or cast.
- `argv[0]` is the program name, and it is not parsed. If `argc` is 0 or 1, the command line is parsed as one in which nothing was given. `argv[argc]` is not read.
- `root` must be the root of a validated schema. Passing a schema that has not been validated, or a subcommand, breaks a [precondition](#preconditions); no error code is returned for it.
- Parsing does not change the schema, the value types, the argv array or the argv strings. No state is kept between calls, so the same schema and the same argv always give the same result. While the schema is not changed, several threads can parse with it at the same time.
- Elements are read once each, from the first to the last. Parsing stops at the first element that does not match the schema.

## Reading the result

A result (`argx_result_t`) is read with the same functions whether parsing succeeded or ended in a parse error. The reading functions take a `const` result, change nothing, allocate nothing and do not fail. Several threads can read one result at the same time.

The **selected commands** are the root and each subcommand whose name appeared on the command line, in that order. Only their options and positionals can have values.

| What is read | Functions | Value when there is "none" |
| --- | --- | --- |
| Whether parsing succeeded | `argx_result_error` | Returns `ARGX_ERROR_NONE` or `ARGX_ERROR_INVALID_COMMAND_LINE` |
| The selected commands | `argx_result_command_count`, `argx_result_command_at`, `argx_result_command` | There is at least one. Index 0 is the root. `argx_result_command` returns the last one |
| Flag, negatable flag | `argx_result_flag` | `false` (a negatable flag is in its initial state) |
| Count | `argx_result_count` | 0 |
| The source of the value of an option | `argx_result_option_source` | `ARGX_SOURCE_NONE` |
| The value of an option with a value | `argx_result_option_string`, `_int`, `_float`, `_choice` | Returns `false` and does not change `*out` |
| The buffer of the value of an option with a value | `argx_result_option_value` | `NULL` |
| The number of values and the source of a positional | `argx_result_positional_count`, `argx_result_positional_source` | 0, `ARGX_SOURCE_NONE` |
| The value of a positional | `argx_result_positional_string`, `_int`, `_float`, `_choice` | Returns `false` and does not change `*out` |
| The buffer of the value of a positional | `argx_result_positional_value` | `NULL` |
| The details of a parse error | `argx_result_error_kind` and others ([Parse errors](#parse-errors)) | `ARGX_PARSE_ERROR_NONE`, `NULL`, `ARGX_SOURCE_NONE`, 0, `'\0'` |

- An option or positional is given by its handle in the schema: the handle that an add function wrote, or one that a reading or lookup function of the schema returned.
- There is a typed reading function for each kind of value type: `const char *` for `_string`, `int64_t` for `_int`, `double` for `_float`, and `size_t` (the index of the matching choice) for `_choice`.
- `_value` works with any kind of value type, and is how you read the value of a user-defined value type. It returns a pointer to the buffer that the value type wrote the value to. The buffer holds an object of the C type of the value of the value type (`const char *`, `int64_t`, `double`, `size_t`, or what the conversion function of the user-defined value type wrote).
- The values of a positional are read by index, from 0, in the order they appeared on the command line. A required or optional positional has 0 or 1 values; a positional with a variable number of values has as many as were assigned to it. An index equal to or greater than the number of values returns `false` or `NULL`, as if there were no value. This does not break a precondition.
- A string value points into the copy of the arguments that the result holds, or to the default value in the schema. In a result from `argx_parse_wide`, it is UTF-8.

### Rules for reading

What the reading functions return, for each state of an option or positional. In the source column, `ARGX_SOURCE_` is left out.

| State | Flag | Count | Negatable flag | Option with a value, positional | Source |
| --- | --- | --- | --- | --- | --- |
| Its command is selected, and it was given on the command line | `true` | The number of times | The last one given | The value | `COMMAND_LINE` |
| Its command is selected, it was not given, and its default value was applied | — | — | — | The default value | `DEFAULT` |
| Its command is selected, and it was not given (other than above) | `false` | 0 | The initial state | No value | `DEFAULT` for flags, counts and negatable flags; `NONE` for options with a value and positionals |
| Its command is not selected | `false` | 0 | The initial state | No value | `NONE` |

- Reading an option or positional of a command that was not selected is neither a failure nor a broken precondition. It reads as in the last row.
- In a result with a parse error, the second row does not occur ([The result of a parse error](#the-result-of-a-parse-error)).
- Values may be added to the enumeration of sources, `argx_source_t`, in later versions. When you branch on it with `switch`, write a `default` label.

## Parse errors

When the arguments do not match the schema, `argx_parse` returns `ARGX_ERROR_INVALID_COMMAND_LINE` and records one parse error in the result: the first one found, going from the start of argv. Missing required items are checked after the last element has been read, so an error about an element of the command line always comes first.

### Kinds and messages

The kind is the `argx_parse_error_kind_t` that `argx_result_error_kind` returns, and the message is returned by `argx_result_error_message`. In the table below, `ARGX_PARSE_ERROR_` is left out of the kinds.

| Kind | Wording | Example |
| --- | --- | --- |
| `UNKNOWN_OPTION` | `unknown option '<name>'` | `unknown option '--colour'`, `unknown option '-x'` |
| `UNKNOWN_SUBCOMMAND` | `unknown command '<value>'` | `unknown command 'comit'` |
| `MISSING_VALUE` | `option '<name>' requires a value` | `option '--output' requires a value` |
| `UNEXPECTED_VALUE` | `option '<name>' does not take a value` | `option '--no-color' does not take a value` |
| `INVALID_VALUE` (option) | `invalid value '<value>' for option '<name>' (expected <type>)` | `invalid value 'abc' for option '--jobs' (expected integer)` |
| `INVALID_VALUE` (positional) | `invalid value '<value>' for argument '<name>' (expected <type>)` | `invalid value 'x' for argument 'COUNT' (expected integer)` |
| `INVALID_VALUE` (default value) | The two wordings above, with `invalid value` replaced by `invalid default value` | `invalid default value '64x' for option '--block-size' (expected size)` |
| `MISSING_OPTION` | `missing required option '<name>'` | `missing required option '--output'` |
| `MISSING_POSITIONAL` | `missing required argument '<name>'` | `missing required argument 'FILE'` |
| `UNEXPECTED_POSITIONAL` | `unexpected argument '<value>'` | `unexpected argument 'extra'` |
| `INVALID_ENCODING` | `invalid character encoding in argument '<value>'` | — |

- The message is one line of English, with no line break, no prefix such as the program name, and no full stop. You can show it to the user of your program as it is. Names and values are inserted as they are, without changes.
- The `<name>` of an option is the prefix (`--` or `-`) joined to the name, in the form that the user wrote: `--no-<name>` if the negated name was written, and `-` and the character if it was one character of a bundle. A value attached with `=` is not included (`--bogus=1` gives `unknown option '--bogus'`).
- For an option that was not written (a missing required option, a default value), `<name>` is `--<long name>` if the option has a long name, and `-<short name>` if it does not.
- The `<name>` of a positional is its name in the schema. `<type>` is the name of the value type (`argx_type_name`).
- `<value>` is the element itself for an unknown subcommand and an unexpected positional, and the value string for a value that failed to convert. For `INVALID_ENCODING`, it is the element in UTF-8, with each unit that cannot be converted replaced by U+FFFD.
- Values may be added to the enumeration of kinds in later versions. When you branch on it with `switch`, write a `default` label.
- `argx_error_message(ARGX_ERROR_INVALID_COMMAND_LINE)` is only `"invalid command line"`. Read the message of each kind from the result. `ARGX_ERROR_INVALID_VALUE`, which the conversion function of a value type returns, is never returned by parsing.

### Parse error details

Besides the message, the following functions read the details of the error. The column names are the function names with `argx_result_error_` left out, and `ARGX_SOURCE_` is left out of the sources.

| Kind | `argument` | `source` | `value` | `option` / `positional` | `short_name` |
| --- | --- | --- | --- | --- | --- |
| `UNKNOWN_OPTION` | The element | `COMMAND_LINE` | `NULL` | `NULL` | In a bundle, that character |
| `UNKNOWN_SUBCOMMAND` | The element | `COMMAND_LINE` | The element | `NULL` | `'\0'` |
| `MISSING_VALUE` | The element that the option was written in | `COMMAND_LINE` | `NULL` | The option | In a bundle, that character |
| `UNEXPECTED_VALUE` | The element | `COMMAND_LINE` | The text right after the `=` | The option | `'\0'` |
| `INVALID_VALUE` (command line) | The element that holds the value | `COMMAND_LINE` | The value string | The option or positional | For an option with a value in a bundle, that character |
| `INVALID_VALUE` (default value) | `NULL` | `DEFAULT` | The default value string | The option or positional | `'\0'` |
| `MISSING_OPTION`, `MISSING_POSITIONAL` | `NULL` | `NONE` | `NULL` | The option or positional | `'\0'` |
| `UNEXPECTED_POSITIONAL` | The element | `COMMAND_LINE` | The element | `NULL` | `'\0'` |
| `INVALID_ENCODING` | The element (UTF-8, with U+FFFD as replacements) | `COMMAND_LINE` | The element | `NULL` | `'\0'` |

- `argument` (`argx_result_error_argument`) is the whole offending element. For an error in a bundle, it is the whole bundle (`-vx`).
- `index` (`argx_result_error_index`) is the index of `argument` in the argv that you passed (`argv[0]` is 0). It has a meaning only when `source` (`argx_result_error_source`) is `ARGX_SOURCE_COMMAND_LINE`; otherwise it is 0.
- For `INVALID_VALUE`, when the value was taken from the next element (`--jobs many`, `-j many`), `argument` and `index` are those of the element of the value.
- `command` (`argx_result_error_command`) is the current command when the error was found. For a missing required item and a default value, it is the command that the option or positional belongs to. In a result with a parse error, it is never `NULL`.
- `option` (`argx_result_error_option`) and `positional` (`argx_result_error_positional`): one of them is set, depending on whether the cause is an option or a positional, and the other is `NULL`.
- `short_name` (`argx_result_error_short_name`) is the character when one character of a bundle is the cause. A lone short option (`-n x`) is also a bundle of one character, so `'n'` is returned. When the option was written as a long option, it is `'\0'`.
- On a result whose parsing succeeded, every function returns the value that means "none" (`ARGX_PARSE_ERROR_NONE`, `NULL`, `ARGX_SOURCE_NONE`, 0, `'\0'`).
- The returned strings are valid until the result is destroyed. Do not free them. The command, option and positional belong to the schema.

### The result of a parse error

Besides the details of the error, you can read from a result with a parse error what was read before the error was found, with the same functions as on success.

- You can read the selected commands, the flags, counts and negatable flags that were given, and the values that were converted successfully. Their source is always `ARGX_SOURCE_COMMAND_LINE`.
- The elements after the offending element have not been read.
- **Default values have not been applied.** Options with a value and positionals that were not given have no value, even if they have a default value. Flags, counts and negatable flags that were not given are "not given", 0, and in their initial state.

### Printing the message

The library prints nothing. Your code prints the message:

```
fprintf(stderr, "pack: %s\n", argx_result_error_message(result));
```

Anything more, such as adding the usage line, pointing at the offending element or changing the wording for each kind, is done in your code by reading the [Parse error details](#parse-error-details). An example that prints the usage line of the command where the error was found, with the message, is in the [Guide](guide.md#5-print-the-help-or-the-parse-error). The messages are in English only.

## `--help`

The library does not treat `--help` specially. `--help` is a flag that is defined in the schema like any other option, and it is read from the result with `argx_result_flag`. Parsing neither generates nor prints the help; generating it is in [Help and Usage](help.md). How [examples/pack.c](../examples/pack.c) reads `--help` and prints the help is in the [Guide](guide.md#4-parse-argv-and-check---help-first).

- An option is recognized only while its command is the current command ([Subcommands](#subcommands)). With `--help` defined only on the root, `pack --help` and `pack --help create` are accepted, but `pack create --help` is an unknown option, because `create` has no `--help`.
- A result with a parse error holds what was read before the error ([The result of a parse error](#the-result-of-a-parse-error)). `pack create --help` does not give `--output`, a required option of `create`, so `argx_parse` returns a parse error for a missing required option; `--help` was read before that error was found, so it reads as given.
- If an element before `--help` causes a parse error, parsing stops there and `--help` is not read (`pack create --bogus --help` is an unknown option error).
- The `--help` of a command that was not selected reads as `false`. `argx_result_command` returns the last selected command, and passing it to `argx_help_write` generates the help of that command.

## Wide-character argv

`argx_parse_wide` parses a wide-character argv, such as the one that `wmain` receives. On Windows, it is the way to receive arguments that contain non-ASCII characters without losing any of them to the code page. An example with `wmain` is in the [Guide](guide.md#wide-character-argv-with-wmain).

- **`argx_parse_wide` is provided on every platform,** not only on Windows.
- How a wide string is read depends on the width of `wchar_t`, not on the locale:

  | Width of `wchar_t` | Example platforms | How it is read |
  | --- | --- | --- |
  | 16 bits | Windows | UTF-16. A pair of a high and a low surrogate is read as one character |
  | Wider | Linux, macOS | UTF-32 |

- Each element is converted to UTF-8, and then parsed as by `argx_parse`. The result, or the parse error, is the same as for a `char` argv with the same contents in UTF-8.
- Every string read from the result is a UTF-8 `char` string: string values, and the offending element, the value and the message of a parse error.
- An element that contains a sequence that cannot be converted to UTF-8 is the parse error `ARGX_PARSE_ERROR_INVALID_ENCODING`. In UTF-16, that is a surrogate that is not part of a pair. In UTF-32, it is a negative value, a value in the surrogate range (U+D800 to U+DFFF), or a value greater than U+10FFFF.
  - The error is found just before the element is read, also when the element is the value of an option with a value. If an earlier element has another parse error, that error is returned.
  - The offending element and the value are UTF-8 strings in which each unit that cannot be converted is replaced by U+FFFD.
  - `argv[0]` is not parsed, so it never causes this error.
- `argx_parse`, in contrast, treats the elements of a `char` argv as byte sequences, without checking or converting their encoding. Bytes that are invalid in UTF-8 also become values as they are.
- The return values, `*out`, ownership and preconditions are the same as for `argx_parse`.
- The library does not call operating system APIs. Making the console display the UTF-8 strings (on Windows, setting the output code page of the console, and so on) is up to the program.

## Command-line syntax

These are the forms that the users of your program can write on the command line. They are the same for `argx_parse` and `argx_parse_wide`.

### Terms

- An **element** is one string of argv (`argv[1]` onward).
- The **terminator** is the first `--` that is read as an element on its own. Every element after it is a value.
- The **current command** is the root at first. It becomes a subcommand each time the name of that subcommand appears. **Options are looked up only in the current command**: the options of the parent and of the subcommands are not looked at ([Subcommands](#subcommands)).
- An **operand** is an element that is neither an option nor the terminator. If the current command has subcommands, an operand is the name of a subcommand; if it does not, it is the value of a positional. In a validated schema, no command has both subcommands and positionals.

### How elements are classified

Each element is classified by the first row that matches.

| Form of the element | Condition | Classified as |
| --- | --- | --- |
| Any | After the terminator | The value of a positional (for a command that has subcommands, an `UNEXPECTED_POSITIONAL` parse error) |
| `--` | — | The terminator. It does not become a value |
| `--x…` | — | A long option. It is split into the name and the value at the first `=` |
| `-` | — | An operand |
| `-N…`, `-.N…` (N is a digit) | The current command has no short name that is a digit | An operand |
| `-x…` | — | A bundle of short options |
| Anything else (including the empty string) | — | An operand |

- Every element after the terminator is a value, whatever its form: `-v`, `--name` and a second `--` too. The terminator stays in effect after parsing switches to a subcommand.
- An element that is taken as the value of an option with a value is not classified. The `-x` of `--name -x` and the `--` of `--name --` are both values.
- Option names and subcommand names are compared as whole names, byte by byte. Uppercase and lowercase letters make different names, and a prefix of a name does not match.

Edge cases:

- An element in the form of a negative number (`-1`, `-.5`) is an operand only if the current command has no option whose short name is a digit. If it has even one, every such element is read as a bundle (in `-.5`, the `.` is an unknown option). To pass a negative number to a positional of such a command, write it after the terminator.

### How options take values

A long option is matched against the long names and the negated names. In a bundle of short options, the characters after the `-` are matched one at a time, from the first, against the short names.

| Form | Kind of option | Behavior |
| --- | --- | --- |
| `--name=value` | With a value | The text right after the `=` is the value (it may be empty) |
| `--name` | With a value | The next element is the value, whatever it contains. If there is none, `MISSING_VALUE` |
| `--name=value`, `--no-name=value` | Flag, negatable flag, count | `UNEXPECTED_VALUE` |
| Characters remain after `-o` | With a value | All of the remaining characters are the value. This ends the bundle |
| `-o` is the last in the bundle | With a value | The next element is the value, whatever it contains. If there is none, `MISSING_VALUE` |

Forms of options that take no value:

| Form | Kind of option | Behavior |
| --- | --- | --- |
| `--name`, `-n` | Flag | Given |
| `--name`, `-n` | Count | The count goes up by 1 |
| `--name`, `-n` | Negatable flag | On |
| `--no-name` | Negatable flag | Off |

- `-abc` is the same as `-a -b -c`, and `-vvv` gives a count 3 times. `-abovalue` is the same as `-a -b -ovalue`, and `-abo value` is the same as `-a -b -o value`.
- The name of a long option ends before the first `=`, so the value of `--name=a=b` is `a=b`.
- A value is passed to the value type as it was written, without changes; white space at the start and the end is kept. The input that each value type accepts is in [Schemas](schema.md#value-types).

Edge cases:

- In a bundle, the value of an option with a value is all of the remaining characters, so the value of `-o=value` is `=value`. The `=` is not removed.
- After an option that takes no value, every character is matched as a short name. The second `-` of `-a-b` and the `=` of `-a=1` cannot be short names, so they are unknown options (`unknown option '--'` and `unknown option '-='`).

### Repeated options

The same option can be given any number of times, by its short name or by its long name.

| Kind of option | Result of giving it more than once |
| --- | --- |
| Flag | Stays given |
| Count | The number of times it was given (`-v --verbose -vv` is 4) |
| Negatable flag | The last one given (`--color --no-color` is off; in the reverse order, on) |
| With a value | The last value (`--out a --out b` is `b`) |

Each value of an option with a value is converted by the value type as it is read, including a value that is later replaced. So `--num x --num 5` is a parse error for the failed conversion of `x`.

### Assigning positionals

In a command that has no subcommands, operands are assigned in the order they appear to the positionals, in the order the positionals were added.

| Arity of the positional | Assignment |
| --- | --- |
| `ARGX_ARITY_REQUIRED`, `ARGX_ARITY_OPTIONAL` | One value is assigned. The next operand goes to the next positional |
| `ARGX_ARITY_ZERO_OR_MORE`, `ARGX_ARITY_ONE_OR_MORE` | The value is assigned, and so is every later operand |

- Options can be written before, between or after the values of positionals. The assignment does not change.
- An operand that is left over after every positional has its values is an `UNEXPECTED_POSITIONAL` parse error. So is any operand in a command that has no positionals.
- A lone `-` and the empty string become values of positionals. To pass a value that starts with `-`, write it after the terminator `--`.
- A value is converted by the value type of its positional as it is read.

### Subcommands

In a command that has subcommands, an operand is matched against the names of its subcommands.

- The matching subcommand is selected and becomes the current command. The elements after its name are read with the options, positionals and subcommands of that subcommand. There is no limit to the depth of nesting.
- A name that matches none of the subcommands of the current command is an `UNKNOWN_SUBCOMMAND` parse error.
- **Write an option after the name of the command that defines it, and before the name of that command's subcommand.** The `-v` of `pack -v create …` is an option of `pack`. The `-v` of `pack create -v …` is looked up in `create`; if `create` has no `-v`, it is an unknown option. Subcommands do not inherit the options of their parent.
- When a parent and a child have options with the same name, each option belongs to its own command, at its own position.
- A command line that ends without the name of a subcommand is not a parse error. `argx_result_command` then returns the command that has subcommands. A program that requires a subcommand checks this itself, as `run` in [examples/pack.c](../examples/pack.c) does.
- An element after the terminator does not become the name of a subcommand. A command that has subcommands has no positionals, so it is an `UNEXPECTED_POSITIONAL` parse error.
- The options and positionals of subcommands that were not selected read as not given ([Rules for reading](#rules-for-reading)).

### Required checks and default values

When parsing has read the last element without a parse error, two steps are done for the selected commands. The options and positionals of commands that were not selected are neither checked nor given default values, even if they are required.

1. **The required checks.** A required option with a value (`argx_option_set_required`) with no value, or a positional of `ARGX_ARITY_REQUIRED` or `ARGX_ARITY_ONE_OR_MORE` with no value at all, is a `MISSING_OPTION` or `MISSING_POSITIONAL` parse error. When two or more are missing, the first one is reported: commands closer to the root first, and within one command, options before positionals, each in the order they were added.
2. **Applying the default values,** only when nothing is missing. Each option with a value and each positional of `ARGX_ARITY_OPTIONAL` that was not given and has a default value gets the default value, converted by its value type. Its source becomes `ARGX_SOURCE_DEFAULT`.

- After a parse that succeeded, every required option with a value of the selected commands has a value, and every positional of `ARGX_ARITY_REQUIRED` or `ARGX_ARITY_ONE_OR_MORE` has at least one.
- An option with a value or an optional positional that was not given and has no default value has no value (its source is `ARGX_SOURCE_NONE`). A positional of `ARGX_ARITY_ZERO_OR_MORE` that was not given has 0 values.
- A flag that was not given is "not given", a count is 0, and a negatable flag is in its initial state (`argx_option_set_initial`). Their source is `ARGX_SOURCE_DEFAULT`.
- The only sources of a value are the command line and the default values. Environment variables and the like are not read.
- If the conversion of a default value fails, it is an `INVALID_VALUE` parse error. Validation of the schema has already checked the default values, so this happens only when a user-defined value type gives a different answer for the same input.

## Lifetime rules

| Object | Owner | How long it is valid |
| --- | --- | --- |
| A result (`argx_result_t`) | You, who called `argx_parse` | Until you destroy it with `argx_result_destroy`. The same for a result with a parse error |
| Strings and value buffers read from the result | The result | Until the result is destroyed. Do not free or modify them |
| The details and message of a parse error | The result | Until the result is destroyed. Do not free or modify them |
| Commands, options and positionals read from the result | The schema | Until the root of the schema is destroyed |
| The argv array and strings | You | They need to be valid only during the call to `argx_parse` |
| The schema and the value types it uses | You | Do not change or destroy them until all of their results have been destroyed |

- **Destroy the result before the schema.** A result borrows the schema and the value types: destroying it cleans up the values it holds with the release functions of the value types. The order of destruction is: the result, the schema, the value types, and then your data that the value types borrowed ([Schemas](schema.md#borrowed-value-types-and-the-order-of-destruction)).
- **Do not change the schema while a result exists.** While even one result is left, do not add options, positionals or subcommands to the schema, or set attributes.
- **You may change or free argv after parsing.** The result holds copies of all the strings of argv, and refers to neither the argv array nor its strings after `argx_parse` returns.
- **Strings and values read from the result are valid until the result is destroyed.** To use one after that, copy it first.
- On an allocation failure (`ARGX_ERROR_OUT_OF_MEMORY`), `*out` is `NULL` and there is nothing to destroy. All the memory allocated for that call, and the converted values, have been cleaned up.
- `argx_result_destroy(NULL)` does nothing. If you initialize `result` to `NULL`, as [examples/pack.c](../examples/pack.c) does, the cleanup is the same on every path.
- Destroying a result neither changes nor destroys the schema and the value types, so you can parse with the same schema any number of times. Results are independent of each other and can be destroyed in any order.
- All memory is allocated from the allocator set for the library (`argx_set_allocator`). Do not set the allocator again while a result is left ([Ownership and Lifetime](ownership.md#5-allocator)).
- The library does not detect destroying the schema before a result, or changing the schema while a result exists.

## Preconditions

Breaking a precondition is a programming error. No error code is returned. Preconditions that can be checked are checked with `assert`, and in the Debug configuration the process aborts when one is broken. In the Release configuration they are not checked, and the behavior is undefined ([Ownership and Lifetime](ownership.md#runtime-failures-and-programming-errors)). The preconditions of each function are also written in the comments of the public header `include/argx/parse.h`, under `Preconditions:`.

What every function has in common:

- Pointer arguments are not `NULL` (`root`, `result`, `option`, `positional`, `out`). The only exceptions are the `result` of `argx_result_destroy`, and `argv` when `argc` is 0. Checked with `assert`.
- An `option` or `positional` belongs to the schema of this result: following the parents from its command leads to the root that was passed to `argx_parse`. Passing one of another schema breaks a precondition. Checked with `assert`.
- The result has not been destroyed, and its schema and the value types the schema uses have been neither destroyed nor changed. Not checked.

What each function adds (functions that are not listed add nothing):

| Function | Checked with `assert` | Not checked |
| --- | --- | --- |
| `argx_parse`, `argx_parse_wide` | `root` is a root command (not a subcommand), and its schema has been validated (`argx_command_is_validated`). `argc` is 0 or greater. If `argc` is greater than 0, `argv` and `argv[0]` to `argv[argc - 1]` are not `NULL` | The schema and its value types are neither changed nor destroyed until the result is destroyed |
| `argx_result_destroy` | — | `result` is `NULL` or a result that has not been destroyed |
| `argx_result_command_at` | `index` is less than `argx_result_command_count` | — |
| `argx_result_flag` | `option` is a flag or a negatable flag | — |
| `argx_result_count` | `option` is a count | — |
| `argx_result_option_source` | — (any kind of option) | — |
| `argx_result_option_string`, `_int`, `_float`, `_choice` | `option` is an option with a value whose value type is of the kind `ARGX_TYPE_STRING`, `ARGX_TYPE_INT`, `ARGX_TYPE_FLOAT` and `ARGX_TYPE_CHOICE` respectively | — |
| `argx_result_option_value` | `option` is an option with a value | The returned pointer is read with the C type of the value of the value type |
| `argx_result_positional_string`, `_int`, `_float`, `_choice` | The value type of `positional` is of the kind `ARGX_TYPE_STRING`, `ARGX_TYPE_INT`, `ARGX_TYPE_FLOAT` and `ARGX_TYPE_CHOICE` respectively | — |
| `argx_result_positional_value` | — | The returned pointer is read with the C type of the value of the value type |

- These do not break a precondition, and each returns the value that means "none" ([Reading the result](#reading-the-result)): reading an option or positional of a command that was not selected; reading the value of an option with a value or a positional that has no value; reading a positional with an index equal to or greater than the number of values; reading the details of a parse error from a result that succeeded.
- Adding an option, positional or subcommand to a schema, or setting an attribute, makes the schema not validated again. Validate it again before parsing ([Schemas](schema.md#validation)).
- Neither the compiler nor the library detects reading the pointer that `_value` returns with a type other than the C type of the value of the value type.
