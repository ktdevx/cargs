/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - value types: how an input string is converted to a value.
 *
 * A value type tells how a command line argument string is checked and
 * converted. It has a name, a kind, a value size, a conversion function
 * and, if its values need it, a cleanup function. It tells nothing else:
 * neither when a conversion is requested nor where the value is kept.
 *
 * A value type never changes, so one value type can be used in several
 * places and from several threads at the same time.
 *
 * The kinds of value types and the values they write:
 *
 *     kind                  name        type of the value   value
 *     ARGX_TYPE_STRING    "string"    const char *        the pointer to the input string
 *     ARGX_TYPE_INT       "integer"   int64_t             the integer
 *     ARGX_TYPE_FLOAT     "float"     double              the number
 *     ARGX_TYPE_CHOICE    "choice"    size_t              the index of the choice that matched, from 0
 *     ARGX_TYPE_CUSTOM    any         any                 what the conversion function of the type writes
 *
 * How a conversion is requested (argx_type_convert, argx_type_release):
 *
 * - The caller provides the area the value is written to. The area has at
 *   least argx_type_value_size() bytes and is aligned for an object of
 *   any type. The area belongs to the caller: the library neither
 *   allocates nor frees it.
 * - When the conversion succeeds, the value is in the area. When the caller
 *   is done with the value, it calls argx_type_release() for it exactly
 *   once.
 * - When the conversion fails, the area's contents are unspecified, and
 *   argx_type_release() must not be called for it.
 * - The value may refer to the input string. The caller keeps the input
 *   string valid and unchanged until it has released the value.
 *
 * The same calls work for every value type. The caller does not need to
 * know the kind to request a conversion; it needs the kind only to read the
 * value.
 */
#ifndef ARGX_TYPE_H
#define ARGX_TYPE_H

#include <argx/error.h>
#include <argx/export.h>

#include <stddef.h>

/* A value type. The contents are not public. */
typedef struct argx_type argx_type_t;

/*
 * The kind of a value type. It tells the C type of its values; see the
 * table at the top of this header.
 *
 * Later versions may add kinds. Write code that may receive a value it does
 * not know: give a switch over argx_type_kind_t a default label.
 */
typedef enum argx_type_kind
{
    ARGX_TYPE_STRING,
    ARGX_TYPE_INT,
    ARGX_TYPE_FLOAT,
    ARGX_TYPE_CHOICE,
    ARGX_TYPE_CUSTOM
} argx_type_kind_t;

/*
 * A value type's conversion function. Converts text and writes the value
 * to the area that value points to. The area is as described at the top of
 * this header. context is the value type's user data.
 *
 * Returns ARGX_ERROR_NONE on success, ARGX_ERROR_INVALID_VALUE when the
 * input is not valid, and ARGX_ERROR_OUT_OF_MEMORY when an allocation
 * fails. Any value other than ARGX_ERROR_NONE is a failure, and
 * argx_type_convert() returns it. A user-written function returns
 * ARGX_ERROR_INVALID_VALUE for input it does not accept.
 *
 * On failure, the function leaves nothing to clean up: the cleanup function
 * is not called for a failed conversion.
 *
 * The function has no side effects: it only writes the value. It is called
 * whenever a conversion is requested. Schema validation requests one for
 * every default value each time it runs.
 */
typedef argx_error_t (*argx_convert_fn_t)(const char *text, void *value, void *context);

/*
 * A value type's cleanup function. Cleans up a value that a conversion
 * with the same value type wrote to the area that value points to. It does
 * not free the area itself. context is the value type's user data.
 *
 * It is called once for each argx_type_release() call, and
 * argx_type_release() is called exactly once for each successful
 * conversion.
 */
