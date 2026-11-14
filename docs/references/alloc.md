# Allocator

The library takes all of its memory from one allocator: four functions with the same forms as the standard `malloc`, `realloc`, `calloc` and `free`. Unless you set one, it uses the standard functions.

This document is the API reference of `include/argx/alloc.h`; the comments of the header describe each function in detail. When the allocator may be set is [rule 5](ownership.md#5-allocator) of Ownership and Lifetime.

## Functions

| Functions | Section |
| --- | --- |
| `argx_set_allocator` | [Setting the allocator](#setting-the-allocator) |

## Setting the allocator

- The library keeps a copy of the `argx_allocator_t` you pass. Once the call returns, you may discard the structure.
- Keep the four functions you set valid until you set a different allocator, and until every object allocated with that allocator has been destroyed.

If you change the setting while objects exist, memory from the old allocator is freed with the new one. The library does not detect this mistake. It does not synchronize changes to the setting either.

`argx_set_allocator()` cannot fail at run time, so it returns nothing. It allocates no memory. `NULL` goes back to the default.

```c
#include <argx/argx.h>

/* Your functions. Give them the same form as the standard malloc, realloc, calloc and free. */
void *my_malloc(size_t size);
void *my_realloc(void *ptr, size_t size);
void *my_calloc(size_t count, size_t size);
void my_free(void *ptr);

int main(void)
{
    /* The order of the fields is malloc_fn, realloc_fn, calloc_fn, free_fn. */
    argx_allocator_t allocator = { my_malloc, my_realloc, my_calloc, my_free };

    /* Set it once, before calling any other function of the library. */
    argx_set_allocator(&allocator);

    /* Use the library from here. Destroy every object you create. */

    /* To go back to the default, pass NULL after destroying every object. */
    argx_set_allocator(NULL);
    return 0;
}
```

That none of the four functions is `NULL` is a precondition of `argx_set_allocator()`. You cannot pass a structure that was only initialized with zeros.

## The allocator functions

What your functions must do (the full text is in the comments of `include/argx/alloc.h`):

- `malloc_fn`, `calloc_fn` and `realloc_fn` return memory that is aligned for an object of any type. They return `NULL` when they fail.
- `calloc_fn` returns memory with all of its `count * size` bytes set to 0.
- `realloc_fn`, when it succeeds, keeps the content up to the smaller of the old size and the new size. When it fails, it keeps the original memory valid as it is.
- If you use the library from several threads, the four functions work correctly when they are called at the same time.

The library does not check these. On its side, the library never requests a size of 0 or a count of 0, never passes `NULL` to reallocation or freeing, and never frees the same pointer twice.

The allocator does not carry user data (a context). If your allocator needs state, keep that state on your side.

## Several modules

There is one setting inside the library. If you link the static library into several modules (for example, an executable and a DLL) separately, each module has its own setting.
