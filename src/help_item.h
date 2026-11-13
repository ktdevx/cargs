/*
 * CLArgs - internal declarations for the items of the help: how one
 * option or positional of a schema is written.
 *
 * The functions write to a clargs_out_t. Written to one that only counts
 * the columns, an item gives its width with the same code that writes it.
 *
 * The strings of the schema are written as they are, byte by byte. The
 * functions read the schema only, through its public functions; they
 * change nothing, allocate nothing and know nothing about the help
 * options: the breaking of lines and the indent are up to the caller.
 *
 * Preconditions of every function: no argument is NULL. Checked with
 * assert.
 */
#ifndef CLARGS_HELP_ITEM_H
#define CLARGS_HELP_ITEM_H

#include "out.h"

#include <clargs/schema.h>

/*
 * Writes text, a string of the schema, as it is. Such a string may hold a
 * '\n', which clargs_out_text() does not take: the parts between the
 * newlines are written with clargs_out_text() and each newline with
 * clargs_out_newline(), so the column starts again at 0 after it.
 */
void clargs_help_item_string(clargs_out_t *out, const char *text);

/*
 * Writes the heading of positional, by its arity: "<name>" for exactly one
 * argument, "[name]" for none or one, "[name]..." for none or more and
 * "<name>..." for one or more. An arity that is not known is written as
 * exactly one.
 */
void clargs_help_item_positional_heading(clargs_out_t *out, const clargs_positional_t *positional);

/*
 * Writes option the way a required option stands in a usage line: "--" and
 * the long name, or "-" and the short name when it has no long name, then
 * a blank and the display name of the value between "<" and ">". When no
 * display name is set, the name of the value type stands in its place.
 *
 * Preconditions: option takes a value, CLARGS_OPTION_VALUE. Checked with
 * assert.
 */
void clargs_help_item_usage_option(clargs_out_t *out, const clargs_option_t *option);

#endif /* CLARGS_HELP_ITEM_H */
