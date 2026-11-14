/*
 * ArgX - the items of the help: how one option or positional of a schema
 * is written.
 */
#include "help_item.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

void argx_help_item_string(argx_out_t *out, const char *text)
{
    const char *rest = text;

    assert(out != NULL);
    assert(text != NULL);

    for (;;)
    {
        const char *newline = strchr(rest, '\n');

        if (newline == NULL)
        {
            argx_out_string(out, rest);
            return;
        }

        argx_out_text(out, rest, (size_t)(newline - rest));
        argx_out_newline(out);
        rest = newline + 1;
    }
}

/*
 * Writes the display name of the value of option between "<" and ">": the
 * value name that was set, or else the name of the value type. option
 * takes a value.
 */
static void item_value_name(argx_out_t *out, const argx_option_t *option)
{
    const char *name = argx_option_value_name(option);

    if (name == NULL)
    {
        name = argx_type_name(argx_option_type(option));
    }

    argx_out_text(out, "<", 1);
    argx_help_item_string(out, name);
    argx_out_text(out, ">", 1);
}

void argx_help_item_option_heading(argx_out_t *out, const argx_option_t *option)
{
    const char *long_name;
    char short_name;
    bool negatable = false;
    bool count = false;
    bool value = false;

    assert(out != NULL);
    assert(option != NULL);

    switch (argx_option_kind(option))
    {
    case ARGX_OPTION_NEGATABLE:
        negatable = true;
        break;
    case ARGX_OPTION_COUNT:
        count = true;
        break;
    case ARGX_OPTION_VALUE:
        value = true;
        break;
    case ARGX_OPTION_FLAG:
    default:
        /* A kind of a later version is written in the simplest form. */
        break;
    }

    long_name = argx_option_long_name(option);
    short_name = argx_option_short_name(option);

    /*
     * The blanks belong to the heading: they are written with the text,
     * not remembered, so they count for its width.
     */
    if (short_name != '\0')
    {
        argx_out_text(out, "-", 1);
        argx_out_text(out, &short_name, 1);
        if (long_name != NULL)
        {
            argx_out_text(out, ", ", 2);
        }
    }
    else
    {
        argx_out_text(out, "    ", 4);
    }

    if (long_name != NULL)
    {
        argx_out_text(out, "--", 2);
        if (negatable)
        {
            argx_out_text(out, "[no-]", 5);
        }
        argx_help_item_string(out, long_name);
    }

    if (count)
    {
        argx_out_text(out, "...", 3);
    }

    if (value)
    {
        argx_out_text(out, " ", 1);
        item_value_name(out, option);
    }
}

void argx_help_item_positional_heading(argx_out_t *out, const argx_positional_t *positional)
{
    bool required = true;
    bool repeated = false;

    assert(out != NULL);
    assert(positional != NULL);

    switch (argx_positional_arity(positional))
    {
    case ARGX_ARITY_OPTIONAL:
        required = false;
        break;
    case ARGX_ARITY_ZERO_OR_MORE:
        required = false;
        repeated = true;
        break;
    case ARGX_ARITY_ONE_OR_MORE:
        repeated = true;
        break;
    case ARGX_ARITY_REQUIRED:
    default:
        /* An arity of a later version is written in the simplest form. */
        break;
    }

    argx_out_text(out, required ? "<" : "[", 1);
    argx_help_item_string(out, argx_positional_name(positional));
    argx_out_text(out, required ? ">" : "]", 1);
    if (repeated)
    {
        argx_out_text(out, "...", 3);
    }
}

void argx_help_item_usage_option(argx_out_t *out, const argx_option_t *option)
{
    const char *long_name;

    assert(out != NULL);
    assert(option != NULL);
    assert(argx_option_kind(option) == ARGX_OPTION_VALUE);

    long_name = argx_option_long_name(option);
    if (long_name != NULL)
    {
        argx_out_text(out, "--", 2);
        argx_help_item_string(out, long_name);
    }
    else
    {
        char short_name = argx_option_short_name(option);

        argx_out_text(out, "-", 1);
        argx_out_text(out, &short_name, 1);
    }

    /*
     * The blank belongs to the item: it is written with the text, not
     * remembered as a blank between two items.
     */
    argx_out_text(out, " ", 1);
    item_value_name(out, option);
}

