/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * Tests of the internal part that counts columns while it passes text to a
 * write function: "out.h".
 *
 * The part allocates nothing, so the tests need no allocator. The write
 * function of the tests appends what it is given to an array of a fixed
 * size and records how it was called: the number of calls, whether a call
 * had the length 0, and whether the user data arrived as it was given.
 */
#include "out.h"

#include <argx/argx.h>

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
    size_t calls;         /* the number of calls, the failed one included */
    size_t empty_calls;   /* the calls with the length 0 or without a text */
    size_t fail_at;       /* the call that reports a failure; 0: none */
} sink_t;

static void sink_init(sink_t *sink)
{
    memset(sink, 0, sizeof *sink);
}

/*
 * Appends the part to the sink. The call number fail_at takes nothing and
 * reports a failure, and so would every later call.
 */
static bool sink_write(void *user_data, const char *text, size_t length)
{
    sink_t *sink = user_data;

    ++sink->calls;
    if ((text == NULL) || (length == 0))
    {
        ++sink->empty_calls;
        return true;
    }
    if ((sink->fail_at != 0) && (sink->calls >= sink->fail_at))
    {
        return false;
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

/* ---- The column ---------------------------------------------------------- */

/* A new part is at column 0, has not failed, and has called nothing. */
NAOS_TEST(init_starts_at_column_zero)
{
    sink_t sink;
    argx_out_t out;

    sink_init(&sink);
    argx_out_init(&out, sink_write, &sink);

    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 0);
    NAOS_CHECK_FALSE(argx_out_failed(&out));
    NAOS_CHECK_UINT_EQ(sink.calls, 0);
}

/*
 * The column advances by the number of bytes, and the parts arrive in the
 * order they were written.
 */
NAOS_TEST(text_advances_the_column)
{
    sink_t sink;
    argx_out_t out;

    sink_init(&sink);
    argx_out_init(&out, sink_write, &sink);

    argx_out_text(&out, "Usage:", 6);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 6);
    argx_out_string(&out, "app");
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 9);

    check_sink(&sink, "Usage:app");
    NAOS_CHECK_FALSE(argx_out_failed(&out));
}

/*
 * Only the first length bytes are written: the text need not be
 * terminated.
 */
NAOS_TEST(text_takes_only_its_length)
{
    sink_t sink;
    argx_out_t out;

    sink_init(&sink);
    argx_out_init(&out, sink_write, &sink);

    argx_out_text(&out, "abcdef", 3);

    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 3);
    check_sink(&sink, "abc");
}

/*
 * One byte is one column, whatever the byte is: the three bytes of a
 * character outside ASCII are three columns, and they are passed on as
 * they are.
 */
NAOS_TEST(one_byte_is_one_column)
{
    sink_t sink;
    argx_out_t out;

    sink_init(&sink);
    argx_out_init(&out, sink_write, &sink);

    argx_out_string(&out, "\xE3\x81\x82");
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 3);
    argx_out_string(&out, "\t\x01");
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 5);

    check_sink(&sink, "\xE3\x81\x82\t\x01");
}

/*
 * A text of the length 0 does nothing: no call, no column, and the text
 * may be NULL then.
 */
NAOS_TEST(empty_text_does_nothing)
{
    sink_t sink;
    argx_out_t out;

    sink_init(&sink);
    argx_out_init(&out, sink_write, &sink);

    argx_out_text(&out, "", 0);
    argx_out_text(&out, NULL, 0);
    argx_out_text(&out, "abc", 0);
    argx_out_string(&out, "");

    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 0);
    NAOS_CHECK_UINT_EQ(sink.calls, 0);
}

/* A newline is the one byte '\n', and the column starts again at 0. */
NAOS_TEST(newline_resets_the_column)
{
    sink_t sink;
    argx_out_t out;

    sink_init(&sink);
    argx_out_init(&out, sink_write, &sink);

    argx_out_string(&out, "first");
    argx_out_newline(&out);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 0);
    argx_out_string(&out, "second");
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 6);
    argx_out_newline(&out);
    argx_out_newline(&out);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 0);

    check_sink(&sink, "first\nsecond\n\n");
}

/* ---- The blanks ------------------------------------------------------------ */

/*
 * Blanks count for the column at once, but they are not written until a
 * text follows.
 */
NAOS_TEST(spaces_wait_for_the_next_text)
{
    sink_t sink;
    argx_out_t out;

    sink_init(&sink);
    argx_out_init(&out, sink_write, &sink);

    argx_out_string(&out, "a");
    argx_out_spaces(&out, 3);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 4);
    check_sink(&sink, "a");

    argx_out_string(&out, "b");
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 5);
    check_sink(&sink, "a   b");
}

