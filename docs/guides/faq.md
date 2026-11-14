# FAQ

## Command lines

### Why is `--help` an unknown option after a subcommand?

An option is recognized only on the command that defines it, and the library adds no `--help` of its own. Define `--help` on every command ([`--help`](../references/parse.md#--help)).

### Why does `pack create --help` report a missing required option?

It is a parse error, but the result still holds `--help`. Check `--help` before the parse error ([Tutorial](tutorial.md#4-parse-argv-and-check---help-first)).

### How do I require a subcommand?

Check after parsing whether `argx_result_command` returns a command that has subcommands, as `run` in [examples/pack.c](../../examples/pack.c) does. A missing subcommand is not a parse error.

### Where do I write an option: before or after the subcommand?

After the name of the command that defines it. Subcommands do not inherit their parent's options ([Subcommands](../references/syntax.md#subcommands)).

### How do I pass a value that starts with `-`, or a negative number?

Write it after `--`. A negative number such as `-1` is a value even without `--`, unless the command has a digit as a short option name ([How elements are classified](../references/syntax.md#how-elements-are-classified)).

### What happens when an option is given more than once?

A flag stays given, a count counts every time, and the others take the last one ([Repeated options](../references/syntax.md#repeated-options)).

### Can values come from environment variables?

No. Values come only from the command line and the defaults.

### How do I receive non-ASCII arguments on Windows?

Use `wmain` and pass its argv to `argx_parse_wide`. Strings read from the result are UTF-8 ([Wide-character argv](../references/parse.md#wide-character-argv)).

## Values

### How do I read the value of a user-defined value type?

Read the pointer from `argx_result_option_value` or `argx_result_positional_value` as the C type your conversion function wrote ([Reading the result](../references/parse.md#reading-the-result)).

### Why does validation fail with `ARGX_ERROR_INVALID_DEFAULT`?

A value type cannot convert the default value of its option or positional. The output parameters of `argx_command_validate` tell which one ([Validation](../references/schema.md#validation)).

## Help

### Can I translate the help, add colors, or hide an option?

No. The fixed phrases are English only, and the parts cannot be colored, reordered, hidden or grouped ([Limits](../references/help-layout.md#limits)).

### How do I fit the help to the terminal?

Get the terminal width in your program and pass it to `argx_help_options_set_width` ([Help options](../references/help.md#help-options)).

### Why does a description with non-ASCII characters wrap too early?

Widths are counted in bytes, not display columns ([Limits](../references/help-layout.md#limits)).

### How do I get the help as a string?

Call `argx_help_format` with size 0 to get the length, then with a buffer of the length plus 1 ([Formatting into a buffer](../references/help.md#formatting-into-a-buffer)).

## Debugging

### Why did a wrong call not stop my program?

Preconditions are checked with `assert` only in Debug; in Release, breaking one is undefined behavior ([Runtime failures and programming errors](../references/ownership.md#runtime-failures-and-programming-errors)).