argx_error_t argx_help_item_text_append(argx_vec_t *text, const char *part, size_t length)
{
    size_t index;

    assert(text != NULL);
    assert(text->item_size == 1);
    assert((part != NULL) || (length == 0));

    for (index = 0; index < length; ++index)
    {
        argx_error_t error = argx_vec_push(text, part + index);

        if (error != ARGX_ERROR_NONE)
        {
            return error;
        }
    }
    return ARGX_ERROR_NONE;
}

/* Appends the string part to text. */
static argx_error_t item_text_string(argx_vec_t *text, const char *part)
{
    return argx_help_item_text_append(text, part, strlen(part));
}

/*
 * Starts an annotation at the end of text: appends the blank that
 * separates it from what stands before, unless text is empty or ends with
 * a newline, and then opening, the start of the annotation.
 */
static argx_error_t item_note_open(argx_vec_t *text, const char *opening)
{
    if ((text->count != 0) && (*(const char *)argx_vec_at(text, text->count - 1) != '\n'))
    {
        argx_error_t error = argx_help_item_text_append(text, " ", 1);

        if (error != ARGX_ERROR_NONE)
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
static argx_error_t item_choices_note(argx_vec_t *text, const argx_type_t *type)
{
    argx_error_t error;
    size_t count;
    size_t index;

    switch (argx_type_kind(type))
    {
    case ARGX_TYPE_CHOICE:
        break;
    case ARGX_TYPE_STRING:
    case ARGX_TYPE_INT:
    case ARGX_TYPE_FLOAT:
    case ARGX_TYPE_CUSTOM:
    default:
        /* A kind of a later version is taken as one without choices. */
        return ARGX_ERROR_NONE;
    }

    error = item_note_open(text, "[choices: ");
    count = argx_type_choice_count(type);
    for (index = 0; (index < count) && (error == ARGX_ERROR_NONE); ++index)
    {
        if (index != 0)
        {
            error = argx_help_item_text_append(text, ", ", 2);
        }
        if (error == ARGX_ERROR_NONE)
        {
            error = item_text_string(text, argx_type_choice_at(type, index));
        }
    }
    if (error == ARGX_ERROR_NONE)
    {
        error = argx_help_item_text_append(text, "]", 1);
    }
    return error;
}

/*
 * Appends the annotation of a default value, "[default: VALUE]", with the
 * string value as it is.
 */
static argx_error_t item_default_note(argx_vec_t *text, const char *value)
{
    argx_error_t error = item_note_open(text, "[default: ");

    if (error == ARGX_ERROR_NONE)
    {
        error = item_text_string(text, value);
    }
    if (error == ARGX_ERROR_NONE)
    {
        error = argx_help_item_text_append(text, "]", 1);
    }
    return error;
}

/*
 * Appends the annotations of an option with a value or of a positional:
 * the choices of type, then default_value when it is not NULL.
 */
static argx_error_t item_value_notes(argx_vec_t *text, const argx_type_t *type,
                                       const char *default_value)
{
    argx_error_t error = item_choices_note(text, type);

    if ((error == ARGX_ERROR_NONE) && (default_value != NULL))
    {
        error = item_default_note(text, default_value);
    }
    return error;
}

argx_error_t argx_help_item_option_notes(argx_vec_t *text, const argx_option_t *option)
{
    argx_error_t error = ARGX_ERROR_NONE;

    assert(text != NULL);
    assert(text->item_size == 1);
    assert(option != NULL);

    switch (argx_option_kind(option))
    {
    case ARGX_OPTION_VALUE:
        error = item_value_notes(text, argx_option_type(option), argx_option_default(option));
        if ((error == ARGX_ERROR_NONE) && argx_option_is_required(option))
        {
            error = item_note_open(text, "[required]");
        }
        break;
    case ARGX_OPTION_NEGATABLE:
        if (argx_option_initial(option))
        {
            error = item_note_open(text, "[default: on]");
        }
        break;
    case ARGX_OPTION_FLAG:
    case ARGX_OPTION_COUNT:
    default:
        /* A kind of a later version has no annotation. */
        break;
    }
    return error;
}

argx_error_t argx_help_item_positional_notes(argx_vec_t *text,
                                                 const argx_positional_t *positional)
{
    assert(text != NULL);
    assert(text->item_size == 1);
    assert(positional != NULL);

    return item_value_notes(text, argx_positional_type(positional),
                            argx_positional_default(positional));
}
