/*
 * SPDX-FileCopyrightText: 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - the help options: the settings of the layout of a help text.
 */
#include "help_internal.h"

#include "alloc_internal.h"

#include <assert.h>

/*
 * The value that is kept for a setting: value, or the limit if value is
 * greater.
 */
static size_t clamp_setting(size_t value)
{
    return (value > ARGX_HELP_SETTING_MAX) ? ARGX_HELP_SETTING_MAX : value;
}

argx_error_t argx_help_options_create(argx_help_options_t **out)
{
    argx_help_options_t *options;

    assert(out != NULL);

    *out = NULL;

    options = argx_malloc(sizeof(*options));
    if (options == NULL)
    {
        return ARGX_ERROR_OUT_OF_MEMORY;
    }

    options->width = ARGX_HELP_DEFAULT_WIDTH;
    options->indent = ARGX_HELP_DEFAULT_INDENT;
    options->max_column = ARGX_HELP_DEFAULT_MAX_COLUMN;

    *out = options;
    return ARGX_ERROR_NONE;
}

void argx_help_options_destroy(argx_help_options_t *options)
{
    argx_free(options);
}

void argx_help_options_set_width(argx_help_options_t *options, size_t width)
{
    assert(options != NULL);

    options->width = clamp_setting(width);
}

void argx_help_options_set_indent(argx_help_options_t *options, size_t indent)
{
    assert(options != NULL);

    options->indent = clamp_setting(indent);
}

void argx_help_options_set_max_column(argx_help_options_t *options, size_t column)
{
    assert(options != NULL);

    options->max_column = clamp_setting(column);
}
