/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * Tests of the internal function that writes one text broken into lines:
 * "wrap.h".
 *
 * The function allocates nothing, so the tests need no allocator. The
 * write function of the tests appends what it is given to an array of a
 * fixed size, and the tests compare the whole text with the string they
 * expect. No string that is expected has a blank at the end of a line.
 */
#include <argx/argx.h>

#include "out.h"
#include "wrap.h"

#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* The size of the text a sink holds: enough for every test. */
#define SINK_SIZE ((size_t)256)

/* What the write function of the tests has received. */
typedef struct sink
{
    char text[SINK_SIZE]; /* the parts, joined in the order of the calls */
    size_t length;        /* the number of bytes of text */
    size_t calls;         /* the number of calls */
    size_t empty_calls;   /* the calls with the length 0 or without a text */
} sink_t;

static void sink_init(sink_t *sink)
{
    memset(sink, 0, sizeof *sink);
}

/* Appends the part to the sink. */
static bool sink_write(void *user_data, const char *text, size_t length)
{
    sink_t *sink = user_data;

    ++sink->calls;
    if ((text == NULL) || (length == 0))
    {
        ++sink->empty_calls;
        return true;
    }
    if (length > (SINK_SIZE - sink->length))
    {
        return false;
    }

    memcpy(sink->text + sink->length, text, length);
    sink->length += length;
    return true;
}

/* The sink holds exactly the string expected, and no call was empty. */
static void check_sink(const sink_t *sink, const char *expected)
{
    NAOS_CHECK_MEM_EQ(sink->text, sink->length, expected, strlen(expected));
    NAOS_CHECK_UINT_EQ(sink->empty_calls, 0);
}

/*
 * Writes the string text to a new output, with the column and the width
 * given, and checks that exactly the string expected was written and that
 * nothing failed.
 */
static void check_wrap(const char *text, size_t column, size_t width, const char *expected)
{
    sink_t sink;
    argx_out_t out;

    sink_init(&sink);
    argx_out_init(&out, sink_write, &sink);

    argx_wrap_write(&out, text, strlen(text), column, width);

    check_sink(&sink, expected);
    NAOS_CHECK_FALSE(argx_out_failed(&out));
}

/* ---- The width ----------------------------------------------------------- */

/* A text that fits is written as it is, without a newline after it. */
NAOS_TEST(text_that_fits_is_one_line)
{
    check_wrap("one two three", 0, 80, "one two three");
    check_wrap("word", 0, 80, "word");
}

/*
 * A word that ends exactly at the width stays on the line; with one
 * column less it goes to the next line.
 */
NAOS_TEST(word_at_the_width)
{
    check_wrap("aaa bbb", 0, 7, "aaa bbb");
    check_wrap("aaa bbb", 0, 6, "aaa\nbbb");

    /* The same with a start column: the width counts from column 0. */
    check_wrap("aaa bbb", 4, 11, "    aaa bbb");
    check_wrap("aaa bbb", 4, 10, "    aaa\n    bbb");
}

/*
 * A line is broken at the last blank that leaves it within the width, and
 * every line takes as many words as fit.
 */
NAOS_TEST(lines_are_filled)
{
    check_wrap("one two three four five", 0, 9, "one two\nthree\nfour five");
    check_wrap("a b c d e f", 0, 3, "a b\nc d\ne f");
    check_wrap("a b c d e f", 0, 4, "a b\nc d\ne f");
    check_wrap("a b c d e f", 0, 5, "a b c\nd e f");
}

/*
 * The first word of a line is written even when it does not fit: a word
 * is never divided, and it stands on a line of its own.
 */
NAOS_TEST(first_word_wider_than_the_width)
{
    check_wrap("abcdefghij", 0, 5, "abcdefghij");
    check_wrap("abcdefghij kl", 0, 5, "abcdefghij\nkl");
    check_wrap("ab abcdefghij kl", 0, 5, "ab\nabcdefghij\nkl");
    check_wrap("abcdefghij kl", 2, 5, "  abcdefghij\n  kl");
}

/*
 * When the start column is at the width or to the right of it, every word
 * is the first word of its line.
 */
NAOS_TEST(column_beyond_the_width)
{
    check_wrap("a b c", 4, 4, "    a\n    b\n    c");
    check_wrap("a b c", 6, 4, "      a\n      b\n      c");
}

/* The width 0 is no limit: only the newlines of the text end a line. */
NAOS_TEST(width_zero_does_not_wrap)
{
    check_wrap("one two three four five six seven eight nine ten eleven twelve", 0, 0,
               "one two three four five six seven eight nine ten eleven twelve");
    check_wrap("one two three\nfour five", 3, 0, "   one two three\n   four five");
}

/* ---- Blanks -------------------------------------------------------------- */

