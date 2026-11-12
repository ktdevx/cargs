/*
 * CLArgs - value types: how an input string is converted to a value.
 *
 * A value type tells how the string of a command line argument is checked
 * and converted. It has a name, a kind, the size of its values, a function
 * that converts, and, if its values need it, a function that cleans a value
 * up. It tells nothing else: neither when a conversion is requested nor
 * where the value is kept.
 *
 * A value type never changes. So one value type can be used in several
 * places and from several threads at the same time.
 *
 * The built-in value types and the values they write:
 *
 *     kind                  name        type of the value   value
 *     CLARGS_TYPE_STRING    "string"    const char *        the pointer to the input string
 *     CLARGS_TYPE_INT       "integer"   int64_t             the integer
 *     CLARGS_TYPE_FLOAT     "float"     double              the number
 *     CLARGS_TYPE_CHOICE    "choice"    size_t              the index of the choice that matched, from 0
 *     CLARGS_TYPE_CUSTOM    any         any                 what the conversion function of the type writes
 *
 * How a conversion is requested (clargs_type_convert, clargs_type_release):
 *
 * - The caller provides the area the value is written to. The area has at
 *   least clargs_type_value_size() bytes and is aligned for an object of
 *   any type. The area stays the caller's: the library neither allocates
 *   nor frees it.
 * - When the conversion succeeds, the value is in the area. When the caller
 *   has finished with the value, it calls clargs_type_release() for it,
 *   exactly once.
 * - When the conversion fails, the contents of the area are unspecified,
 *   and clargs_type_release() must not be called for it.
 * - The value may refer to the input string. The caller keeps the input
 *   string valid and unchanged until it has released the value.
 *
 * The same calls work for every value type. The caller does not need to
 * know the kind to request a conversion; it needs the kind only to read the
 * value.
 */
#ifndef CLARGS_TYPE_H
#define CLARGS_TYPE_H

#include <clargs/error.h>
#include <clargs/export.h>

#include <stddef.h>

/* A value type. The contents are not public. */
typedef struct clargs_type clargs_type_t;

/*
 * The kind of a value type. It tells the type of the values; see the table
 * at the top of this header.
 *
 * Later versions may add kinds. Write code that may receive a value it does
 * not know: give a switch over clargs_type_kind_t a default label.
 */
typedef enum clargs_type_kind {
    CLARGS_TYPE_STRING,
    CLARGS_TYPE_INT,
    CLARGS_TYPE_FLOAT,
    CLARGS_TYPE_CHOICE,
    CLARGS_TYPE_CUSTOM
} clargs_type_kind_t;

/*
 * The conversion function of a value type. Converts text and writes the
 * value to the area that value points to; the area is as described at the
 * top of this header. context is the user data of the value type.
 *
 * Returns CLARGS_ERROR_NONE on success, CLARGS_ERROR_INVALID_VALUE when the
 * input is not valid, and CLARGS_ERROR_OUT_OF_MEMORY when an allocation
 * fails. Any value other than CLARGS_ERROR_NONE is a failure.
 */
typedef clargs_error_t (*clargs_convert_fn_t)(const char *text, void *value, void *context);

/*
 * The cleanup function of a value type. Cleans up a value that a conversion
 * of the same value type wrote to the area that value points to. It does
 * not free the area itself. context is the user data of the value type.
 */
typedef void (*clargs_release_fn_t)(void *value, void *context);

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Return the built-in value types of the kinds CLARGS_TYPE_STRING,
 * CLARGS_TYPE_INT and CLARGS_TYPE_FLOAT.
 *
 * Each function returns the same pointer on every call, and never NULL. The
 * object is valid until the program ends and never changes. The functions
 * allocate nothing and cannot fail; there is nothing to destroy.
 *
 * - string: takes every input, the empty string included. The value is the
 *   pointer to the input string itself, not a copy.
 * - integer: takes decimal digits with one sign ('+' or '-') in front or
 *   none, when the value is in the range of a signed 64-bit integer.
 *   Nothing else is taken: neither the empty string nor white space nor any
 *   other character.
 * - float: takes the decimal notation, with an exponent or without ("1",
 *   "1.5", ".5", "-1.5e-3"), when the value is finite. The decimal point is
 *   '.', whatever the locale is. Nothing else is taken: neither the empty
 *   string nor white space, nor the spellings of infinity and of
 *   not-a-number, nor the hexadecimal notation.
 *
 * The integer and the float conversion fail with CLARGS_ERROR_INVALID_VALUE.
 * The float conversion of a very long input can also fail with
 * CLARGS_ERROR_OUT_OF_MEMORY.
 */
CLARGS_API const clargs_type_t *clargs_type_string(void);
CLARGS_API const clargs_type_t *clargs_type_int(void);
CLARGS_API const clargs_type_t *clargs_type_float(void);

/*
 * Returns the name of the value type, such as "integer". The result is
 * never NULL. The string is valid as long as the value type is; do not free
 * it and do not modify it.
 *
 * Preconditions: type is not NULL. Checked with assert.
 */
CLARGS_API const char *clargs_type_name(const clargs_type_t *type);

/*
 * Returns the kind of the value type.
 *
 * Preconditions: type is not NULL. Checked with assert.
 */
CLARGS_API clargs_type_kind_t clargs_type_kind(const clargs_type_t *type);

/*
 * Returns the size of a value of the value type in bytes: the least size of
 * the area that is passed to clargs_type_convert(). It is greater than 0.
 *
 * Preconditions: type is not NULL. Checked with assert.
 */
CLARGS_API size_t clargs_type_value_size(const clargs_type_t *type);

/*
 * Converts text with the value type and writes the value to the area that
 * value points to. Calls the conversion function of the value type and
 * returns what it returns: CLARGS_ERROR_NONE on success, and any other
 * value on failure (CLARGS_ERROR_INVALID_VALUE when the input is not
 * valid).
 *
 * After a success, call clargs_type_release() for the value exactly once.
 * After a failure, the contents of the area are unspecified; do not call
 * clargs_type_release() for it. The value may refer to text: keep text
 * valid and unchanged until the value has been released.
 *
 * Preconditions:
 *
 * - type, text and value are not NULL. Checked with assert.
 * - value points to an area of at least clargs_type_value_size(type) bytes
 *   that is aligned for an object of any type. Not checked.
 */
CLARGS_API clargs_error_t clargs_type_convert(const clargs_type_t *type, const char *text, void *value);

/*
 * Cleans up a value that clargs_type_convert() wrote with the same value
 * type. Calls the cleanup function of the value type if it has one, and
 * does nothing otherwise; the built-in value types have none. The area
 * itself is not freed.
 *
 * Preconditions:
 *
 * - type and value are not NULL. Checked with assert.
 * - value points to a value that a successful clargs_type_convert() with
 *   the same value type wrote, and that has not been released yet. Not
 *   checked.
 */
CLARGS_API void clargs_type_release(const clargs_type_t *type, void *value);

#ifdef __cplusplus
}
#endif

#endif /* CLARGS_TYPE_H */
