# How-to

Short recipes for specific tasks. To learn ArgX from the start, read the [Tutorial](../getting-started/tutorial.md). For why ArgX behaves as it does, see the [FAQ](faq.md).

## Command lines

### Requiring a subcommand

A command line that names no subcommand is not a parse error. Check the last selected command after parsing, as `run` in [examples/pack.c](../../examples/pack.c) does:

```c
const argx_command_t *command = argx_result_command(result);

if ((command != schema->create) && (command != schema->list))
{
    fprintf(stderr, "pack: a command is required\n");
    return EXIT_FAILURE;
}
```

### Passing a value that starts with `-`

Write it after `--`: `pack create -o out.tar -- -notes.txt`. A negative number such as `-1` is a value even without `--`, unless the command has a digit as a short option name ([How elements are classified](../references/syntax.md#how-elements-are-classified)).

### Wide-character argv with `wmain`

To receive non-ASCII arguments on Windows, pass the argv of `wmain` to `argx_parse_wide` ([Wide-character argv](../references/parse.md#wide-character-argv)). Strings read from the result are UTF-8:

```c
int wmain(int argc, wchar_t **argv)
{
    ...
    error = argx_parse_wide(root, argc, argv, &result);
    ...
    if (argx_result_positional_string(result, name, 0, &text))
    {
        printf("hello, %s\n", text);
    }
    ...
}
```

To display them, set the console output code page yourself.

## Values

### Reading the value of a user-defined value type

Read the pointer from `argx_result_option_value` or `argx_result_positional_value` as the C type your conversion function wrote ([Reading the result](../references/parse.md#reading-the-result)). For the `size` type of [examples/pack.c](../../examples/pack.c), which writes an `unsigned long`:

```c
const unsigned long *block_size = argx_result_option_value(result, schema->block_size);

if (block_size != NULL)
{
    printf("block size: %lu\n", *block_size);
}
```

## Help

### Fitting the help to the terminal

ArgX never reads the terminal. Get its width in your program and set it on the help options ([Help options](../references/help.md#help-options)):

```c
argx_help_options_t *options = NULL;

if (argx_help_options_create(&options) == ARGX_ERROR_NONE)
{
    argx_help_options_set_width(options, terminal_width);
    argx_help_write(command, options, write_to_stream, stdout);
    argx_help_options_destroy(options);
}
```

### Getting the help as a string

Call [`argx_help_format`](../references/help.md#formatting-into-a-buffer) once for the length and once for the text. Unlike `argx_help_write`, it never leaves partial output.

```c
/* Returns the help as a string to free(), or NULL. */
static char *help_string(const argx_command_t *command)
{
    char *text = NULL;
    size_t length = 0;

    if (argx_help_format(command, NULL, NULL, 0, &length) != ARGX_ERROR_NONE)
    {
        return NULL;
    }

    text = malloc(length + 1);
    if (text == NULL)
    {
        return NULL;
    }

    if (argx_help_format(command, NULL, text, length + 1, NULL) != ARGX_ERROR_NONE)
    {
        free(text);
        return NULL;
    }
    return text;
}
```

### Printing a parse error with the usage line from a fixed buffer

`argx_help_format_usage` cannot fail; it cuts the line when it does not fit.

```c
static void print_error(const argx_command_t *command, const char *message)
{
    char usage[256];
    size_t length = 0;

    argx_help_format_usage(command, NULL, usage, sizeof(usage), &length);

    if (length >= sizeof(usage))
    {
        fprintf(stderr, "pack: %s\n", message);
    }
    else
    {
        fprintf(stderr, "pack: %s\n%s", message, usage);
    }
}
```
