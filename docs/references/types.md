# Value Types

API reference of `include/argx/type.h`. A value type checks an input string and converts it to a value. Each option with a value and each positional has one.

## Functions

| Functions | Section |
| --- | --- |
| `argx_type_string`, `argx_type_int`, `argx_type_float`, `argx_type_create_choice`, `argx_type_destroy` | [Built-in and created value types](#built-in-and-created-value-types) |
| `argx_type_name`, `argx_type_kind`, `argx_type_value_size`, `argx_type_choice_count`, `argx_type_choice_at` | [Built-in and created value types](#built-in-and-created-value-types) |
| `argx_type_create` | [User-defined value types](#user-defined-value-types) |
| `argx_type_convert`, `argx_type_release` | [Converting a value](#converting-a-value) |

## Overview

A value type never changes after it is created, so it can be shared by options, positionals, schemas and threads.

| Kind | How to get it | Type name | C type of the value | Value |
| --- | --- | --- | --- | --- |
| `ARGX_TYPE_STRING` | `argx_type_string()` | `"string"` | `const char *` | The input string itself (not a copy) |
| `ARGX_TYPE_INT` | `argx_type_int()` | `"integer"` | `int64_t` | The integer |
| `ARGX_TYPE_FLOAT` | `argx_type_float()` | `"float"` | `double` | The number |
| `ARGX_TYPE_CHOICE` | `argx_type_create_choice()` | `"choice"` | `size_t` | The index of the matching choice |
| `ARGX_TYPE_CUSTOM` | `argx_type_create()` | Yours | Yours | Written by your conversion function |

### Accepted input

- **String:** anything, including `""`.
- **Integer:** decimal digits with an optional `+` or `-`, within a signed 64-bit integer. No white space.
- **Float:** finite decimal notation with an optional exponent (`1`, `.5`, `-1.5e-3`), with `.` as the decimal point in every locale. No white space, infinity, NaN or hexadecimal.
- **Choice:** an exact, case-sensitive match of one choice. `""` can be a choice.

### Built-in and created value types

- The string, integer and float types are static: they never fail and are never destroyed.
- Choice and user-defined types are created by you and destroyed with `argx_type_destroy`. Their strings are copied.
- A type that is not a choice has 0 choices.

### Converting a value

Validation and parsing convert values themselves; these functions are for your own code.

- The caller provides a buffer of at least `argx_type_value_size()` bytes, suitably aligned for any type.
- After a successful `argx_type_convert`, call `argx_type_release` on the value exactly once. After a failure, the buffer is undefined and nothing is released.
- The value may refer to the input, which must stay unchanged until the value is released.
- Values are `void *`; read them with the C type in the table above.

## User-defined value types

```
argx_error_t argx_type_create(const char *name, size_t value_size,
                              argx_convert_fn_t convert_fn,
                              argx_release_fn_t release_fn, void *context,
                              argx_type_t **out);
```

| Argument | Contents |
| --- | --- |
| `name` | A non-empty type name, copied |
| `value_size` | The size of the value in bytes, greater than 0 |
| `convert_fn` | `argx_error_t (*)(const char *text, void *value, void *context)` |
| `release_fn` | `void (*)(void *value, void *context)`, or `NULL` if values need no release |
| `context` | User data for both functions, or `NULL` |

An example is in the [Tutorial](../guides/tutorial.md#1-create-the-value-types).

### The conversion function

- Writes the value of `text` to `value` and returns `ARGX_ERROR_NONE`. The value may refer to `text`.
- Returns `ARGX_ERROR_INVALID_VALUE` for rejected input, `ARGX_ERROR_OUT_OF_MEMORY` for a failed allocation. Any other return is also a failure, passed on by `argx_type_convert` unchanged.
- On failure, leaves nothing to release.
- **Has no side effects** and always gives the same result for the same input: every validation converts each default value.

### The release function

Releases what a successful conversion wrote (not the buffer itself), exactly once per success. Validation also releases each default value it converts.

### User data (`context`)

`context` is borrowed: it must stay valid until the value type is destroyed, and must be safe to read concurrently if the type is used from several threads. Creating and destroying a type calls neither function.

## Borrowed value types and the order of destruction

`argx_command_add_value` and `argx_command_add_positional` keep only a pointer to their value type.

| Object | Owner | Must stay valid |
| --- | --- | --- |
| String, integer and float types | The library | Always (`argx_type_destroy` ignores them) |
| Choice and user-defined types | You | Until every schema using them is destroyed |
| `context` of a user-defined type | You | Until the type is destroyed |

Destroy in this order:

1. The results parsed with the schema ([Parsing](parse.md#lifetime-rules)).
2. Every schema that uses the value types (`argx_command_destroy`).
3. The value types (`argx_type_destroy`).
4. Their `context`.

- A wrong order is not detected.
- Destroying `NULL` does nothing, so the same order works after a failed creation.
- `argx_option_type` and `argx_positional_type` return the value type you passed, which outlives the schema.

## Errors when creating

On failure, the output is `NULL` and nothing is left allocated. Both functions also return `ARGX_ERROR_OUT_OF_MEMORY`.

| Function | Condition | Error code |
| --- | --- | --- |
| `argx_type_create_choice` | 0 choices, or a duplicate | `ARGX_ERROR_INVALID_DEFINITION` |
| `argx_type_create` | An empty type name | `ARGX_ERROR_INVALID_NAME` |

## Preconditions

The full list is under `Preconditions:` in `include/argx/type.h` ([programming errors](ownership.md#runtime-failures-and-programming-errors)).

- Pointer arguments are not `NULL`, except the argument of `argx_type_destroy`, `release_fn` and `context` of `argx_type_create`, and `choices` of `argx_type_create_choice` when `count` is 0.
- The `index` of `argx_type_choice_at` is less than the number of choices.
- A value type is not used after it is destroyed, and is not destroyed before its schemas.
