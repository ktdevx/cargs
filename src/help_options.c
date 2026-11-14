/*
 * CLArgs - the help options: the settings of the layout of a help text.
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
    return (value > CLARGS_HELP_SETTING_MAX) ? CLARGS_HELP_SETTING_MAX : value;
}

clargs_error_t clargs_help_options_create(clargs_help_options_t **out)
{
    clargs_help_options_t *options;

    assert(out != NULL);

    *out = NULL;

    options = clargs_malloc(sizeof(*options));
    if (options == NULL)
    {
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }

    options->width = CLARGS_HELP_DEFAULT_WIDTH;
    options->indent = CLARGS_HELP_DEFAULT_INDENT;
    options->max_column = CLARGS_HELP_DEFAULT_MAX_COLUMN;

    *out = options;
    return CLARGS_ERROR_NONE;
}

void clargs_help_options_destroy(clargs_help_options_t *options)
{
    clargs_free(options);
}

void clargs_help_options_set_width(clargs_help_options_t *options, size_t width)
{
    assert(options != NULL);

    options->width = clamp_setting(width);
}

void clargs_help_options_set_indent(clargs_help_options_t *options, size_t indent)
{
    assert(options != NULL);

    options->indent = clamp_setting(indent);
}

void clargs_help_options_set_max_column(clargs_help_options_t *options, size_t column)
{
    assert(options != NULL);

    options->max_column = clamp_setting(column);
}
