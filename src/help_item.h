/*
 * CLArgs - internal declarations for the items of the help: how one
 * option or positional of a schema is written.
 *
 * The functions write to a clargs_out_t. The width of an item is the
 * number of all its bytes, and the caller gets it with the same code that
 * writes the item: it writes the item once to a clargs_out_t whose write
 * function only adds up the lengths it is given. The column of an output
 * would not do: a name may hold a newline, behind which the column starts
 * again at 0. No function leaves a blank that is not yet written, so every
 * byte of an item reaches the write function.
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
 * Writes the heading of option, the way it stands in the list of the
 * options. The names come first: "-s, --long" with both names, "-s" with a
 * short name only, and four blanks and "--long" with a long name only, so
 * that the long names of a list stand one below the other. Then, by the
 * kind of the option:
 *
 * - a flag: nothing more.
 * - a negatable flag: the long name is written as "--[no-]long".
 * - a count: "..." right behind the names.
 * - an option with a value: a blank and the display name of the value
 *   between "<" and ">". When no display name is set, the name of the
 *   value type stands in its place.
 *
 * A kind that is not known is written as a flag.
 */
void clargs_help_item_option_heading(clargs_out_t *out, const clargs_option_t *option);

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
