/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - internal declarations for help items: how one option or
 * positional of a schema is written.
 *
 * Headings and usage line items are written to an argx_out_t. An item's
 * width is the number of all its bytes. The caller measures it with the
 * same code that writes the item: it writes the item once to an
 * argx_out_t whose write function only adds up the lengths it receives.
 * The output column would not do: a name may hold a newline, after which
 * the column restarts at 0. No function returns with a blank still
 * remembered, so every byte of an item reaches the write function.
 *
 * An item's annotations are not written but appended to the cell text:
 * one item's description and its annotations, as one string that the
 * caller then breaks into lines. The cell text is an argx_vec_t of bytes.
 * Only the functions that append to it allocate.
 *
 * Schema strings are written as they are, byte by byte. The functions
 * only read the schema, through its public functions. They change nothing
 * and know nothing about the help options: line breaking and the indent
 * are up to the caller.
 *
 * Preconditions of every function: no argument is NULL. Checked with
 * assert.
 */
#ifndef ARGX_HELP_ITEM_H
#define ARGX_HELP_ITEM_H

#include "out.h"
#include "vec.h"

#include <argx/schema.h>

#include <stddef.h>

/*
 * Writes text, a schema string, as it is. Such a string may hold a '\n',
 * which argx_out_text() does not accept. So the parts between newlines
 * are written with argx_out_text() and each newline with
 * argx_out_newline(), and the column restarts at 0 after it.
 */
void argx_help_item_string(argx_out_t *out, const char *text);

/*
 * Writes option's heading as it appears in the option list. The names
 * come first: "-s, --long" with both names, "-s" with only a short name,
 * and four blanks then "--long" with only a long name, so that the long
 * names in a list line up. Then, depending on the option's kind:
 *
 * - a flag: nothing more.
 * - a negatable flag: the long name is written as "--[no-]long".
 * - a count: "..." right after the names.
 * - an option with a value: a blank and the value's display name between
 *   "<" and ">". When no display name is set, the value type name stands
 *   in its place.
 *
 * An unknown kind is written as a flag.
 */
void argx_help_item_option_heading(argx_out_t *out, const argx_option_t *option);

/*
 * Writes positional's heading according to its arity: "<name>" for
 * exactly one argument, "[name]" for zero or one, "[name]..." for zero or
 * more and "<name>..." for one or more. An unknown arity is written as
 * exactly one.
 */
void argx_help_item_positional_heading(argx_out_t *out, const argx_positional_t *positional);

/*
 * Writes option as a required option appears in a usage line: "--" and
 * the long name, or "-" and the short name when it has no long name, then
 * a blank and the value's display name between "<" and ">". When no
 * display name is set, the value type name stands in its place.
 *
 * Preconditions: option takes a value, ARGX_OPTION_VALUE. Checked with
 * assert.
 */
void argx_help_item_usage_option(argx_out_t *out, const argx_option_t *option);

/*
 * Appends length bytes of part to the end of text, a cell text. Nothing
 * happens when length is 0.
 *
 * Returns ARGX_ERROR_NONE, or ARGX_ERROR_OUT_OF_MEMORY when an
 * allocation fails. text may then hold part of what was to be appended;
 * the caller stops the generation and destroys text.
 *
 * Preconditions: text was initialized with argx_vec_init() for 1-byte
 * items; part is not NULL unless length is 0, and does not point into
 * text. Checked with assert, except for the last.
 */
argx_error_t argx_help_item_text_append(argx_vec_t *text, const char *part, size_t length);

/*
 * Appends option's annotations to the end of text, a cell text, in this
 * order:
 *
 *  1. "[choices: a, b, c]": option takes a value and its value type is of
 *     kind ARGX_TYPE_CHOICE. The choices appear in the order they were
 *     defined, separated by ", ".
 *  2. "[default: VALUE]": option takes a value and has a default value;
 *     VALUE is the string that was set, unchanged. "[default: on]": option
 *     is a negatable flag whose initial state is on. A negatable flag
 *     whose initial state is off has no annotation.
 *  3. "[required]": option takes a value and is required.
 *
 * One blank precedes each annotation unless text is empty or ends with a
 * newline. So one blank separates the first annotation from a description
 * that does not end with a newline, and each annotation from the previous
 * one. When option has no annotation, text is not changed.
 *
 * An unknown value type kind is treated as one without choices, and an
 * option of an unknown kind has no annotation.
 *
 * Returns and preconditions: as argx_help_item_text_append().
 */
argx_error_t argx_help_item_option_notes(argx_vec_t *text, const argx_option_t *option);

/*
 * Appends positional's annotations to the end of text, a cell text:
 * "[choices: a, b, c]" and "[default: VALUE]", with the same conditions,
 * order and blanks as argx_help_item_option_notes(). A positional has no
 * "[required]": its heading shows whether it is required.
 *
 * Returns and preconditions: as argx_help_item_text_append().
 */
argx_error_t argx_help_item_positional_notes(argx_vec_t *text,
                                             const argx_positional_t *positional);

#endif /* ARGX_HELP_ITEM_H */
