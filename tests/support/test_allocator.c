/*
 * CLArgs test support - the test allocator. See test_allocator.h.
 */
#include "test_allocator.h"

#include <stdlib.h>

/* The state: one test allocator per test executable. */
static size_t fail_request; /* the request to fail, from 1; 0 for none */
static size_t request_count;
static size_t live_count;
static size_t violation_count;

/* Counts one request. Returns 1 if this request is the one to fail. */
static int count_request(void)
{
    ++request_count;
    return (fail_request != 0) && (request_count == fail_request);
}

static void *test_malloc(size_t size)
{
    int fail = count_request();
    void *ptr;

    if (size == 0)
    {
        ++violation_count;
        return NULL;
    }
    if (fail)
    {
        return NULL;
    }

    ptr = malloc(size);
    if (ptr != NULL)
    {
        ++live_count;
    }
    return ptr;
}

static void *test_realloc(void *ptr, size_t size)
{
    int fail = count_request();

    if ((ptr == NULL) || (size == 0))
    {
        ++violation_count;
        return NULL;
    }
    if (fail)
    {
        return NULL;
    }

    /* A block is replaced by a block: the count of live blocks stays. */
    return realloc(ptr, size);
}

static void *test_calloc(size_t count, size_t size)
{
    int fail = count_request();
    void *ptr;

    if ((count == 0) || (size == 0))
    {
        ++violation_count;
        return NULL;
    }
    if (fail)
    {
        return NULL;
    }

    ptr = calloc(count, size);
    if (ptr != NULL)
    {
        ++live_count;
    }
    return ptr;
}

static void test_free(void *ptr)
{
    if (ptr == NULL)
    {
        ++violation_count;
        return;
    }

    free(ptr);
    --live_count;
}

clargs_allocator_t clargs_test_allocator_get(void)
{
    clargs_allocator_t allocator;

    allocator.malloc_fn = test_malloc;
    allocator.realloc_fn = test_realloc;
    allocator.calloc_fn = test_calloc;
    allocator.free_fn = test_free;
    return allocator;
}

void clargs_test_allocator_install(void)
{
    clargs_allocator_t allocator = clargs_test_allocator_get();

    clargs_test_allocator_reset();
    clargs_set_allocator(&allocator);
}

void clargs_test_allocator_uninstall(void)
{
    clargs_set_allocator(NULL);
}

void clargs_test_allocator_reset(void)
{
    fail_request = 0;
    request_count = 0;
    live_count = 0;
    violation_count = 0;
}

void clargs_test_allocator_fail_at(size_t request)
{
    fail_request = request;
}

size_t clargs_test_allocator_requests(void)
{
    return request_count;
}

size_t clargs_test_allocator_live(void)
{
    return live_count;
}

size_t clargs_test_allocator_violations(void)
{
    return violation_count;
}
