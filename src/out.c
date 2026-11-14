/*
 * CLArgs - the output of the help: counts columns while it passes text to
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
#define CLARGS_OUT_BLANKS "                                "
#define CLARGS_OUT_BLANKS_LENGTH (sizeof CLARGS_OUT_BLANKS - 1)

/*
 * Passes the length bytes of text to the write function, unless there is
 * none or it has failed before, and remembers a failure. This is the only
 * place that calls the write function. length is not 0.
 */
static void out_write(clargs_out_t *out, const char *text, size_t length)
{
    assert(length != 0);

    if (out->write_fn == NULL || out->failed) {
        return;
    }
    if (!out->write_fn(out->user_data, text, length)) {
        out->failed = true;
    }
}

/*
 * Writes the blanks that are remembered and forgets them. The column
 * counts them already.
 */
static void out_flush_blanks(clargs_out_t *out)
{
    size_t rest = out->pending;

    out->pending = 0;
    while (rest != 0) {
        size_t part = rest < CLARGS_OUT_BLANKS_LENGTH ? rest : CLARGS_OUT_BLANKS_LENGTH;

        out_write(out, CLARGS_OUT_BLANKS, part);
        rest -= part;
    }
}

void clargs_out_init(clargs_out_t *out, clargs_help_write_fn_t write_fn, void *user_data)
{
    assert(out != NULL);

    out->write_fn = write_fn;
    out->user_data = user_data;
    out->column = 0;
    out->pending = 0;
    out->failed = false;
}

void clargs_out_text(clargs_out_t *out, const char *text, size_t length)
{
    assert(out != NULL);

    if (length == 0) {
        return;
    }

    assert(text != NULL);
    assert(memchr(text, '\n', length) == NULL);

    out_flush_blanks(out);
    out_write(out, text, length);
    out->column += length;
}

void clargs_out_string(clargs_out_t *out, const char *text)
{
    assert(text != NULL);

    clargs_out_text(out, text, strlen(text));
}

void clargs_out_spaces(clargs_out_t *out, size_t count)
{
    assert(out != NULL);

    out->pending += count;
    out->column += count;
}

void clargs_out_pad_to(clargs_out_t *out, size_t column)
{
    assert(out != NULL);

    if (out->column < column) {
        clargs_out_spaces(out, column - out->column);
    }
}

void clargs_out_newline(clargs_out_t *out)
{
    assert(out != NULL);

    /* The blanks at the end of the line are dropped, not written. */
    out->pending = 0;
    out_write(out, "\n", 1);
    out->column = 0;
}

size_t clargs_out_column(const clargs_out_t *out)
{
    assert(out != NULL);

    return out->column;
}

bool clargs_out_failed(const clargs_out_t *out)
{
    assert(out != NULL);

    return out->failed;
}
