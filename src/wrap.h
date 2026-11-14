/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - internal declarations for help wrapping: one text is written to
 * an argx_out_t, broken into lines at blanks so that it fits a width.
 *
 * A word is a run of bytes other than the ASCII blank (0x20) and the
 * newline (0x0A). A tab, like every other byte, is part of a word. Widths
 * and columns count bytes, as in "out.h": one byte is one column.
 *
 * The function allocates nothing and knows nothing about a schema.
 */
#ifndef ARGX_WRAP_H
#define ARGX_WRAP_H

#include "out.h"

#include <stddef.h>

/*
 * Writes length bytes of text to out. Every line starts at column, and a
 * line is broken before a word that would end to the right of width.
 *
 *  1. Trailing newlines are removed from text. If nothing is left,
 *     nothing happens: out is not changed.
 *  2. A newline in the text ends the line, and what follows starts at
 *     column on the next line. An empty line in the text produces a line
 *     that holds only the newline.
 *  3. Lines break only between words, never inside a word. The first word
 *     on an output line is written even when it does not fit, so a word
 *     that is too long stands on its own line and ends to the right of
 *     width.
 *  4. Blanks at a line break are not written, nor are blanks at the end
 *     of a line. Blanks between two words on one line, and blanks at the
 *     start of a line in the text, are all written, however many there
 *     are.
 *  5. When width is 0, there is no limit: lines end only at the newlines
 *     in the text.
 *
 * The newline of the last line is not written; the caller ends the line
 * with argx_out_newline(). The bytes of text are written unchanged.
 *
 * Preconditions: out is not NULL, text is not NULL unless length is 0, and
 * the current column of out is not to the right of column. They are
 * checked with assert.
 */
void argx_wrap_write(argx_out_t *out, const char *text, size_t length,
                     size_t column, size_t width);

#endif /* ARGX_WRAP_H */
