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
