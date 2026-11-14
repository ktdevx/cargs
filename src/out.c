/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - the help output: counts columns while it passes text to a write
 * function.
 */
#include "out.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/*
 * The blanks passed to the write function in one call. More blanks than
 * this take several calls.
 */
#define ARGX_OUT_BLANKS "                                "
#define ARGX_OUT_BLANKS_LENGTH (sizeof ARGX_OUT_BLANKS - 1)

/*
 * Passes length bytes of text to the write function, unless there is none
 * or it has already failed, and records a failure. This is the only place
 * that calls the write function. length is not 0.
 */
static void out_write(argx_out_t *out, const char *text, size_t length)
{
    assert(length != 0);

    if ((out->write_fn == NULL) || out->failed)
    {
        return;
    }
    if (!out->write_fn(out->user_data, text, length))
    {
        out->failed = true;
    }
}

/*
 * Writes the remembered blanks and forgets them. The column already
 * counts them.
 */
static void out_flush_blanks(argx_out_t *out)
{
    size_t rest = out->pending;

    out->pending = 0;
    while (rest != 0)
    {
        size_t part = (rest < ARGX_OUT_BLANKS_LENGTH) ? rest : ARGX_OUT_BLANKS_LENGTH;

        out_write(out, ARGX_OUT_BLANKS, part);
        rest -= part;
    }
}

void argx_out_init(argx_out_t *out, argx_help_write_fn_t write_fn, void *user_data)
{
    assert(out != NULL);

    out->write_fn = write_fn;
    out->user_data = user_data;
    out->column = 0;
    out->pending = 0;
    out->failed = false;
}

void argx_out_text(argx_out_t *out, const char *text, size_t length)
{
    assert(out != NULL);

    if (length == 0)
    {
        return;
    }

    assert(text != NULL);
    assert(memchr(text, '\n', length) == NULL);

    out_flush_blanks(out);
    out_write(out, text, length);
    out->column += length;
}

void argx_out_string(argx_out_t *out, const char *text)
{
    assert(text != NULL);

    argx_out_text(out, text, strlen(text));
}

void argx_out_spaces(argx_out_t *out, size_t count)
{
    assert(out != NULL);

    out->pending += count;
    out->column += count;
}

void argx_out_pad_to(argx_out_t *out, size_t column)
{
    assert(out != NULL);

    if (out->column < column)
    {
        argx_out_spaces(out, column - out->column);
    }
}

void argx_out_newline(argx_out_t *out)
{
    assert(out != NULL);

    /* The blanks at the end of the line are dropped, not written. */
    out->pending = 0;
    out_write(out, "\n", 1);
    out->column = 0;
}

size_t argx_out_column(const argx_out_t *out)
{
    assert(out != NULL);

    return out->column;
}

bool argx_out_failed(const argx_out_t *out)
{
    assert(out != NULL);

    return out->failed;
}