/*
 * Blanks at the start of a line are kept in the same way, and blanks that
 * are remembered in several calls add up.
 */
NAOS_TEST(spaces_add_up)
{
    sink_t sink;
    argx_out_t out;

    sink_init(&sink);
    argx_out_init(&out, sink_write, &sink);

    argx_out_spaces(&out, 2);
    argx_out_spaces(&out, 0);
    argx_out_spaces(&out, 2);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 4);
    NAOS_CHECK_UINT_EQ(sink.calls, 0);

    argx_out_string(&out, "x");
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 5);
    check_sink(&sink, "    x");
}

/*
 * A text of the length 0 does not write the blanks: they still wait, and a
 * newline drops them.
 */
NAOS_TEST(empty_text_keeps_the_spaces_waiting)
{
    sink_t sink;
    argx_out_t out;

    sink_init(&sink);
    argx_out_init(&out, sink_write, &sink);

    argx_out_string(&out, "a");
    argx_out_spaces(&out, 2);
    argx_out_text(&out, "", 0);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 3);
    check_sink(&sink, "a");

    argx_out_newline(&out);
    check_sink(&sink, "a\n");
}

/*
 * A newline drops the blanks that wait, so no line ends with a blank, and
 * a line of blanks alone is an empty line. The dropped blanks do not come
 * back on the next line.
 */
NAOS_TEST(newline_drops_the_spaces)
{
    sink_t sink;
    argx_out_t out;

    sink_init(&sink);
    argx_out_init(&out, sink_write, &sink);

    argx_out_string(&out, "a");
    argx_out_spaces(&out, 4);
    argx_out_newline(&out);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 0);

    argx_out_spaces(&out, 8);
    argx_out_newline(&out);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 0);

    argx_out_string(&out, "b");
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 1);
    argx_out_newline(&out);

    check_sink(&sink, "a\n\nb\n");
}

/*
 * Any number of blanks is written, also more than the part passes on in
 * one call.
 */
NAOS_TEST(many_spaces_are_all_written)
{
    static const size_t counts[] = { 1, 31, 32, 33, 64, 65, 100 };
    size_t i;

    for (i = 0; i < (sizeof counts / sizeof counts[0]); ++i)
    {
        char expected[SINK_SIZE];
        sink_t sink;
        argx_out_t out;

        memset(expected, ' ', counts[i]);
        expected[counts[i]] = 'x';
        expected[counts[i] + 1] = '\0';

        sink_init(&sink);
        argx_out_init(&out, sink_write, &sink);

        argx_out_spaces(&out, counts[i]);
        argx_out_string(&out, "x");

        NAOS_CHECK_UINT_EQ(argx_out_column(&out), counts[i] + 1);
        check_sink(&sink, expected);
    }
}

/* ---- The padding ----------------------------------------------------------- */

/* Padding to a column to the right remembers the blanks up to it. */
NAOS_TEST(pad_to_reaches_the_column)
{
    sink_t sink;
    argx_out_t out;

    sink_init(&sink);
    argx_out_init(&out, sink_write, &sink);

    argx_out_string(&out, "ab");
    argx_out_pad_to(&out, 6);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 6);
    check_sink(&sink, "ab");

    argx_out_string(&out, "c");
    check_sink(&sink, "ab    c");
}

/*
 * The boundaries: one column short is one blank, the column itself and a
 * column to the left are nothing.
 */
NAOS_TEST(pad_to_boundaries)
{
    sink_t sink;
    argx_out_t out;

    sink_init(&sink);
    argx_out_init(&out, sink_write, &sink);

    argx_out_string(&out, "abcd");

    argx_out_pad_to(&out, 0);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 4);
    argx_out_pad_to(&out, 3);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 4);
    argx_out_pad_to(&out, 4);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 4);
    argx_out_string(&out, "e");
    check_sink(&sink, "abcde");

    argx_out_pad_to(&out, 6);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 6);
    argx_out_string(&out, "f");
    check_sink(&sink, "abcde f");
}

/* The blanks that wait are part of the column that padding starts from. */
NAOS_TEST(pad_to_counts_the_waiting_spaces)
{
    sink_t sink;
    argx_out_t out;

    sink_init(&sink);
    argx_out_init(&out, sink_write, &sink);

    argx_out_pad_to(&out, 2);
    argx_out_string(&out, "a");
    argx_out_spaces(&out, 2);
    argx_out_pad_to(&out, 5);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 5);
    argx_out_pad_to(&out, 8);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 8);
    argx_out_string(&out, "b");

    check_sink(&sink, "  a     b");
}

/* ---- The write function ---------------------------------------------------- */

