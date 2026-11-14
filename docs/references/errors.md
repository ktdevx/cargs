# Error Codes

API reference of `include/argx/error.h`. A function that can fail at run time returns an `argx_error_t`; only `ARGX_ERROR_NONE` is success. An error code tells only the kind of failure, not where it happened. Which failures are error codes is in [Ownership and Lifetime](ownership.md#runtime-failures-and-programming-errors).

## Functions

| Functions | Section |
| --- | --- |
| `argx_error_message` | [Messages](#messages) |

## Error codes

| Error code | Message | Meaning | Returned by |
| --- | --- | --- | --- |
| `ARGX_ERROR_NONE` | `"success"` | Success | Every function that can fail |
| `ARGX_ERROR_OUT_OF_MEMORY` | `"out of memory"` | An allocation failed | Every function that allocates |
| `ARGX_ERROR_INVALID_NAME` | `"invalid name"` | A name is invalid, empty or missing | The add functions of [Schemas](schema.md#errors-when-adding-and-setting), `argx_type_create` ([Value Types](types.md#errors-when-creating)) |
| `ARGX_ERROR_NAME_CONFLICT` | `"name already in use"` | Names conflict within a command | The add functions of [Schemas](schema.md#errors-when-adding-and-setting) |
| `ARGX_ERROR_INVALID_DEFINITION` | `"inconsistent definition"` | Definitions contradict each other | The add and set functions and validation of [Schemas](schema.md#definition-errors), `argx_type_create_choice` ([Value Types](types.md#errors-when-creating)) |
| `ARGX_ERROR_INVALID_DEFAULT` | `"default value rejected by its value type"` | A default value cannot be converted | `argx_command_validate` ([Schemas](schema.md#errors-found-by-validation)) |
| `ARGX_ERROR_INVALID_VALUE` | `"invalid value"` | A conversion rejected its input | Conversion functions and `argx_type_convert` ([Value Types](types.md#the-conversion-function)). Never parsing |
| `ARGX_ERROR_INVALID_COMMAND_LINE` | `"invalid command line"` | The command line does not match; the result holds the parse error | `argx_parse`, `argx_parse_wide` ([Parsing](parse.md#entry-points)) |
| `ARGX_ERROR_WRITE_FAILED` | `"write failed"` | The write function returned `false` | `argx_help_write`, `argx_help_write_usage` ([Help and Usage](help.md#on-failure)) |

- Values follow the table from 0, never change and are never reused.
- New codes are added at the end, so give a `switch` a `default` label.
- `ARGX_ERROR_LIST(X)` expands to `X(name, message)` for each code in value order.

## Messages

`argx_error_message` returns the message in the table, or `"unknown error code"` for any other value. It never fails, allocates nothing, has no preconditions, and the string lives until the program ends. A parse error has its own, more specific message ([Parsing](parse.md#kinds-and-messages)).
