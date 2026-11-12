/*
 * CLArgs - value types.
 */
#include "type_internal.h"

#include "number.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The conversion function of the string type. Every input is taken, and the
 * value is the pointer to the input itself. */
static clargs_error_t convert_string(const char *text, void *value, void *context)
{
    (void)context;

    *(const char **)value = text;
    return CLARGS_ERROR_NONE;
}

/* The conversion function of the integer type. */
static clargs_error_t convert_int(const char *text, void *value, void *context)
{
    (void)context;

    return clargs_number_parse_int(text, (int64_t *)value);
}

/* The conversion function of the floating-point type. */
static clargs_error_t convert_float(const char *text, void *value, void *context)
{
    (void)context;

    return clargs_number_parse_float(text, (double *)value);
}

/* The built-in value types: fixed objects that are never modified. */
static const clargs_type_t string_type = {
    CLARGS_TYPE_STRING, "string", sizeof(const char *), convert_string, NULL, NULL, NULL, 0, true
};

static const clargs_type_t int_type = {
    CLARGS_TYPE_INT, "integer", sizeof(int64_t), convert_int, NULL, NULL, NULL, 0, true
};

static const clargs_type_t float_type = {
    CLARGS_TYPE_FLOAT, "float", sizeof(double), convert_float, NULL, NULL, NULL, 0, true
};

const clargs_type_t *clargs_type_string(void)
{
    return &string_type;
}

const clargs_type_t *clargs_type_int(void)
{
    return &int_type;
}

const clargs_type_t *clargs_type_float(void)
{
    return &float_type;
}

const char *clargs_type_name(const clargs_type_t *type)
{
    assert(type != NULL);

    return type->name;
}

clargs_type_kind_t clargs_type_kind(const clargs_type_t *type)
{
    assert(type != NULL);

    return type->kind;
}

size_t clargs_type_value_size(const clargs_type_t *type)
{
    assert(type != NULL);

    return type->value_size;
}

clargs_error_t clargs_type_convert(const clargs_type_t *type, const char *text, void *value)
{
    assert(type != NULL);
    assert(text != NULL);
    assert(value != NULL);

    return type->convert_fn(text, value, type->context);
}

void clargs_type_release(const clargs_type_t *type, void *value)
{
    assert(type != NULL);
    assert(value != NULL);

    if (type->release_fn != NULL) {
        type->release_fn(value, type->context);
    }
}
