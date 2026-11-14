/*
 * CLine - the help options: the settings of the layout of a help text.
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
    return (value > CLINE_HELP_SETTING_MAX) ? CLINE_HELP_SETTING_MAX : value;
}

cline_error_t cline_help_options_create(cline_help_options_t **out)
{
    cline_help_options_t *options;

    assert(out != NULL);

    *out = NULL;

    options = cline_malloc(sizeof(*options));
    if (options == NULL)
    {
        return CLINE_ERROR_OUT_OF_MEMORY;
    }

    options->width = CLINE_HELP_DEFAULT_WIDTH;
    options->indent = CLINE_HELP_DEFAULT_INDENT;
    options->max_column = CLINE_HELP_DEFAULT_MAX_COLUMN;

    *out = options;
    return CLINE_ERROR_NONE;
}

void cline_help_options_destroy(cline_help_options_t *options)
{
    cline_free(options);
}

void cline_help_options_set_width(cline_help_options_t *options, size_t width)
{
    assert(options != NULL);

    options->width = clamp_setting(width);
}

void cline_help_options_set_indent(cline_help_options_t *options, size_t indent)
{
    assert(options != NULL);

    options->indent = clamp_setting(indent);
}

void cline_help_options_set_max_column(cline_help_options_t *options, size_t column)
{
    assert(options != NULL);

    options->max_column = clamp_setting(column);
}
