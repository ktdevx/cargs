/*
 * CLine - the items of the help: how one option or positional of a schema
 * is written.
 */
#include "help_item.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

void cline_help_item_string(cline_out_t *out, const char *text)
{
    const char *rest = text;

    assert(out != NULL);
    assert(text != NULL);

    for (;;)
    {
        const char *newline = strchr(rest, '\n');

        if (newline == NULL)
        {
            cline_out_string(out, rest);
            return;
        }

        cline_out_text(out, rest, (size_t)(newline - rest));
        cline_out_newline(out);
        rest = newline + 1;
    }
}

/*
 * Writes the display name of the value of option between "<" and ">": the
 * value name that was set, or else the name of the value type. option
 * takes a value.
 */
static void item_value_name(cline_out_t *out, const cline_option_t *option)
{
    const char *name = cline_option_value_name(option);

    if (name == NULL)
    {
        name = cline_type_name(cline_option_type(option));
    }

    cline_out_text(out, "<", 1);
    cline_help_item_string(out, name);
    cline_out_text(out, ">", 1);
}

void cline_help_item_option_heading(cline_out_t *out, const cline_option_t *option)
{
    const char *long_name;
    char short_name;
    bool negatable = false;
    bool count = false;
    bool value = false;

    assert(out != NULL);
    assert(option != NULL);

    switch (cline_option_kind(option))
    {
    case CLINE_OPTION_NEGATABLE:
        negatable = true;
        break;
    case CLINE_OPTION_COUNT:
        count = true;
        break;
    case CLINE_OPTION_VALUE:
        value = true;
        break;
    case CLINE_OPTION_FLAG:
    default:
        /* A kind of a later version is written in the simplest form. */
        break;
    }

    long_name = cline_option_long_name(option);
    short_name = cline_option_short_name(option);

    /*
     * The blanks belong to the heading: they are written with the text,
     * not remembered, so they count for its width.
     */
    if (short_name != '\0')
    {
        cline_out_text(out, "-", 1);
        cline_out_text(out, &short_name, 1);
        if (long_name != NULL)
        {
            cline_out_text(out, ", ", 2);
        }
    }
    else
    {
        cline_out_text(out, "    ", 4);
    }

    if (long_name != NULL)
    {
        cline_out_text(out, "--", 2);
        if (negatable)
        {
            cline_out_text(out, "[no-]", 5);
        }
        cline_help_item_string(out, long_name);
    }

    if (count)
    {
        cline_out_text(out, "...", 3);
    }

    if (value)
    {
        cline_out_text(out, " ", 1);
        item_value_name(out, option);
    }
}

void cline_help_item_positional_heading(cline_out_t *out, const cline_positional_t *positional)
{
    bool required = true;
    bool repeated = false;

    assert(out != NULL);
    assert(positional != NULL);

    switch (cline_positional_arity(positional))
    {
    case CLINE_ARITY_OPTIONAL:
        required = false;
        break;
    case CLINE_ARITY_ZERO_OR_MORE:
        required = false;
        repeated = true;
        break;
    case CLINE_ARITY_ONE_OR_MORE:
        repeated = true;
        break;
    case CLINE_ARITY_REQUIRED:
    default:
        /* An arity of a later version is written in the simplest form. */
        break;
    }

    cline_out_text(out, required ? "<" : "[", 1);
    cline_help_item_string(out, cline_positional_name(positional));
    cline_out_text(out, required ? ">" : "]", 1);
    if (repeated)
    {
        cline_out_text(out, "...", 3);
    }
}

void cline_help_item_usage_option(cline_out_t *out, const cline_option_t *option)
{
    const char *long_name;

    assert(out != NULL);
    assert(option != NULL);
    assert(cline_option_kind(option) == CLINE_OPTION_VALUE);

    long_name = cline_option_long_name(option);
    if (long_name != NULL)
    {
        cline_out_text(out, "--", 2);
        cline_help_item_string(out, long_name);
    }
    else
    {
        char short_name = cline_option_short_name(option);

        cline_out_text(out, "-", 1);
        cline_out_text(out, &short_name, 1);
    }

    /*
     * The blank belongs to the item: it is written with the text, not
     * remembered as a blank between two items.
     */
    cline_out_text(out, " ", 1);
    item_value_name(out, option);
}

