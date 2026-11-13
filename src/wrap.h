/*
 * CLArgs - internal declarations for the wrapping of the help: one text is
 * written to a clargs_out_t, broken into lines at the blanks so that it
 * fits a width.
 *
 * A word is a run of bytes other than the ASCII blank (0x20) and the
 * newline (0x0A). A tab and every other byte is a part of a word. A width
 * and a column count bytes, as "out.h" does: one byte is one column.
 *
 * The function allocates nothing and knows nothing about a schema.
 */
#ifndef CLARGS_WRAP_H
#define CLARGS_WRAP_H

#include "out.h"

#include <stddef.h>

/*
 * Writes the length bytes of text to out. Every line of it starts at
 * column, and a line is broken before the word that would end to the right
 * of width.
 *
 *  1. The newlines at the end of text are taken off. When nothing is left,
 *     nothing happens: out is not changed.
 *  2. A newline in the text ends the line, and what follows starts at
 *     column on the next line. An empty line of the text gives a line that
 *     holds the newline only.
 *  3. A line is broken between words only, never inside a word. The first
 *     word of a line of the output is written even when it does not fit,
 *     so a word that is too long stands on a line of its own and ends to
 *     the right of width.
 *  4. The blanks where a line is broken are not written, and neither are
 *     the blanks at the end of a line. The blanks between two words of one
 *     line, and those at the start of a line of the text, are written in
 *     the number they have.
 *  5. When width is 0, there is no limit: a line ends at the newlines of
 *     the text only.
 *
 * The newline of the last line is not written; the caller ends the line
 * with clargs_out_newline(). The bytes of text are written as they are.
 *
 * Preconditions: out is not NULL, text is not NULL unless length is 0, and
 * the current column of out is not to the right of column. They are
 * checked with assert.
 */
void clargs_wrap_write(clargs_out_t *out, const char *text, size_t length,
                       size_t column, size_t width);

#endif /* CLARGS_WRAP_H */
