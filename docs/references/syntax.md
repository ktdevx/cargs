# Command-Line Syntax

This document specifies the command lines that a program using ArgX accepts: how each element of argv is read, how options take values, how positionals and subcommands are matched, and when required items are checked and default values applied. The rules are the same for `argx_parse` and `argx_parse_wide`. What parsing returns, and the parse errors named here, are in [Parsing](parse.md).

## Terms

- An **element** is one string of argv (`argv[1]` onward).
- The **terminator** is the first `--` that is read as an element on its own. Every element after it is a value.
- The **current command** is the root at first. It becomes a subcommand each time the name of that subcommand appears. **Options are looked up only in the current command**: the options of the parent and of the subcommands are not looked at ([Subcommands](#subcommands)).
- An **operand** is an element that is neither an option nor the terminator. If the current command has subcommands, an operand is the name of a subcommand; if it does not, it is the value of a positional. In a validated schema, no command has both subcommands and positionals.

## How elements are classified

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

## How options take values

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
- A value is passed to the value type as it was written, without changes; white space at the start and the end is kept. The input that each value type accepts is in [Value Types](types.md#accepted-input).

Edge cases:

- In a bundle, the value of an option with a value is all of the remaining characters, so the value of `-o=value` is `=value`. The `=` is not removed.
- After an option that takes no value, every character is matched as a short name. The second `-` of `-a-b` and the `=` of `-a=1` cannot be short names, so they are unknown options (`unknown option '--'` and `unknown option '-='`).

## Repeated options

The same option can be given any number of times, by its short name or by its long name.

| Kind of option | Result of giving it more than once |
| --- | --- |
| Flag | Stays given |
| Count | The number of times it was given (`-v --verbose -vv` is 4) |
| Negatable flag | The last one given (`--color --no-color` is off; in the reverse order, on) |
| With a value | The last value (`--out a --out b` is `b`) |

Each value of an option with a value is converted by the value type as it is read, including a value that is later replaced. So `--num x --num 5` is a parse error for the failed conversion of `x`.

## Assigning positionals

In a command that has no subcommands, operands are assigned in the order they appear to the positionals, in the order the positionals were added.

| Arity of the positional | Assignment |
| --- | --- |
| `ARGX_ARITY_REQUIRED`, `ARGX_ARITY_OPTIONAL` | One value is assigned. The next operand goes to the next positional |
| `ARGX_ARITY_ZERO_OR_MORE`, `ARGX_ARITY_ONE_OR_MORE` | The value is assigned, and so is every later operand |

- Options can be written before, between or after the values of positionals. The assignment does not change.
- An operand that is left over after every positional has its values is an `UNEXPECTED_POSITIONAL` parse error. So is any operand in a command that has no positionals.
- A lone `-` and the empty string become values of positionals. To pass a value that starts with `-`, write it after the terminator `--`.
- A value is converted by the value type of its positional as it is read.

## Subcommands

In a command that has subcommands, an operand is matched against the names of its subcommands.

- The matching subcommand is selected and becomes the current command. The elements after its name are read with the options, positionals and subcommands of that subcommand. There is no limit to the depth of nesting.
- A name that matches none of the subcommands of the current command is an `UNKNOWN_SUBCOMMAND` parse error.
- **Write an option after the name of the command that defines it, and before the name of that command's subcommand.** The `-v` of `pack -v create …` is an option of `pack`. The `-v` of `pack create -v …` is looked up in `create`; if `create` has no `-v`, it is an unknown option. Subcommands do not inherit the options of their parent.
- When a parent and a child have options with the same name, each option belongs to its own command, at its own position.
- A command line that ends without the name of a subcommand is not a parse error. `argx_result_command` then returns the command that has subcommands. A program that requires a subcommand checks this itself, as `run` in [examples/pack.c](../../examples/pack.c) does.
- An element after the terminator does not become the name of a subcommand. A command that has subcommands has no positionals, so it is an `UNEXPECTED_POSITIONAL` parse error.
- The options and positionals of subcommands that were not selected read as not given ([Rules for reading](parse.md#rules-for-reading)).

## Required checks and default values

When parsing has read the last element without a parse error, two steps are done for the selected commands. The options and positionals of commands that were not selected are neither checked nor given default values, even if they are required.

1. **The required checks.** A required option with a value (`argx_option_set_required`) with no value, or a positional of `ARGX_ARITY_REQUIRED` or `ARGX_ARITY_ONE_OR_MORE` with no value at all, is a `MISSING_OPTION` or `MISSING_POSITIONAL` parse error. When two or more are missing, the first one is reported: commands closer to the root first, and within one command, options before positionals, each in the order they were added.
2. **Applying the default values,** only when nothing is missing. Each option with a value and each positional of `ARGX_ARITY_OPTIONAL` that was not given and has a default value gets the default value, converted by its value type. Its source becomes `ARGX_SOURCE_DEFAULT`.

- After a parse that succeeded, every required option with a value of the selected commands has a value, and every positional of `ARGX_ARITY_REQUIRED` or `ARGX_ARITY_ONE_OR_MORE` has at least one.
- An option with a value or an optional positional that was not given and has no default value has no value (its source is `ARGX_SOURCE_NONE`). A positional of `ARGX_ARITY_ZERO_OR_MORE` that was not given has 0 values.
- A flag that was not given is "not given", a count is 0, and a negatable flag is in its initial state (`argx_option_set_initial`). Their source is `ARGX_SOURCE_DEFAULT`.
- The only sources of a value are the command line and the default values. Environment variables and the like are not read.
- If the conversion of a default value fails, it is an `INVALID_VALUE` parse error. Validation of the schema has already checked the default values, so this happens only when a user-defined value type gives a different answer for the same input.