/* The user data is passed on as it was given, also when it is NULL. */
static bool expect_null_user_data(void *user_data, const char *text, size_t length)
{
    (void)text;
    (void)length;
    return user_data == NULL;
}

NAOS_TEST(user_data_is_passed_on)
{
    sink_t sink;
    argx_out_t out;

    sink_init(&sink);
    argx_out_init(&out, sink_write, &sink);
    argx_out_string(&out, "a");
    NAOS_CHECK_UINT_EQ(sink.calls, 1);

    argx_out_init(&out, expect_null_user_data, NULL);
    argx_out_string(&out, "a");
    argx_out_newline(&out);
    NAOS_CHECK_FALSE(argx_out_failed(&out));
}

/* No call has the length 0, whatever is written. */
NAOS_TEST(no_call_is_empty)
{
    sink_t sink;
    argx_out_t out;

    sink_init(&sink);
    argx_out_init(&out, sink_write, &sink);

    argx_out_text(&out, "", 0);
    argx_out_spaces(&out, 0);
    argx_out_string(&out, "a");
    argx_out_spaces(&out, 0);
    argx_out_pad_to(&out, 1);
    argx_out_string(&out, "b");
    argx_out_spaces(&out, 32);
    argx_out_string(&out, "c");
    argx_out_spaces(&out, 64);
    argx_out_newline(&out);
    argx_out_newline(&out);

    NAOS_CHECK_TRUE(sink.calls > 0);
    NAOS_CHECK_UINT_EQ(sink.empty_calls, 0);
    check_sink(&sink, "ab                                c\n\n");
}

/*
 * After the write function has reported a failure it is not called again,
 * by any of the functions. The columns are counted as before.
 */
NAOS_TEST(failure_stops_the_calls)
{
    sink_t sink;
    argx_out_t out;

    sink_init(&sink);
    sink.fail_at = 2;
    argx_out_init(&out, sink_write, &sink);

    argx_out_string(&out, "one");
    NAOS_CHECK_FALSE(argx_out_failed(&out));
    argx_out_string(&out, "two");
    NAOS_CHECK_TRUE(argx_out_failed(&out));
    NAOS_CHECK_UINT_EQ(sink.calls, 2);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 6);

    argx_out_string(&out, "three");
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 11);
    argx_out_spaces(&out, 2);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 13);
    argx_out_pad_to(&out, 20);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 20);
    argx_out_string(&out, "four");
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 24);
    argx_out_newline(&out);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 0);
    argx_out_string(&out, "five");
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 4);

    NAOS_CHECK_TRUE(argx_out_failed(&out));
    NAOS_CHECK_UINT_EQ(sink.calls, 2);
    check_sink(&sink, "one");
}

/* A failure of the very first call, and a failure of a newline. */
NAOS_TEST(failure_of_first_call_and_of_newline)
{
    sink_t sink;
    argx_out_t out;

    sink_init(&sink);
    sink.fail_at = 1;
    argx_out_init(&out, sink_write, &sink);
    argx_out_string(&out, "one");
    argx_out_newline(&out);
    NAOS_CHECK_TRUE(argx_out_failed(&out));
    NAOS_CHECK_UINT_EQ(sink.calls, 1);
    NAOS_CHECK_UINT_EQ(sink.length, 0);

    sink_init(&sink);
    sink.fail_at = 2;
    argx_out_init(&out, sink_write, &sink);
    argx_out_string(&out, "one");
    argx_out_newline(&out);
    NAOS_CHECK_TRUE(argx_out_failed(&out));
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 0);
    argx_out_string(&out, "two");
    argx_out_newline(&out);
    NAOS_CHECK_UINT_EQ(sink.calls, 2);
    check_sink(&sink, "one");
}

/*
 * When the blanks that wait fail to be written, neither the rest of the
 * blanks nor the text that follows is passed on.
 */
NAOS_TEST(failure_while_writing_the_spaces)
{
    sink_t sink;
    argx_out_t out;

    sink_init(&sink);
    sink.fail_at = 2;
    argx_out_init(&out, sink_write, &sink);

    argx_out_string(&out, "a");
    argx_out_spaces(&out, 100);
    argx_out_string(&out, "b");

    NAOS_CHECK_TRUE(argx_out_failed(&out));
    NAOS_CHECK_UINT_EQ(sink.calls, 2);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 102);
    check_sink(&sink, "a");

    argx_out_string(&out, "c");
    argx_out_newline(&out);
    NAOS_CHECK_UINT_EQ(sink.calls, 2);
}