typedef void (*argx_release_fn_t)(void *value, void *context);

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Return the built-in value types of kind ARGX_TYPE_STRING, ARGX_TYPE_INT
 * and ARGX_TYPE_FLOAT.
 *
 * Each function returns the same pointer on every call, and never NULL. The
 * object is valid until the program ends and never changes. The functions
 * allocate nothing and cannot fail; there is nothing to destroy.
 *
 * - string: takes any input, including the empty string. The value is a
 *   pointer to the input string itself, not a copy.
 * - integer: takes decimal digits, optionally preceded by one sign ('+' or
 *   '-'), when the value fits in a signed 64-bit integer. Nothing else is
 *   taken: not the empty string, not white space, not any other character.
 * - float: takes decimal notation, with or without an exponent ("1",
 *   "1.5", ".5", "-1.5e-3"), when the value is finite. The decimal point is
 *   '.' regardless of the locale. Nothing else is taken: not the empty
 *   string, not white space, not the spellings of infinity or not-a-number,
 *   and not hexadecimal notation.
 *
 * The integer and float conversions fail with ARGX_ERROR_INVALID_VALUE.
 * The float conversion can also fail with ARGX_ERROR_OUT_OF_MEMORY for a
 * very long input.
 *
 * Preconditions: none.
 */
ARGX_API const argx_type_t *argx_type_string(void);
ARGX_API const argx_type_t *argx_type_int(void);
ARGX_API const argx_type_t *argx_type_float(void);

/*
 * Creates a value type of kind ARGX_TYPE_CHOICE from the count strings in
 * the array choices, and writes it to *out. Its name is "choice".
 *
 * The value type takes only an input that exactly matches one of the
 * choices; the match is case-sensitive. The value is the index of the
 * matching choice, from 0, in array order. The empty string can be a
 * choice.
 *
 * The strings are copied, so the array and its strings may be changed or
 * freed after the call. The caller owns the value type and destroys it with
 * argx_type_destroy().
 *
 * Returns ARGX_ERROR_NONE on success. On failure, nothing is created,
 * nothing stays allocated, and *out is NULL:
 *
 * - ARGX_ERROR_INVALID_DEFINITION: count is 0, or a string appears in the
 *   array more than once.
 * - ARGX_ERROR_OUT_OF_MEMORY: an allocation failed.
 *
 * Preconditions:
 *
 * - out is not NULL. Checked with assert.
 * - If count is not 0, choices and each of its count elements are not NULL.
 *   Checked with assert.
 */
ARGX_API argx_error_t argx_type_create_choice(const char *const *choices, size_t count,
                                              argx_type_t **out);

/*
 * Creates a user-defined value type of kind ARGX_TYPE_CUSTOM and writes it
 * to *out. It works with the same functions as every other value type.
 *
 * - name: the type name, such as "port". It is copied, so the string may
 *   be changed or freed after the call. Any non-empty string is a valid
 *   name.
 * - value_size: the value size in bytes. The area a conversion writes to
 *   has at least this size.
 * - convert_fn: the conversion function. argx_type_convert() calls it
 *   with the input, the area and context, and returns its result
 *   unchanged. It returns ARGX_ERROR_INVALID_VALUE for invalid input; see
 *   argx_convert_fn_t.
 * - release_fn: the cleanup function, or NULL when the values need no
 *   cleanup. argx_type_release() calls it with the area and context.
 * - context: the user data passed to both functions, or NULL. It is only
 *   borrowed: the library neither copies nor frees it. Keep it valid until
 *   the value type has been destroyed.
 *
 * The caller owns the value type and destroys it with
 * argx_type_destroy(), after every schema that uses it has been
 * destroyed. Neither creating nor destroying it calls either function.
 *
 * Returns ARGX_ERROR_NONE on success. On failure, nothing is created,
 * nothing stays allocated, and *out is NULL:
 *
 * - ARGX_ERROR_INVALID_NAME: name is the empty string.
 * - ARGX_ERROR_OUT_OF_MEMORY: an allocation failed.
 *
 * Preconditions:
 *
 * - out is not NULL. Checked with assert.
 * - name is not NULL. Checked with assert.
 * - value_size is greater than 0. Checked with assert.
 * - convert_fn is not NULL. Checked with assert.
 */
