/*
 * CLArgs - the items of the help: how one option or positional of a schema
 * is written.
 */
#include "help_item.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

void clargs_help_item_string(clargs_out_t *out, const char *text)
{
    const char *rest = text;

    assert(out != NULL);
    assert(text != NULL);

    for (;;) {
        const char *newline = strchr(rest, '\n');

        if (newline == NULL) {
            clargs_out_string(out, rest);
            return;
        }

        clargs_out_text(out, rest, (size_t)(newline - rest));
        clargs_out_newline(out);
        rest = newline + 1;
    }
}

/* Writes the display name of the value of option between "<" and ">": the
 * value name that was set, or else the name of the value type. option
 * takes a value. */
static void item_value_name(clargs_out_t *out, const clargs_option_t *option)
{
    const char *name = clargs_option_value_name(option);

    if (name == NULL) {
        name = clargs_type_name(clargs_option_type(option));
    }

    clargs_out_text(out, "<", 1);
    clargs_help_item_string(out, name);
    clargs_out_text(out, ">", 1);
}

void clargs_help_item_option_heading(clargs_out_t *out, const clargs_option_t *option)
{
    const char *long_name;
    char short_name;
    bool negatable = false;
    bool count = false;
    bool value = false;

    assert(out != NULL);
    assert(option != NULL);

    switch (clargs_option_kind(option)) {
    case CLARGS_OPTION_NEGATABLE:
        negatable = true;
        break;
    case CLARGS_OPTION_COUNT:
        count = true;
        break;
    case CLARGS_OPTION_VALUE:
        value = true;
        break;
    case CLARGS_OPTION_FLAG:
    default:
        /* A kind of a later version is written in the simplest form. */
        break;
    }

    long_name = clargs_option_long_name(option);
    short_name = clargs_option_short_name(option);

    /* The blanks belong to the heading: they are written with the text,
     * not remembered, so they count for its width. */
    if (short_name != '\0') {
        clargs_out_text(out, "-", 1);
        clargs_out_text(out, &short_name, 1);
        if (long_name != NULL) {
            clargs_out_text(out, ", ", 2);
        }
    } else {
        clargs_out_text(out, "    ", 4);
    }

    if (long_name != NULL) {
        clargs_out_text(out, "--", 2);
        if (negatable) {
            clargs_out_text(out, "[no-]", 5);
        }
        clargs_help_item_string(out, long_name);
    }

    if (count) {
        clargs_out_text(out, "...", 3);
    }

    if (value) {
        clargs_out_text(out, " ", 1);
        item_value_name(out, option);
    }
}

void clargs_help_item_positional_heading(clargs_out_t *out, const clargs_positional_t *positional)
{
    bool required = true;
    bool repeated = false;

    assert(out != NULL);
    assert(positional != NULL);

    switch (clargs_positional_arity(positional)) {
    case CLARGS_ARITY_OPTIONAL:
        required = false;
        break;
    case CLARGS_ARITY_ZERO_OR_MORE:
        required = false;
        repeated = true;
        break;
    case CLARGS_ARITY_ONE_OR_MORE:
        repeated = true;
        break;
    case CLARGS_ARITY_REQUIRED:
    default:
        /* An arity of a later version is written in the simplest form. */
        break;
    }

    clargs_out_text(out, required ? "<" : "[", 1);
    clargs_help_item_string(out, clargs_positional_name(positional));
    clargs_out_text(out, required ? ">" : "]", 1);
    if (repeated) {
        clargs_out_text(out, "...", 3);
    }
}

void clargs_help_item_usage_option(clargs_out_t *out, const clargs_option_t *option)
{
    const char *long_name;

    assert(out != NULL);
    assert(option != NULL);
    assert(clargs_option_kind(option) == CLARGS_OPTION_VALUE);

    long_name = clargs_option_long_name(option);
    if (long_name != NULL) {
        clargs_out_text(out, "--", 2);
        clargs_help_item_string(out, long_name);
    } else {
        char short_name = clargs_option_short_name(option);

        clargs_out_text(out, "-", 1);
        clargs_out_text(out, &short_name, 1);
    }

    /* The blank belongs to the item: it is written with the text, not
     * remembered as a blank between two items. */
    clargs_out_text(out, " ", 1);
    item_value_name(out, option);
}

