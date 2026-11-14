# FAQ

Short answers to common questions, each with a link to the rules it comes from.

## Command lines

### Why is `--help` an unknown option after a subcommand?

An option is recognized only while its command is the current command, and the library adds no `--help` of its own. If only the root defines `--help`, `pack create --help` is an unknown option. Define `--help` on every command ([`--help`](../references/parse.md#--help), [Guide](guide.md#2-define-the-schema)).

### Why does `pack create --help` report a missing required option?

The required option is missing, so it is a parse error, but the result still holds the `--help` that was read before the error was found. Check `--help` before the parse error ([Guide](guide.md#4-parse-argv-and-check---help-first)).

### How do I require a subcommand?

A command line that ends without the name of a subcommand is not a parse error: `argx_result_command` then returns the command that has subcommands. Check it after parsing, as `run` in [examples/pack.c](../../examples/pack.c) does ([Subcommands](../references/syntax.md#subcommands)).

### Where do I write an option: before or after the subcommand?

After the name of the command that defines it, and before the name of that command's subcommand. Subcommands do not inherit the options of their parent ([Subcommands](../references/syntax.md#subcommands)).

### How do I pass a value that starts with `-`, or a negative number?

Write it after the terminator `--`; every element after it is a value. A negative number such as `-1` is also read as a value without the terminator, unless the current command has an option whose short name is a digit ([How elements are classified](../references/syntax.md#how-elements-are-classified)).

### What happens when an option is given more than once?

A flag stays given, a count counts every time, and a negatable flag and an option with a value take the last one ([Repeated options](../references/syntax.md#repeated-options)).

### Can values come from environment variables?

No. The only sources of a value are the command line and the default values; the library does not read environment variables ([Required checks and default values](../references/syntax.md#required-checks-and-default-values)).

### How do I receive non-ASCII arguments on Windows?

Use `wmain` and pass its argv to `argx_parse_wide`. Every string read from the result is UTF-8 ([Guide](guide.md#wide-character-argv-with-wmain), [Wide-character argv](../references/parse.md#wide-character-argv)).

## Values

### How do I read the value of a user-defined value type?

Call `argx_result_option_value` or `argx_result_positional_value`, and read the pointer it returns with the C type that your conversion function wrote ([Reading the result](../references/parse.md#reading-the-result)).

### Why does validation fail with `ARGX_ERROR_INVALID_DEFAULT`?

The value type of an option or positional cannot convert its default value. The output parameters of `argx_command_validate` tell which definition it is ([Validation](../references/schema.md#validation), [Guide](guide.md#3-validate-the-schema)).

## Help

### Can I translate the help, add colors, or hide an option?

No. The fixed phrases are in English only, and there is no way to add colors, reorder the parts, hide an option or group options ([Limits](../references/help-layout.md#limits)).

### How do I fit the help to the terminal?

Find the width of the terminal in your program and set it with `argx_help_options_set_width`. The library reads neither the terminal nor environment variables ([Help options](../references/help.md#help-options)).

### Why does a description with non-ASCII characters wrap too early?

Widths are counted in bytes, and a non-ASCII character takes 2 bytes or more in UTF-8. The library does not look up display widths ([Limits](../references/help-layout.md#limits)).

### How do I get the help as a string?

Call `argx_help_format` twice: first with the size 0 for the length, then with an array of the length plus 1 ([Guide](guide.md#getting-the-help-as-a-string), [Formatting into a buffer](../references/help.md#formatting-into-a-buffer)).

## Building

### Why did a wrong call not stop my program?

Preconditions are checked with `assert` only in the Debug configuration. In the Release configuration, a broken precondition is undefined behavior. Use the Debug configuration during development ([Runtime failures and programming errors](../references/ownership.md#runtime-failures-and-programming-errors)).
