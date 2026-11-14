# Schemas

A schema describes, with function calls, which command lines your program accepts. It is a tree of commands, and each command has options and positionals. Options with a value and positionals each have a value type, which says how their input strings are converted. This document covers defining, validating, reading and destroying a schema.

A schema knows nothing about argv. This document is the API reference of `include/argx/schema.h`; the comments of the header describe each function in detail, and how a program uses them is in the [Guide](../guides/guide.md). Value types are in [Value Types](types.md), parsing in [Parsing](parse.md), the help in [Help and Usage](help.md), and the rules for memory in [Ownership and Lifetime](ownership.md).

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

A schema is a tree of commands. The root command represents the program, and every command can have subcommands. There is no limit on the depth of nesting.

| To do this | Function | Notes |
| --- | --- | --- |
| Create the root | `argx_command_create` | The name is any non-empty string. The rules for subcommand names do not apply |
| Add a subcommand | `argx_command_add_subcommand` | The name has one character or more. The first is an ASCII letter or digit; the rest are ASCII letters, digits, `-` or `_` |
| Set the description | `argx_command_set_description` | An empty string can also be set |
| Destroy the whole tree | `argx_command_destroy` | Call it only on the root. `NULL` does nothing |

- A command cannot have both positionals and subcommands. Options can be used with either.
- The description of a command appears in the help of that command. Only its first line appears in the `Commands:` list of the parent command's help ([Help Text Layout](help-layout.md#structure)).

## Options

There are four kinds of options, and each kind has its own add function.

| Kind | Add function | Meaning |
| --- | --- | --- |
| `ARGX_OPTION_FLAG` | `argx_command_add_flag` | Whether it was given |
| `ARGX_OPTION_NEGATABLE` | `argx_command_add_negatable` | Switched with `--name` and `--no-name` |
| `ARGX_OPTION_COUNT` | `argx_command_add_count` | How many times it was given |
| `ARGX_OPTION_VALUE` | `argx_command_add_value` | Takes one value. You specify a value type |

### Names

- An option has a short name, a long name, or both. A missing short name is written as `'\0'`, and a missing long name as `NULL`. This is the same in the arguments of the add functions and in the return values of the read functions.
- A short name is one ASCII letter or digit.
- A long name has two characters or more. The first is an ASCII letter or digit; the rest are ASCII letters, digits, `-` or `_`.
- A negatable flag always has a long name. Its negated name is `no-` followed by the long name, and it is made when the option is added (if the long name is `color`, it is `no-color`). The negated name also takes part in the name conflict checks within the same command.

### Attributes

All attributes are optional. After you add an option, you set each attribute with its own function.

| Attribute | Set function | Kinds it can be set on | Value when not set |
| --- | --- | --- | --- |
| Description | `argx_option_set_description` | All | `NULL` |
| Value name | `argx_option_set_value_name` | `ARGX_OPTION_VALUE` | `NULL` |
| Required | `argx_option_set_required` | `ARGX_OPTION_VALUE` | `false` |
| Default value | `argx_option_set_default` | `ARGX_OPTION_VALUE` | `NULL` |
| Initial state | `argx_option_set_initial` | `ARGX_OPTION_NEGATABLE` | `false` (off) |

- Setting an attribute on a kind it does not apply to is an error, whatever value you try to set (even `false`).
- A default value is a string in the same form as it would be written on the command line. Setting it does not check whether the value type can convert it. Validation checks that.
- You can set both required and a default value, but an option that has both is an error in validation.
- The description and the value name do not affect parsing. They are used in the help:
  - The description becomes the description in the `Options:` list.
  - The value name appears in the heading and the usage line in the form `<FILE>`. If no value name is set, the name of the value type appears instead.
  - Required, the default value, the initial state and the choices appear as annotations ([Help Text Layout](help-layout.md)).
