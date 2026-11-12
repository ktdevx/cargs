/*
 * CLArgs test support - the test allocator: an allocator that can fail a
 * chosen request and that counts requests, live blocks and violations of
 * the allocator contract.
 *
 * It is not a part of the library and is not installed. A test uses it by
 * adding support/test_allocator.c to the SOURCES of its clargs_add_test()
 * call and by including this header as "../support/test_allocator.h". It
 * depends only on <clargs/clargs.h> and the C standard library, so tests of
 * the public API and tests of the internal functions can both use it.
 *
 * The state is kept in static variables: there is one test allocator per
 * test executable, shared by all of its tests, and it is not synchronized.
 * A test that calls install ends with uninstall.
 *
 * What is counted:
 *
 * - A request is a call of malloc_fn, calloc_fn or realloc_fn. free_fn is
 *   not a request.
 * - A live block is a block that was allocated and not freed yet.
 * - A violation is a call that the library must never make (see "How the
 *   library calls the functions of the allocator" in <clargs/alloc.h>): a
 *   request of size 0 or count 0, a realloc_fn of NULL, a free_fn of NULL.
 *   A violating call is counted and does nothing else: a violating request
 *   returns NULL, allocates nothing, and leaves the block it was given
 *   valid. It still counts as a request.
 *
 * The memory really comes from the standard malloc, calloc, realloc and
 * free.
 */
#ifndef CLARGS_TEST_ALLOCATOR_H
#define CLARGS_TEST_ALLOCATOR_H

#include <clargs/clargs.h>

#include <stddef.h>

/* Returns the four functions of the test allocator. Changes no state. */
clargs_allocator_t clargs_test_allocator_get(void);

/* Resets like clargs_test_allocator_reset(), then sets the test allocator
 * as the allocator of the library. */
void clargs_test_allocator_install(void);

/* Sets the library back to its default allocator. The counts stay as they
 * are, so they can be read afterwards. */
void clargs_test_allocator_uninstall(void);

/* Puts the counts of requests, live blocks and violations back to 0 and
 * forgets the request to fail. If blocks are still live when it is called,
 * the count of live blocks is wrong from then on. */
void clargs_test_allocator_reset(void);

/* Makes the request-th request after the last reset fail, counted from 1
 * over malloc_fn, calloc_fn and realloc_fn together. Only that request
 * fails: it returns NULL, and a failed realloc_fn leaves its block valid
 * and unchanged. With 0, no request fails. */
void clargs_test_allocator_fail_at(size_t request);

/* The number of requests since the last reset, the failed ones included. */
size_t clargs_test_allocator_requests(void);

/* The number of live blocks: up by one with an allocation that succeeds,
 * down by one with a free, unchanged by a reallocation. */
size_t clargs_test_allocator_live(void);

/* The number of violations of the allocator contract since the last
 * reset. */
size_t clargs_test_allocator_violations(void);

#endif /* CLARGS_TEST_ALLOCATOR_H */
