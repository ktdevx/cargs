/*
 * CLArgs - value types.
 */
#include "type_internal.h"

#include "alloc_internal.h"
#include "number.h"
#include "str.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * The conversion function of the string type. Every input is taken, and the
 * value is the pointer to the input itself.
 */
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

/*
 * The conversion function of a choice type. context is the value type
 * itself. The value is the index of the choice that is exactly text.
 */
static clargs_error_t convert_choice(const char *text, void *value, void *context)
{
    const clargs_type_t *type = (const clargs_type_t *)context;
    size_t index;

    for (index = 0; index < type->choice_count; ++index) {
        if (strcmp(type->choices[index], text) == 0) {
            *(size_t *)value = index;
            return CLARGS_ERROR_NONE;
        }
    }
    return CLARGS_ERROR_INVALID_VALUE;
}

/*
 * Returns true when a string is in the count strings of choices more than
 * once.
 */
static bool has_duplicate(const char *const *choices, size_t count)
{
    size_t first;
    size_t second;

    for (first = 0; first < count; ++first) {
        for (second = first + 1; second < count; ++second) {
            if (strcmp(choices[first], choices[second]) == 0) {
                return true;
            }
        }
    }
    return false;
}

/* The built-in value types: fixed objects that are never modified. */
static const clargs_type_t string_type = {
    CLARGS_TYPE_STRING, "string", NULL, sizeof(const char *), convert_string, NULL, NULL, NULL, 0, true
};

static const clargs_type_t int_type = {
    CLARGS_TYPE_INT, "integer", NULL, sizeof(int64_t), convert_int, NULL, NULL, NULL, 0, true
};

static const clargs_type_t float_type = {
    CLARGS_TYPE_FLOAT, "float", NULL, sizeof(double), convert_float, NULL, NULL, NULL, 0, true
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

clargs_error_t clargs_type_create_choice(const char *const *choices, size_t count, clargs_type_t **out)
{
    clargs_type_t *type;
    size_t index;

    assert(out != NULL);

    *out = NULL;

    if (count == 0) {
        return CLARGS_ERROR_INVALID_DEFINITION;
    }

    assert(choices != NULL);
    for (index = 0; index < count; ++index) {
        assert(choices[index] != NULL);
    }

    if (has_duplicate(choices, count)) {
        return CLARGS_ERROR_INVALID_DEFINITION;
    }

    /* The size of the array of the copies must not overflow. */
    if (count > SIZE_MAX / sizeof(char *)) {
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }

    type = (clargs_type_t *)clargs_malloc(sizeof(*type));
    if (type == NULL) {
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }

    type->kind = CLARGS_TYPE_CHOICE;
    type->name = "choice";
    type->owned_name = NULL;
    type->value_size = sizeof(size_t);
    type->convert_fn = convert_choice;
    type->release_fn = NULL;
    type->context = type;
    type->choice_count = 0;
    type->is_static = false;

    type->choices = (char **)clargs_malloc(count * sizeof(char *));
    if (type->choices == NULL) {
        clargs_type_destroy(type);
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }

    /*
     * choice_count is the number of the copies made so far, so that a
     * failure frees exactly those.
     */
    for (index = 0; index < count; ++index) {
        type->choices[index] = clargs_strdup(choices[index]);
        if (type->choices[index] == NULL) {
            clargs_type_destroy(type);
            return CLARGS_ERROR_OUT_OF_MEMORY;
        }
        ++type->choice_count;
    }

    *out = type;
    return CLARGS_ERROR_NONE;
}

clargs_error_t clargs_type_create(const char *name, size_t value_size, clargs_convert_fn_t convert_fn,
                                  clargs_release_fn_t release_fn, void *context, clargs_type_t **out)
{
    clargs_type_t *type;

    assert(out != NULL);

    *out = NULL;

    assert(name != NULL);
    assert(value_size > 0);
    assert(convert_fn != NULL);

    if (name[0] == '\0') {
        return CLARGS_ERROR_INVALID_NAME;
    }

    type = (clargs_type_t *)clargs_malloc(sizeof(*type));
    if (type == NULL) {
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }

    type->owned_name = clargs_strdup(name);
    if (type->owned_name == NULL) {
        clargs_free(type);
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }

    type->kind = CLARGS_TYPE_CUSTOM;
    type->name = type->owned_name;
    type->value_size = value_size;
    type->convert_fn = convert_fn;
    type->release_fn = release_fn;
    type->context = context;
    type->choices = NULL;
    type->choice_count = 0;
    type->is_static = false;

    *out = type;
    return CLARGS_ERROR_NONE;
}

void clargs_type_destroy(clargs_type_t *type)
{
    size_t index;

    if (type == NULL || type->is_static) {
        return;
    }

    if (type->choices != NULL) {
        for (index = 0; index < type->choice_count; ++index) {
            clargs_free(type->choices[index]);
        }
        clargs_free(type->choices);
    }
    /* Only a copied name is freed; the other names are fixed strings. */
    if (type->owned_name != NULL) {
        clargs_free(type->owned_name);
    }
    clargs_free(type);
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

size_t clargs_type_choice_count(const clargs_type_t *type)
{
    assert(type != NULL);

    return type->choice_count;
}

const char *clargs_type_choice_at(const clargs_type_t *type, size_t index)
{
    assert(type != NULL);
    assert(index < type->choice_count);

    return type->choices[index];
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
