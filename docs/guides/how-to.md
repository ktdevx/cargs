# How-to

Short recipes for specific tasks. To learn ArgX from the start, read the [Tutorial](tutorial.md).

## Getting the help as a string

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

## Printing a parse error with the usage line from a fixed buffer

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

## Wide-character argv with `wmain`

On Windows, pass the argv of `wmain` to `argx_parse_wide` ([Wide-character argv](../references/parse.md#wide-character-argv)). Strings read from the result are UTF-8:

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
