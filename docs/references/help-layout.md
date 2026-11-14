# Help Text Layout

The text that the functions of [Help and Usage](help.md) generate. It depends only on the schema and the help options, never on the locale, the environment or the terminal.

The rules assume names, value names, defaults and choices without leading or trailing blanks or newlines, and descriptions that do not start with a newline or a blank line; see [Descriptions](#descriptions) and [Limits](#limits) for the rest.

The examples use `pack create`, with the description `Create an archive.` and these definitions in order:

| Definition | Kind | Value name | Attributes | Description |
| --- | --- | --- | --- | --- |
| `-h, --help` | Flag | | | `Print this help.` |
| `-o, --output` | Option with a value, string | `FILE` | Required | `The archive to write.` |
| `-l, --level` | Option with a value, integer | `LEVEL` | Default value `6` | `The compression level.` |
| `file` | Positional, one or more, string | | | `The files to pack.` |

```
Usage: pack create [OPTIONS] --output <FILE> <file>...   <- usage line

Create an archive.                                       <- description of the command

Arguments:                                               <- list
  <file>...            The files to pack.                <- item

Options:
  -h, --help           Print this help.
  -o, --output <FILE>  The archive to write. [required]
  ^^^^^^^^^^^^^^^^^^^  ^^^^^^^^^^^^^^^^^^^^^ ^^^^^^^^^^
  heading              description text      annotation
  |                    |
  indent (column 2)    description column (column 23)
```

- A **list** is `Commands:`, `Arguments:` or `Options:`; an **item** is one entry in it. An item's **description** is its description text plus its annotations.
- A **column** is a byte offset from the start of the line, from 0. All widths are in bytes.
- Lines end with `\n` only, including the last one, and never with a blank unless a name does.
- Fixed phrases are English. Schema strings are written unchanged; descriptions and annotations are wrapped.

## Structure

| Order | Part | Appears when | Content |
| --- | --- | --- | --- |
| 1 | Usage line | Always | See [Usage line](#usage-line) |
| 2 | Description of the command | It is set | From column 0, wrapped, without a trailing newline |
| 3 | `Commands:` | There are subcommands | The direct subcommands, with the first line of each description |
| 4 | `Arguments:` | There are positionals | The positionals, with full descriptions |
| 5 | `Options:` | There are options | The options, with full descriptions |

- Parts are separated by one empty line, with none at the start or end. Absent parts leave no trace.
- Lists show items in definition order, and only those of the command itself, never the parent's.

A command with nothing defined:

```
Usage: tool
```

## Usage line

| Order | Item | Appears when | Notation |
| --- | --- | --- | --- |
| 1 | Command path | Always | `Usage: ` and the names from the root, separated by blanks |
| 2 | `[OPTIONS]` | There is an option that is not required | `[OPTIONS]` |
| 3 | Required options | Each required option with a value, in order | `--long <value name>`, or `-s <value name>` |
| 4 | Positionals | Each positional, in order | By arity, below |
| 5 | `[COMMAND]` | There are subcommands | `[COMMAND]` |

| Arity | Notation (also the heading in `Arguments:`) |
| --- | --- |
| `ARGX_ARITY_REQUIRED` | `<name>` |
| `ARGX_ARITY_OPTIONAL` | `[name]` |
| `ARGX_ARITY_ZERO_OR_MORE` | `[name]...` |
| `ARGX_ARITY_ONE_OR_MORE` | `<name>...` |

```
Usage: tool --output <FILE> -j <integer> <source> [target] [extra]...
Usage: tool remote add [OPTIONS] <name>
```

- Items are separated by one blank. When an item would pass the width (if not 0), the blank becomes a newline and 7 blanks, aligning with the command path.
- Items are never split; one wider than the width runs past it on its own line. The command path is never wrapped.

At the width 40:

```
Usage: pack create [OPTIONS]
       --output <FILE> <file>...
```

## Headings

A subcommand's heading is its name; a positional's is its usage notation. An option's heading is:

| Names | Notation |
| --- | --- |
| Short and long | `-s, --long` |
| Short only | `-s` |
| Long only | 4 blanks, then `--long` |

followed by:

| Kind | Notation | Example |
| --- | --- | --- |
| Flag | Nothing | `-h, --help` |
| Negatable flag | `--[no-]long` | `--[no-]color` |
| Count | `...` | `-v, --verbose...` |
| Option with a value | A blank and `<value name>`, or the type name without one | `-o, --output <FILE>`, `-j <integer>` |

```
Options:
  -h, --help           Both names
  -q                   A short name only
      --dry-run        A long name only
  -c, --[no-]check     A negatable flag
  -d...                A count with a short name only
  -j <integer>         No value name: the name of the type
      --mode <choice>  A choice [choices: fast, small]
```

## Annotations

Annotations follow the description text of options and positionals, in this order:

| Order | Annotation | Appears when |
| --- | --- | --- |
| 1 | `[choices: a, b, c]` | The value type is a choice; choices in definition order |
| 2 | `[default: value]` | A default value is set |
| 2 | `[default: on]` | A negatable flag is initially on |
| 3 | `[required]` | An option with a value is required (positionals show it in their heading) |

- Annotations are separated from the text and each other by one blank, and wrap like descriptions.
- Without description text, they start at the description column.
- If the text ends with a newline, they start on the next line at the description column; each extra newline adds an empty line. Without annotations, a trailing newline is not written.

```
Options:
  -m, --mode <MODE>    What to prefer. [choices: fast, small] [default: fast]
  -k, --kind <choice>  [choices: fast, small] [required]
      --[no-]color     Color the output. [default: on]
  -t, --title <TEXT>   The title of the archive.
                       [default: untitled]
```

## Description column

An item is the indent, the heading, and the description from the description column, which is shared by all lists of one help.

1. **The maximum** is the smaller of the maximum description column option and half the width (rounded down); with width 0, just the option.
2. The maximum is raised to "indent + 2" if lower.
3. **The description column** is "indent + widest heading + 2", capped at the maximum.

If "indent + heading + 2" fits within the column, the description follows on the same line; otherwise it starts on the next line at the column.

```
Options:
  -v, --verbose...              Say more
      --a-rather-long-name-of-option <FILE>
                                Write to FILE
      --exactly-two-blanks-fit  On the line of the heading
      --one-blank-is-not-enough
                                On the next line
```

## Wrapping

Descriptions stay within the width, with continuation lines at the starting column (0 for the command, the description column for items).

- A word is a run of bytes other than blank (0x20) and newline (0x0A); tabs are part of words.
- Lines break only at blanks, before the word that would pass the width. Words are never split; a first word that is too wide runs past the width.
- A newline always starts a new line; consecutive newlines give empty lines.
- Blanks inside a line and after a newline are kept (use them to indent); blanks at a break or a line end are not written.
- With the width 0, lines break only at newlines.

```
Options:
  -x, --exclude <PATTERN>  Leave out the files that match:
                             *.o    one kind of file
                             build/ a directory
  -u, --url                See
                           https://example.com/a/very/long/address/that/has/no/blank/in/it/at/all
                           for more.
```

## Descriptions

- **A description of only blanks and newlines counts as not set.** In `Commands:`, a subcommand whose first line is blank shows only its heading.
- **Trailing blanks are dropped; trailing newlines are kept:** `"foo\n  "` is `"foo\n"`.
- **A leading newline or blank line is kept,** and may add empty lines between parts. Its exact output may change in later versions.

## Limits

- **Widths are counted in bytes,** so non-ASCII text wraps early and widens the description column. Tabs and control characters count one column per byte.
- **A newline in a name breaks the columns.** Names and value names are written as they are, newlines included, but still measured as one line. Do not put newlines in names.
- **Blanks in names and `\r` anywhere are written unchanged,** so a root named `"tool "` gives `Usage: tool `.
- The help cannot be translated, colored, reordered, hidden or grouped, and a subcommand's help never shows its parent's options.
