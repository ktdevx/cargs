/*
 * CLArgs - internal declarations for value types.
 *
 * The contents of a value type. Built-in and user-defined value types are
 * the same structure: a conversion is always a call of convert_fn.
 */
#ifndef CLARGS_TYPE_INTERNAL_H
#define CLARGS_TYPE_INTERNAL_H

#include <clargs/type.h>

#include <stdbool.h>
#include <stddef.h>

/*
 * A value type. It is not modified after it has been made.
 *
 * - name: the name of the type. Never NULL.
 * - value_size: the size of a value in bytes. Greater than 0.
 * - convert_fn: the conversion function. Never NULL.
 * - release_fn: the cleanup function, or NULL when the values need none.
 * - context: the user data that is passed to convert_fn and release_fn, or
 *   NULL.
 * - choices, choice_count: the list of the choices of a value type of the
 *   kind CLARGS_TYPE_CHOICE. NULL and 0 for every other kind.
 * - is_static: true for a built-in value type, which is a fixed object that
 *   is never destroyed.
 */
struct clargs_type {
    clargs_type_kind_t kind;
    const char *name;
    size_t value_size;
    clargs_convert_fn_t convert_fn;
    clargs_release_fn_t release_fn;
    void *context;
    char **choices;
    size_t choice_count;
    bool is_static;
};

#endif /* CLARGS_TYPE_INTERNAL_H */
