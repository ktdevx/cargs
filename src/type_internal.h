/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - internal declarations for value types.
 *
 * The value type structure. Built-in and user-defined value types share
 * it, so a conversion is always a call to convert_fn.
 */
#ifndef ARGX_TYPE_INTERNAL_H
#define ARGX_TYPE_INTERNAL_H

#include <argx/type.h>

#include <stdbool.h>
#include <stddef.h>

/*
 * A value type. It is never modified after it is made.
 *
 * - name: the type name. Never NULL. A fixed string for the built-in value
 *   types and for the ARGX_TYPE_CHOICE kind; owned_name for the
 *   ARGX_TYPE_CUSTOM kind.
 * - owned_name: the value type's own copy of the name, freed when the
 *   value type is destroyed. Only the ARGX_TYPE_CUSTOM kind has one. NULL
 *   for every other kind, whose name is a fixed string that is not freed.
 * - value_size: the size of one value in bytes. Greater than 0.
 * - convert_fn: the conversion function. Never NULL.
 * - release_fn: the cleanup function, or NULL when the values need none.
 * - context: the user data passed to convert_fn and release_fn, or NULL.
 *   For the ARGX_TYPE_CHOICE kind, the value type itself. For the
 *   ARGX_TYPE_CUSTOM kind, the caller's user data: borrowed, never freed.
 * - choices, choice_count: the choices of an ARGX_TYPE_CHOICE value type,
 *   in definition order. The value type owns the array and each string.
 *   NULL and 0 for every other kind.
 * - is_static: true for a built-in value type, which is a fixed object
 *   that is never destroyed.
 */
struct argx_type
{
    argx_type_kind_t kind;
    const char *name;
    char *owned_name;
    size_t value_size;
    argx_convert_fn_t convert_fn;
    argx_release_fn_t release_fn;
    void *context;
    char **choices;
    size_t choice_count;
    bool is_static;
};

#endif /* ARGX_TYPE_INTERNAL_H */
