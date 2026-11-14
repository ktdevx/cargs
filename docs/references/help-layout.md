# Help Text Layout

This document specifies the text of the help and of the usage line that the generating functions of [Help and Usage](help.md) produce: the parts of the help and their order, the usage line, the headings, the annotations, the description column, wrapping, and the limits.

The text follows from the schema and the help options by the rules below and nothing else: the same command and help options always give the same bytes, whatever the locale, the environment variables or the terminal. Tests can compare it with an expected string exactly.

The rules describe a schema whose names, value names, default values and choices have no leading or trailing blanks and no newlines, and whose descriptions do not start with a newline or a line of blanks. Other schemas are covered in [Descriptions](#descriptions) and [Limits](#limits).

The examples of this document use `pack create`, a subcommand of the made-up program `pack`. It has the description `Create an archive.` and these definitions, in this order:

| Definition | Kind | Value name | Attributes | Description |
| --- | --- | --- | --- | --- |
| `-h, --help` | Flag | | | `Print this help.` |
| `-o, --output` | Option with a value, string | `FILE` | Required | `The archive to write.` |
| `-l, --level` | Option with a value, integer | `LEVEL` | Default value `6` | `The compression level.` |
| `file` | Positional, one or more, string | | | `The files to pack.` |

The parts of a help have these names in this document (the help of `pack create`):

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

- A **list** is one of `Commands:`, `Arguments:` and `Options:`. An **item** is one subcommand, positional or option in a list.
- The **description** of an item is its description text followed by its annotations.
- A **column** is a byte offset from the start of the line; the start is column 0. Every width in this document is a number of bytes.

What holds for every text:

- Lines are separated by `\n` only, never `\r\n`. The text ends with `\n`.
- No line ends with a blank, unless a name of the schema does ([Limits](#limits)).
- The fixed phrases (`Usage:`, `Commands:`, `[OPTIONS]`, `[default: …]` and so on) are in English.
- The strings of the schema are written without any conversion, case included. Names and value names are written as they are. Descriptions and annotations are wrapped ([Wrapping](#wrapping)), which drops the blanks at the end of a line and at the wrapping points. A newline at the end of a description is not written, except before annotations ([Annotations](#annotations)).

## Structure

The help consists of these parts, in this order:

| Order | Part | Appears when | Content |
| --- | --- | --- | --- |
| 1 | Usage line | Always | See [Usage line](#usage-line) |
| 2 | Description of the command | A description is set | Starts at column 0, wrapped at the width. A newline at the end is not written |
| 3 | `Commands:` | There is a subcommand | The direct subcommands. The heading is the name, and the description is the first line of the description text |
| 4 | `Arguments:` | There is a positional | The positionals, with the whole description text and the annotations |
| 5 | `Options:` | There is an option | The options, with the whole description text and the annotations |

- A part that does not appear leaves out its name line too.
- One empty line separates the parts. There is no empty line at the start or at the end of the text.
- A list is its name on a line of its own, followed by its items in the order they were defined.
- A list shows only what is defined on the command itself: not the options of the parent, not what belongs to subcommands, and no option that is not in the schema.
- A command cannot have both positionals and subcommands ([Schemas](schema.md#commands)), so a help has at most two lists: `Commands:` and `Options:`, or `Arguments:` and `Options:`.

The help of a command `tool` with no description, options, positionals or subcommands is the usage line alone:

```
Usage: tool
```

A subcommand in `Commands:` shows only the first line of its description. Here `create` has a description of two lines, and `check` has none:

```
Usage: tool [COMMAND]

Commands:
  create  Create an archive.
  check
```

The help of `create` itself shows the whole description:

```
Usage: tool create

Create an archive.
The files are packed in the order of the command line.
```

## Usage line

The usage line consists of these items, in this order:

| Order | Item | Appears when | Notation |
| --- | --- | --- | --- |
| 1 | Command path | Always | `Usage: ` and the names from the root to the command, separated by one blank |
| 2 | `[OPTIONS]` | There is an option that is not required | `[OPTIONS]`. Flags, negatable flags and counts are never required |
| 3 | Required options | One per required option with a value, in definition order | `--long <value name>`, or `-s <value name>` without a long name |
| 4 | Positionals | One per positional, in definition order | By arity, as in the next table |
| 5 | `[COMMAND]` | There is a subcommand | `[COMMAND]` |

The notation of a positional, which the headings of `Arguments:` use too:

| Arity | Notation |
| --- | --- |
| `ARGX_ARITY_REQUIRED` | `<name>` |
| `ARGX_ARITY_OPTIONAL` | `[name]` |
| `ARGX_ARITY_ZERO_OR_MORE` | `[name]...` |
| `ARGX_ARITY_ONE_OR_MORE` | `<name>...` |

When no value name is set, the name of the value type is shown ([Headings](#headings)).

A command with the required options `--output` (value name `FILE`) and `-j` (an integer with no long name and no value name), and the positionals `source` (required), `target` (optional) and `extra` (zero or more):

```
Usage: tool --output <FILE> -j <integer> <source> [target] [extra]...
```

A command with the flag `-q` and the positional `file` (one or more):

```
Usage: tool [OPTIONS] <file>...
```

`add`, a subcommand of `remote`, which is a subcommand of `tool`; then `remote` itself:

```
Usage: tool remote add [OPTIONS] <name>
Usage: tool remote [COMMAND]
```

Wrapping:

- Items are separated by one blank.
- When the width is not 0 and "current column + 1 + width of the item" exceeds the width, a newline and 7 blanks replace that blank. 7 is the width of `Usage: `, so continuation lines line up with the command path.
- An item is never split, not even at a blank in a value name. An item wider than the width goes on a line of its own and runs past the width.
- The command path always follows `Usage: ` directly and is never wrapped, even when it is wider than the width.
- With the width 0, the line is never wrapped.

The usage line of `pack create` fits on one line at the default width 80. At the width 40, `--output <FILE>` after `[OPTIONS]` would reach column 44, so the line wraps there:

```
Usage: pack create [OPTIONS]
       --output <FILE> <file>...
```

A required option with the value name `FILE OR DIRECTORY`, at the width 20. The item on the second line is 28 columns wide, but it is not split:

```
Usage: tool
       --output <FILE OR DIRECTORY>
       <source>
```

## Headings

The heading of a subcommand is its name. The heading of a positional is its notation in the [usage line](#usage-line). The heading of an option is its names, followed by a part that depends on its kind.

The names:

| Names of the option | Notation |
| --- | --- |
| Short and long | `-s, --long` |
| Short only | `-s` |
| Long only | 4 blanks, then `--long`, so that it lines up with the long names of the options above |

The part for each kind:

| Kind | Notation | Example |
| --- | --- | --- |
| Flag | Nothing | `-h, --help` |
| Negatable flag | The long name is written `--[no-]long` | `--[no-]color` |
| Count | `...` right after the names | `-v, --verbose...` |
| Option with a value | One blank, then `<value name>` | `-o, --output <FILE>` |

An option with a value and no value name (`argx_option_set_value_name`) shows the name of its value type instead: `string`, `integer` or `float` for the built-in types, `choice` for a choice type, and the name given at creation for a user-defined type. The usage line does the same.

Every notation in one help:

```
Usage: tool [OPTIONS]

Options:
  -h, --help           Both names
  -q                   A short name only
      --dry-run        A long name only
      --[no-]color     A negatable flag
  -c, --[no-]check     A negatable flag with a short name
  -v, --verbose...     A count
  -d...                A count with a short name only
  -o, --output <FILE>  A value with a value name
  -j <integer>         A value without one: the name of the type
      --ratio <float>  A float
      --name <string>  A string
      --mode <choice>  A choice [choices: fast, small]
```

## Annotations

Annotations follow the description text of an option or a positional, in this order. Subcommands have none.

| Order | Annotation | Appears when |
| --- | --- | --- |
| 1 | `[choices: a, b, c]` | The value type is a choice. The choices are in definition order, separated by `, ` |
| 2 | `[default: value]` | A default value is set. The value is the string that was set, wrapped like the rest of the description |
| 2 | `[default: on]` | A negatable flag whose initial state is on. Off gives no annotation |
| 3 | `[required]` | A required option with a value. Positionals never get it: their heading already shows whether they are required |

- One blank separates the description text from an annotation, and annotations from each other.
- With no description text, the annotations start at the description column.
- When the description text ends with a newline, the annotations start on the next line at the description column, with no blank before them. Use this to put annotations on a line of their own. Each further newline at the end adds an empty line before the annotations. With no annotations, a newline at the end of the text is not written.
- Annotations wrap like descriptions, including at the blanks inside an annotation.
- `[default: …]` and `[required]` never appear together: a required option with a default value fails validation ([Schemas](schema.md#errors-found-by-validation)).

Below, `--kind` has no description; the description of `--title` ends with a newline (`"The title of the archive.\n"`); `--color` is initially on and `--sort` initially off:

```
Usage: tool [OPTIONS] --output <FILE> --kind <choice> <format> [level]

Arguments:
  <format>             The format of the archive. [choices: tar, zip, cpio]
  [level]              The compression level. [default: 6]

Options:
  -m, --mode <MODE>    What to prefer. [choices: fast, small] [default: fast]
  -o, --output <FILE>  The archive to write. [required]
  -k, --kind <choice>  [choices: fast, small] [required]
      --[no-]color     Color the output. [default: on]
      --[no-]sort      Sort the names.
  -t, --title <TEXT>   The title of the archive.
                       [default: untitled]
```

## Description column

An item is the indent (2 blanks by default), then the heading, then the description starting at the description column. One help has one description column, shared by all its lists. An item with neither description text nor annotations is the heading alone.

The description column is decided in three steps:

1. **The maximum** is the smaller of the maximum description column help option (32 by default) and half of the width, rounded down. With the width 0, only the help option counts.
2. If the maximum is less than "indent + 2", it becomes "indent + 2".
3. **The description column** is "indent + width of the widest heading + 2", but no more than the maximum.

An item whose "indent + heading + 2" fits within the description column has its description on the same line, at least 2 blanks after the heading. Otherwise the heading is on a line of its own, and the description starts at the description column of the next line.

In the help of `pack create` (at the start of this document), the widest headings, `-o, --output <FILE>` and `-l, --level <LEVEL>`, are 19 columns. The description column is 2 + 19 + 2 = 23, below the maximum 32, so both lists start their descriptions at column 23.

Below, with the default help options, the widest heading is 41 columns, so the description column is the maximum, 32. `--exactly-two-blanks-fit` is 28 columns, and 2 + 28 + 2 = 32 still fits on its line; `--one-blank-is-not-enough`, one column wider, does not. The last option has no description:

```
Usage: tool [OPTIONS]

Options:
  -v, --verbose...              Say more
      --a-rather-long-name-of-option <FILE>
                                Write to FILE
      --exactly-two-blanks-fit  On the line of the heading
      --one-blank-is-not-enough
                                On the next line
      --a-long-name-without-a-description
```

At the width 40, the maximum is half the width, 20:

```
Usage: tool [OPTIONS]

Options:
  -o, --output <FILE>
                    The archive to
                    write.
      --compression-level <LEVEL>
                    The compression
                    level.
```

## Wrapping

Descriptions are wrapped so that they stay within the width. The description of the command starts at column 0, the description of an item at the description column, and continuation lines start at the same column.

- A word is a run of bytes other than the blank (0x20) and the newline (0x0A). Tabs and other bytes are part of words.
- Lines break only at blanks between words, before the word that would exceed the width. Words are never split, and the blanks at the break are not written.
- The first word of a line is written even when it is too wide, and runs past the width.
- A newline in a description always starts a new line at the same column. Consecutive newlines give empty lines.
- Runs of blanks inside a line, and blanks right after a newline, are kept as they are. Use blanks after a newline to indent a line.
- Blanks at the end of a line are not written.
- With the width 0, lines break only at the newlines in the description.

At the width 60, below, the description of the command has an empty line, the description of `--exclude` has newlines followed by blanks, and the description of `--url` has a word too wide for the width:

```
Usage: tool [OPTIONS] --output <FILE>

Pack the files that are named on the command line into one
archive, in the order in which they are named.

An archive that exists is replaced.

Options:
  -o, --output <FILE>      The archive to write. A file of
                           this name that exists is replaced
                           without a question. [required]
  -x, --exclude <PATTERN>  Leave out the files that match:
                             *.o    one kind of file
                             build/ a directory
  -u, --url                See
                           https://example.com/a/very/long/address/that/has/no/blank/in/it/at/all
                           for more.
```

## Descriptions

- **A description of only blanks (0x20) and newlines (0x0A) counts as not set.** `""`, `"   "` and `" \n"` all mean no description, for commands, options and positionals alike. In `Commands:`, only the first line of a subcommand's description counts: if it has only blanks, the item is the heading alone, even when more lines follow.
- **Trailing blanks of a description are dropped, trailing newlines are kept.** In the blanks and newlines after the last other byte of a description, the blanks are ignored: `"foo\n  "` gives the same text as `"foo\n"`, and `"foo\n \n"` the same as `"foo\n\n"`.
- **A newline, or a line of only blanks, at the start of a description is kept.** It is not removed from the description of a command (in its own help), an option or a positional. It can add empty lines and move where the description or the annotations start, so "one empty line between parts" in [Structure](#structure) may no longer hold. A command with the description `"\nPack files."` gives:

  ```
  Usage: tool [OPTIONS]


  Pack files.

  Options:
    -q, --quiet    Print nothing.
    -v, --verbose  Print more.
  ```

  The text for such descriptions may change in later versions.

## Limits

- **Widths are counted in bytes.** One byte is one column, so text with non-ASCII characters (2 bytes or more each in UTF-8) wraps before its visible width reaches the width, and a heading with such characters counts as wider than it looks, pushing the description column right. The library does not look up display widths.

  At the width 50, below, the description of `--quiet` is in Greek, 2 bytes per letter. It would fit visually, but not in bytes, so it wraps:

  ```
  Usage: tool [OPTIONS]

  Options:
    -q, --quiet    Μην εκτυπώνεις
                   τίποτα
    -v, --verbose  Print more than usual
  ```

- Tabs and control characters also count as one column per byte.
- **A newline in a name breaks the columns.** The name of the root command, of a positional or of a user-defined value type, and a value name, may contain newlines, and the help writes them as they are. Their width still counts every byte, newlines included, but in the output the rest starts at column 0. So the blanks before the description column and the wrapping points of the usage line no longer match what you see. Do not put newlines in names.

  A positional named `"first\nsecond"` has the heading `<first\nsecond>`, counted as 14 columns, so the description column is 18. In the output, `second>` starts at column 0, and the blanks before the description look too few:

  ```
  Usage: tool [OPTIONS] <first
  second>

  Arguments:
    <first
  second>           A name with a newline

  Options:
    -q, --quiet     Print nothing
  ```

- **Blanks in names, and `\r` anywhere, are written as they are.** Blanks at the end of a name, or right before a newline in a name, are written, so a line can end with a blank: the usage line of a root command named `"tool "` with nothing else defined is `Usage: tool `. A root command name that starts with a newline leaves the blank of `Usage: ` at the end of the first line (`"\ntool"` gives a first line `Usage: `). A `\r` in any string of the schema (names, descriptions, value names, default values, choices) is written unchanged; the library itself adds none.
- The fixed phrases are in English only. There is no way to translate them, add colors, or reorder the parts of the help.
- There is no way to hide an option or to group options.
- The help of a subcommand does not show the options of its parent.

