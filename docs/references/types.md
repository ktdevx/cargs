# Value Types

A value type describes how an input string is checked and what value it is converted to. Options with a value and positionals each have one value type. This document covers the built-in value types, choice value types, user-defined value types, converting values, and how long a value type must stay valid.

This document is the API reference of `include/argx/type.h`; the comments of the header describe each function in detail, and how a program uses them is in the [Guide](../guides/guide.md). Adding options and positionals with a value type is in [Schemas](schema.md), and the rules for memory are in [Ownership and Lifetime](ownership.md).

## Functions

| Functions | Section |
| --- | --- |
| `argx_type_string`, `argx_type_int`, `argx_type_float`, `argx_type_create_choice`, `argx_type_destroy` | [Built-in and created value types](#built-in-and-created-value-types) |
| `argx_type_name`, `argx_type_kind`, `argx_type_value_size`, `argx_type_choice_count`, `argx_type_choice_at` | [Built-in and created value types](#built-in-and-created-value-types) |
| `argx_type_create` | [User-defined value types](#user-defined-value-types) |
| `argx_type_convert`, `argx_type_release` | [Converting a value](#converting-a-value) |

## Overview

A value type describes how an input string is checked and what value it is converted to. An option with a value and a positional each have one value type. A value type describes only the conversion. It does not describe when the conversion is requested or where the value is kept.

A value type does not change after it is created. So one value type can be shared by several options and positionals, several schemas, and several threads.

| Kind | How to get it | Type name | C type of the value | Value |
| --- | --- | --- | --- | --- |
| `ARGX_TYPE_STRING` | `argx_type_string()` | `"string"` | `const char *` | A pointer to the input string (not a copy) |
| `ARGX_TYPE_INT` | `argx_type_int()` | `"integer"` | `int64_t` | The converted integer |
| `ARGX_TYPE_FLOAT` | `argx_type_float()` | `"float"` | `double` | The converted value |
| `ARGX_TYPE_CHOICE` | `argx_type_create_choice()` | `"choice"` | `size_t` | The index of the matching choice (from 0) |
| `ARGX_TYPE_CUSTOM` | `argx_type_create()` | Given by you | Decided by you | Written by your conversion function |

### Accepted input

- **String:** any input. An empty string is also accepted. The conversion always succeeds.
- **Integer:** a sequence of decimal digits. It may start with one sign (`+` or `-`). Only values that fit in a signed 64-bit integer are accepted. An empty string, white space and other characters are not accepted.
- **Float:** decimal notation. It may have an exponent (`1`, `1.5`, `.5`, `-1.5e-3`). Only finite values are accepted. The decimal point is `.`, whatever the locale. An empty string, white space, notations for infinity and NaN, and hexadecimal notation are not accepted.
- **Choice:** an input that exactly matches one of the choices in the list. The match is case-sensitive. An empty string can also be a choice.

### Built-in and created value types

- For the string, integer and float value types, the function returns the same pointer every time. They allocate nothing, so they do not fail, and there is nothing to destroy.
- You create choice value types and user-defined value types yourself, and you destroy them yourself with `argx_type_destroy`. The strings in the list of choices and the name of a user-defined value type are copied.
- A value type can be read with `argx_type_name`, `argx_type_kind`, `argx_type_value_size`, `argx_type_choice_count` and `argx_type_choice_at`. For a value type that is not a choice, the number of choices is 0.

### Converting a value

Validation and parsing request the conversions they need. The functions in this section are for code that converts values itself.

A conversion is requested with `argx_type_convert` and `argx_type_release`. They are called in the same way for every value type. You need to know the kind only when you read the value.

- The caller that requests a conversion provides the buffer the value is written to. The buffer is at least `argx_type_value_size()` bytes, with an alignment that is suitable for an object of any type. The library neither allocates nor frees this buffer.
- If the conversion succeeds, the value has been written to the buffer. When you are done with it, call `argx_type_release` on that value exactly once.
- If the conversion fails, the contents of the buffer are undefined. Do not call `argx_type_release`.
- The value may refer to the input string (the value of the string value type is the pointer to the input itself). The caller keeps the input string valid and unchanged until the value has been released.

Because values are passed as `void *`, the compiler does not detect a wrong buffer size or a wrong type. Read the value with the type given in the table above.

## User-defined value types

`argx_type_create` creates a user-defined value type from the type name, the size of the value, the conversion function, the release function and your user data. An example, the value type `size` of [examples/pack.c](../../examples/pack.c), is in the [Guide](../guides/guide.md#1-create-the-value-types).

```
argx_error_t argx_type_create(const char *name, size_t value_size,
                              argx_convert_fn_t convert_fn,
                              argx_release_fn_t release_fn, void *context,
                              argx_type_t **out);
```

| Argument | Contents |
| --- | --- |
| `name` | The type name (such as `"size"`). Any non-empty string. It is copied |
| `value_size` | The size of the value in bytes. Greater than 0. The buffer that the conversion writes to is at least this size |
| `convert_fn` | The conversion function. `argx_error_t (*)(const char *text, void *value, void *context)` |
| `release_fn` | The release function. `void (*)(void *value, void *context)`. `NULL` if the value needs no release |
| `context` | User data passed to both functions. `NULL` if not needed |

### The conversion function

- Convert `text`, write the value to the buffer that `value` points to, and return `ARGX_ERROR_NONE`. The buffer is at least `value_size` bytes, with an alignment that is suitable for any type.
- For an input it does not accept, return `ARGX_ERROR_INVALID_VALUE`. If an allocation inside the conversion fails, return `ARGX_ERROR_OUT_OF_MEMORY`.
- Any return value other than `ARGX_ERROR_NONE` is treated as a failure. `argx_type_convert` returns the return value of the conversion function as it is.
- When it fails, it leaves nothing that needs a release. The release function is not called for a failed conversion.
- The value may refer to `text`.
- **It has no side effects.** On every validation, the conversion function is called once for each default value that the validation reaches among the definition elements with that value type. If it prints, counts, or changes some outside state, that happens on every validation. Write it so that it always returns the same result for the same input.

### The release function

- It releases a value that a successful conversion of the same value type wrote (it frees the memory the value holds, for example). It does not free the buffer itself.
- It is called exactly once for each successful conversion. It is also called by validation: every time the conversion of a default value succeeds, validation requests its release and keeps no value.

### User data (`context`)

- A value type only borrows `context`. The library neither copies nor frees it.
- What `context` points to must stay valid until the value type is destroyed.
- If you use a value type from several threads at the same time, write the conversion function and the release function so that they work correctly when they read `context` at the same time.

Creating and destroying a value type calls neither the conversion function nor the release function.

## Borrowed value types and the order of destruction

A schema borrows its value types. `argx_command_add_value` and `argx_command_add_positional` do not copy the value type they are given. They only keep a pointer to it.

| Object | Owner | How long it must stay valid |
| --- | --- | --- |
| String, integer and float value types | The library | Valid until the program ends. Not destroyed (passing them to `argx_type_destroy` does nothing) |
| Choice value types, user-defined value types | The user who created them | Until every schema that uses them has been destroyed |
| The `context` of a user-defined value type | The user | Until the value type is destroyed |
| The schema (root command) | The user who created it | — |

Destroy them in this order:

1. If there are results (`argx_result_t`) parsed with the schema, destroy all of them. While a result remains, do not change or destroy the schema ([Parsing](parse.md#lifetime-rules)).
2. Destroy the schema (`argx_command_destroy`). The value types are not destroyed. If one value type is used by several schemas, destroy all of them.
3. Destroy the value types that the schema used (`argx_type_destroy`).
4. Clean up the `context` that the value types borrowed, in your own way.

- Do not destroy a value type before the schemas that use it. The library does not detect this error.
- `argx_type_destroy(NULL)` and `argx_command_destroy(NULL)` do nothing. So you can write the destruction in the same order even after a creation has failed.
- What `argx_option_type` and `argx_positional_type` return is the very value type that was passed when the element was added. It belongs to you, and it does not become invalid when the schema is destroyed.

## Errors when creating

When a creation fails, the output parameter is set to `NULL` and no memory that the call allocated is left behind. Besides the conditions below, both functions return `ARGX_ERROR_OUT_OF_MEMORY` when an allocation fails.

| Function | Condition | Error code |
| --- | --- | --- |
| `argx_type_create_choice` | There are 0 choices, or the same string appears twice or more | `ARGX_ERROR_INVALID_DEFINITION` |
| `argx_type_create` | The type name is empty | `ARGX_ERROR_INVALID_NAME` |

## Preconditions

Breaking a precondition is a programming error: no error code is returned ([Ownership and Lifetime](ownership.md#runtime-failures-and-programming-errors)). The preconditions of each function are in the comments of `include/argx/type.h`, under `Preconditions:`. What they have in common is:

- Pointer arguments are not `NULL`, unless the comment says they "may be `NULL`". The arguments that may be `NULL` are:
  - the argument of `argx_type_destroy`;
  - `release_fn` and `context` of `argx_type_create`;
  - `choices` of `argx_type_create_choice` when `count` is 0.
- An index (the `index` of `argx_type_choice_at`) is less than the number of choices.
- A value type has not been destroyed. It is not destroyed before the schemas that use it.
