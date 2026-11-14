/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - value types.
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
 * The string type's conversion function. Every input is accepted, and the
 * value is a pointer to the input itself.
 */
static argx_error_t convert_string(const char *text, void *value, void *context)
{
    (void)context;

    *(const char **)value = text;
    return ARGX_ERROR_NONE;
}

/* The integer type's conversion function. */
static argx_error_t convert_int(const char *text, void *value, void *context)
{
    (void)context;

    return argx_number_parse_int(text, (int64_t *)value);
}

/* The floating-point type's conversion function. */
static argx_error_t convert_float(const char *text, void *value, void *context)
{
    (void)context;

    return argx_number_parse_float(text, (double *)value);
}

/*
 * A choice type's conversion function. context is the value type itself.
 * The value is the index of the choice that exactly matches text.
 */
static argx_error_t convert_choice(const char *text, void *value, void *context)
{
    const argx_type_t *type = (const argx_type_t *)context;
    size_t index;

    for (index = 0; index < type->choice_count; ++index)
    {
        if (strcmp(type->choices[index], text) == 0)
        {
            *(size_t *)value = index;
            return ARGX_ERROR_NONE;
        }
    }

    return ARGX_ERROR_INVALID_VALUE;
}

/*
 * Returns true when any string appears more than once among the count
 * strings of choices.
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
static const argx_type_t string_type = {
    ARGX_TYPE_STRING, "string", NULL, sizeof(const char *), convert_string, NULL, NULL, NULL, 0, true
};

static const argx_type_t int_type = {
    ARGX_TYPE_INT, "integer", NULL, sizeof(int64_t), convert_int, NULL, NULL, NULL, 0, true
};

static const argx_type_t float_type = {
    ARGX_TYPE_FLOAT, "float", NULL, sizeof(double), convert_float, NULL, NULL, NULL, 0, true
};

const argx_type_t *argx_type_string(void)
{
    return &string_type;
}

const argx_type_t *argx_type_int(void)
{
    return &int_type;
}

const argx_type_t *argx_type_float(void)
{
    return &float_type;
}

argx_error_t argx_type_create_choice(const char *const *choices, size_t count, argx_type_t **out)
{
    argx_type_t *type;
    size_t index;

    assert(out != NULL);

    *out = NULL;

    if (count == 0)
    {
        return ARGX_ERROR_INVALID_DEFINITION;
    }

    assert(choices != NULL);
    for (index = 0; index < count; ++index)
    {
        assert(choices[index] != NULL);
    }

    if (has_duplicate(choices, count))
    {
        return ARGX_ERROR_INVALID_DEFINITION;
    }

    /* The size of the array of copies must not overflow. */
    if (count > (SIZE_MAX / sizeof(char *)))
    {
        return ARGX_ERROR_OUT_OF_MEMORY;
    }

    type = (argx_type_t *)argx_malloc(sizeof(*type));
    if (type == NULL)
    {
        return ARGX_ERROR_OUT_OF_MEMORY;
    }

    type->kind = ARGX_TYPE_CHOICE;
    type->name = "choice";
    type->owned_name = NULL;
    type->value_size = sizeof(size_t);
    type->convert_fn = convert_choice;
    type->release_fn = NULL;
    type->context = type;
    type->choice_count = 0;
    type->is_static = false;

    type->choices = (char **)argx_malloc(count * sizeof(char *));
    if (type->choices == NULL)
    {
        argx_type_destroy(type);
        return ARGX_ERROR_OUT_OF_MEMORY;
    }

    /*
     * choice_count is the number of copies made so far, so a failure frees
     * exactly those.
     */
    for (index = 0; index < count; ++index)
    {
        type->choices[index] = argx_strdup(choices[index]);
        if (type->choices[index] == NULL)
        {
            argx_type_destroy(type);
            return ARGX_ERROR_OUT_OF_MEMORY;
        }
        ++type->choice_count;
    }

    *out = type;

    return ARGX_ERROR_NONE;
}

argx_error_t argx_type_create(const char *name, size_t value_size, argx_convert_fn_t convert_fn,
                              argx_release_fn_t release_fn, void *context, argx_type_t **out)
{
    argx_type_t *type;

    assert(out != NULL);

    *out = NULL;

    assert(name != NULL);
    assert(value_size > 0);
    assert(convert_fn != NULL);

    if (name[0] == '\0')
    {
        return ARGX_ERROR_INVALID_NAME;
    }

    type = (argx_type_t *)argx_malloc(sizeof(*type));
    if (type == NULL)
    {
        return ARGX_ERROR_OUT_OF_MEMORY;
    }

    type->owned_name = argx_strdup(name);
    if (type->owned_name == NULL)
    {
        argx_free(type);
        return ARGX_ERROR_OUT_OF_MEMORY;
    }

    type->kind = ARGX_TYPE_CUSTOM;
    type->name = type->owned_name;
    type->value_size = value_size;
    type->convert_fn = convert_fn;
    type->release_fn = release_fn;
    type->context = context;
    type->choices = NULL;
    type->choice_count = 0;
    type->is_static = false;

    *out = type;

    return ARGX_ERROR_NONE;
}

void argx_type_destroy(argx_type_t *type)
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
            argx_free(type->choices[index]);
        }
        argx_free(type->choices);
    }

    /* Only a copied name is freed; the other names are fixed strings. */
    if (type->owned_name != NULL)
    {
        argx_free(type->owned_name);
    }
    argx_free(type);
}

const char *argx_type_name(const argx_type_t *type)
{
    assert(type != NULL);

    return type->name;
}

argx_type_kind_t argx_type_kind(const argx_type_t *type)
{
    assert(type != NULL);

    return type->kind;
}

size_t argx_type_value_size(const argx_type_t *type)
{
    assert(type != NULL);

    return type->value_size;
}

size_t argx_type_choice_count(const argx_type_t *type)
{
    assert(type != NULL);

    return type->choice_count;
}

const char *argx_type_choice_at(const argx_type_t *type, size_t index)
{
    assert(type != NULL);
    assert(index < type->choice_count);

    return type->choices[index];
}

argx_error_t argx_type_convert(const argx_type_t *type, const char *text, void *value)
{
    assert(type != NULL);
    assert(text != NULL);
    assert(value != NULL);

    return type->convert_fn(text, value, type->context);
}

void argx_type_release(const argx_type_t *type, void *value)
{
    assert(type != NULL);
    assert(value != NULL);

    if (type->release_fn != NULL)
    {
        type->release_fn(value, type->context);
    }
}