/* Starting again forgets the failure, the column and the blanks. */
NAOS_TEST(init_forgets_everything)
{
    sink_t sink;
    argx_out_t out;

    sink_init(&sink);
    sink.fail_at = 1;
    argx_out_init(&out, sink_write, &sink);
    argx_out_string(&out, "one");
    argx_out_spaces(&out, 3);
    NAOS_CHECK_TRUE(argx_out_failed(&out));

    sink_init(&sink);
    argx_out_init(&out, sink_write, &sink);
    NAOS_CHECK_FALSE(argx_out_failed(&out));
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 0);
    argx_out_string(&out, "two");
    check_sink(&sink, "two");
}

/* ---- Counting only --------------------------------------------------------- */

/*
 * Without a write function nothing is passed on and nothing fails; the
 * columns are counted exactly as with one.
 */
NAOS_TEST(counting_only_advances_the_column)
{
    argx_out_t out;

    argx_out_init(&out, NULL, NULL);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 0);

    argx_out_string(&out, "--output");
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 8);
    argx_out_spaces(&out, 1);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 9);
    argx_out_text(&out, "<FILE>", 6);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 15);
    argx_out_pad_to(&out, 40);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 40);
    argx_out_pad_to(&out, 10);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 40);
    argx_out_text(&out, "", 0);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 40);
    NAOS_CHECK_FALSE(argx_out_failed(&out));

    argx_out_newline(&out);
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 0);
    argx_out_spaces(&out, 100);
    argx_out_string(&out, "x");
    NAOS_CHECK_UINT_EQ(argx_out_column(&out), 101);
    NAOS_CHECK_FALSE(argx_out_failed(&out));
}

/*
 * The same calls give the same columns with a write function and
 * without one.
 */
static void write_sample(argx_out_t *out, size_t *columns)
{
    argx_out_spaces(out, 2);
    argx_out_string(out, "-v, --verbose");
    columns[0] = argx_out_column(out);
    argx_out_pad_to(out, 20);
    argx_out_string(out, "Say more");
    columns[1] = argx_out_column(out);
    argx_out_spaces(out, 5);
    columns[2] = argx_out_column(out);
    argx_out_newline(out);
    columns[3] = argx_out_column(out);
}

NAOS_TEST(counting_only_matches_writing)
{
    sink_t sink;
    argx_out_t writing;
    argx_out_t counting;
    size_t written[4];
    size_t counted[4];
    size_t i;

    sink_init(&sink);
    argx_out_init(&writing, sink_write, &sink);
    argx_out_init(&counting, NULL, NULL);

    write_sample(&writing, written);
    write_sample(&counting, counted);

    for (i = 0; i < 4; ++i)
    {
        NAOS_CHECK_UINT_EQ(counted[i], written[i]);
    }
    NAOS_CHECK_UINT_EQ(written[1], 28);
    check_sink(&sink, "  -v, --verbose     Say more\n");
}

/* ---- The registration ------------------------------------------------------ */

NAOS_SUITE(column,
    NAOS_ADD_TEST(init_starts_at_column_zero),
    NAOS_ADD_TEST(text_advances_the_column),
    NAOS_ADD_TEST(text_takes_only_its_length),
    NAOS_ADD_TEST(one_byte_is_one_column),
    NAOS_ADD_TEST(empty_text_does_nothing),
    NAOS_ADD_TEST(newline_resets_the_column)
)

NAOS_SUITE(blanks,
    NAOS_ADD_TEST(spaces_wait_for_the_next_text),
    NAOS_ADD_TEST(spaces_add_up),
    NAOS_ADD_TEST(empty_text_keeps_the_spaces_waiting),
    NAOS_ADD_TEST(newline_drops_the_spaces),
    NAOS_ADD_TEST(many_spaces_are_all_written)
)

NAOS_SUITE(padding,
    NAOS_ADD_TEST(pad_to_reaches_the_column),
    NAOS_ADD_TEST(pad_to_boundaries),
    NAOS_ADD_TEST(pad_to_counts_the_waiting_spaces)
)

NAOS_SUITE(write_function,
    NAOS_ADD_TEST(user_data_is_passed_on),
    NAOS_ADD_TEST(no_call_is_empty),
    NAOS_ADD_TEST(failure_stops_the_calls),
    NAOS_ADD_TEST(failure_of_first_call_and_of_newline),
    NAOS_ADD_TEST(failure_while_writing_the_spaces),
    NAOS_ADD_TEST(init_forgets_everything)
)

NAOS_SUITE(counting_only,
    NAOS_ADD_TEST(counting_only_advances_the_column),
    NAOS_ADD_TEST(counting_only_matches_writing)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(column),
    NAOS_ADD_SUITE(blanks),
    NAOS_ADD_SUITE(padding),
    NAOS_ADD_SUITE(write_function),
    NAOS_ADD_SUITE(counting_only)
)
