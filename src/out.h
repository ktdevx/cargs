/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - internal declarations for the help output: the layer that counts
 * columns while it passes text to a write function.
 *
 * Everything the help writes goes through an argx_out_t. It tracks the
 * column of the current line. One byte is one column, whatever the byte.
 *
 * Blanks are not written when they are requested. The output remembers
 * how many there are and writes them when text follows on the same line;
 * a newline drops them. So no line ends with a blank, whatever the
 * callers do. Remembered blanks count toward the column at once.
 *
 * Once the write function has reported a failure, no function calls it
 * again. Columns are still counted, so callers need not check for the
 * failure after every call.
 *
 * Without a write function (NULL), nothing is written and nothing fails;
 * only the columns are counted. This measures the width of a text with
 * the same code that writes it.
 *
 * The functions allocate nothing and know nothing about a schema. An
 * argx_out_t belongs to the caller and is used for one generation by one
 * thread.
 *
 * Preconditions of every function: out is not NULL (checked with assert),
 * and, except for argx_out_init(), it was initialised with
 * argx_out_init() (not checked).
 */
#ifndef ARGX_OUT_H
#define ARGX_OUT_H

#include <argx/help.h>

#include <stdbool.h>
#include <stddef.h>

/*
 * The output state. Only the functions in this header read and write its
 * members.
 */
typedef struct argx_out
{
    argx_help_write_fn_t write_fn; /* NULL: only the columns are counted */
    void *user_data;               /* passed to write_fn unchanged */
    size_t column;                 /* the current column, including unwritten blanks */
    size_t pending;                /* the number of unwritten blanks */
    bool failed;                   /* write_fn has reported a failure */
} argx_out_t;

/*
 * Starts an output at column 0, with no remembered blanks and no failure.
 * write_fn receives the text along with user_data. When write_fn is NULL,
 * nothing is written and only the columns are counted.
 */
void argx_out_init(argx_out_t *out, argx_help_write_fn_t write_fn, void *user_data);

/*
 * Writes length bytes of text: first the remembered blanks, then the text.
 * The column advances by length. When length is 0, nothing happens and
 * the blanks stay remembered.
 *
 * Preconditions: text is not NULL unless length is 0, and it holds no
 * '\n' (argx_out_newline() ends a line). They are checked with assert.
 */
void argx_out_text(argx_out_t *out, const char *text, size_t length);

/*
 * Like argx_out_text(), but takes the length from the string text.
 *
 * Preconditions: text is not NULL and holds no '\n'. They are checked with
 * assert.
 */
void argx_out_string(argx_out_t *out, const char *text);

/*
 * Remembers count more blanks and advances the column by count. Nothing is
 * written yet.
 */
void argx_out_spaces(argx_out_t *out, size_t count);

/*
 * When the current column is left of column, remembers the blanks needed
 * to reach it, as argx_out_spaces() does. Otherwise nothing happens.
 */
void argx_out_pad_to(argx_out_t *out, size_t column);

/*
 * Ends the line: drops the remembered blanks, writes a single '\n' byte
 * and sets the column to 0.
 */
void argx_out_newline(argx_out_t *out);

/*
 * Returns the current column: the number of bytes written on this line
 * plus the number of remembered blanks.
 */
size_t argx_out_column(const argx_out_t *out);

/*
 * Returns true when the write function has reported a failure. Always
 * false without a write function.
 */
bool argx_out_failed(const argx_out_t *out);

#endif /* ARGX_OUT_H */
