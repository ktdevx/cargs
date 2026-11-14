/*
 * SPDX-FileCopyrightText: 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - the wrapping of the help: writes one text broken into lines at
 * the blanks.
 */
#include "wrap.h"

#include "out.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

void argx_wrap_write(argx_out_t *out, const char *text, size_t length,
                     size_t column, size_t width)
{
    size_t position = 0;
    bool line_has_word = false; /* a word was written on this line of the output */

    assert(out != NULL);
    assert((text != NULL) || (length == 0));
    assert(argx_out_column(out) <= column);

    while ((length != 0) && (text[length - 1] == '\n'))
    {
        --length;
    }
    if (length == 0)
    {
        return;
    }

    argx_out_pad_to(out, column);

    while (position < length)
    {
        size_t start = position;

        if (text[position] == '\n')
        {
            /*
             * The blanks that are remembered, those that lead to column
             * included, are dropped by the newline.
             */
            ++position;
            argx_out_newline(out);
            argx_out_pad_to(out, column);
            line_has_word = false;
        }
        else if (text[position] == ' ')
        {
            while ((position < length) && (text[position] == ' '))
            {
                ++position;
            }
            argx_out_spaces(out, position - start);
        }
        else
        {
            size_t current;
            size_t word;

            while ((position < length) && (text[position] != ' ') && (text[position] != '\n'))
            {
                ++position;
            }
            word = position - start;

            /*
             * "current + word > width" without the sum: the subtraction
             * cannot wrap around, because current < width there.
             */
            current = argx_out_column(out);
            if (line_has_word && (width != 0) && ((current >= width) || (word > (width - current))))
            {
                /* The blanks before the word are dropped by the newline. */
                argx_out_newline(out);
                argx_out_pad_to(out, column);
            }

            argx_out_text(out, text + start, word);
            line_has_word = true;
        }
    }
}
