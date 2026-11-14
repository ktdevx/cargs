# Ownership and Lifetime

Who allocates and frees memory, and how long what you pass and get back stays valid. Every public function follows these rules; the details of each function are in the comments of `include/argx/`. The reasons are in [Design](../guides/design.md), and how the library code applies them is in [Development Conventions](../development/conventions.md#applying-the-ownership-rules).

In the examples, `argx_example_*` names are placeholders.

## Runtime failures and programming errors

| Kind | What it is | Examples | Handling |
| --- | --- | --- | --- |
| Runtime failure | Can happen in a correct program | Allocation failure; invalid input; schema definition errors that come from the content of a string (an invalid or empty name, an unconvertible default) or from the relation between definitions (a name conflict, positionals with subcommands, required with a default, contradicting positional arities, an attribute that does not apply, empty or duplicate choices) | An `argx_error_t` ([rule 7](#7-how-failures-are-returned)) |
| Programming error | Happens until the calling code is fixed | `NULL` for a required argument, an allocator with a missing function, an index out of range, a value outside an enumeration, a destroyed object | A precondition, with no error code |

- Checkable preconditions are checked with the standard `assert`. Without `NDEBUG` (Debug), a violation aborts with the condition, file and line; with `NDEBUG` (Release, RelWithDebInfo, MinSizeRel), nothing is checked and the behavior is undefined.
- Other preconditions, such as setting the allocator only while no object exists, are only documented.
- The preconditions of each function are in its comment. `assert` expressions have no side effects.
- On a runtime failure, the library never aborts, prints or sets `errno`.

## 1. Creation and destruction

```c
argx_error_t argx_<type>_create(…, argx_<type>_t **out);
void argx_<type>_destroy(argx_<type>_t *object);
```

- Destroy what you got from `create` only with the matching `destroy`, which never fails.
- Destroying an object frees everything that belongs to it: its children and the strings it copied. Handles of children become invalid then; children are never destroyed on their own.

```c
error = argx_example_create("name", &example);
if (error != ARGX_ERROR_NONE)
{
    /* example is NULL: nothing to destroy. */
    fprintf(stderr, "%s\n", argx_error_message(error));
    return 1;
}

/* Use example. */

argx_example_destroy(example);
```

## 2. Strings you pass

The library copies every string it keeps after the call, so you may change or free it once the call returns. A function that borrows instead says so in its comment, with how long the string must stay valid.

## 3. Strings and objects the library returns

- Returned strings and handles belong to the object that returned them and stay valid until it is destroyed, unless the comment states a shorter period. Do not free them.
- The string of `argx_error_message()` lives until the program ends.
- Do not destroy an object that another one refers to first (for example, a schema before its results).

## 4. NULL

- `destroy(NULL)` does nothing, so cleanup needs no checks.
- `argx_set_allocator(NULL)` restores the default allocator.
- Elsewhere, `NULL` is a precondition violation unless the comment says "may be `NULL`".

## 5. Allocator

There is one allocator for the whole library, by default the standard functions. Set it with `argx_set_allocator()` while no ArgX object exists and no other thread is calling the library ([Allocator](alloc.md)).

## 6. Whoever allocates frees

The library frees its memory only through `destroy`, and never frees memory you allocated. Your `free` does not necessarily match the library's allocator.

## 7. How failures are returned

- A function that can fail returns `argx_error_t` and its result through an output argument. A function that cannot fail returns its value directly, or nothing.
- On failure, an output that receives a created object or string is `NULL`, and other outputs are unchanged. Exceptions, stated in their comments:
  - A parse error writes a result that you destroy ([Parsing](parse.md#entry-points)).
  - An allocation failure in `argx_help_format` leaves an empty string and a length of 0 ([Help and Usage](help.md#on-failure)).
- On failure, everything the call allocated is freed.
- A failed change leaves the object as it was, or, where a comment says otherwise, at least usable and destroyable.

So a failure never leaves more to clean up.

## 8. Threads

- The only mutable global state is the allocator.
- Calls on different objects may run concurrently, if your allocator allows it. You synchronize calls on the same object.

## Where the rules are applied

| Objects | Where |
| --- | --- |
| Schemas and value types | [Rules common to all definitions](schema.md#rules-common-to-all-definitions), [Borrowed value types and the order of destruction](types.md#borrowed-value-types-and-the-order-of-destruction) |
| Results and parse errors | [Entry points](parse.md#entry-points), [Lifetime rules](parse.md#lifetime-rules) |
| Help | [On failure](help.md#on-failure), [Lifetime rules](help.md#lifetime-rules) |
| Allocator | [Allocator](alloc.md) |
| Error codes | [Error Codes](errors.md) |