/*
 * The blanks between two words of one line are written in the number they
 * have, and they count for the width.
 */
NAOS_TEST(blanks_inside_a_line_are_kept)
{
    check_wrap("a   b  c", 0, 80, "a   b  c");
    check_wrap("aa   bb", 0, 7, "aa   bb");
}

/* The blanks where a line is broken are not written, however many. */
NAOS_TEST(blanks_at_a_break_are_dropped)
{
    check_wrap("aa   bb", 0, 6, "aa\nbb");
    check_wrap("aa   bb", 3, 6, "   aa\n   bb");
    check_wrap("aa bb    cc dd", 0, 5, "aa bb\ncc dd");
}

/*
 * The blanks at the start of a line of the text are written, after the
 * start column. The word after them is the first word of its line, so it
 * is written even when it does not fit.
 */
NAOS_TEST(blanks_at_the_start_of_a_line_are_kept)
{
    check_wrap("  a b", 0, 80, "  a b");
    check_wrap("x\n  y z", 0, 80, "x\n  y z");
    check_wrap("x\n  y z", 2, 80, "  x\n    y z");
    check_wrap("   abcdef gh", 0, 5, "   abcdef\ngh");
    check_wrap("aaaa\n   bbbbbb", 0, 5, "aaaa\n   bbbbbb");
}

/* The blanks at the end of a line of the text are not written. */
NAOS_TEST(blanks_at_the_end_of_a_line_are_dropped)
{
    check_wrap("a  \nb", 0, 80, "a\nb");
    check_wrap("a  \n  \nb", 2, 80, "  a\n\n  b");
    check_wrap("a b   \n", 0, 80, "a b");
}

/*
 * The blanks at the end of the text are not written when the caller ends
 * the line, as it does.
 */
NAOS_TEST(blanks_at_the_end_of_the_text_are_dropped)
{
    sink_t sink;
    argx_out_t out;

    sink_init(&sink);
    argx_out_init(&out, sink_write, &sink);

    argx_wrap_write(&out, "a b  ", 5, 0, 80);
    argx_out_newline(&out);
    argx_wrap_write(&out, "   ", 3, 2, 80);
    argx_out_newline(&out);
    argx_out_string(&out, "next");

    check_sink(&sink, "a b\n\nnext");
}

/* ---- Newlines ------------------------------------------------------------ */

/*
 * A newline in the text ends the line, and what follows starts at the
 * start column.
 */
NAOS_TEST(newline_starts_a_line_at_the_column)
{
    check_wrap("a\nb", 0, 80, "a\nb");
    check_wrap("one two\nthree", 3, 80, "   one two\n   three");
}

/*
 * A newline starts a new count: a line of the text is wrapped on its
 * own.
 */
NAOS_TEST(newline_and_width_together)
{
    check_wrap("aa bb cc\ndd ee ff", 1, 6, " aa bb\n cc\n dd ee\n ff");
}

/*
 * An empty line of the text is a line with the newline only: the blanks
 * that lead to the start column are not written.
 */
NAOS_TEST(newlines_in_a_row)
{
    check_wrap("a\n\nb", 0, 80, "a\n\nb");
    check_wrap("a\n\n\nb", 2, 80, "  a\n\n\n  b");
    check_wrap("\na", 2, 80, "\n  a");
}

/*
 * The newlines at the end of the text are taken off: the last line is
 * left open for the caller.
 */
NAOS_TEST(newlines_at_the_end_are_taken_off)
{
    check_wrap("a\n", 0, 80, "a");
    check_wrap("a b\n\n\n", 2, 80, "  a b");
    check_wrap("a\nb\n", 2, 80, "  a\n  b");
}

/*
 * An empty text, and a text of newlines only, writes nothing and leaves
 * the output as it was: the column does not move to the start column.
 */
NAOS_TEST(empty_text_writes_nothing)
{
    static const char *const texts[] = { "", "\n", "\n\n\n" };
    size_t i;

    for (i = 0; i < (sizeof texts / sizeof texts[0]); ++i)
    {
        sink_t sink;
        argx_out_t out;

        sink_init(&sink);
        argx_out_init(&out, sink_write, &sink);
        argx_out_string(&out, "ab");

        argx_wrap_write(&out, texts[i], strlen(texts[i]), 8, 20);

        NAOS_CHECK_UINT_EQ(argx_out_column(&out), 2);
        NAOS_CHECK_UINT_EQ(sink.calls, 1);
        argx_out_string(&out, "c");
        check_sink(&sink, "abc");
    }
}

/* With the length 0 the text may be NULL. */
NAOS_TEST(null_text_of_length_zero)
{
    sink_t sink;
    argx_out_t out;

    sink_init(&sink);
    argx_out_init(&out, sink_write, &sink);

    argx_wrap_write(&out, NULL, 0, 4, 20);

    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 0);
    NAOS_CHECK_UINT_EQ(sink.calls, 0);
}