cline_error_t cline_help_item_text_append(cline_vec_t *text, const char *part, size_t length)
{
    size_t index;

    assert(text != NULL);
    assert(text->item_size == 1);
    assert((part != NULL) || (length == 0));

    for (index = 0; index < length; ++index)
    {
        cline_error_t error = cline_vec_push(text, part + index);

        if (error != CLINE_ERROR_NONE)
        {
            return error;
        }
    }
    return CLINE_ERROR_NONE;
}

/* Appends the string part to text. */
static cline_error_t item_text_string(cline_vec_t *text, const char *part)
{
    return cline_help_item_text_append(text, part, strlen(part));
}

/*
 * Starts an annotation at the end of text: appends the blank that
 * separates it from what stands before, unless text is empty or ends with
 * a newline, and then opening, the start of the annotation.
 */
static cline_error_t item_note_open(cline_vec_t *text, const char *opening)
{
    if ((text->count != 0) && (*(const char *)cline_vec_at(text, text->count - 1) != '\n'))
    {
        cline_error_t error = cline_help_item_text_append(text, " ", 1);

        if (error != CLINE_ERROR_NONE)
        {
            return error;
        }
    }
    return item_text_string(text, opening);
}

/*
 * Appends the annotation of the choices of type, "[choices: a, b, c]",
 * when type is of the kind with choices.
 */
static cline_error_t item_choices_note(cline_vec_t *text, const cline_type_t *type)
{
    cline_error_t error;
    size_t count;
    size_t index;

    switch (cline_type_kind(type))
    {
    case CLINE_TYPE_CHOICE:
        break;
    case CLINE_TYPE_STRING:
    case CLINE_TYPE_INT:
    case CLINE_TYPE_FLOAT:
    case CLINE_TYPE_CUSTOM:
    default:
        /* A kind of a later version is taken as one without choices. */
        return CLINE_ERROR_NONE;
    }

    error = item_note_open(text, "[choices: ");
    count = cline_type_choice_count(type);
    for (index = 0; (index < count) && (error == CLINE_ERROR_NONE); ++index)
    {
        if (index != 0)
        {
            error = cline_help_item_text_append(text, ", ", 2);
        }
        if (error == CLINE_ERROR_NONE)
        {
            error = item_text_string(text, cline_type_choice_at(type, index));
        }
    }
    if (error == CLINE_ERROR_NONE)
    {
        error = cline_help_item_text_append(text, "]", 1);
    }
    return error;
}

/*
 * Appends the annotation of a default value, "[default: VALUE]", with the
 * string value as it is.
 */
static cline_error_t item_default_note(cline_vec_t *text, const char *value)
{
    cline_error_t error = item_note_open(text, "[default: ");

    if (error == CLINE_ERROR_NONE)
    {
        error = item_text_string(text, value);
    }
    if (error == CLINE_ERROR_NONE)
    {
        error = cline_help_item_text_append(text, "]", 1);
    }
    return error;
}

/*
 * Appends the annotations of an option with a value or of a positional:
 * the choices of type, then default_value when it is not NULL.
 */
static cline_error_t item_value_notes(cline_vec_t *text, const cline_type_t *type,
                                       const char *default_value)
{
    cline_error_t error = item_choices_note(text, type);

    if ((error == CLINE_ERROR_NONE) && (default_value != NULL))
    {
        error = item_default_note(text, default_value);
    }
    return error;
}

cline_error_t cline_help_item_option_notes(cline_vec_t *text, const cline_option_t *option)
{
    cline_error_t error = CLINE_ERROR_NONE;

    assert(text != NULL);
    assert(text->item_size == 1);
    assert(option != NULL);

    switch (cline_option_kind(option))
    {
    case CLINE_OPTION_VALUE:
        error = item_value_notes(text, cline_option_type(option), cline_option_default(option));
        if ((error == CLINE_ERROR_NONE) && cline_option_is_required(option))
        {
            error = item_note_open(text, "[required]");
        }
        break;
    case CLINE_OPTION_NEGATABLE:
        if (cline_option_initial(option))
        {
            error = item_note_open(text, "[default: on]");
        }
        break;
    case CLINE_OPTION_FLAG:
    case CLINE_OPTION_COUNT:
    default:
        /* A kind of a later version has no annotation. */
        break;
    }
    return error;
}

cline_error_t cline_help_item_positional_notes(cline_vec_t *text,
                                                 const cline_positional_t *positional)
{
    assert(text != NULL);
    assert(text->item_size == 1);
    assert(positional != NULL);

    return item_value_notes(text, cline_positional_type(positional),
                            cline_positional_default(positional));
}
