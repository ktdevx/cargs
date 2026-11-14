/*
 * CLine - value types.
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
static cline_error_t convert_string(const char *text, void *value, void *context)
{
    (void)context;

    *(const char **)value = text;
    return CLINE_ERROR_NONE;
}

/* The conversion function of the integer type. */
static cline_error_t convert_int(const char *text, void *value, void *context)
{
    (void)context;

    return cline_number_parse_int(text, (int64_t *)value);
}

/* The conversion function of the floating-point type. */
static cline_error_t convert_float(const char *text, void *value, void *context)
{
    (void)context;

    return cline_number_parse_float(text, (double *)value);
}

/*
 * The conversion function of a choice type. context is the value type
 * itself. The value is the index of the choice that is exactly text.
 */
static cline_error_t convert_choice(const char *text, void *value, void *context)
{
    const cline_type_t *type = (const cline_type_t *)context;
    size_t index;

    for (index = 0; index < type->choice_count; ++index)
    {
        if (strcmp(type->choices[index], text) == 0)
        {
            *(size_t *)value = index;
            return CLINE_ERROR_NONE;
        }
    }
    return CLINE_ERROR_INVALID_VALUE;
}

/*
 * Returns true when a string is in the count strings of choices more than
 * once.
 */
static bool has_duplicate(const char *const *choices, size_t count)
{
    size_t first;
    size_t second;

    for (first = 0; first < count; ++first)
    {
        for (second = first + 1; second < count; ++second)
        {
            if (strcmp(choices[first], choices[second]) == 0)
            {
                return true;
            }
        }
    }
    return false;
}

/* The built-in value types: fixed objects that are never modified. */
static const cline_type_t string_type = {
    CLINE_TYPE_STRING, "string", NULL, sizeof(const char *), convert_string, NULL, NULL, NULL, 0, true
};

static const cline_type_t int_type = {
    CLINE_TYPE_INT, "integer", NULL, sizeof(int64_t), convert_int, NULL, NULL, NULL, 0, true
};

static const cline_type_t float_type = {
    CLINE_TYPE_FLOAT, "float", NULL, sizeof(double), convert_float, NULL, NULL, NULL, 0, true
};

const cline_type_t *cline_type_string(void)
{
    return &string_type;
}

const cline_type_t *cline_type_int(void)
{
    return &int_type;
}

const cline_type_t *cline_type_float(void)
{
    return &float_type;
}

cline_error_t cline_type_create_choice(const char *const *choices, size_t count, cline_type_t **out)
{
    cline_type_t *type;
    size_t index;

    assert(out != NULL);

    *out = NULL;

    if (count == 0)
    {
        return CLINE_ERROR_INVALID_DEFINITION;
    }

    assert(choices != NULL);
    for (index = 0; index < count; ++index)
    {
        assert(choices[index] != NULL);
    }

    if (has_duplicate(choices, count))
    {
        return CLINE_ERROR_INVALID_DEFINITION;
    }

    /* The size of the array of the copies must not overflow. */
    if (count > (SIZE_MAX / sizeof(char *)))
    {
        return CLINE_ERROR_OUT_OF_MEMORY;
    }

    type = (cline_type_t *)cline_malloc(sizeof(*type));
    if (type == NULL)
    {
        return CLINE_ERROR_OUT_OF_MEMORY;
    }

    type->kind = CLINE_TYPE_CHOICE;
    type->name = "choice";
    type->owned_name = NULL;
    type->value_size = sizeof(size_t);
    type->convert_fn = convert_choice;
    type->release_fn = NULL;
    type->context = type;
    type->choice_count = 0;
    type->is_static = false;

    type->choices = (char **)cline_malloc(count * sizeof(char *));
    if (type->choices == NULL)
    {
        cline_type_destroy(type);
        return CLINE_ERROR_OUT_OF_MEMORY;
    }

    /*
     * choice_count is the number of the copies made so far, so that a
     * failure frees exactly those.
     */
    for (index = 0; index < count; ++index)
    {
        type->choices[index] = cline_strdup(choices[index]);
        if (type->choices[index] == NULL)
        {
            cline_type_destroy(type);
            return CLINE_ERROR_OUT_OF_MEMORY;
        }
        ++type->choice_count;
    }

    *out = type;
    return CLINE_ERROR_NONE;
}

cline_error_t cline_type_create(const char *name, size_t value_size, cline_convert_fn_t convert_fn,
                                  cline_release_fn_t release_fn, void *context, cline_type_t **out)
{
    cline_type_t *type;

    assert(out != NULL);

    *out = NULL;

    assert(name != NULL);
    assert(value_size > 0);
    assert(convert_fn != NULL);

    if (name[0] == '\0')
    {
        return CLINE_ERROR_INVALID_NAME;
    }

    type = (cline_type_t *)cline_malloc(sizeof(*type));
    if (type == NULL)
    {
        return CLINE_ERROR_OUT_OF_MEMORY;
    }

    type->owned_name = cline_strdup(name);
    if (type->owned_name == NULL)
    {
        cline_free(type);
        return CLINE_ERROR_OUT_OF_MEMORY;
    }

    type->kind = CLINE_TYPE_CUSTOM;
    type->name = type->owned_name;
    type->value_size = value_size;
    type->convert_fn = convert_fn;
    type->release_fn = release_fn;
    type->context = context;
    type->choices = NULL;
    type->choice_count = 0;
    type->is_static = false;

    *out = type;
    return CLINE_ERROR_NONE;
}

void cline_type_destroy(cline_type_t *type)
{
    size_t index;

    if ((type == NULL) || type->is_static)
    {
        return;
    }

    if (type->choices != NULL)
    {
        for (index = 0; index < type->choice_count; ++index)
        {
            cline_free(type->choices[index]);
        }
        cline_free(type->choices);
    }
    /* Only a copied name is freed; the other names are fixed strings. */
    if (type->owned_name != NULL)
    {
        cline_free(type->owned_name);
    }
    cline_free(type);
}

const char *cline_type_name(const cline_type_t *type)
{
    assert(type != NULL);

    return type->name;
}

cline_type_kind_t cline_type_kind(const cline_type_t *type)
{
    assert(type != NULL);

    return type->kind;
}

size_t cline_type_value_size(const cline_type_t *type)
{
    assert(type != NULL);

    return type->value_size;
}

size_t cline_type_choice_count(const cline_type_t *type)
{
    assert(type != NULL);

    return type->choice_count;
}

const char *cline_type_choice_at(const cline_type_t *type, size_t index)
{
    assert(type != NULL);
    assert(index < type->choice_count);

    return type->choices[index];
}

cline_error_t cline_type_convert(const cline_type_t *type, const char *text, void *value)
{
    assert(type != NULL);
    assert(text != NULL);
    assert(value != NULL);

    return type->convert_fn(text, value, type->context);
}

void cline_type_release(const cline_type_t *type, void *value)
{
    assert(type != NULL);
    assert(value != NULL);

    if (type->release_fn != NULL)
    {
        type->release_fn(value, type->context);
    }
}