clargs_error_t clargs_help_item_text_append(clargs_vec_t *text, const char *part, size_t length)
{
    size_t index;

    assert(text != NULL);
    assert(text->item_size == 1);
    assert(part != NULL || length == 0);

    for (index = 0; index < length; ++index) {
        clargs_error_t error = clargs_vec_push(text, part + index);

        if (error != CLARGS_ERROR_NONE) {
            return error;
        }
    }
    return CLARGS_ERROR_NONE;
}

/* Appends the string part to text. */
static clargs_error_t item_text_string(clargs_vec_t *text, const char *part)
{
    return clargs_help_item_text_append(text, part, strlen(part));
}

/* Starts an annotation at the end of text: appends the blank that
 * separates it from what stands before, unless text is empty or ends with
 * a newline, and then opening, the start of the annotation. */
static clargs_error_t item_note_open(clargs_vec_t *text, const char *opening)
{
    if (text->count != 0 && *(const char *)clargs_vec_at(text, text->count - 1) != '\n') {
        clargs_error_t error = clargs_help_item_text_append(text, " ", 1);

        if (error != CLARGS_ERROR_NONE) {
            return error;
        }
    }
    return item_text_string(text, opening);
}

/* Appends the annotation of the choices of type, "[choices: a, b, c]",
 * when type is of the kind with choices. */
static clargs_error_t item_choices_note(clargs_vec_t *text, const clargs_type_t *type)
{
    clargs_error_t error;
    size_t count;
    size_t index;

    switch (clargs_type_kind(type)) {
    case CLARGS_TYPE_CHOICE:
        break;
    case CLARGS_TYPE_STRING:
    case CLARGS_TYPE_INT:
    case CLARGS_TYPE_FLOAT:
    case CLARGS_TYPE_CUSTOM:
    default:
        /* A kind of a later version is taken as one without choices. */
        return CLARGS_ERROR_NONE;
    }

    error = item_note_open(text, "[choices: ");
    count = clargs_type_choice_count(type);
    for (index = 0; index < count && error == CLARGS_ERROR_NONE; ++index) {
        if (index != 0) {
            error = clargs_help_item_text_append(text, ", ", 2);
        }
        if (error == CLARGS_ERROR_NONE) {
            error = item_text_string(text, clargs_type_choice_at(type, index));
        }
    }
    if (error == CLARGS_ERROR_NONE) {
        error = clargs_help_item_text_append(text, "]", 1);
    }
    return error;
}

/* Appends the annotation of a default value, "[default: VALUE]", with the
 * string value as it is. */
static clargs_error_t item_default_note(clargs_vec_t *text, const char *value)
{
    clargs_error_t error = item_note_open(text, "[default: ");

    if (error == CLARGS_ERROR_NONE) {
        error = item_text_string(text, value);
    }
    if (error == CLARGS_ERROR_NONE) {
        error = clargs_help_item_text_append(text, "]", 1);
    }
    return error;
}

/* Appends the annotations of an option with a value or of a positional:
 * the choices of type, then default_value when it is not NULL. */
static clargs_error_t item_value_notes(clargs_vec_t *text, const clargs_type_t *type,
                                       const char *default_value)
{
    clargs_error_t error = item_choices_note(text, type);

    if (error == CLARGS_ERROR_NONE && default_value != NULL) {
        error = item_default_note(text, default_value);
    }
    return error;
}

clargs_error_t clargs_help_item_option_notes(clargs_vec_t *text, const clargs_option_t *option)
{
    clargs_error_t error = CLARGS_ERROR_NONE;

    assert(text != NULL);
    assert(text->item_size == 1);
    assert(option != NULL);

    switch (clargs_option_kind(option)) {
    case CLARGS_OPTION_VALUE:
        error = item_value_notes(text, clargs_option_type(option), clargs_option_default(option));
        if (error == CLARGS_ERROR_NONE && clargs_option_is_required(option)) {
            error = item_note_open(text, "[required]");
        }
        break;
    case CLARGS_OPTION_NEGATABLE:
        if (clargs_option_initial(option)) {
            error = item_note_open(text, "[default: on]");
        }
        break;
    case CLARGS_OPTION_FLAG:
    case CLARGS_OPTION_COUNT:
    default:
        /* A kind of a later version has no annotation. */
        break;
    }
    return error;
}

clargs_error_t clargs_help_item_positional_notes(clargs_vec_t *text,
                                                 const clargs_positional_t *positional)
{
    assert(text != NULL);
    assert(text->item_size == 1);
    assert(positional != NULL);

    return item_value_notes(text, clargs_positional_type(positional),
                            clargs_positional_default(positional));
}
