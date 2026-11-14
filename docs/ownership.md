# Ownership and Lifetime

This document defines who allocates memory and who frees it, and how long the things you pass and the things you get back stay valid. Every public function of CLine follows these rules. It is written for you as a user of the library: it tells you what you free yourself and what you must not free.

This document covers the rules themselves and how they apply to schemas, parsing, results and help. How to define a schema, parse argv and generate help is in [Schemas](schema.md), [Parsing](parse.md) and [Help and Usage](help.md). The detailed description and the preconditions of each function are in the comments of the public headers under `include/cline/`. How the library code implements these rules is in [Development Conventions](conventions.md#applying-the-ownership-rules).

Contents:

- [The rules at a glance](#the-rules-at-a-glance)
- [Runtime failures and programming errors](#runtime-failures-and-programming-errors)
- [1. Creation and destruction](#1-creation-and-destruction)
- [2. Strings you pass](#2-strings-you-pass)
- [3. Strings and objects the library returns](#3-strings-and-objects-the-library-returns)
- [4. NULL](#4-null)
- [5. Allocator](#5-allocator)
- [6. Whoever allocates frees](#6-whoever-allocates-frees)
- [7. How failures are returned](#7-how-failures-are-returned)
- [8. Threads](#8-threads)
- [Schemas, argv and results](#schemas-argv-and-results)
- [Help](#help)
- [Allocator and error functions](#allocator-and-error-functions)

In the examples of this document, names that start with `cline_example_` are placeholders used for explanation; they do not exist in the library. The other names (`cline_set_allocator`, `cline_error_message`, `cline_strdup` and so on) are real.

## The rules at a glance

| No. | Rule | Summary |
|---|---|---|
| 1 | Creation and destruction | Destroy an object you got from `create` only with the matching `destroy`. Destroying it also frees everything that belongs to it |
| 2 | Strings you pass | The library duplicates them. Functions that borrow are the exception, and their comments state for how long |
| 3 | Strings and objects the library returns | The object that returned them owns them. They stay valid until that object is destroyed. You do not free them |
| 4 | `NULL` | `NULL` passed to `destroy` does nothing. Elsewhere you cannot pass it unless the comment allows it |
| 5 | Allocator | There is one for the whole library. Set it while no object exists |
| 6 | Whoever allocates frees | The library frees the library's memory, and you free your memory |
| 7 | How failures are returned | Functions return `cline_error_t`. On failure, the outputs are left in a defined state, no memory leaks, and existing objects do not change |
| 8 | Threads | Different objects can be used at the same time. You synchronize the use of the same object |

## Runtime failures and programming errors

The rules divide failures into two kinds. Every rule assumes this division.

| Kind | What it is | Examples | Handling |
|---|---|---|---|
| Runtime failure | Something that can happen even in a correct program | Memory allocation failure. Schema definition errors that come from the content of strings or from the relation between definitions (the table below). Errors in the input of the user of your program, and so on | Returned as `cline_error_t` ([rule 7](#7-how-failures-are-returned)) |
| Programming error | Something that always happens until the calling code is fixed, and that cannot be handled at run time | `NULL` for a required argument, an allocator with a missing function, an index out of range, use of an object that was already destroyed | A precondition of the function. No error code is assigned |

Programming errors get no error code because the caller cannot handle them at run time, and making the caller write code that checks for them would be pointless.

Schema definition errors are divided into these two kinds by where they come from.

| Where the error comes from | Examples | Handling |
|---|---|---|
| The content of a string | A name that breaks the naming rules, an empty name, a default value or value that the value type cannot convert | Runtime failure. Returned as `cline_error_t` |
| The relation between definitions | A name conflict within the same command, positionals used together with subcommands, required used together with a default value, a contradiction in the order of positionals, setting an attribute on a kind of definition element it does not apply to, a list of choices that is empty or has duplicates | Runtime failure. Returned as `cline_error_t` |
| The form of an argument | `NULL` for a required argument, an index out of range, a value that is not in the enumeration | Programming error. Stays a precondition |

Most definition errors are also fixed by fixing the calling code. Even so, the errors that come from the content of strings and from the relation between definitions are error codes, for these reasons:

- Names, default values and the strings of choices can be built from data decided at run time, such as a configuration file or a plug-in. In that case they can happen even in a correct program.
- If the caller tried to check them before calling the library, it would have to implement the naming rules and the comparison with the definitions added so far in the same way as the library. Only the library can check them.
- In the Release configuration, `assert` does not check anything. If these were preconditions, a wrong definition would remain undetected, and later parsing and help would behave in undefined ways. With an error code, the caller receives the kind of error in every configuration.

Errors in the form of an argument are none of these. Whether a pointer is not `NULL`, whether an index is less than the count, and whether an enumeration value is a defined one are decided only by the calling code, and you can check them by looking at the call site. So they stay preconditions.

Preconditions are handled as follows:

- Preconditions that can be checked are checked at the entry of the function with the standard `assert` (`<assert.h>`). This is the same for public and internal functions. The library has no macro of its own and no mechanism to replace this behavior.
- In a build where `NDEBUG` is not defined (the CMake Debug configuration), breaking a precondition makes `assert` write the condition, the file name and the line number to standard error, and abort the process.
- In a build where `NDEBUG` is defined (the CMake Release, RelWithDebInfo and MinSizeRel configurations), nothing is checked. The behavior when a precondition is broken is undefined.
- Preconditions that cannot be checked (such as setting the allocator only while no object exists) are only written in comments and documents.

On a runtime failure, the library does not abort the process, prints nothing, and does not set `errno`. In the Debug configuration, only programming errors cause an abort.

During development, we recommend that you use the library built in the Debug configuration. In the Release configuration, errors go undetected and can turn into crashes or memory corruption.

What the library guarantees:

- The preconditions of each public function are written in the comment of the function in its header.
- Error codes exist only for runtime failures. A programming error becomes a precondition. Definition errors are divided by where they come from, as in the table above.
- The expressions in `assert` have no side effects. Whether `assert` is present does not change the behavior of a correct call.

## 1. Creation and destruction

- A function that creates an object has this form. It can fail, so it returns `cline_error_t` and returns the object through an output argument.

  ```c
  cline_error_t cline_<type>_create(…, cline_<type>_t **out);
  ```

- A function that destroys an object has this form. It does not fail.

  ```c
  void cline_<type>_destroy(cline_<type>_t *object);
  ```

- Destroy an object you got from `create` only with the matching `destroy`.
- Destroying an object frees the object and everything that belongs to it. What belongs to it is the children added to it and the strings it duplicated.
- The handles of children become invalid when the parent is destroyed. There is no function to destroy a child on its own. The only objects you destroy are the ones you got from `create`.

How you write it (an example with placeholder names):

```c
cline_example_t *example;
cline_error_t error;

error = cline_example_create("name", &example);
if (error != CLINE_ERROR_NONE)
{
    /* example is NULL. There is nothing to destroy. */
    fprintf(stderr, "%s\n", cline_error_message(error));
    return 1;
}

/* Use example. */

cline_example_destroy(example);
```

## 2. Strings you pass

- When the library uses a string after the call returns, the library duplicates it. Once the call returns, you may change or free the string.
- Functions that borrow a string without duplicating it are the exception. The comment of such a function says so, and says how long you must keep the string valid. A function whose comment does not say so duplicates.

Duplicating is the default so that you do not have to think about the lifetime of strings. It is safe to pass a local array or a buffer that you overwrite later.

```c
char name[32];

snprintf(name, sizeof(name), "item-%d", 1);
error = cline_example_create(name, &example);   /* Placeholder name. Duplicates name */
/* From here on, overwriting name does not affect example. */
```

Inside the library, strings are duplicated with `cline_strdup`, an internal function.

## 3. Strings and objects the library returns

- The strings and handles that a function returns are owned by the object that returned them. They stay valid until that object is destroyed. You do not free them.
- If the comment of a function states a shorter period, follow it.
- The string of `cline_error_message()` stays valid until the program ends.
- If one object refers to another (for example, a result refers to a schema), do not destroy the referred object first. The comment of the function states this order.

```c
const char *name = cline_example_name(example);   /* Placeholder name. Owned by example */

cline_example_destroy(example);
/* From here on, name cannot be used. Do not write free(name) either. */
```

## 4. NULL

- Passing `NULL` to `destroy` does nothing. When cleaning up after a failure, you can call `destroy` without checking whether the object was created.
- `NULL` for `cline_set_allocator()` means going back to the default allocator.
- For any other argument, you cannot pass `NULL` unless the comment of the function says "may be `NULL`". Passing it breaks a precondition ([Runtime failures and programming errors](#runtime-failures-and-programming-errors)). No error code is returned.

## 5. Allocator

- There is one allocator for the whole library. Public functions do not take an allocator as an argument. If you do not set one, the standard `malloc`, `realloc`, `calloc` and `free` are used.
- Call `cline_set_allocator()` while no object of the library exists and no other thread is calling the library. Usually you call it once, at the start of the program.
- The library keeps a copy of the `cline_allocator_t` you pass. Once the call returns, you may discard the structure.
- Keep the four functions you set valid until you set a different allocator, and until every object allocated with that allocator has been destroyed.

The time when you may set it is limited so that memory allocated with one allocator is never freed with another. If you change the setting while objects exist, memory from the old allocator is freed with the new one. The library does not detect this mistake. It does not synchronize changes to the setting either.

```c
#include <cline/cline.h>

/* Your functions. Give them the same form as the standard malloc, realloc, calloc and free. */
void *my_malloc(size_t size);
void *my_realloc(void *ptr, size_t size);
void *my_calloc(size_t count, size_t size);
void my_free(void *ptr);

int main(void)
{
    /* The order of the fields is malloc_fn, realloc_fn, calloc_fn, free_fn. */
    cline_allocator_t allocator = { my_malloc, my_realloc, my_calloc, my_free };

    /* Set it once, before calling any other function of the library. */
    cline_set_allocator(&allocator);

    /* Use the library from here. Destroy every object you create. */

    /* To go back to the default, pass NULL after destroying every object. */
    cline_set_allocator(NULL);
    return 0;
}
```

That none of the four functions is `NULL` is a precondition of `cline_set_allocator()`. You cannot pass a structure that was only initialized with zeros.

What your functions must do (the full text is in the comments of `include/cline/alloc.h`):

- `malloc_fn`, `calloc_fn` and `realloc_fn` return memory that is aligned for an object of any type. They return `NULL` when they fail.
- `calloc_fn` returns memory with all of its `count * size` bytes set to 0.
- `realloc_fn`, when it succeeds, keeps the content up to the smaller of the old size and the new size. When it fails, it keeps the original memory valid as it is.
- If you use the library from several threads, the four functions work correctly when they are called at the same time.

The library does not check these. On its side, the library never requests a size of 0 or a count of 0, never passes `NULL` to reallocation or freeing, and never frees the same pointer twice.

The allocator does not carry user data (a context). If your allocator needs state, keep that state on your side.

There is one setting inside the library. If you link the static library into several modules (for example, an executable and a DLL) separately, each module has its own setting.

## 6. Whoever allocates frees

- Memory allocated by the library is freed only through the library's `destroy`. There is no public API that makes you call `free` or your own freeing function on it directly.
- There is also no public API in which the library frees memory that you allocated.

The library's memory belongs to the allocator that is set. Your `free` does not necessarily match it. The same is true the other way around.

## 7. How failures are returned

- A public function that can fail at run time returns `cline_error_t`, and returns its result through an output argument. Only `CLINE_ERROR_NONE` means success.
- A function that cannot fail at run time (`destroy`, simple getters, `cline_set_allocator()`) returns its value directly, or returns nothing. Breaking a precondition does not count as a failure.
- On failure, an output that receives a created object or string becomes `NULL`. Other outputs are not changed.
  - The exception is when a parsing function (`cline_parse`, `cline_parse_wide`) returns a parse error. A result is written to the output, and you destroy it ([Schemas, argv and results](#schemas-argv-and-results)). A function that is an exception states in its comment what happens to the output for each return value.
  - The other exception is when a function that generates help into a buffer (`cline_help_format`) fails to allocate. Your buffer becomes an empty string, and the length output becomes 0 ([Help](#help)).
- On failure, all memory allocated during that call is freed before the function returns.
- When a function that changes an existing object fails, the object keeps the same content as before the call. A function that cannot do this at least keeps the object in a state where it can be used and destroyed, and states the remaining differences in its comment.

From your side, a failure never leaves more to clean up. After a failed `create`, the output is `NULL`, and after a failed change, the target is as it was (except for the differences stated in the comment of a function that cannot keep the original content). In both cases, calling `destroy` as usual is enough.

## 8. Threads

- The only mutable global state of the library is the allocator that is set.
- While the setting does not change, calls on different objects can be made at the same time without synchronization, provided that your allocator can handle calls at the same time.
- You synchronize calls on the same object made at the same time.
- Call `cline_set_allocator()` while no other thread is calling the library ([rule 5](#5-allocator)).

## Schemas, argv and results

The following table applies the rules to common questions about schemas, argv, results, and the lifetime of value types, including the result that parsing (`cline_parse`, `cline_parse_wide`) gives you.

| Question | How the rules apply |
|---|---|
| Does the schema duplicate strings? | Yes (the default of [rule 2](#2-strings-you-pass)) |
| Does the result duplicate the strings of argv? | Yes (the default of [rule 2](#2-strings-you-pass)). The result keeps a duplicate of every string of argv. For a wide-character argv, it keeps them converted to UTF-8. Once the parse call returns, you may change or free both the argv array and its strings. Strings you read from the result stay valid until the result is destroyed ([rule 3](#3-strings-and-objects-the-library-returns)) |
| May a result outlive its schema? | No. A result refers to the schema and the value types, so destroy the result first ([rule 3](#3-strings-and-objects-the-library-returns)). The order of destruction is result, schema, value types |
| May I change the schema while a result exists? | No. A result refers to the commands, options and positionals of the schema. While even one result of that schema is left, do not add definition elements to the schema, do not set attributes, and do not destroy the schema or the value types. To change it, destroy all results first. The library does not detect this mistake |
| What happens to the output on a parse error? | A result is written. On a parse error (`CLINE_ERROR_INVALID_COMMAND_LINE`), a result is written to `*out`, and you destroy it with `cline_result_destroy`. You read the kind of error, the message, and what was read up to the error from that result. This is an exception to "on failure, the output becomes `NULL`" of [rule 7](#7-how-failures-are-returned), and the comment of the function says so. On a memory allocation failure (`CLINE_ERROR_OUT_OF_MEMORY`), as rule 7 says, `*out` is `NULL`, and no memory allocated for that parse and no converted value is left |
| Does the schema duplicate value types? How long must a value type stay valid? | No. The schema borrows value types. Value types you create (choices, user-defined) are owned by you, and you keep them valid until every schema that uses them has been destroyed. Destroy the schema first and the value type after it ([rule 3](#3-strings-and-objects-the-library-returns)). Destroying a schema does not destroy the value types ([rule 6](#6-whoever-allocates-frees)). The user data that a user-defined value type borrows must stay valid until the value type is destroyed. The built-in value types (string, integer, floating-point) belong to the library, and you do not destroy them. The details are in [Schemas](schema.md#borrowed-value-types-and-the-order-of-destruction) |

No function borrows (the exception of rule 2) in parsing. The details are in [Parsing](parse.md#lifetime-rules).

## Help

The rules apply to generating the help and the usage line (`cline_help_write`, `cline_help_write_usage`, `cline_help_format`, `cline_help_format_usage`) and to the help options (`cline_help_options_t`).

| Question | How the rules apply |
|---|---|
| Who creates and who destroys the help options? | You create them with `cline_help_options_create` and destroy them with `cline_help_options_destroy` ([rule 1](#1-creation-and-destruction)). The help options do not belong to a schema. Destroying the schema does not destroy the help options. You may destroy either one first. `cline_help_options_destroy(NULL)` does nothing ([rule 4](#4-null)) |
| Do the generating functions return something you must free? | No. The text is passed to your write function or written to your buffer. No function returns a string allocated by the library ([rule 6](#6-whoever-allocates-frees)). Memory allocated for generation is all freed before the function returns, on success and on failure ([rule 7](#7-how-failures-are-returned)) |
| How long are the strings passed to the write function valid? | Only during that call (the "shorter period" of [rule 3](#3-strings-and-objects-the-library-returns)). The strings are not terminated. To use one later, duplicate it inside the write function. Do not free or change them |
| How long does generation use what you pass? | Only during the call. The schema and the help options are only borrowed and are not changed. The write function, the data passed to the write function, and the buffer are not used after the call returns either. Nothing is duplicated and kept |
| May I change the schema during generation? | No. During a generating call, do not change or destroy the schema or the value types it uses. Do not change or destroy the schema passed to generation from inside the write function. The library does not detect this mistake |
| What happens to the output on failure? | Text already passed to the write function cannot be taken back. What was passed before the failure stays at your destination. When `cline_help_format` fails to allocate, if the buffer size is not 0, the buffer becomes an empty string and the length output becomes 0. This is an exception to "other outputs are not changed" of [rule 7](#7-how-failures-are-returned), and the comment of the function says so. When `cline_help_options_create` fails, `*out` is `NULL`, as rule 7 says |
| May I generate from several threads at the same time? | Yes ([rule 8](#8-threads)). Generation keeps no state across calls. A schema and help options that are not being changed can be passed to generation from several threads at the same time. While a function that sets or destroys the help options is running, other threads must not use those help options |

No function borrows (the exception of rule 2) in help. The details are in [Help and Usage](help.md#lifetime-rules).

## Allocator and error functions

The public functions for the allocator and for errors also follow these rules.

| Function | How it follows the rules |
|---|---|
| `void cline_set_allocator(const cline_allocator_t *allocator)` | It cannot fail at run time, so it returns nothing (rule 7). It allocates no memory. `NULL` goes back to the default (rule 4). It copies the structure (rule 5). That none of the four functions is `NULL` is a precondition, checked with `assert`. The preconditions are in the comments of `include/cline/alloc.h` |
| `const char *cline_error_message(cline_error_t error)` | It cannot fail at run time, so it returns the string directly (rule 7). The string stays valid until the program ends, and you do not free it (rule 3). It allocates no memory. It has no preconditions. For a value that is not in the list, it returns `"unknown error code"` |
