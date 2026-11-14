# Schemas

This document explains how to define, with function calls, which command lines a program accepts. What you define is called a schema. A schema is made of commands, options, positionals, and value types, which describe how values are converted.

This document covers defining, validating, reading and destroying a schema. It does not cover parsing argv or generating help text. A schema knows nothing about argv. How to parse argv with a validated schema and read the values is in [Parsing](parse.md). How to generate help and the usage line from a validated schema is in [Help and Usage](help.md).

The detailed description and the preconditions of each function are in the comments of the public headers `include/clargs/schema.h` and `include/clargs/type.h`. The rules for who owns memory are in [Ownership and Lifetime](ownership.md).

Contents:

- [A complete example](#a-complete-example)
- [Commands](#commands)
- [Options](#options)
- [Positionals](#positionals)
- [Value types](#value-types)
- [Writing a user-defined value type](#writing-a-user-defined-value-type)
- [Borrowed value types and the order of destruction](#borrowed-value-types-and-the-order-of-destruction)
- [Definition errors](#definition-errors)
- [Validation and finding the cause of a failure](#validation-and-finding-the-cause-of-a-failure)
- [Guarantees of a validated schema](#guarantees-of-a-validated-schema)
- [Reading a schema](#reading-a-schema)
- [Preconditions](#preconditions)

## A complete example

The example defines, validates and destroys the command line of `pack`, a made-up program. It describes command lines of this form:

```
pack [-v]... [-q] [--color | --no-color] create -o FILE [-l LEVEL] [--ratio RATIO]
                                                [--format tar|zip] [--block-size SIZE] file...
pack [-v]... [-q] [--color | --no-color] list archive [pattern]
```

The example is one translation unit, and it includes only `<clargs/clargs.h>` and standard headers. You can copy it out and compile it as it is. If the validation succeeds, it prints `the schema of pack is valid` and exits with status 0.

```c
#include <clargs/clargs.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* The user data of the value type "size": the largest size it takes. */
typedef struct size_limit
{
    unsigned long max;
} size_limit_t;

/*
 * The conversion function of the value type "size". Takes decimal digits
 * with an optional unit, 'k' or 'm' ("512", "64k", "2m"), and writes the
 * number of bytes as an unsigned long.
 *
 * It returns CLARGS_ERROR_INVALID_VALUE for an input that it does not take,
 * and it has no side effect: the validation of a schema calls it for every
 * default value of this type.
 */
static clargs_error_t convert_size(const char *text, void *value, void *context)
{
    const size_limit_t *limit = context;
    const char *p = text;
    unsigned long bytes = 0;
    unsigned long unit = 1;

    if (*p < '0' || *p > '9')
    {
        return CLARGS_ERROR_INVALID_VALUE;
    }
    for (; *p >= '0' && *p <= '9'; ++p)
    {
        unsigned long digit = (unsigned long)(*p - '0');

        if (bytes > (limit->max - digit) / 10)
        {
            return CLARGS_ERROR_INVALID_VALUE;
        }
        bytes = bytes * 10 + digit;
    }
    if (*p == 'k')
    {
        unit = 1024UL;
        ++p;
    }
    else if (*p == 'm')
    {
        unit = 1024UL * 1024UL;
        ++p;
    }
    if (*p != '\0' || bytes > limit->max / unit)
    {
        return CLARGS_ERROR_INVALID_VALUE;
    }

    *(unsigned long *)value = bytes * unit;
    return CLARGS_ERROR_NONE;
}

/* Keeps the result of a call in *error and tells whether the call failed. */
static bool failed(clargs_error_t *error, clargs_error_t result)
{
    *error = result;
    return result != CLARGS_ERROR_NONE;
}

/* The options of the root: they stand in front of the subcommand. */
static clargs_error_t define_common_options(clargs_command_t *root)
{
    clargs_option_t *option;
    clargs_error_t error;

    /* A count: -v, -vv, --verbose. */
    if (failed(&error, clargs_command_add_count(root, 'v', "verbose", &option)) ||
        failed(&error, clargs_option_set_description(option, "Print more; can be repeated.")))
    {
        return error;
    }

    /* A flag: -q, --quiet. */
    if (failed(&error, clargs_command_add_flag(root, 'q', "quiet", &option)) ||
        failed(&error, clargs_option_set_description(option, "Print nothing.")))
    {
        return error;
    }

    /*
     * A negatable flag without a short name: --color and --no-color. It is
     * on when neither is given.
     */
    if (failed(&error, clargs_command_add_negatable(root, '\0', "color", &option)) ||
        failed(&error, clargs_option_set_description(option, "Color the output.")) ||
        failed(&error, clargs_option_set_initial(option, true)))
    {
        return error;
    }

    return CLARGS_ERROR_NONE;
}

/* The subcommand "create": options that take a value, and its positional. */
static clargs_error_t define_create(clargs_command_t *root, const clargs_type_t *format,
                                    const clargs_type_t *size)
{
    clargs_command_t *create;
    clargs_option_t *option;
    clargs_positional_t *positional;
    clargs_error_t error;

    if (failed(&error, clargs_command_add_subcommand(root, "create", &create)) ||
        failed(&error, clargs_command_set_description(create, "Create an archive.")))
    {
        return error;
    }

    /* A required option with the built-in value type string. */
    if (failed(&error, clargs_command_add_value(create, 'o', "output", clargs_type_string(),
                                                &option)) ||
        failed(&error, clargs_option_set_description(option, "The archive to write.")) ||
        failed(&error, clargs_option_set_value_name(option, "FILE")) ||
        failed(&error, clargs_option_set_required(option, true)))
    {
        return error;
    }

    /*
     * The built-in value type integer, with a default value. A default
     * value is a string, written as on a command line.
     */
    if (failed(&error, clargs_command_add_value(create, 'l', "level", clargs_type_int(),
                                                &option)) ||
        failed(&error, clargs_option_set_description(option, "The compression level.")) ||
        failed(&error, clargs_option_set_value_name(option, "LEVEL")) ||
        failed(&error, clargs_option_set_default(option, "6")))
    {
        return error;
    }

    /* The built-in value type float; only a long name. */
    if (failed(&error, clargs_command_add_value(create, '\0', "ratio", clargs_type_float(),
                                                &option)) ||
        failed(&error, clargs_option_set_description(option, "Stop below this ratio.")) ||
        failed(&error, clargs_option_set_value_name(option, "RATIO")) ||
        failed(&error, clargs_option_set_default(option, "0.5")))
    {
        return error;
    }

    /* A choice. */
    if (failed(&error, clargs_command_add_value(create, '\0', "format", format, &option)) ||
        failed(&error, clargs_option_set_description(option, "The format of the archive.")) ||
        failed(&error, clargs_option_set_default(option, "tar")))
    {
        return error;
    }

    /* A user-defined value type. */
    if (failed(&error, clargs_command_add_value(create, '\0', "block-size", size, &option)) ||
        failed(&error, clargs_option_set_description(option, "The size of a block.")) ||
        failed(&error, clargs_option_set_value_name(option, "SIZE")) ||
        failed(&error, clargs_option_set_default(option, "64k")))
    {
        return error;
    }

    /* One or more files. */
    if (failed(&error, clargs_command_add_positional(create, "file", clargs_type_string(),
                                                     CLARGS_ARITY_ONE_OR_MORE, &positional)) ||
        failed(&error, clargs_positional_set_description(positional, "The files to pack.")))
    {
        return error;
    }

    return CLARGS_ERROR_NONE;
}

/* The subcommand "list": a required positional, then an optional one. */
static clargs_error_t define_list(clargs_command_t *root)
{
    clargs_command_t *list;
    clargs_positional_t *positional;
    clargs_error_t error;

    if (failed(&error, clargs_command_add_subcommand(root, "list", &list)) ||
        failed(&error, clargs_command_set_description(list, "List the files of an archive.")))
    {
        return error;
    }

    if (failed(&error, clargs_command_add_positional(list, "archive", clargs_type_string(),
                                                     CLARGS_ARITY_REQUIRED, &positional)) ||
        failed(&error, clargs_positional_set_description(positional, "The archive to read.")))
    {
        return error;
    }

    /* Only an optional positional can have a default value. */
    if (failed(&error, clargs_command_add_positional(list, "pattern", clargs_type_string(),
                                                     CLARGS_ARITY_OPTIONAL, &positional)) ||
        failed(&error, clargs_positional_set_description(positional, "The files to list.")) ||
        failed(&error, clargs_positional_set_default(positional, "*")))
    {
        return error;
    }

    return CLARGS_ERROR_NONE;
}

/* Tells which definition the validation found a fault in. */
static void report_fault(clargs_error_t error, const clargs_command_t *command,
                         const clargs_option_t *option, const clargs_positional_t *positional)
{
    fprintf(stderr, "pack: the schema is not valid: %s\n", clargs_error_message(error));

    if (command != NULL)
    {
        fprintf(stderr, "  in the command \"%s\"\n", clargs_command_name(command));
    }
    if (option != NULL)
    {
        if (clargs_option_long_name(option) != NULL)
        {
            fprintf(stderr, "  at the option --%s\n", clargs_option_long_name(option));
        }
        else
        {
            fprintf(stderr, "  at the option -%c\n", clargs_option_short_name(option));
        }
        if (clargs_option_default(option) != NULL)
        {
            fprintf(stderr, "  whose default value is \"%s\"\n", clargs_option_default(option));
        }
    }
    if (positional != NULL)
    {
        fprintf(stderr, "  at the positional \"%s\"\n", clargs_positional_name(positional));
        if (clargs_positional_default(positional) != NULL)
        {
            fprintf(stderr, "  whose default value is \"%s\"\n",
                    clargs_positional_default(positional));
        }
    }
}

int main(void)
{
    static const char *const formats[] = { "tar", "zip" };
    size_limit_t limit = { 1024UL * 1024UL * 1024UL };

    clargs_type_t *format = NULL;
    clargs_type_t *size = NULL;
    clargs_command_t *root = NULL;
    const clargs_command_t *faulty_command;
    const clargs_option_t *faulty_option;
    const clargs_positional_t *faulty_positional;
    clargs_error_t error;
    int status = EXIT_FAILURE;

    /* The value types come first: the schema borrows them. */
    if (failed(&error, clargs_type_create_choice(formats, 2, &format)) ||
        failed(&error, clargs_type_create("size", sizeof(unsigned long), convert_size, NULL,
                                          &limit, &size)) ||
        failed(&error, clargs_command_create("pack", &root)) ||
        failed(&error, clargs_command_set_description(root, "Pack files into an archive.")) ||
        failed(&error, define_common_options(root)) ||
        failed(&error, define_create(root, format, size)) ||
        failed(&error, define_list(root)))
    {
        /*
         * An addition or a set failed. The schema is as it was before the
         * call that failed, and it can still be destroyed.
         */
        fprintf(stderr, "pack: cannot define the command line: %s\n",
                clargs_error_message(error));
    } else if (failed(&error, clargs_command_validate(root, &faulty_command, &faulty_option,
                                                      &faulty_positional)))
    {
        report_fault(error, faulty_command, faulty_option, faulty_positional);
    }
    else if (clargs_command_is_validated(root))
    {
        printf("the schema of %s is valid\n", clargs_command_name(root));
        status = EXIT_SUCCESS;
    }

    /*
     * The schema first, then the value types it borrowed. Destroying NULL
     * does nothing, so this is also right after a failure.
     */
    clargs_command_destroy(root);
    clargs_type_destroy(size);
    clargs_type_destroy(format);

    return status;
}
```

Key points of the example:

- It creates two value types (a choice and a user-defined one) and one root command. These three are also the only things it destroys. The subcommands, options and positionals are freed when the root is destroyed.
- The handles that the add functions write (`option`, `positional`, `create`) are used to set attributes. You do not destroy them.
- Every function returns a value other than `CLARGS_ERROR_NONE` when it fails. The example relies on `||` evaluating from left to right and stopping at the first failure, so that it never passes a `NULL` handle written by a failed addition to the next set function.
- Validation is called once, after the definition is complete. If it fails, the example passes the handles written to the three output parameters to the read functions to find which definition has the error (`report_fault`).
- The schema is destroyed first, and the value types after it.

## Commands

A schema is a tree of commands. The root command represents the program, and every command can have subcommands. There is no limit on the depth of nesting.

| To do this | Function | Notes |
|---|---|---|
| Create the root | `clargs_command_create` | The name is any non-empty string. The rules for subcommand names do not apply |
| Add a subcommand | `clargs_command_add_subcommand` | The name has one character or more. The first is an ASCII letter or digit; the rest are ASCII letters, digits, `-` or `_` |
| Set the description | `clargs_command_set_description` | An empty string can also be set. Setting it again replaces it |
| Destroy the whole tree | `clargs_command_destroy` | Call it only on the root. `NULL` does nothing |

- The root owns the whole tree. There are no functions that destroy a subcommand, an option or a positional on its own.
- Every string you pass is copied. Once the call returns, you may change or free the string.
- A command cannot have both positionals and subcommands. Options can be used with either.
- Names are compared byte by byte. Uppercase and lowercase letters make different names.
- Name conflicts are checked per command. A parent and a child command, or two different subcommands, can use the same names.
- There are no functions that remove a definition element, and none that copy a schema.
- The description of a command appears in the help of that command. Only its first line appears in the `Commands:` list of the parent command's help ([Help and Usage](help.md#structure)).

## Options

There are four kinds of options, and each kind has its own add function.

| Kind | Add function | Meaning |
|---|---|---|
| `CLARGS_OPTION_FLAG` | `clargs_command_add_flag` | Whether it was given |
| `CLARGS_OPTION_NEGATABLE` | `clargs_command_add_negatable` | Switched with `--name` and `--no-name` |
| `CLARGS_OPTION_COUNT` | `clargs_command_add_count` | How many times it was given |
| `CLARGS_OPTION_VALUE` | `clargs_command_add_value` | Takes one value. You specify a value type |

Names:

- An option has a short name, a long name, or both. A missing short name is written as `'\0'`, and a missing long name as `NULL`. This is the same in the arguments of the add functions and in the return values of the read functions.
- A short name is one ASCII letter or digit.
- A long name has two characters or more. The first is an ASCII letter or digit; the rest are ASCII letters, digits, `-` or `_`.
- A negatable flag always has a long name. Its negated name is `no-` followed by the long name, and it is made when the option is added (if the long name is `color`, it is `no-color`). The negated name also takes part in the name conflict checks within the same command.

All attributes are optional. After you add an option, you set each attribute with its own function.

| Attribute | Set function | Kinds it can be set on | Value when not set |
|---|---|---|---|
| Description | `clargs_option_set_description` | All | `NULL` |
| Value name | `clargs_option_set_value_name` | `CLARGS_OPTION_VALUE` | `NULL` |
| Required | `clargs_option_set_required` | `CLARGS_OPTION_VALUE` | `false` |
| Default value | `clargs_option_set_default` | `CLARGS_OPTION_VALUE` | `NULL` |
| Initial state | `clargs_option_set_initial` | `CLARGS_OPTION_NEGATABLE` | `false` (off) |

- String attributes are copied. An empty string can also be set, and it is different from not being set (`NULL`). Setting an attribute again replaces the previous value. There are no functions that clear a setting.
- A default value is passed as a string in the same form as it would be written on the command line. Setting it does not check whether the value type can convert it. Validation checks that.
- You can set both required and a default value, but an option that has both is an error in validation.
- The description and the value name do not affect parsing. They are used in the help text. The description becomes the description in the `Options:` list, and the value name appears in the heading and the usage line in the form `<FILE>`. If no value name is set, the name of the value type appears instead. Required, the default value, the initial state and the choices appear as annotations ([Help and Usage](help.md#text-layout-rules)).
- Do not put a newline in a value name. It breaks the columns of the help ([Help and Usage](help.md#limits)).

## Positionals

A positional is added with `clargs_command_add_positional`. You specify its name, its value type and its arity. The positionals of a command receive the command-line arguments in the order they were added.

| Arity | Meaning |
|---|---|
| `CLARGS_ARITY_REQUIRED` | Exactly one |
| `CLARGS_ARITY_OPTIONAL` | Zero or one |
| `CLARGS_ARITY_ZERO_OR_MORE` | Zero or more |
| `CLARGS_ARITY_ONE_OR_MORE` | One or more |

- The name is for display and identification, and it does not appear on the command line. Any non-empty string can be used. Within the same command, the names must differ from each other. The name appears in the usage line and in the `Arguments:` list in forms such as `<name>` and `[name]...` ([Help and Usage](help.md#usage-line)). A newline in the name breaks the columns of the help ([Help and Usage](help.md#limits)).
- There are two attributes. The description (`clargs_positional_set_description`) can be set for any arity. The default value (`clargs_positional_set_default`) can be set only on a positional with `CLARGS_ARITY_OPTIONAL`. Strings are handled in the same way as option attributes.
- Whether the sequence of arities is consistent (no required positional after an optional one, no positional after a variadic one) is not checked when a positional is added. Validation checks that.

## Value types

A value type describes how an input string is checked and what value it is converted to. An option with a value and a positional each have one value type. A value type describes only the conversion. It does not describe when the conversion is requested or where the value is kept.

A value type does not change after it is created. So one value type can be shared by several options and positionals, several schemas, and several threads.

The kinds of value types, and the type of the value that the conversion writes to the buffer:

| Kind | How to get it | Type name | C type of the value | Value |
|---|---|---|---|---|
| `CLARGS_TYPE_STRING` | `clargs_type_string()` | `"string"` | `const char *` | A pointer to the input string (not a copy) |
| `CLARGS_TYPE_INT` | `clargs_type_int()` | `"integer"` | `int64_t` | The converted integer |
| `CLARGS_TYPE_FLOAT` | `clargs_type_float()` | `"float"` | `double` | The converted value |
| `CLARGS_TYPE_CHOICE` | `clargs_type_create_choice()` | `"choice"` | `size_t` | The index of the matching choice (from 0) |
| `CLARGS_TYPE_CUSTOM` | `clargs_type_create()` | Given by you | Decided by you | Written by your conversion function |

The input that the built-in value types accept:

- String: any input. An empty string is also accepted. The conversion always succeeds.
- Integer: a sequence of decimal digits. It may start with one sign (`+` or `-`). Only values that fit in a signed 64-bit integer are accepted. An empty string, white space and other characters are not accepted.
- Float: decimal notation. It may have an exponent (`1`, `1.5`, `.5`, `-1.5e-3`). Only finite values are accepted. The decimal point is `.`, whatever the locale. An empty string, white space, notations for infinity and NaN, and hexadecimal notation are not accepted.
- Choice: an input that exactly matches one of the choices in the list. The match is case-sensitive. An empty string can also be a choice.

For the string, integer and float value types, the function returns the same pointer every time. They allocate nothing, so they do not fail, and there is nothing to destroy. You create choice value types and user-defined value types yourself, and you destroy them yourself with `clargs_type_destroy`. The strings in the list of choices and the name of a user-defined value type are copied.

A value type can be read with these functions: `clargs_type_name`, `clargs_type_kind`, `clargs_type_value_size`, `clargs_type_choice_count`, `clargs_type_choice_at`. For a value type that is not a choice, the number of choices is 0.

A conversion is requested with `clargs_type_convert` and `clargs_type_release`. They are called in the same way for every value type. You need to know the kind only when you read the value. If you only define a schema, you do not need to call these two functions (validation calls them to check the default values).

- The caller that requests a conversion provides the buffer the value is written to. The buffer is at least `clargs_type_value_size()` bytes, with an alignment that is suitable for an object of any type. The library neither allocates nor frees this buffer.
- If the conversion succeeds, the value has been written to the buffer. When you are done with it, call `clargs_type_release` on that value exactly once.
- If the conversion fails, the contents of the buffer are undefined. Do not call `clargs_type_release`.
- The value may refer to the input string (the value of the string value type is the pointer to the input itself). The caller that requested the conversion keeps the input string valid and unchanged until the value has been released.

Because values are passed as `void *`, the compiler does not detect a wrong buffer size or a wrong type. Read the value with the type given in the table above.

## Writing a user-defined value type

Pass the type name, the size of the value, the conversion function, the release function and your user data to `clargs_type_create`.

```
clargs_error_t clargs_type_create(const char *name, size_t value_size,
                                  clargs_convert_fn_t convert_fn,
                                  clargs_release_fn_t release_fn, void *context,
                                  clargs_type_t **out);
```

| Argument | Contents |
|---|---|
| `name` | The type name (such as `"size"`). Any non-empty string. It is copied |
| `value_size` | The size of the value in bytes. Greater than 0. The buffer that the conversion writes to is at least this size |
| `convert_fn` | The conversion function. `clargs_error_t (*)(const char *text, void *value, void *context)` |
| `release_fn` | The release function. `void (*)(void *value, void *context)`. `NULL` if the value needs no release |
| `context` | User data passed to both functions. `NULL` if not needed |

What the conversion function must do:

- Convert `text`, write the value to the buffer that `value` points to, and return `CLARGS_ERROR_NONE`. The buffer is at least `value_size` bytes, with an alignment that is suitable for any type.
- For an input it does not accept, return `CLARGS_ERROR_INVALID_VALUE`. If an allocation inside the conversion fails, return `CLARGS_ERROR_OUT_OF_MEMORY`.
- Any return value other than `CLARGS_ERROR_NONE` is treated as a failure. `clargs_type_convert` returns the return value of the conversion function as it is.
- When it fails, it leaves nothing that needs a release. The release function is not called for a failed conversion.
- **It has no side effects.** On every validation, the conversion function is called once for each default value that the validation reaches among the definition elements with that value type. If it prints, counts, or changes some outside state, that happens on every validation. Write it so that it always returns the same result for the same input.
- The value may refer to `text`.

What the release function must do:

- Release a value that a successful conversion of the same value type wrote (free the memory the value holds, for example). It does not free the buffer itself.
- It is called exactly once for each successful conversion. It is also called by validation: every time the conversion of a default value succeeds, validation requests its release and keeps no value.

User data (`context`):

- A value type only borrows `context`. The library neither copies nor frees it.
- You keep what `context` points to valid until you destroy the value type. In the example above, `limit` is a local variable of `main`, and it stays valid until after the value type is destroyed.
- If you use a value type from several threads at the same time, write the conversion function and the release function so that they work correctly when they read `context` at the same time.

Creating and destroying a value type calls neither the conversion function nor the release function.

## Borrowed value types and the order of destruction

A schema borrows its value types. `clargs_command_add_value` and `clargs_command_add_positional` do not copy the value type they are given. They only keep a pointer to it.

| Object | Owner | How long it must stay valid |
|---|---|---|
| String, integer and float value types | The library | Valid until the program ends. Not destroyed (passing them to `clargs_type_destroy` does nothing) |
| Choice value types, user-defined value types | The user who created them | Until every schema that uses them has been destroyed |
| The `context` of a user-defined value type | The user | Until the value type is destroyed |
| The schema (root command) | The user who created it | — |

The order of destruction:

1. Destroy the schema (`clargs_command_destroy`). The value types are not destroyed.
2. Destroy the value types that the schema used (`clargs_type_destroy`).
3. Clean up the `context` that the value types borrowed, in your own way.

- Do not destroy a value type before the schemas that use it. The library does not detect this error.
- If there are results (`clargs_result_t`) parsed with the schema, destroy all of them before the schema. While a result remains, do not change or destroy the schema ([Parsing](parse.md#lifetime-rules)).
- If one value type is used by several schemas, destroy all of them before you destroy the value type.
- `clargs_type_destroy(NULL)` and `clargs_command_destroy(NULL)` do nothing. So you can write the destruction in the same order even after a creation has failed.
- What `clargs_option_type` and `clargs_positional_type` return is the very value type that was passed when the element was added. It belongs to you, and it does not become invalid when the schema is destroyed.

## Definition errors

Errors in a definition are returned as error codes. Errors that the add and set functions can check are returned right away. Errors that can only be found by looking at the whole tree are returned by validation.

| Error code | Message (`clargs_error_message`) | Meaning |
|---|---|---|
| `CLARGS_ERROR_OUT_OF_MEMORY` | `"out of memory"` | A memory allocation failed |
| `CLARGS_ERROR_INVALID_NAME` | `"invalid name"` | A name does not follow the rules, is empty, or is missing |
| `CLARGS_ERROR_NAME_CONFLICT` | `"name already in use"` | Names conflict within the same command |
| `CLARGS_ERROR_INVALID_DEFINITION` | `"inconsistent definition"` | Definitions contradict each other |
| `CLARGS_ERROR_INVALID_DEFAULT` | `"default value rejected by its value type"` | In validation, the value type cannot convert a default value |
| `CLARGS_ERROR_INVALID_VALUE` | `"invalid value"` | In the conversion of a value type, the input is not valid |

If an add or set function fails, the schema has the same contents as before the call, and its validation state does not change. The output parameter that receives the created object is set to `NULL`. No memory that the call allocated is left behind. After the failure, you can keep using the schema, and you can add an element again with the same name.

### Errors when adding and setting

Within one function, the conditions are checked from the top row down, and the first one that applies is returned. Besides these, every function returns `CLARGS_ERROR_OUT_OF_MEMORY` when an allocation fails.

| Function | Condition | Error code |
|---|---|---|
| `clargs_command_create` | The name is empty | `INVALID_NAME` |
| `clargs_command_add_subcommand` | The name does not follow the rules (including an empty name) | `INVALID_NAME` |
| | The command it is added to has positionals | `INVALID_DEFINITION` |
| | The command it is added to already has a subcommand with the same name | `NAME_CONFLICT` |
| `clargs_command_add_flag`, `_add_negatable`, `_add_count`, `_add_value` | There is neither a short name nor a long name | `INVALID_NAME` |
| | A negatable flag has no long name | `INVALID_NAME` |
| | The short name or the long name does not follow the rules | `INVALID_NAME` |
| | The same command already has an option with the same short name | `NAME_CONFLICT` |
| | The same command already has a long name or a negated name equal to the long name | `NAME_CONFLICT` |
| | For a negatable flag, the same command already has a long name or a negated name equal to its negated name | `NAME_CONFLICT` |
| `clargs_command_add_positional` | The name is empty | `INVALID_NAME` |
| | The command it is added to has subcommands | `INVALID_DEFINITION` |
| | The command it is added to already has a positional with the same name | `NAME_CONFLICT` |
| `clargs_option_set_value_name`, `_set_required`, `_set_default` | The option is not an option with a value | `INVALID_DEFINITION` |
| `clargs_option_set_initial` | The option is not a negatable flag | `INVALID_DEFINITION` |
| `clargs_positional_set_default` | The arity is not `CLARGS_ARITY_OPTIONAL` | `INVALID_DEFINITION` |
| `clargs_type_create_choice` | There are 0 choices, or the same string appears twice or more | `INVALID_DEFINITION` |
| `clargs_type_create` | The type name is empty | `INVALID_NAME` |

- Setting an attribute on a kind it does not apply to is an error, whatever value you try to set (even `false`).
- Setting a description (on a command, an option or a positional) fails only when an allocation fails.

### Errors found by validation

These are returned by `clargs_command_validate`. The order of the checks is in [Validation and finding the cause of a failure](#validation-and-finding-the-cause-of-a-failure).

| Condition | Error code | Reported definition element |
|---|---|---|
| In the sequence of positionals, `REQUIRED` or `ONE_OR_MORE` comes after `OPTIONAL` or `ZERO_OR_MORE` | `INVALID_DEFINITION` | The later positional |
| In the sequence of positionals, a positional comes after `ZERO_OR_MORE` or `ONE_OR_MORE` | `INVALID_DEFINITION` | The later positional |
| An option is required and also has a default value | `INVALID_DEFINITION` | That option |
| The value type of an option cannot convert its default value | `INVALID_DEFAULT` | That option |
| The value type of a positional cannot convert its default value | `INVALID_DEFAULT` | That positional |
| The allocation of the value buffer failed, or the conversion returned `CLARGS_ERROR_OUT_OF_MEMORY` | `OUT_OF_MEMORY` | None |

- If the conversion of a default value fails with anything other than `CLARGS_ERROR_OUT_OF_MEMORY`, the result is `CLARGS_ERROR_INVALID_DEFAULT`, whatever code the conversion function returned.
- Validation does not check the errors that are returned when adding and setting (invalid names, name conflicts, positionals together with subcommands, attributes on kinds they do not apply to). This is because the add and set functions never let you build such a schema.

### What is not an error code

A `NULL` for a required argument, an index out of range, a value that is not in an enumeration, and the use of a destroyed handle are programming errors. No error code is returned ([Preconditions](#preconditions)).

## Validation and finding the cause of a failure

```
clargs_error_t clargs_command_validate(clargs_command_t *root,
                                       const clargs_command_t **command,
                                       const clargs_option_t **option,
                                       const clargs_positional_t **positional);
bool clargs_command_is_validated(const clargs_command_t *command);
```

`clargs_command_validate` checks the root and every subcommand below it. You pass the root. When it succeeds, the tree becomes validated.

The order of the checks is fixed. Validation returns the first error it finds and stops, so the same definition always gives the same result.

- Commands are visited starting from the root. A command itself is checked first, then its subcommands in the order they were added. For each subcommand, everything below it is checked before moving on to the next subcommand.
- Within one command, the order is:
  1. The sequence of positionals, in the order they were added.
  2. The options, in the order they were added. For each option, the combination of required and a default value is checked first, then the conversion of the default value.
  3. The conversion of the default values of the positionals, in the order they were added.

The result and the three output parameters:

| Return value | `*command` | `*option` | `*positional` | Validation state |
|---|---|---|---|---|
| `CLARGS_ERROR_NONE` | `NULL` | `NULL` | `NULL` | Becomes validated |
| `CLARGS_ERROR_INVALID_DEFINITION`, `CLARGS_ERROR_INVALID_DEFAULT` (error in an option) | The command the option belongs to | The option with the error | `NULL` | Unchanged |
| `CLARGS_ERROR_INVALID_DEFINITION`, `CLARGS_ERROR_INVALID_DEFAULT` (error in a positional) | The command the positional belongs to | `NULL` | The positional with the error | Unchanged |
| `CLARGS_ERROR_OUT_OF_MEMORY` | `NULL` | `NULL` | `NULL` | Unchanged |

- Each of the three output parameters can be omitted by passing `NULL`. An output parameter that is not `NULL` is always written on every call.
- The handles written are ones in the tree. They stay valid until the root is destroyed, and you do not free them.
- Validation does not change the definition. Whatever it returns, the commands, options, positionals and their attributes are as they were before the call.

How to find the cause of a failure (`report_fault` in the example):

1. Pass the return value to `clargs_error_message` to get the kind of error.
2. Pass `*command` to `clargs_command_name` to get which command's definition it is. To walk up to the parents, use `clargs_command_parent` (`NULL` for the root).
3. If `*option` is not `NULL`, use `clargs_option_long_name` (`NULL` if none), `clargs_option_short_name` (`'\0'` if none), `clargs_option_default`, `clargs_option_is_required` and so on to find which option has which error.
4. If `*positional` is not `NULL`, use `clargs_positional_name`, `clargs_positional_arity`, `clargs_positional_default` and so on.
5. If all three are `NULL` and the return value is `CLARGS_ERROR_OUT_OF_MEMORY`, it is not an error in the definition. The definition is unchanged, so you can validate again once enough memory is available.

To fix an error, set the attribute again (replace the default value with a correct one, set required to `false`, and so on). There are no functions that remove a definition element, so for an error in the sequence of positionals, destroy the schema and build it again.

The validation state:

- A root right after it is created is not validated.
- When validation succeeds, the tree becomes validated. A failed validation does not change the state.
- A successful addition or set, on any part of the tree, marks the tree as not validated again. This happens even when the same value is set again. A failed addition or set does not change the state.
- You can pass any command of the tree to `clargs_command_is_validated`. The result is the same.
- You can call validation as many times as you like. Call it again after you change the definition.
- Only a validated schema can be passed to parsing (`clargs_parse`, `clargs_parse_wide`). Passing a schema that is not validated (one for which `clargs_command_is_validated` returns `false`: one that has never been validated successfully, or one that was changed by an addition or a set after validation) to parsing violates a precondition. No error code is returned. It is checked with `assert`: in the Debug configuration the process aborts, and in the Release configuration the behavior is undefined ([Preconditions](#preconditions), [Parsing](parse.md#preconditions)).

For each default value it reaches, validation allocates a value buffer, asks the value type to convert the value, requests its release if the conversion succeeds, and frees the buffer. So the conversion function and the release function of a user-defined value type are called on every validation ([Writing a user-defined value type](#writing-a-user-defined-value-type)). No value is left after validation.

## Guarantees of a validated schema

What always holds for any schema (the add and set functions ensure it):

- Within one command, short names are all different. Long names and negated names, taken together, are all different. Subcommand names are all different. Positional names are all different.
- An option has at least one of a short name and a long name. A negatable flag has a long name and a negated name.
- Short names, long names and subcommand names follow the naming rules. The name of the root and the names of positionals are not empty.
- A command never has both positionals and subcommands.
- Options with a value and positionals have a value type, and other options do not.
- Attributes are set only on definition elements of the kinds they apply to.

In a validated schema (a tree for which `clargs_command_is_validated` returns `true`), the following also holds for every command:

- The sequence of positionals is one of these:
  - Zero or more `CLARGS_ARITY_REQUIRED`, then zero or more `CLARGS_ARITY_OPTIONAL`, and last at most one `CLARGS_ARITY_ZERO_OR_MORE`.
  - Zero or more `CLARGS_ARITY_REQUIRED`, and last exactly one `CLARGS_ARITY_ONE_OR_MORE`.
- A required option with a value has no default value.
- Every default value was converted successfully by its value type at the time of validation. If the conversion function has no side effects and returns the same result for the same input, converting it later also succeeds.

Code that reads a schema (parsing and help generation) can rely on these for a validated schema. For a schema that is not validated, it cannot. This is why both parsing and help generation have a validated schema as a precondition ([Validation and finding the cause of a failure](#validation-and-finding-the-cause-of-a-failure)).

## Reading a schema

The read functions take `const` handles, change nothing, and do not fail.

| Object | Functions |
|---|---|
| Command | `clargs_command_name`, `clargs_command_description`, `clargs_command_parent`, `clargs_command_is_validated` |
| Enumerating within a command | `clargs_command_option_count` and `clargs_command_option_at`, `clargs_command_positional_count` and `clargs_command_positional_at`, `clargs_command_subcommand_count` and `clargs_command_subcommand_at` |
| Lookup by name | `clargs_command_find_short`, `clargs_command_find_long`, `clargs_command_find_subcommand` |
| Option | `clargs_option_command`, `clargs_option_kind`, `clargs_option_short_name`, `clargs_option_long_name`, `clargs_option_negated_name`, `clargs_option_type`, `clargs_option_description`, `clargs_option_value_name`, `clargs_option_is_required`, `clargs_option_default`, `clargs_option_initial` |
| Positional | `clargs_positional_command`, `clargs_positional_name`, `clargs_positional_arity`, `clargs_positional_type`, `clargs_positional_description`, `clargs_positional_default` |

- Enumeration is in the order the elements were added. Indexes start at 0.
- "None" is represented by `NULL` for strings and handles, and by `'\0'` for short names. A description, value name or default value that is not set is `NULL`. An attribute that was set to an empty string reads as an empty string.
- For an option of a kind that an attribute does not apply to, the negated name, the value type, the value name and the default value are `NULL`, and required and the initial state are `false`.
- A lookup covers only the definitions of the command you pass. It does not look at parent or child commands. It compares the whole name, byte by byte.
- `clargs_command_find_long` looks up the first `length` bytes of `name` as the name. This lets you look up a name inside a string such as `name=value` without copying it. It also matches negated names, and it writes which one matched to `*negated` (it writes nothing if no option is found).
- Passing `'\0'` to `clargs_command_find_short` always returns `NULL`.
- The strings and handles returned stay valid until the root is destroyed. You do not free them. Adding definitions does not change the handles.
- However, a string read from an attribute that can be set again (the description of a command; the description, value name or default value of an option; the description or default value of a positional) stays valid until the root is destroyed or that attribute is set again, whichever comes first. Setting the attribute again frees the previous string. Names are never replaced.
- While a schema is not being changed, its read functions can be called from several threads at the same time. Changing or validating a schema cannot be done at the same time as any other call on it. You do the synchronization.

## Preconditions

The preconditions of each function are written in the comments of the public headers, under `Preconditions:`. What they have in common is:

- Pointer arguments are not `NULL`, unless the comment says they "may be `NULL`". The arguments that may be `NULL` are: the argument of a destroy function, the `out` of an add function, `long_name` to mean that there is no long name, `release_fn` and `context` of `clargs_type_create`, `choices` of `clargs_type_create_choice` when `count` is 0, `negated` of `clargs_command_find_long`, and the three output parameters of `clargs_command_validate`.
- An index (the `index` of an `_at` function) is less than the corresponding count.
- An enumeration argument (the `arity` of `clargs_command_add_positional`) is one of the defined constants.
- What you pass to `clargs_command_destroy` and `clargs_command_validate` is the root. Do not pass a subcommand.
- Handles have not been destroyed. A value type has not been destroyed before the schemas that use it.

Breaking a precondition is a programming error. No error code is returned. Preconditions that can be checked are checked with `assert`, and in the Debug configuration the process aborts when one is broken. In the Release configuration they are not checked, and the behavior is undefined. During development, we recommend that you use the library built in the Debug configuration ([Ownership and Lifetime](ownership.md#runtime-failures-and-programming-errors)).
