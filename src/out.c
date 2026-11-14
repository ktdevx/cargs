/*
 * CLine - the output of the help: counts columns while it passes text to
 * a write function.
 */
#include "out.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/*
 * The blanks that are passed to the write function in one call. More
 * blanks than these take several calls.
 */
#define CLINE_OUT_BLANKS "                                "
#define CLINE_OUT_BLANKS_LENGTH (sizeof CLINE_OUT_BLANKS - 1)

/*
 * Passes the length bytes of text to the write function, unless there is
 * none or it has failed before, and remembers a failure. This is the only
 * place that calls the write function. length is not 0.
 */
static void out_write(cline_out_t *out, const char *text, size_t length)
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
 * Writes the blanks that are remembered and forgets them. The column
 * counts them already.
 */
static void out_flush_blanks(cline_out_t *out)
{
    size_t rest = out->pending;

    out->pending = 0;
    while (rest != 0)
    {
        size_t part = (rest < CLINE_OUT_BLANKS_LENGTH) ? rest : CLINE_OUT_BLANKS_LENGTH;

        out_write(out, CLINE_OUT_BLANKS, part);
        rest -= part;
    }
}

void cline_out_init(cline_out_t *out, cline_help_write_fn_t write_fn, void *user_data)
{
    assert(out != NULL);

    out->write_fn = write_fn;
    out->user_data = user_data;
    out->column = 0;
    out->pending = 0;
    out->failed = false;
}

void cline_out_text(cline_out_t *out, const char *text, size_t length)
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

void cline_out_string(cline_out_t *out, const char *text)
{
    assert(text != NULL);

    cline_out_text(out, text, strlen(text));
}

void cline_out_spaces(cline_out_t *out, size_t count)
{
    assert(out != NULL);

    out->pending += count;
    out->column += count;
}

void cline_out_pad_to(cline_out_t *out, size_t column)
{
    assert(out != NULL);

    if (out->column < column)
    {
        cline_out_spaces(out, column - out->column);
    }
}

void cline_out_newline(cline_out_t *out)
{
    assert(out != NULL);

    /* The blanks at the end of the line are dropped, not written. */
    out->pending = 0;
    out_write(out, "\n", 1);
    out->column = 0;
}

size_t cline_out_column(const cline_out_t *out)
{
    assert(out != NULL);

    return out->column;
}

bool cline_out_failed(const cline_out_t *out)
{
    assert(out != NULL);

    return out->failed;
}
