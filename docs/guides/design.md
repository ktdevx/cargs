# Design

Why ArgX works the way it does. Each section gives a decision, its reason and its cost. You do not need this to use the library.

## The library prints nothing and ends no process

Parsing and help generation print nothing, never end the process and read no environment variables. The help is handed to a write function of the program or written into its buffer.

- **Why:** a library cannot know how a program reports errors or what its exit statuses mean. Results and help also depend only on the schema and argv, so tests can compare them exactly.
- **Cost:** even the simplest program writes a few lines to print the help and the parse error ([Tutorial](tutorial.md#5-print-the-help-or-the-parse-error)), and the layout of the help cannot change beyond the help options.
- **Rules:** [Parsing](../references/parse.md), [Help and Usage](../references/help.md), [Help Text Layout](../references/help-layout.md).

## `--help` is an ordinary option

The library defines no option of its own. `--help` is a flag that the program adds and reads like any other.

- **Why:** the library does not print, so it cannot answer `--help` itself. The program also chooses the names and where the help goes.
- **Cost:** the program defines `--help` on every command and checks it before the parse error.
- **Rules:** [`--help`](../references/parse.md#--help).

## A schema is validated before it is used

Some errors can be seen only in the whole tree, such as the order of positional arities or a default its value type cannot convert. `argx_command_validate` checks them once, and parsing and help accept only a validated schema.

- **Why:** parsing and help can rely on fixed guarantees instead of checking on every call, and a failure points to the exact definition.
- **Cost:** one more call, and a schema changed after validation must be validated again.
- **Rules:** [Validation](../references/schema.md#validation).

## Runtime failures are error codes, programming errors are preconditions

A failure that can happen in a correct program returns an `argx_error_t`. A mistake in the calling code, such as `NULL` for a required argument or an index out of range, breaks a precondition checked with `assert`.

- **Why preconditions:** the caller cannot handle such a mistake at run time, and `assert` shows it where it is made.
- **Why definition errors are error codes:** invalid names, name conflicts, inconsistent definitions and unconvertible defaults can come from run-time data such as a configuration file, only the library can check them, and `assert` checks nothing in Release.
- **Cost:** in Release, a broken precondition goes undetected. Develop with Debug.
- **Rules:** [Runtime failures and programming errors](../references/ownership.md#runtime-failures-and-programming-errors), [Error Codes](../references/errors.md).

## A parse error gives a result, under one error code

When the command line does not match the schema, `argx_parse` returns `ARGX_ERROR_INVALID_COMMAND_LINE` and still writes a result that holds the kind, the message and the details of the error.

- **Why:** no separate error object is needed, the program can still see a `--help` given before the error, and new kinds of parse error do not add error codes.
- **Cost:** unlike other failures, the output is not `NULL`, and the result must be destroyed.
- **Rules:** [Parse errors](../references/parse.md#parse-errors).

## Strings are copied

Every string passed to the library is copied unless the function says it borrows it. A result copies all of argv.

- **Why:** you need not think about how long a string lives, and strings read from a result live exactly as long as the result.
- **Cost:** more allocations, and a copy of argv in every result.
- **Rules:** [rule 2](../references/ownership.md#2-strings-you-pass) of Ownership and Lifetime.

## A schema borrows its value types

A schema keeps a pointer to each value type it is given. The program destroys the schema first and the value types after it.

- **Why:** one value type can serve several options, schemas and threads, without reference counts that would need synchronization.
- **Cost:** the program destroys them in the right order; the library does not detect a wrong one.
- **Rules:** [Borrowed value types and the order of destruction](../references/types.md#borrowed-value-types-and-the-order-of-destruction).

## A value is written into the caller's buffer

A value type is a size, a conversion function and a release function. The caller provides a buffer of that size, and the conversion writes into it.

- **Why:** no union of all value kinds becomes public, built-in types allocate nothing, and built-in and user-defined types work the same way.
- **Cost:** values are passed as `void *`, so the compiler does not check their type.
- **Rules:** [User-defined value types](../references/types.md#user-defined-value-types).

## Everything in a schema can be read back

Every attribute of a schema has a public reading function.

- **Why:** a program can inspect a definition that failed validation, or build its own help or shell completion.
- **Cost:** more public functions that can no longer change.
- **Rules:** [Reading a schema](../references/schema.md#reading-a-schema).

## One allocator for the whole library

All memory comes from one allocator of four `malloc`-like functions, set with `argx_set_allocator`.

- **Why:** an existing allocator plugs in as it is, and no function needs an allocator argument.
- **Cost:** it is global state, which may change only while no ArgX object exists, and it carries no user data.
- **Rules:** [Allocator](../references/alloc.md).
