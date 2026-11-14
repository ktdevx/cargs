# Error Codes

A function that can fail at run time returns an `argx_error_t`. Only `ARGX_ERROR_NONE` means success; every other value is a failure. An error code tells only what kind of failure occurred. It carries no position and no subject: where the failure is comes from the function that failed, such as the output parameters of validation or the details of a parse error.

This document is the API reference of `include/argx/error.h`; the comments of the header describe each function in detail. Which failures are error codes and which are programming errors is in [Ownership and Lifetime](ownership.md#runtime-failures-and-programming-errors).

## Functions

| Functions | Section |
| --- | --- |
| `argx_error_message` | [Messages](#messages) |

## Error codes

| Error code | Message (`argx_error_message`) | Meaning | Returned by |
| --- | --- | --- | --- |
| `ARGX_ERROR_NONE` | `"success"` | Success | Every function that can fail |
| `ARGX_ERROR_OUT_OF_MEMORY` | `"out of memory"` | A memory allocation failed | Every function that allocates |
| `ARGX_ERROR_INVALID_NAME` | `"invalid name"` | A name does not follow the rules, is empty, or is missing | The add functions of [Schemas](schema.md#errors-when-adding-and-setting), `argx_type_create` ([Value Types](types.md#errors-when-creating)) |
| `ARGX_ERROR_NAME_CONFLICT` | `"name already in use"` | Names conflict within the same command | The add functions of [Schemas](schema.md#errors-when-adding-and-setting) |
| `ARGX_ERROR_INVALID_DEFINITION` | `"inconsistent definition"` | Definitions contradict each other | The add and set functions and validation of [Schemas](schema.md#definition-errors), `argx_type_create_choice` ([Value Types](types.md#errors-when-creating)) |
| `ARGX_ERROR_INVALID_DEFAULT` | `"default value rejected by its value type"` | In validation, the value type cannot convert a default value | `argx_command_validate` ([Schemas](schema.md#errors-found-by-validation)) |
| `ARGX_ERROR_INVALID_VALUE` | `"invalid value"` | In the conversion of a value type, the input is not valid | The conversion functions of value types and `argx_type_convert` ([Value Types](types.md#the-conversion-function)). Never returned by parsing |
| `ARGX_ERROR_INVALID_COMMAND_LINE` | `"invalid command line"` | The command line does not match the schema. The result holds the parse error | `argx_parse`, `argx_parse_wide` ([Parsing](parse.md#entry-points)) |
| `ARGX_ERROR_WRITE_FAILED` | `"write failed"` | The write function returned `false` | `argx_help_write`, `argx_help_write_usage` ([Help and Usage](help.md#on-failure)) |

- Values follow the order of the table, starting at 0. A published value never changes and is never reused for another meaning.
- Later versions add error codes at the end. When you branch on an `argx_error_t` with `switch`, write a `default` label.
- `ARGX_ERROR_LIST(X)` expands to one `X(name, message)` for each error code, in value order. You can use it to generate your own tables.

## Messages

`argx_error_message` returns the message of an error code: a short English phrase, as in the table above.

- Any value may be passed. The result is never `NULL` and never empty. For a value that is not an error code, it is `"unknown error code"`.
- It cannot fail at run time, so it returns the string directly. It allocates no memory and has no preconditions.
- The string is valid until the program ends. Do not free it and do not modify it.
- The message of an error code is only the kind of failure. A parse error has a message of its own, which tells what was wrong with the command line ([Parsing](parse.md#kinds-and-messages)).
