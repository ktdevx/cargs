# Allocator

API reference of `include/argx/alloc.h`. The library takes all of its memory from one allocator of four functions shaped like `malloc`, `realloc`, `calloc` and `free`; by default, the standard ones. When it may be set is [rule 5](ownership.md#5-allocator) of Ownership and Lifetime.

## Functions

| Functions | Section |
| --- | --- |
| `argx_set_allocator` | [Setting the allocator](#setting-the-allocator) |

## Setting the allocator

```c
#include <argx/argx.h>

void *my_malloc(size_t size);
void *my_realloc(void *ptr, size_t size);
void *my_calloc(size_t count, size_t size);
void my_free(void *ptr);

int main(void)
{
    /* The fields are malloc_fn, realloc_fn, calloc_fn, free_fn. */
    argx_allocator_t allocator = { my_malloc, my_realloc, my_calloc, my_free };

    argx_set_allocator(&allocator);   /* Before any other call. */

    /* ... use the library and destroy every object ... */

    argx_set_allocator(NULL);         /* Back to the default. */
    return 0;
}
```

- The library copies the structure; the four functions must stay valid while any object allocated with them exists.
- None of the four may be `NULL` (a precondition).
- It cannot fail and allocates nothing. Changes are not synchronized, and a change while objects exist is not detected.

## The allocator functions

- `malloc_fn`, `calloc_fn` and `realloc_fn` return memory aligned for any type, or `NULL` on failure.
- `calloc_fn` zeroes all `count * size` bytes.
- `realloc_fn` keeps the content up to the smaller size, and on failure leaves the original memory valid.
- If the library is used from several threads, the functions must be safe to call concurrently.

The library does not check these. In turn, it never requests a size or count of 0, never reallocates or frees `NULL`, and never frees twice.

The allocator carries no user data. If the static library is linked into several modules (an executable and a DLL), each has its own setting.