ARGX_API argx_error_t argx_type_create(const char *name, size_t value_size,
                                       argx_convert_fn_t convert_fn,
                                       argx_release_fn_t release_fn, void *context,
                                       argx_type_t **out);

/*
 * Destroys a value type created by argx_type_create_choice() or
 * argx_type_create() and frees its memory. Does nothing for NULL or for a
 * built-in value type (one returned by argx_type_string(), argx_type_int()
 * or argx_type_float()).
 *
 * The user data of a user-defined value type is not freed: it belongs to
 * the caller, and it is not used after this call.
 *
 * Preconditions: nothing uses the value type any more. Destroy every schema
 * that uses a value type before the value type itself, because a schema
 * only borrows its value types. Not checked.
 */
ARGX_API void argx_type_destroy(argx_type_t *type);

/*
 * Returns the value type's name, such as "integer". The result is
 * never NULL. The string is valid as long as the value type is; do not free
 * it and do not modify it.
 *
 * Preconditions: type is not NULL. Checked with assert.
 */
ARGX_API const char *argx_type_name(const argx_type_t *type);

/*
 * Returns the value type's kind.
 *
 * Preconditions: type is not NULL. Checked with assert.
 */
ARGX_API argx_type_kind_t argx_type_kind(const argx_type_t *type);

/*
 * Returns the size in bytes of one value of the value type: the minimum
 * size of the area passed to argx_type_convert(). It is greater than 0.
 *
 * Preconditions: type is not NULL. Checked with assert.
 */
ARGX_API size_t argx_type_value_size(const argx_type_t *type);

/*
 * Returns the value type's number of choices. It is 0 for a value type
 * whose kind is not ARGX_TYPE_CHOICE.
 *
 * Preconditions: type is not NULL. Checked with assert.
 */
ARGX_API size_t argx_type_choice_count(const argx_type_t *type);

/*
 * Returns the choice at index, counting from 0. The choices are in the
 * order they were given to argx_type_create_choice(). The result is never
 * NULL. The string is valid as long as the value type is; do not free it
 * and do not modify it.
 *
 * A value type whose kind is not ARGX_TYPE_CHOICE has no choices, so any
 * call for it violates the precondition on index.
 *
 * Preconditions:
 *
 * - type is not NULL. Checked with assert.
 * - index is less than argx_type_choice_count(type). Checked with assert.
 */
ARGX_API const char *argx_type_choice_at(const argx_type_t *type, size_t index);

/*
 * Converts text with the value type and writes the value to the area that
 * value points to. Calls the value type's conversion function and returns
 * its result: ARGX_ERROR_NONE on success, and any other value on failure
 * (ARGX_ERROR_INVALID_VALUE when the input is not valid).
 *
 * After a success, call argx_type_release() for the value exactly once.
 * After a failure, the contents of the area are unspecified; do not call
 * argx_type_release() for it. The value may refer to text: keep text
 * valid and unchanged until the value has been released.
 *
 * Preconditions:
 *
 * - type, text and value are not NULL. Checked with assert.
 * - value points to an area of at least argx_type_value_size(type) bytes
 *   that is aligned for an object of any type. Not checked.
 */
ARGX_API argx_error_t argx_type_convert(const argx_type_t *type, const char *text, void *value);

/*
 * Cleans up a value that argx_type_convert() wrote with the same value
 * type. Calls the value type's cleanup function if it has one, and does
 * nothing otherwise; the built-in value types have none. The area itself
 * is not freed.
 *
 * Preconditions:
 *
 * - type and value are not NULL. Checked with assert.
 * - value points to a value written by a successful argx_type_convert()
 *   with the same value type, and not yet released. Not checked.
 */
ARGX_API void argx_type_release(const argx_type_t *type, void *value);

#ifdef __cplusplus
}
#endif

#endif /* ARGX_TYPE_H */