/* ---- The start column ---------------------------------------------------- */

/*
 * When the output is to the left of the start column, blanks lead to it;
 * the lines that follow start at the same column.
 */
NAOS_TEST(column_to_the_right_of_the_output)
{
    sink_t sink;
    argx_out_t out;

    sink_init(&sink);
    argx_out_init(&out, sink_write, &sink);

    argx_out_spaces(&out, 2);
    argx_out_string(&out, "-v");
    argx_wrap_write(&out, "one two three\nfour", 18, 8, 16);
    argx_out_newline(&out);

    check_sink(&sink, "  -v    one two\n"
                      "        three\n"
                      "        four\n");
}

/*
 * When the output is at the start column already, nothing is put before
 * the text.
 */
NAOS_TEST(column_equal_to_the_output)
{
    sink_t sink;
    argx_out_t out;

    sink_init(&sink);
    argx_out_init(&out, sink_write, &sink);

    argx_out_string(&out, "name: ");
    argx_wrap_write(&out, "aa bb cc", 8, 6, 11);

    check_sink(&sink, "name: aa bb\n"
                      "      cc");
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 8);
}

/* ---- The bytes ----------------------------------------------------------- */

/*
 * Only the blank and the newline end a word. A tab and the bytes of a
 * character outside ASCII are parts of a word, one column each, and they
 * are written as they are.
 */
NAOS_TEST(other_bytes_are_parts_of_a_word)
{
    check_wrap("a\tb c", 0, 3, "a\tb\nc");
    check_wrap("a\tb c", 0, 5, "a\tb c");
    check_wrap("\xE3\x81\x82 b", 0, 5, "\xE3\x81\x82 b");
    check_wrap("\xE3\x81\x82 b", 0, 4, "\xE3\x81\x82\nb");
    check_wrap("a\rb\x01 c", 0, 4, "a\rb\x01\nc");
}

/*
 * Only the first length bytes are read: the text need not be
 * terminated.
 */
NAOS_TEST(text_takes_only_its_length)
{
    sink_t sink;
    argx_out_t out;

    sink_init(&sink);
    argx_out_init(&out, sink_write, &sink);

    argx_wrap_write(&out, "abc def ghi", 7, 1, 5);

    check_sink(&sink, " abc\n def");
}

/*
 * An output without a write function counts the columns of the last
 * line.
 */
NAOS_TEST(counting_only_output)
{
    argx_out_t out;

    argx_out_init(&out, NULL, NULL);

    argx_wrap_write(&out, "aaa bbb cc", 10, 4, 11);

    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 6);
    NAOS_CHECK_FALSE(argx_out_failed(&out));
}

/* ---- The registration ------------------------------------------------------ */

NAOS_SUITE(width,
    NAOS_ADD_TEST(text_that_fits_is_one_line),
    NAOS_ADD_TEST(word_at_the_width),
    NAOS_ADD_TEST(lines_are_filled),
    NAOS_ADD_TEST(first_word_wider_than_the_width),
    NAOS_ADD_TEST(column_beyond_the_width),
    NAOS_ADD_TEST(width_zero_does_not_wrap)
)

NAOS_SUITE(blanks,
    NAOS_ADD_TEST(blanks_inside_a_line_are_kept),
    NAOS_ADD_TEST(blanks_at_a_break_are_dropped),
    NAOS_ADD_TEST(blanks_at_the_start_of_a_line_are_kept),
    NAOS_ADD_TEST(blanks_at_the_end_of_a_line_are_dropped),
    NAOS_ADD_TEST(blanks_at_the_end_of_the_text_are_dropped)
)

NAOS_SUITE(newlines,
    NAOS_ADD_TEST(newline_starts_a_line_at_the_column),
    NAOS_ADD_TEST(newline_and_width_together),
    NAOS_ADD_TEST(newlines_in_a_row),
    NAOS_ADD_TEST(newlines_at_the_end_are_taken_off),
    NAOS_ADD_TEST(empty_text_writes_nothing),
    NAOS_ADD_TEST(null_text_of_length_zero)
)

NAOS_SUITE(start_column,
    NAOS_ADD_TEST(column_to_the_right_of_the_output),
    NAOS_ADD_TEST(column_equal_to_the_output)
)

NAOS_SUITE(bytes,
    NAOS_ADD_TEST(other_bytes_are_parts_of_a_word),
    NAOS_ADD_TEST(text_takes_only_its_length),
    NAOS_ADD_TEST(counting_only_output)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(width),
    NAOS_ADD_SUITE(blanks),
    NAOS_ADD_SUITE(newlines),
    NAOS_ADD_SUITE(start_column),
    NAOS_ADD_SUITE(bytes)
)
