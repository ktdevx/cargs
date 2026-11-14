# Schemas

API reference of `include/argx/schema.h`. A schema is a tree of commands, each with options and positionals, that describes the command lines your program accepts.

## Functions

| Functions | Section |
| --- | --- |
| `argx_command_create`, `argx_command_destroy`, `argx_command_add_subcommand`, `argx_command_set_description` | [Commands](#commands) |
| `argx_command_add_flag`, `argx_command_add_negatable`, `argx_command_add_count`, `argx_command_add_value` | [Options](#options) |
| `argx_option_set_description`, `argx_option_set_value_name`, `argx_option_set_required`, `argx_option_set_default`, `argx_option_set_initial` | [Attributes](#attributes) |
| `argx_command_add_positional`, `argx_positional_set_description`, `argx_positional_set_default` | [Positionals](#positionals) |
| `argx_command_validate`, `argx_command_is_validated` | [Validation](#validation) |
| `argx_command_name`, `argx_command_description`, `argx_command_parent`, `argx_command_option_count`, `argx_command_option_at`, `argx_command_positional_count`, `argx_command_positional_at`, `argx_command_subcommand_count`, `argx_command_subcommand_at`, `argx_command_find_short`, `argx_command_find_long`, `argx_command_find_subcommand` | [Reading a schema](#reading-a-schema) |
| `argx_option_command`, `argx_option_kind`, `argx_option_short_name`, `argx_option_long_name`, `argx_option_negated_name`, `argx_option_type`, `argx_option_description`, `argx_option_value_name`, `argx_option_is_required`, `argx_option_default`, `argx_option_initial` | [Reading a schema](#reading-a-schema) |
| `argx_positional_command`, `argx_positional_name`, `argx_positional_arity`, `argx_positional_type`, `argx_positional_description`, `argx_positional_default` | [Reading a schema](#reading-a-schema) |

## Commands

| To do this | Function | Notes |
| --- | --- | --- |
| Create the root | `argx_command_create` | Any non-empty name |
| Add a subcommand | `argx_command_add_subcommand` | An ASCII letter or digit, then letters, digits, `-` or `_` |
| Set the description | `argx_command_set_description` | May be empty |
| Destroy the tree | `argx_command_destroy` | On the root only. `NULL` does nothing |

- Nesting has no depth limit.
- A command cannot have both positionals and subcommands.
- Only the first line of a description appears in the parent's `Commands:` list ([Help Text Layout](help-layout.md#structure)).

## Options

| Kind | Add function | Meaning |
| --- | --- | --- |
| `ARGX_OPTION_FLAG` | `argx_command_add_flag` | Whether it was given |
| `ARGX_OPTION_NEGATABLE` | `argx_command_add_negatable` | Switched with `--name` and `--no-name` |
| `ARGX_OPTION_COUNT` | `argx_command_add_count` | How many times it was given |
| `ARGX_OPTION_VALUE` | `argx_command_add_value` | Takes one value of a value type |

### Names

- An option has a short name (`'\0'` if none), a long name (`NULL` if none), or both.
- A short name is one ASCII letter or digit. A long name is two or more characters: an ASCII letter or digit, then letters, digits, `-` or `_`.
- A negatable flag must have a long name; its negated name `no-<long name>` takes part in conflict checks.

### Attributes

| Attribute | Set function | Applies to | When not set |
| --- | --- | --- | --- |
| Description | `argx_option_set_description` | All | `NULL` |
| Value name | `argx_option_set_value_name` | `ARGX_OPTION_VALUE` | `NULL` |
| Required | `argx_option_set_required` | `ARGX_OPTION_VALUE` | `false` |
| Default value | `argx_option_set_default` | `ARGX_OPTION_VALUE` | `NULL` |
| Initial state | `argx_option_set_initial` | `ARGX_OPTION_NEGATABLE` | `false` |

- Setting an attribute on a kind it does not apply to is an error, whatever the value.
- A default value is written as on the command line; validation checks that it converts.
- Required together with a default is a validation error.
- The description and value name only affect the help. Without a value name, the type name is shown. A newline in a value name breaks the help columns ([Limits](help-layout.md#limits)).

## Positionals

`argx_command_add_positional` takes a name, a value type and an arity. Positionals take arguments in the order they were added.

| Arity | Values |
| --- | --- |
| `ARGX_ARITY_REQUIRED` | Exactly one |
| `ARGX_ARITY_OPTIONAL` | Zero or one |
| `ARGX_ARITY_ZERO_OR_MORE` | Zero or more |
| `ARGX_ARITY_ONE_OR_MORE` | One or more |

- The name is any non-empty string, unique within the command. It appears only in the help ([Usage line](help-layout.md#usage-line)); a newline in it breaks the columns.
- The description can be set for any arity; a default value only for `ARGX_ARITY_OPTIONAL`.
- The order of arities is checked by validation, not when adding.

## Rules common to all definitions

- **Strings are copied.**
- **Setting again replaces.** No function clears a setting.
- **Empty is not unset.** `""` differs from `NULL`.
- **Names are compared byte by byte,** case-sensitively.
- **Conflicts are checked per command.** A parent and a child, or two subcommands, may reuse names.
- **The root owns the whole tree.** Handles are never destroyed on their own.
- **Nothing can be removed or copied.**

## Validation

```
argx_error_t argx_command_validate(argx_command_t *root,
                                   const argx_command_t **command,
                                   const argx_option_t **option,
                                   const argx_positional_t **positional);
bool argx_command_is_validated(const argx_command_t *command);
```

`argx_command_validate` checks the whole tree and returns the first error in a fixed [order](#order-of-the-checks).

### The output parameters

| Return value | `*command` | `*option` | `*positional` | Validation state |
| --- | --- | --- | --- | --- |
| `ARGX_ERROR_NONE` | `NULL` | `NULL` | `NULL` | Validated |
| `ARGX_ERROR_INVALID_DEFINITION`, `ARGX_ERROR_INVALID_DEFAULT` (option) | Its command | The option | `NULL` | Unchanged |
| `ARGX_ERROR_INVALID_DEFINITION`, `ARGX_ERROR_INVALID_DEFAULT` (positional) | Its command | `NULL` | The positional | Unchanged |
| `ARGX_ERROR_OUT_OF_MEMORY` | `NULL` | `NULL` | `NULL` | Unchanged |

- Each output may be `NULL`; a non-`NULL` one is always written. The handles live until the root is destroyed.
- Validation never changes the definition, so it can simply be repeated after a fix or after `ARGX_ERROR_OUT_OF_MEMORY`.

### The validation state

- A new root is not validated. A successful validation validates the tree; a failed one leaves the state unchanged.
- Any successful add or set anywhere in the tree, even of the same value, makes it not validated. A failed one does not.
- `argx_command_is_validated` gives the same answer for any command of the tree.
- Parsing requires a validated schema (a precondition, [Parsing](parse.md#preconditions)).

### What validation calls

For each default value, validation converts it into a temporary buffer and releases it on success. Conversion and release functions are therefore called on every validation, and no value is kept.

## Definition errors

Errors the add and set functions can see are returned at once; errors that need the whole tree are returned by validation. Codes and messages are in [Error Codes](errors.md).

A failed add or set leaves the schema and its validation state unchanged, writes a `NULL` handle, and leaks nothing. The schema stays usable, and the same name can be added again.

### Errors when adding and setting

Within a function, the first matching row is returned. Every function can also return `ARGX_ERROR_OUT_OF_MEMORY`, which is the only failure of setting a description.

**`argx_command_create`**

| Condition | Error code |
| --- | --- |
| The name is empty | `ARGX_ERROR_INVALID_NAME` |

**`argx_command_add_subcommand`**

| Condition | Error code |
| --- | --- |
| The name breaks the rules or is empty | `ARGX_ERROR_INVALID_NAME` |
| The command has positionals | `ARGX_ERROR_INVALID_DEFINITION` |
| The command has a subcommand with the same name | `ARGX_ERROR_NAME_CONFLICT` |

**`argx_command_add_flag`, `argx_command_add_negatable`, `argx_command_add_count`, `argx_command_add_value`**

| Condition | Error code |
| --- | --- |
| Neither a short nor a long name | `ARGX_ERROR_INVALID_NAME` |
| A negatable flag without a long name | `ARGX_ERROR_INVALID_NAME` |
| A name breaks the rules | `ARGX_ERROR_INVALID_NAME` |
| The short name is already used in the command | `ARGX_ERROR_NAME_CONFLICT` |
| The long name equals a long or negated name in the command | `ARGX_ERROR_NAME_CONFLICT` |
| The negated name equals a long or negated name in the command | `ARGX_ERROR_NAME_CONFLICT` |

**`argx_command_add_positional`**

| Condition | Error code |
| --- | --- |
| The name is empty | `ARGX_ERROR_INVALID_NAME` |
| The command has subcommands | `ARGX_ERROR_INVALID_DEFINITION` |
| The command has a positional with the same name | `ARGX_ERROR_NAME_CONFLICT` |

**Setting attributes**

| Function | Condition | Error code |
| --- | --- | --- |
| `argx_option_set_value_name`, `argx_option_set_required`, `argx_option_set_default` | Not an option with a value | `ARGX_ERROR_INVALID_DEFINITION` |
| `argx_option_set_initial` | Not a negatable flag | `ARGX_ERROR_INVALID_DEFINITION` |
| `argx_positional_set_default` | Arity is not `ARGX_ARITY_OPTIONAL` | `ARGX_ERROR_INVALID_DEFINITION` |

### Errors found by validation

| Condition | Error code | Reported element |
| --- | --- | --- |
| `ARGX_ARITY_REQUIRED` or `ARGX_ARITY_ONE_OR_MORE` after `ARGX_ARITY_OPTIONAL` or `ARGX_ARITY_ZERO_OR_MORE` | `ARGX_ERROR_INVALID_DEFINITION` | The later positional |
| Any positional after `ARGX_ARITY_ZERO_OR_MORE` or `ARGX_ARITY_ONE_OR_MORE` | `ARGX_ERROR_INVALID_DEFINITION` | The later positional |
| An option is required and has a default | `ARGX_ERROR_INVALID_DEFINITION` | The option |
| An option's default does not convert | `ARGX_ERROR_INVALID_DEFAULT` | The option |
| A positional's default does not convert | `ARGX_ERROR_INVALID_DEFAULT` | The positional |
| An allocation failed, or a conversion returned `ARGX_ERROR_OUT_OF_MEMORY` | `ARGX_ERROR_OUT_OF_MEMORY` | None |

Any other conversion failure becomes `ARGX_ERROR_INVALID_DEFAULT`.

### Order of the checks

Commands are checked depth first from the root, subcommands in the order added. Within a command:

1. The order of positional arities.
2. Each option in the order added: required with a default, then the default's conversion.
3. Each positional's default conversion, in the order added.

## Guarantees of a validated schema

Every schema satisfies these, since the add and set functions enforce them:

- Within a command, short names, long and negated names together, subcommand names and positional names are each unique.
- Every option has a short or long name; a negatable flag has a long and a negated name.
- Names follow the rules, and the root and positional names are not empty.
- No command has both positionals and subcommands.
- Exactly the options with a value and the positionals have a value type.
- Attributes are set only where they apply.

A validated schema also satisfies, for every command:

- The positionals are either zero or more `REQUIRED`, zero or more `OPTIONAL`, and at most one `ZERO_OR_MORE`, or zero or more `REQUIRED` and one `ONE_OR_MORE`.
- No required option has a default.
- Every default converted successfully at validation, so it will again if the conversion is deterministic.

## Reading a schema

The read functions take `const` handles, change nothing and never fail.

| Object | Functions |
| --- | --- |
| Command | `argx_command_name`, `argx_command_description`, `argx_command_parent`, `argx_command_is_validated` |
| Enumeration | `argx_command_option_count`/`_at`, `argx_command_positional_count`/`_at`, `argx_command_subcommand_count`/`_at` |
| Lookup | `argx_command_find_short`, `argx_command_find_long`, `argx_command_find_subcommand` |
| Option | `argx_option_command`, `argx_option_kind`, `argx_option_short_name`, `argx_option_long_name`, `argx_option_negated_name`, `argx_option_type`, `argx_option_description`, `argx_option_value_name`, `argx_option_is_required`, `argx_option_default`, `argx_option_initial` |
| Positional | `argx_positional_command`, `argx_positional_name`, `argx_positional_arity`, `argx_positional_type`, `argx_positional_description`, `argx_positional_default` |

- Enumeration follows the order added, from index 0.
- Unset strings and handles read as `NULL`, a missing short name as `'\0'`, and an attribute that does not apply to the kind as `NULL` or `false`.
- Lookups search only the given command, comparing whole names. `argx_command_find_long` matches the first `length` bytes of `name` (so `name=value` needs no copy), also matches negated names, and writes which matched to `*negated` when found. `argx_command_find_short('\0')` returns `NULL`.
- Returned strings and handles live until the root is destroyed, except that a description, value name or default lives only until that attribute is set again.
- Reads may run concurrently while the schema is not changed. Changing or validating must not overlap any other call on the schema.

## Preconditions

The full list is under `Preconditions:` in the header ([programming errors](ownership.md#runtime-failures-and-programming-errors)).

- Pointer arguments are not `NULL`, except the argument of a destroy function, the `out` of an add function, `long_name` (no long name), `negated` of `argx_command_find_long`, and the outputs of `argx_command_validate`.
- An `index` is less than the count, and `arity` is a defined constant.
- `argx_command_destroy` and `argx_command_validate` take the root.
- Handles are not used after destruction, and value types outlive their schemas ([Value Types](types.md#borrowed-value-types-and-the-order-of-destruction)).
