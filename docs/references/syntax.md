# Command-Line Syntax

The command lines a program using ArgX accepts, for both `argx_parse` and `argx_parse_wide`. Results and parse errors are in [Parsing](parse.md).

## Terms

- An **element** is one string of argv (`argv[1]` onward).
- The **terminator** is the first `--` read as an element on its own. Every element after it is a value.
- The **current command** is the root, then each subcommand whose name appears. **Options are looked up only in the current command.**
- An **operand** is an element that is neither an option nor the terminator: a subcommand name if the current command has subcommands, otherwise a positional value. No validated command has both.

## How elements are classified

Each element is classified by the first matching row.

| Form of the element | Condition | Classified as |
| --- | --- | --- |
| Any | After the terminator | A positional value (`UNEXPECTED_POSITIONAL` in a command with subcommands) |
| `--` | — | The terminator. Not a value |
| `--x…` | — | A long option, split into name and value at the first `=` |
| `-` | — | An operand |
| `-N…`, `-.N…` (N is a digit) | The current command has no digit short name | An operand |
| `-x…` | — | A bundle of short options |
| Anything else, including `""` | — | An operand |

- The terminator stays in effect across subcommands.
- An element taken as the value of an option is not classified: in `--name -x` and `--name --`, `-x` and `--` are values.
- Names are compared whole, byte by byte, and case-sensitively. Prefixes do not match.
- If the current command has a digit short name, `-1` and `-.5` are bundles; write negative numbers after the terminator.

## How options take values

Long options are matched against long and negated names; each character of a bundle is matched against short names in turn.

| Form | Kind of option | Behavior |
| --- | --- | --- |
| `--name=value` | With a value | The text after `=` is the value (may be empty) |
| `--name` | With a value | The next element is the value, whatever it is; `MISSING_VALUE` if none |
| `--name=value`, `--no-name=value` | Flag, negatable flag, count | `UNEXPECTED_VALUE` |
| `-ovalue` | With a value | The rest of the bundle is the value |
| `-o` last in the bundle | With a value | The next element is the value; `MISSING_VALUE` if none |
| `--name`, `-n` | Flag | Given |
| `--name`, `-n` | Count | Count goes up by 1 |
| `--name`, `-n` | Negatable flag | On |
| `--no-name` | Negatable flag | Off |

- `-abc` is `-a -b -c`; `-abovalue` is `-a -b -ovalue`; `-abo value` is `-a -b -o value`.
- The value of `--name=a=b` is `a=b`, and of `-o=value` is `=value`.
- Values are passed to the value type unchanged, white space included ([Accepted input](types.md#accepted-input)).
- In `-a-b` and `-a=1`, `-` and `=` are matched as short names, giving `unknown option '--'` and `unknown option '-='`.

## Repeated options

| Kind of option | Given more than once |
| --- | --- |
| Flag | Stays given |
| Count | The number of times (`-v --verbose -vv` is 4) |
| Negatable flag | The last one (`--color --no-color` is off) |
| With a value | The last value (`--out a --out b` is `b`) |

Every value is converted as it is read, so `--num x --num 5` is a parse error.

## Assigning positionals

Operands are assigned in order to the positionals in the order they were added. A positional of `ARGX_ARITY_REQUIRED` or `ARGX_ARITY_OPTIONAL` takes one value; one of `ARGX_ARITY_ZERO_OR_MORE` or `ARGX_ARITY_ONE_OR_MORE` takes the rest.

- Options may appear anywhere among the positional values.
- A leftover operand, or any operand in a command without positionals, is `UNEXPECTED_POSITIONAL`.
- `-` and `""` are positional values. A value starting with `-` goes after the terminator.

## Subcommands

- An operand matching a subcommand name makes it the current command, at any depth. Any other name is `UNKNOWN_SUBCOMMAND`.
- **An option goes after the name of its command and before the name of that command's subcommand.** In `pack -v create`, `-v` belongs to `pack`; in `pack create -v`, to `create`. Subcommands do not inherit options, and same-named options in parent and child are separate.
- Ending without a subcommand name is not a parse error; `argx_result_command` then returns the command with subcommands.
- Elements after the terminator never name a subcommand.
- Options and positionals of unselected commands read as not given ([Rules for reading](parse.md#rules-for-reading)).

## Required checks and default values

After the last element is read without a parse error, for the selected commands only:

1. **Required checks.** A required option with a value that has no value, or a positional of `ARGX_ARITY_REQUIRED` or `ARGX_ARITY_ONE_OR_MORE` with no value, is `MISSING_OPTION` or `MISSING_POSITIONAL`. The first one is reported: commands nearer the root first, then options before positionals, in the order added.
2. **Default values,** if nothing is missing. An option with a value or an `ARGX_ARITY_OPTIONAL` positional that was not given gets its converted default, with the source `ARGX_SOURCE_DEFAULT`.

- Without a default, such an item has no value (`ARGX_SOURCE_NONE`); a `ARGX_ARITY_ZERO_OR_MORE` positional has 0 values.
- A flag not given is not given, a count is 0, and a negatable flag has its initial state (`argx_option_set_initial`), all with `ARGX_SOURCE_DEFAULT`.
- Values come only from the command line and defaults, never from the environment.
- A failed default conversion is `INVALID_VALUE`. Validation already checked the defaults, so only a user-defined value type that answers inconsistently can cause it.
