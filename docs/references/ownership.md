# Ownership and Lifetime

This document defines who allocates memory and who frees it, and how long the things you pass and the things you get back stay valid. Every public function of ArgX follows these rules. It is written for you as a user of the library: it tells you what you free yourself and what you must not free.

This document specifies these rules. Where each API page applies them is in [Where the rules are applied](#where-the-rules-are-applied), and why some of them were chosen is in [Design](../guides/design.md). The detailed description and the preconditions of each function are in the comments of the public headers under `include/argx/`. How the library code implements these rules is in [Development Conventions](../development/conventions.md#applying-the-ownership-rules).

In the examples of this document, names that start with `argx_example_` are placeholders used for explanation; they do not exist in the library. The other names (`argx_set_allocator`, `argx_error_message`, `argx_strdup` and so on) are real.

## The rules at a glance

| No. | Rule | Summary |
| --- | --- | --- |
| 1 | Creation and destruction | Destroy an object you got from `create` only with the matching `destroy`. Destroying it also frees everything that belongs to it |
| 2 | Strings you pass | The library duplicates them. Functions that borrow are the exception, and their comments state for how long |
| 3 | Strings and objects the library returns | The object that returned them owns them. They stay valid until that object is destroyed. You do not free them |
| 4 | `NULL` | `NULL` passed to `destroy` does nothing. Elsewhere you cannot pass it unless the comment allows it |
| 5 | Allocator | There is one for the whole library. Set it while no object exists |
| 6 | Whoever allocates frees | The library frees the library's memory, and you free your memory |
| 7 | How failures are returned | Functions return `argx_error_t`. On failure, the outputs are left in a defined state, no memory leaks, and existing objects do not change |
| 8 | Threads | Different objects can be used at the same time. You synchronize the use of the same object |

## Runtime failures and programming errors

The rules divide failures into two kinds. Every rule assumes this division.

| Kind | What it is | Examples | Handling |
| --- | --- | --- | --- |
| Runtime failure | Something that can happen even in a correct program | Memory allocation failure. Schema definition errors that come from the content of strings or from the relation between definitions (the table below). Errors in the input of the user of your program, and so on | Returned as `argx_error_t` ([rule 7](#7-how-failures-are-returned)) |
| Programming error | Something that always happens until the calling code is fixed, and that cannot be handled at run time | `NULL` for a required argument, an allocator with a missing function, an index out of range, use of an object that was already destroyed | A precondition of the function. No error code is assigned |

Schema definition errors are divided into these two kinds by where they come from.

| Where the error comes from | Examples | Handling |
| --- | --- | --- |
| The content of a string | A name that breaks the naming rules, an empty name, a default value or value that the value type cannot convert | Runtime failure. Returned as `argx_error_t` |
| The relation between definitions | A name conflict within the same command, positionals used together with subcommands, required used together with a default value, a contradiction in the order of positionals, setting an attribute on a kind of definition element it does not apply to, a list of choices that is empty or has duplicates | Runtime failure. Returned as `argx_error_t` |
| The form of an argument | `NULL` for a required argument, an index out of range, a value that is not in the enumeration | Programming error. Stays a precondition |

Why definition errors are error codes and errors in the form of an argument are preconditions is in [Design](../guides/design.md#runtime-failures-are-error-codes-programming-errors-are-preconditions).

Preconditions are handled as follows:

- Preconditions that can be checked are checked at the entry of the function with the standard `assert` (`<assert.h>`). This is the same for public and internal functions. The library has no macro of its own and no mechanism to replace this behavior.
- In a build where `NDEBUG` is not defined (the CMake Debug configuration), breaking a precondition makes `assert` write the condition, the file name and the line number to standard error, and abort the process.
- In a build where `NDEBUG` is defined (the CMake Release, RelWithDebInfo and MinSizeRel configurations), nothing is checked. The behavior when a precondition is broken is undefined.
- Preconditions that cannot be checked (such as setting the allocator only while no object exists) are only written in comments and documents.

On a runtime failure, the library does not abort the process, prints nothing, and does not set `errno`. In the Debug configuration, only programming errors cause an abort.

What the library guarantees:

- The preconditions of each public function are written in the comment of the function in its header.
- Error codes exist only for runtime failures. A programming error becomes a precondition. Definition errors are divided by where they come from, as in the table above.
- The expressions in `assert` have no side effects. Whether `assert` is present does not change the behavior of a correct call.

## 1. Creation and destruction

- A function that creates an object has this form. It can fail, so it returns `argx_error_t` and returns the object through an output argument.

  ```c
  argx_error_t argx_<type>_create(…, argx_<type>_t **out);
  ```

- A function that destroys an object has this form. It does not fail.

  ```c
  void argx_<type>_destroy(argx_<type>_t *object);
  ```

- Destroy an object you got from `create` only with the matching `destroy`.
- Destroying an object frees the object and everything that belongs to it. What belongs to it is the children added to it and the strings it duplicated.
- The handles of children become invalid when the parent is destroyed. There is no function to destroy a child on its own. The only objects you destroy are the ones you got from `create`.

How you write it (an example with placeholder names):

```c
argx_example_t *example;
argx_error_t error;

error = argx_example_create("name", &example);
if (error != ARGX_ERROR_NONE)
{
    /* example is NULL. There is nothing to destroy. */
    fprintf(stderr, "%s\n", argx_error_message(error));
    return 1;
}

/* Use example. */

argx_example_destroy(example);
```

## 2. Strings you pass

- When the library uses a string after the call returns, the library duplicates it. Once the call returns, you may change or free the string.
- Functions that borrow a string without duplicating it are the exception. The comment of such a function says so, and says how long you must keep the string valid. A function whose comment does not say so duplicates.

It is safe to pass a local array or a buffer that you overwrite later.

```c
char name[32];

snprintf(name, sizeof(name), "item-%d", 1);
error = argx_example_create(name, &example);   /* Placeholder name. Duplicates name */
/* From here on, overwriting name does not affect example. */
```

Inside the library, strings are duplicated with `argx_strdup`, an internal function.

## 3. Strings and objects the library returns

- The strings and handles that a function returns are owned by the object that returned them. They stay valid until that object is destroyed. You do not free them.
- If the comment of a function states a shorter period, follow it.
- The string of `argx_error_message()` stays valid until the program ends.
- If one object refers to another (for example, a result refers to a schema), do not destroy the referred object first. The comment of the function states this order.

```c
const char *name = argx_example_name(example);   /* Placeholder name. Owned by example */

argx_example_destroy(example);
/* From here on, name cannot be used. Do not write free(name) either. */
```

## 4. NULL

- Passing `NULL` to `destroy` does nothing. When cleaning up after a failure, you can call `destroy` without checking whether the object was created.
- `NULL` for `argx_set_allocator()` means going back to the default allocator.
- For any other argument, you cannot pass `NULL` unless the comment of the function says "may be `NULL`". Passing it breaks a precondition ([Runtime failures and programming errors](#runtime-failures-and-programming-errors)). No error code is returned.

## 5. Allocator

- There is one allocator for the whole library. Public functions do not take an allocator as an argument. If you do not set one, the standard `malloc`, `realloc`, `calloc` and `free` are used.
- Call `argx_set_allocator()` while no object of the library exists and no other thread is calling the library. Usually you call it once, at the start of the program.
- The type `argx_allocator_t`, `argx_set_allocator()` and what the four functions must do are in [Allocator](alloc.md).

## 6. Whoever allocates frees

- Memory allocated by the library is freed only through the library's `destroy`. There is no public API that makes you call `free` or your own freeing function on it directly.
- There is also no public API in which the library frees memory that you allocated.

The library's memory belongs to the allocator that is set. Your `free` does not necessarily match it. The same is true the other way around.

## 7. How failures are returned

- A public function that can fail at run time returns `argx_error_t`, and returns its result through an output argument. Only `ARGX_ERROR_NONE` means success.
- A function that cannot fail at run time (`destroy`, simple getters, `argx_set_allocator()`) returns its value directly, or returns nothing. Breaking a precondition does not count as a failure.
- On failure, an output that receives a created object or string becomes `NULL`. Other outputs are not changed.
  - The exception is when a parsing function (`argx_parse`, `argx_parse_wide`) returns a parse error. A result is written to the output, and you destroy it ([Parsing](parse.md#entry-points)). A function that is an exception states in its comment what happens to the output for each return value.
  - The other exception is when a function that generates help into a buffer (`argx_help_format`) fails to allocate. Your buffer becomes an empty string, and the length output becomes 0 ([Help and Usage](help.md#on-failure)).
- On failure, all memory allocated during that call is freed before the function returns.
- When a function that changes an existing object fails, the object keeps the same content as before the call. A function that cannot do this at least keeps the object in a state where it can be used and destroyed, and states the remaining differences in its comment.

From your side, a failure never leaves more to clean up. After a failed `create`, the output is `NULL`, and after a failed change, the target is as it was (except for the differences stated in the comment of a function that cannot keep the original content). In both cases, calling `destroy` as usual is enough.

## 8. Threads

- The only mutable global state of the library is the allocator that is set.
- While the setting does not change, calls on different objects can be made at the same time without synchronization, provided that your allocator can handle calls at the same time.
- You synchronize calls on the same object made at the same time.
- Call `argx_set_allocator()` while no other thread is calling the library ([rule 5](#5-allocator)).

## Where the rules are applied

How these rules apply to each kind of object, with the details, is in the reference for it:

| Objects | Where |
| --- | --- |
| Schemas and value types, and the order of destruction | [Rules common to all definitions](schema.md#rules-common-to-all-definitions), [Borrowed value types and the order of destruction](types.md#borrowed-value-types-and-the-order-of-destruction) |
| Results, argv and parse errors | [Entry points](parse.md#entry-points), [Lifetime rules](parse.md#lifetime-rules) of Parsing |
| Help options, write functions and buffers | [On failure](help.md#on-failure), [Lifetime rules](help.md#lifetime-rules) of Help and Usage |
| The allocator | [Allocator](alloc.md) |
| Error codes and their messages | [Error Codes](errors.md) |

