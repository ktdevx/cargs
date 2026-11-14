# Design

This document explains why ArgX works the way it does. Each section names one decision, says why it was made and what it costs, and links to the reference that holds the rules. Read it when a rule of the references looks surprising; you do not need it to use the library.

## The library prints nothing and ends no process

Parsing and help generation print nothing, never end the process and read no environment variables. The program that uses ArgX decides what to print, where, in which words, and with which exit status; the library only reports what happened, as return values and as the contents of the result.

- **Why:** a library cannot know how a program reports errors, whether it has a console, or what its exit statuses mean. Leaving all output to the program also makes every result and every help text depend only on the schema and argv, so tests can compare them exactly.
- **Cost:** even the simplest program writes a few lines to print the help and the parse error ([Guide](guide.md#5-print-the-help-or-the-parse-error)).
- **Rules:** [Parsing](../references/parse.md), [Help and Usage](../references/help.md#output).

## `--help` is an ordinary option

The library defines no option of its own. `--help` is a flag that the program adds to each command and reads from the result like any other.

- **Why:** this follows from the decision above: the library does not print, so it cannot answer `--help` by itself. The program also stays free to choose the names, whether the help goes to the standard output, and what else to print with it.
- **Cost:** the program defines `--help` on every command and checks it before the parse error ([Guide](guide.md#2-define-the-schema)).
- **Rules:** [`--help`](../references/parse.md#--help).

## A schema is validated before it is used

Some errors in a schema can be seen only by looking at the whole tree: the order of the arities of the positionals, a required option that also has a default value, a default value that its value type cannot convert. `argx_command_validate` checks them once, and parsing and help generation accept only a validated schema.

- **Why:** after validation, parsing and help generation can rely on a fixed set of guarantees instead of checking the schema again on every call. A failed validation also points to the exact definition that is wrong.
- **Cost:** one more call, and a schema that is changed after validation must be validated again. The validation state is kept on the root, and every successful change resets it.
- **Rules:** [Validation](../references/schema.md#validation), [Guarantees of a validated schema](../references/schema.md#guarantees-of-a-validated-schema).

## Runtime failures are error codes, programming errors are preconditions

A failure that can happen in a correct program returns an `argx_error_t`. A mistake in the calling code, such as `NULL` for a required argument, an index out of range or a destroyed object, breaks a precondition: it is checked with the standard `assert` in the Debug configuration and not checked in the Release configuration.

- **Why preconditions:** the caller cannot handle such a mistake at run time, so an error code for it would only add checks that never help. With `assert`, the mistake shows up where it is made, and it cannot pass silently through code that does not check a return value. This is the usual way of C libraries.
- **Why definition errors are error codes:** most errors in a schema are also fixed by fixing the calling code. Even so, an invalid name, a name conflict, an inconsistent definition and a default value that cannot be converted are error codes, for three reasons:
  - Names, default values and the strings of choices can be built from data decided at run time, such as a configuration file or a plug-in. Then they can happen even in a correct program.
  - To check them before calling the library, the caller would have to implement the naming rules and the comparison with the definitions added so far exactly as the library does. Only the library can check them.
  - In the Release configuration, `assert` checks nothing. If these were preconditions, a wrong definition would go undetected, and parsing and help would later behave in undefined ways. An error code reaches the caller in every configuration.
- **Why the form of an argument stays a precondition:** whether a pointer is not `NULL`, whether an index is less than the count and whether an enumeration value is defined depend only on the calling code, and can be seen at the call site.
- **Cost:** in the Release configuration, a broken precondition goes undetected and can turn into a crash or memory corruption. Use the Debug configuration during development.
- **Rules:** [Runtime failures and programming errors](../references/ownership.md#runtime-failures-and-programming-errors), [Error Codes](../references/errors.md).

## A parse error gives a result, under one error code

When the command line does not match the schema, `argx_parse` returns `ARGX_ERROR_INVALID_COMMAND_LINE` and still writes a result. The kind of the parse error, its message and its details are read from that result.

- **Why a result:** the details and the message of the error, and what was read before it, live in one object that the program already knows how to read and destroy. No separate error object is needed. It also lets the program see a `--help` that was given before the error.
- **Why one error code:** the return value only has to tell whether there is a result (success or a parse error) or not (an allocation failure). The program reads the message from the result anyway, and adding a kind of parse error does not grow the list of error codes of the whole library.
- **Cost:** this is an exception to the rule that a failed call leaves its output `NULL`: a result with a parse error must be destroyed too.
- **Rules:** [Entry points](../references/parse.md#entry-points), [Parse errors](../references/parse.md#parse-errors).

## Strings are copied

Every string passed to the library is copied, unless the comment of the function says that it borrows it. A result copies all of argv.

- **Why:** you do not have to think about how long a string must live, and it is safe to pass a local array or a buffer that you overwrite later. For argv in particular, `argx_parse_wide` must keep the strings it converted to UTF-8 anyway; copying for `argx_parse` too gives both entry points one rule, and the strings read from a result, including those of a parse error, are valid exactly as long as the result.
- **Cost:** more allocations when a schema is defined, and memory for a copy of argv in every result.
- **Rules:** [rule 2](../references/ownership.md#2-strings-you-pass) of Ownership and Lifetime, [Lifetime rules](../references/parse.md#lifetime-rules) of Parsing.

## A schema borrows its value types

`argx_command_add_value` and `argx_command_add_positional` keep a pointer to the value type they are given. The program destroys the schema first and the value types after it.

- **Why:** one value type can then serve several options, several schemas and several threads. If a schema took over its value types, two schemas could not share one. If value types counted their references, they would hold state that changes, and defining two schemas on two threads would need synchronization.
- **Cost:** the program keeps the value types it created and destroys them in the right order. The library does not detect a wrong order.
- **Rules:** [Borrowed value types and the order of destruction](../references/types.md#borrowed-value-types-and-the-order-of-destruction).

## A value is written into the caller's buffer

A value type is a size, a conversion function and a release function. Whoever asks for a conversion provides a buffer of that size, and the conversion function writes the value into it.

- **Why:** no union of all value kinds becomes part of the public interface, the built-in value types allocate nothing, and the caller decides where a value lives and for how long. Built-in and user-defined value types work the same way.
- **Cost:** values are passed as `void *`, so the compiler does not check that a value is read with the right type. The kind of the value type tells which type to use.
- **Rules:** [Converting a value](../references/types.md#converting-a-value), [User-defined value types](../references/types.md#user-defined-value-types).

## Everything in a schema can be read back

Every attribute of a schema has a public reading function, and the library itself reads schemas only through them.

- **Why:** a program can inspect the definition that a failed validation points to, and it can build its own help or shell completion from a schema.
- **Cost:** more public functions, whose arguments and meaning can no longer change.
- **Rules:** [Reading a schema](../references/schema.md#reading-a-schema).

## One allocator for the whole library

All memory comes from one allocator: four functions with the same forms as `malloc`, `realloc`, `calloc` and `free`, set with `argx_set_allocator`.

- **Why:** an existing allocator can be plugged in as it is, and no public function needs an allocator argument.
- **Cost:** the setting is global state. It may be changed only while no object of the library exists, so that memory allocated with one allocator is never freed with another, and the library does not detect a change at the wrong time. The allocator carries no user data, and each module that links the static library has its own setting.
- **Rules:** [rule 5](../references/ownership.md#5-allocator) of Ownership and Lifetime, [Allocator](../references/alloc.md).

## The help is generated, not printed

The help text follows from the schema and the help options by fixed rules, and it is handed to a write function of the program or written into its buffer.

- **Why:** the same schema always gives the same bytes, whatever the locale, the environment or the terminal, so a test can compare the help with an expected string. The library needs no `<stdio.h>` and writes nowhere the program did not choose.
- **Cost:** the program writes a short write function, or formats into a buffer. The layout cannot be changed beyond the help options.
- **Rules:** [Help and Usage](../references/help.md), [Help Text Layout](../references/help-layout.md).
