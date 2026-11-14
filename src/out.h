/*
 * CLArgs - internal declarations for the output of the help: the part that
 * counts columns while it passes text to a write function.
 *
 * Everything the help writes goes through a clargs_out_t. It keeps the
 * column of the current line, where one byte is one column, whatever the
 * byte is.
 *
 * Blanks are not written when they are asked for. Their number is
 * remembered, and they are written when a text follows on the same line; a
 * newline drops them. So no line ends with a blank, whatever the callers
 * do. The blanks that are remembered count for the column at once.
 *
 * After the write function has reported a failure, no function calls it
 * again. The columns are counted as before, so the callers need not check
 * for the failure after every call.
 *
 * Without a write function (NULL) nothing is written and nothing fails,
 * and only the columns are counted. That measures the width of a text with
 * the same code that writes it.
 *
 * The functions allocate nothing and know nothing about a schema. A
 * clargs_out_t is an object of the caller, used for one generation and by
 * one thread.
 *
 * Preconditions of every function: out is not NULL (checked with assert),
 * and, except for clargs_out_init(), it was initialised with
 * clargs_out_init() (not checked).
 */
#ifndef CLARGS_OUT_H
#define CLARGS_OUT_H

#include <clargs/help.h>

#include <stdbool.h>
#include <stddef.h>

/*
 * The state of the output. The members are read and written by the
 * functions of this header only.
 */
typedef struct clargs_out
{
    clargs_help_write_fn_t write_fn; /* NULL: only the columns are counted */
    void *user_data;                 /* passed on to write_fn as it is */
    size_t column;                   /* the current column, with the blanks not yet written */
    size_t pending;                  /* the number of blanks not yet written */
    bool failed;                     /* write_fn has reported a failure */
} clargs_out_t;

/*
 * Starts an output at column 0, with no blanks remembered and no failure.
 * write_fn receives the text, together with user_data; when write_fn is
 * NULL, nothing is written and only the columns are counted.
 */
void clargs_out_init(clargs_out_t *out, clargs_help_write_fn_t write_fn, void *user_data);

/*
 * Writes the length bytes of text: first the blanks that are remembered,
 * then the text. The column advances by length. When length is 0, nothing
 * happens: the blanks stay remembered.
 *
 * Preconditions: text is not NULL unless length is 0, and it holds no
 * '\n' (a line is ended with clargs_out_newline()). They are checked with
 * assert.
 */
void clargs_out_text(clargs_out_t *out, const char *text, size_t length);

/*
 * The same as clargs_out_text() with the length of the string text.
 *
 * Preconditions: text is not NULL and holds no '\n'. They are checked with
 * assert.
 */
void clargs_out_string(clargs_out_t *out, const char *text);

/*
 * Remembers count more blanks and advances the column by count. Nothing is
 * written yet.
 */
void clargs_out_spaces(clargs_out_t *out, size_t count);

/*
 * When the current column is to the left of column, remembers the blanks
 * that lead to it, as clargs_out_spaces() does. Otherwise nothing happens.
 */
void clargs_out_pad_to(clargs_out_t *out, size_t column);

/*
 * Ends the line: drops the blanks that are remembered, writes the one byte
 * '\n' and sets the column to 0.
 */
void clargs_out_newline(clargs_out_t *out);

/*
 * Returns the current column: the number of bytes written on this line
 * plus the number of blanks that are remembered.
 */
size_t clargs_out_column(const clargs_out_t *out);

/*
 * Returns true when the write function has reported a failure. Always
 * false without a write function.
 */
bool clargs_out_failed(const clargs_out_t *out);

#endif /* CLARGS_OUT_H */
