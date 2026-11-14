# FAQ

Why ArgX behaves as it does. For step-by-step tasks, see [How-to](how-to.md).

## Command lines

### Why is `--help` an unknown option after a subcommand?

An option is recognized only on the command that defines it, and the library adds no `--help` of its own. Define `--help` on every command ([`--help`](../references/parse.md#--help)).

### Why does `pack create --help` report a missing required option?

It is a parse error, but the result still holds `--help`. Check `--help` before the parse error ([Tutorial](../getting-started/tutorial.md#4-parse-argv-and-check---help-first)).

### Where do I write an option: before or after the subcommand?

After the name of the command that defines it. Subcommands do not inherit their parent's options ([Subcommands](../references/syntax.md#subcommands)).

### What happens when an option is given more than once?

A flag stays given, a count counts every time, and the others take the last one ([Repeated options](../references/syntax.md#repeated-options)).

### Can values come from environment variables?

No. Values come only from the command line and the defaults.

## Values

### Why does validation fail with `ARGX_ERROR_INVALID_DEFAULT`?

A value type cannot convert the default value of its option or positional. The output parameters of `argx_command_validate` tell which one ([Validation](../references/schema.md#validation)).

## Help

### Can I translate the help, add colors, or hide an option?

No. The fixed phrases are English only, and the parts cannot be colored, reordered, hidden or grouped ([Limits](../references/help-layout.md#limits)).

### Why does a description with non-ASCII characters wrap too early?

Widths are counted in bytes, not display columns ([Limits](../references/help-layout.md#limits)).

## Debugging

### Why did a wrong call not stop my program?

Preconditions are checked with `assert` only in Debug; in Release, breaking one is undefined behavior ([Runtime failures and programming errors](../references/ownership.md#runtime-failures-and-programming-errors)).
