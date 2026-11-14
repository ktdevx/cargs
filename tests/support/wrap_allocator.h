/*
 * ArgX test support - the wrap allocator: an allocator that only forwards
 * to the standard malloc, realloc, calloc and free, and counts the calls of
 * each of its four functions and the size of the last malloc_fn and
 * realloc_fn.
 *
 * Where the test allocator ("test_allocator.h") counts the three kinds of
 * request together, the wrap allocator tells them apart: a test uses it to
 * see which function of the allocator the library called, and with which
 * size. It cannot fail a request and does not check the allocator
 * contract.
 *
 * It is not a part of the library and is not installed. A test uses it by
 * adding support/wrap_allocator.c to the SOURCES of its argx_add_test()
 * call and by including this header as "../support/wrap_allocator.h".
 *
 * The functions are functions of this file, not the standard functions
 * themselves, so that no initializer takes the address of an imported
 * function (MSVC warns about that).
 *
 * The counts are kept in static variables: they are shared by all the
 * tests of one executable and are not synchronized.
 */
#ifndef ARGX_WRAP_ALLOCATOR_H
#define ARGX_WRAP_ALLOCATOR_H

#include <argx/argx.h>

#include <stddef.h>

/* What the wrap allocator was asked for since the last reset. */
typedef struct argx_wrap_counts
{
    size_t malloc_calls;  /* the calls of malloc_fn */
    size_t malloc_size;   /* the size of the last malloc_fn; 0 before one */
    size_t realloc_calls; /* the calls of realloc_fn */
    size_t realloc_size;  /* the size of the last realloc_fn; 0 before one */
    size_t calloc_calls;  /* the calls of calloc_fn */
    size_t free_calls;    /* the calls of free_fn */
} argx_wrap_counts_t;

/* Returns the four functions of the wrap allocator. Changes no state. */
argx_allocator_t argx_wrap_allocator_get(void);

/* Puts every count back to 0. */
void argx_wrap_allocator_reset(void);

/* Returns the counts since the last reset. */
argx_wrap_counts_t argx_wrap_allocator_counts(void);

/* Returns the number of calls of the four functions together. */
size_t argx_wrap_allocator_calls(void);

#endif /* ARGX_WRAP_ALLOCATOR_H */
