/*
 * CLine - internal declarations for the items of the help: how one
 * option or positional of a schema is written.
 *
 * The headings and the items of a usage line are written to a
 * cline_out_t. The width of an item is the number of all its bytes, and
 * the caller gets it with the same code that writes the item: it writes
 * the item once to a cline_out_t whose write function only adds up the
 * lengths it is given. The column of an output
 * would not do: a name may hold a newline, behind which the column starts
 * again at 0. No function leaves a blank that is not yet written, so every
 * byte of an item reaches the write function.
 *
 * The annotations of an item are not written but appended to the cell
 * text: the description of one item and its annotations, as one string
 * that the caller then breaks into lines. The cell text is a cline_vec_t
 * of bytes. Only the functions that append to it allocate.
 *
 * The strings of the schema are written as they are, byte by byte. The
 * functions read the schema only, through its public functions; they
 * change nothing and know nothing about the help options: the breaking of
 * lines and the indent are up to the caller.
 *
 * Preconditions of every function: no argument is NULL. Checked with
 * assert.
 */
#ifndef CLINE_HELP_ITEM_H
#define CLINE_HELP_ITEM_H

#include "out.h"
#include "vec.h"

#include <cline/schema.h>

#include <stddef.h>

/*
 * Writes text, a string of the schema, as it is. Such a string may hold a
 * '\n', which cline_out_text() does not take: the parts between the
 * newlines are written with cline_out_text() and each newline with
 * cline_out_newline(), so the column starts again at 0 after it.
 */
void cline_help_item_string(cline_out_t *out, const char *text);

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
void cline_help_item_option_heading(cline_out_t *out, const cline_option_t *option);

/*
 * Writes the heading of positional, by its arity: "<name>" for exactly one
 * argument, "[name]" for none or one, "[name]..." for none or more and
 * "<name>..." for one or more. An arity that is not known is written as
 * exactly one.
 */
void cline_help_item_positional_heading(cline_out_t *out, const cline_positional_t *positional);

/*
 * Writes option the way a required option stands in a usage line: "--" and
 * the long name, or "-" and the short name when it has no long name, then
 * a blank and the display name of the value between "<" and ">". When no
 * display name is set, the name of the value type stands in its place.
 *
 * Preconditions: option takes a value, CLINE_OPTION_VALUE. Checked with
 * assert.
 */
void cline_help_item_usage_option(cline_out_t *out, const cline_option_t *option);

/*
 * Appends the length bytes of part to the end of text, a cell text.
 * Nothing happens when length is 0.
 *
 * Returns CLINE_ERROR_NONE, or CLINE_ERROR_OUT_OF_MEMORY when an
 * allocation fails. text may then hold a part of what was to be appended;
 * the caller stops the generation and destroys text.
 *
 * Preconditions: text was initialized with cline_vec_init() for items of
 * 1 byte; part is not NULL unless length is 0, and does not point into
 * text. Checked with assert, but for the last.
 */
cline_error_t cline_help_item_text_append(cline_vec_t *text, const char *part, size_t length);

/*
 * Appends the annotations of option to the end of text, a cell text, in
 * this order:
 *
 *  1. "[choices: a, b, c]": option takes a value and its value type is of
 *     the kind CLINE_TYPE_CHOICE. The choices stand in the order in which
 *     they were defined, separated by ", ".
 *  2. "[default: VALUE]": option takes a value and has a default value;
 *     VALUE is the string that was set, as it is. "[default: on]": option
 *     is a negatable flag whose initial state is on. A negatable flag
 *     whose initial state is off has no annotation.
 *  3. "[required]": option takes a value and is required.
 *
 * One blank stands before an annotation, unless text is empty or ends with
 * a newline: so one blank separates the first annotation from a
 * description that does not end with a newline, and each annotation from
 * the one before. When option has no annotation, text is not changed.
 *
 * A kind of a value type that is not known is taken as one without
 * choices, and an option of a kind that is not known has no annotation.
 *
 * Returns and preconditions: as cline_help_item_text_append().
 */
cline_error_t cline_help_item_option_notes(cline_vec_t *text, const cline_option_t *option);

/*
 * Appends the annotations of positional to the end of text, a cell text:
 * "[choices: a, b, c]" and "[default: VALUE]", under the conditions, in
 * the order and with the blanks of cline_help_item_option_notes(). A
 * positional has no "[required]": its heading shows whether it is
 * required.
 *
 * Returns and preconditions: as cline_help_item_text_append().
 */
cline_error_t cline_help_item_positional_notes(cline_vec_t *text,
                                                 const cline_positional_t *positional);

#endif /* CLINE_HELP_ITEM_H */