- Do not put a newline in a value name. It breaks the columns of the help ([Help Text Layout](help-layout.md#limits)).

## Positionals

A positional is added with `argx_command_add_positional`. You specify its name, its value type and its arity. The positionals of a command receive the command-line arguments in the order they were added.

| Arity | Meaning |
| --- | --- |
| `ARGX_ARITY_REQUIRED` | Exactly one |
| `ARGX_ARITY_OPTIONAL` | Zero or one |
| `ARGX_ARITY_ZERO_OR_MORE` | Zero or more |
| `ARGX_ARITY_ONE_OR_MORE` | One or more |

- The name is for display and identification, and it does not appear on the command line. Any non-empty string can be used, and the names within the same command must differ from each other.
- The name appears in the usage line and in the `Arguments:` list in forms such as `<name>` and `[name]...` ([Help Text Layout](help-layout.md#usage-line)). A newline in the name breaks the columns of the help ([Help Text Layout](help-layout.md#limits)).
- A positional has two attributes:
  - the description (`argx_positional_set_description`), which can be set for any arity;
  - the default value (`argx_positional_set_default`), which can be set only on a positional with `ARGX_ARITY_OPTIONAL`.
- Whether the sequence of arities is consistent (no required positional after an optional one, no positional after a variadic one) is not checked when a positional is added. Validation checks that.

## Rules common to all definitions

These rules apply to commands, options and positionals alike.

- **Strings are copied.** Every string you pass (names, descriptions, value names, default values) is copied. Once the call returns, you may change or free it.
- **Setting again replaces.** Setting an attribute again replaces the previous value. There are no functions that clear a setting.
- **Empty is not unset.** An empty string can be set, and it is different from not being set (`NULL`).
- **Names are compared byte by byte.** Uppercase and lowercase letters make different names.
- **Name conflicts are checked per command.** A parent and a child command, or two different subcommands, can use the same names.
- **The root owns the whole tree.** There are no functions that destroy a subcommand, an option or a positional on its own. The handles that the add functions write are used to set attributes; you do not destroy them.
- **Nothing can be removed.** There are no functions that remove a definition element, and none that copy a schema.

## Validation

```
argx_error_t argx_command_validate(argx_command_t *root,
                                   const argx_command_t **command,
                                   const argx_option_t **option,
                                   const argx_positional_t **positional);
bool argx_command_is_validated(const argx_command_t *command);
```

`argx_command_validate` checks the root and every subcommand below it. You pass the root. When it succeeds, the tree becomes validated. It returns the first error it finds and stops; the checks are made in a fixed order ([Order of the checks](#order-of-the-checks)), so the same definition always gives the same result. The errors it returns are listed in [Errors found by validation](#errors-found-by-validation).

### The output parameters

| Return value | `*command` | `*option` | `*positional` | Validation state |
| --- | --- | --- | --- | --- |
| `ARGX_ERROR_NONE` | `NULL` | `NULL` | `NULL` | Becomes validated |
| `ARGX_ERROR_INVALID_DEFINITION`, `ARGX_ERROR_INVALID_DEFAULT` (error in an option) | The command the option belongs to | The option with the error | `NULL` | Unchanged |
| `ARGX_ERROR_INVALID_DEFINITION`, `ARGX_ERROR_INVALID_DEFAULT` (error in a positional) | The command the positional belongs to | `NULL` | The positional with the error | Unchanged |
| `ARGX_ERROR_OUT_OF_MEMORY` | `NULL` | `NULL` | `NULL` | Unchanged |

- Each of the three output parameters can be omitted by passing `NULL`. An output parameter that is not `NULL` is always written on every call.
- The handles written are ones in the tree. They stay valid until the root is destroyed, and you do not free them.
- Validation does not change the definition. Whatever it returns, the commands, options, positionals and their attributes are as they were before the call. A failed validation can be repeated after the cause is fixed, and after `ARGX_ERROR_OUT_OF_MEMORY`, once enough memory is available.
- The handles can be read with the functions in [Reading a schema](#reading-a-schema).

### The validation state

- A root right after it is created is not validated.
- When validation succeeds, the tree becomes validated. A failed validation does not change the state.
- A successful addition or set, on any part of the tree, marks the tree as not validated again. This happens even when the same value is set again. A failed addition or set does not change the state.
- You can pass any command of the tree to `argx_command_is_validated`. The result is the same.
- You can call validation as many times as you like. Call it again after you change the definition.
- Only a validated schema can be passed to parsing (`argx_parse`, `argx_parse_wide`). A schema for which `argx_command_is_validated` returns `false` is one that has never been validated successfully, or one that was changed by an addition or a set after validation.
- Passing a schema that is not validated to parsing violates a precondition. No error code is returned. It is checked with `assert`: in the Debug configuration the process aborts, and in the Release configuration the behavior is undefined ([Preconditions](#preconditions), [Parsing](parse.md#preconditions)).

### What validation calls

For each default value it reaches, validation allocates a value buffer, asks the value type to convert the value, requests its release if the conversion succeeds, and frees the buffer. So the conversion function and the release function of a user-defined value type are called on every validation ([User-defined value types](types.md#user-defined-value-types)). No value is left after validation.

## Definition errors

Errors in a definition are returned as error codes. Errors that the add and set functions can check are returned right away. Errors that can only be found by looking at the whole tree are returned by validation.

The error codes, their messages and their meanings are listed in [Error Codes](errors.md).

If an add or set function fails:

- the schema has the same contents as before the call, and its validation state does not change;
- the output parameter that receives the created object is set to `NULL`;
- no memory that the call allocated is left behind.

After the failure, you can keep using the schema, and you can add an element again with the same name.

A `NULL` for a required argument, an index out of range, a value that is not in an enumeration, and the use of a destroyed handle are programming errors, not error codes ([Preconditions](#preconditions)).

### Errors when adding and setting

Within one function, the conditions are checked from the top row down, and the first one that applies is returned. Besides these, every function returns `ARGX_ERROR_OUT_OF_MEMORY` when an allocation fails. Setting a description (on a command, an option or a positional) fails only when an allocation fails.

When an add or set function fails, the schema is as it was before the call, and the handle that it writes is `NULL`. Passing that `NULL` handle to a later call breaks a precondition ([Preconditions](#preconditions)).

**`argx_command_create`**

| Condition | Error code |
| --- | --- |
| The name is empty | `ARGX_ERROR_INVALID_NAME` |

**`argx_command_add_subcommand`**

| Condition | Error code |
| --- | --- |
| The name does not follow the rules (including an empty name) | `ARGX_ERROR_INVALID_NAME` |
| The command it is added to has positionals | `ARGX_ERROR_INVALID_DEFINITION` |
| The command it is added to already has a subcommand with the same name | `ARGX_ERROR_NAME_CONFLICT` |

**`argx_command_add_flag`, `argx_command_add_negatable`, `argx_command_add_count`, `argx_command_add_value`**

| Condition | Error code |
| --- | --- |
| There is neither a short name nor a long name | `ARGX_ERROR_INVALID_NAME` |
| A negatable flag has no long name | `ARGX_ERROR_INVALID_NAME` |
| The short name or the long name does not follow the rules | `ARGX_ERROR_INVALID_NAME` |
| The same command already has an option with the same short name | `ARGX_ERROR_NAME_CONFLICT` |
| The same command already has a long name or a negated name equal to the long name | `ARGX_ERROR_NAME_CONFLICT` |
| For a negatable flag, the same command already has a long name or a negated name equal to its negated name | `ARGX_ERROR_NAME_CONFLICT` |

**`argx_command_add_positional`**

| Condition | Error code |
| --- | --- |
| The name is empty | `ARGX_ERROR_INVALID_NAME` |
| The command it is added to has subcommands | `ARGX_ERROR_INVALID_DEFINITION` |
| The command it is added to already has a positional with the same name | `ARGX_ERROR_NAME_CONFLICT` |

**Setting attributes**

| Function | Condition | Error code |
| --- | --- | --- |
| `argx_option_set_value_name`, `argx_option_set_required`, `argx_option_set_default` | The option is not an option with a value | `ARGX_ERROR_INVALID_DEFINITION` |
| `argx_option_set_initial` | The option is not a negatable flag | `ARGX_ERROR_INVALID_DEFINITION` |
| `argx_positional_set_default` | The arity is not `ARGX_ARITY_OPTIONAL` | `ARGX_ERROR_INVALID_DEFINITION` |

### Errors found by validation

These are returned by `argx_command_validate`, in the [order of the checks](#order-of-the-checks).

| Condition | Error code | Reported definition element |
| --- | --- | --- |
| In the sequence of positionals, `ARGX_ARITY_REQUIRED` or `ARGX_ARITY_ONE_OR_MORE` comes after `ARGX_ARITY_OPTIONAL` or `ARGX_ARITY_ZERO_OR_MORE` | `ARGX_ERROR_INVALID_DEFINITION` | The later positional |
| In the sequence of positionals, a positional comes after `ARGX_ARITY_ZERO_OR_MORE` or `ARGX_ARITY_ONE_OR_MORE` | `ARGX_ERROR_INVALID_DEFINITION` | The later positional |
| An option is required and also has a default value | `ARGX_ERROR_INVALID_DEFINITION` | That option |
| The value type of an option cannot convert its default value | `ARGX_ERROR_INVALID_DEFAULT` | That option |
| The value type of a positional cannot convert its default value | `ARGX_ERROR_INVALID_DEFAULT` | That positional |
| The allocation of the value buffer failed, or the conversion returned `ARGX_ERROR_OUT_OF_MEMORY` | `ARGX_ERROR_OUT_OF_MEMORY` | None |

- If the conversion of a default value fails with anything other than `ARGX_ERROR_OUT_OF_MEMORY`, the result is `ARGX_ERROR_INVALID_DEFAULT`, whatever code the conversion function returned.
- Validation does not check the errors that are returned when adding and setting (invalid names, name conflicts, positionals together with subcommands, attributes on kinds they do not apply to). The add and set functions never let you build such a schema.

### Order of the checks

- Commands are visited starting from the root. A command itself is checked first, then its subcommands in the order they were added. For each subcommand, everything below it is checked before moving on to the next subcommand.
- Within one command, the order is:
  1. The sequence of positionals, in the order they were added.
  2. The options, in the order they were added. For each option, the combination of required and a default value is checked first, then the conversion of the default value.
  3. The conversion of the default values of the positionals, in the order they were added.

## Guarantees of a validated schema

What always holds for any schema (the add and set functions ensure it):

- Within one command, short names are all different. Long names and negated names, taken together, are all different. Subcommand names are all different. Positional names are all different.
- An option has at least one of a short name and a long name. A negatable flag has a long name and a negated name.
- Short names, long names and subcommand names follow the naming rules. The name of the root and the names of positionals are not empty.
- A command never has both positionals and subcommands.
- Options with a value and positionals have a value type, and other options do not.
- Attributes are set only on definition elements of the kinds they apply to.

In a validated schema (a tree for which `argx_command_is_validated` returns `true`), the following also holds for every command:

- The sequence of positionals is one of these:
  - Zero or more `ARGX_ARITY_REQUIRED`, then zero or more `ARGX_ARITY_OPTIONAL`, and last at most one `ARGX_ARITY_ZERO_OR_MORE`.
  - Zero or more `ARGX_ARITY_REQUIRED`, and last exactly one `ARGX_ARITY_ONE_OR_MORE`.
- A required option with a value has no default value.
- Every default value was converted successfully by its value type at the time of validation. If the conversion function has no side effects and returns the same result for the same input, converting it later also succeeds.

Code that reads a schema (parsing and help generation) can rely on these for a validated schema. For a schema that is not validated, it cannot, so both parsing and help generation have a validated schema as a precondition ([Validation](#validation)).

## Reading a schema

The read functions take `const` handles, change nothing, and do not fail.

| Object | Functions |
| --- | --- |
| Command | `argx_command_name`, `argx_command_description`, `argx_command_parent`, `argx_command_is_validated` |
| Enumerating within a command | `argx_command_option_count` and `argx_command_option_at`, `argx_command_positional_count` and `argx_command_positional_at`, `argx_command_subcommand_count` and `argx_command_subcommand_at` |
| Lookup by name | `argx_command_find_short`, `argx_command_find_long`, `argx_command_find_subcommand` |
| Option | `argx_option_command`, `argx_option_kind`, `argx_option_short_name`, `argx_option_long_name`, `argx_option_negated_name`, `argx_option_type`, `argx_option_description`, `argx_option_value_name`, `argx_option_is_required`, `argx_option_default`, `argx_option_initial` |
| Positional | `argx_positional_command`, `argx_positional_name`, `argx_positional_arity`, `argx_positional_type`, `argx_positional_description`, `argx_positional_default` |

Values:

- Enumeration is in the order the elements were added. Indexes start at 0.
- "None" is represented by `NULL` for strings and handles, and by `'\0'` for short names. A description, value name or default value that is not set is `NULL`. An attribute that was set to an empty string reads as an empty string.
- For an option of a kind that an attribute does not apply to, the negated name, the value type, the value name and the default value are `NULL`, and required and the initial state are `false`.

Lookups:

- A lookup covers only the definitions of the command you pass. It does not look at parent or child commands. It compares the whole name, byte by byte.
- `argx_command_find_long` looks up the first `length` bytes of `name` as the name. This lets you look up a name inside a string such as `name=value` without copying it. It also matches negated names, and it writes which one matched to `*negated` (it writes nothing if no option is found).
- Passing `'\0'` to `argx_command_find_short` always returns `NULL`.

Lifetime and threads:

- The strings and handles returned stay valid until the root is destroyed. You do not free them. Adding definitions does not change the handles.
- However, a string read from an attribute that can be set again (the description of a command; the description, value name or default value of an option; the description or default value of a positional) stays valid until the root is destroyed or that attribute is set again, whichever comes first. Setting the attribute again frees the previous string. Names are never replaced.
- While a schema is not being changed, its read functions can be called from several threads at the same time. Changing or validating a schema cannot be done at the same time as any other call on it. You do the synchronization.

## Preconditions

Breaking a precondition is a programming error: no error code is returned ([Ownership and Lifetime](ownership.md#runtime-failures-and-programming-errors)). The preconditions of each function are in the comments of the public headers, under `Preconditions:`. What they have in common is:

- Pointer arguments are not `NULL`, unless the comment says they "may be `NULL`". The arguments that may be `NULL` are:
  - the argument of a destroy function;
  - the `out` of an add function;
  - `long_name`, to mean that there is no long name;
  - `negated` of `argx_command_find_long`;
  - the three output parameters of `argx_command_validate`.
- An index (the `index` of an `_at` function) is less than the corresponding count.
- An enumeration argument (the `arity` of `argx_command_add_positional`) is one of the defined constants.
- What you pass to `argx_command_destroy` and `argx_command_validate` is the root. Do not pass a subcommand.
- Handles have not been destroyed. A value type has not been destroyed before the schemas that use it ([Value Types](types.md#borrowed-value-types-and-the-order-of-destruction)).
