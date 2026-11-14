/*
 * SPDX-FileCopyrightText: 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * Tests of where the text of a help goes: the write function of
 * argx_help_write() and argx_help_write_usage() of <argx/help.h>,
 * and the functions that put the same texts into an array of the caller,
 * argx_help_format() and argx_help_format_usage().
 *
 * What the tests pin down:
 *
 * - The parts that a write function is given, joined in the order of the
 *   calls, are the text that the array gets. No part has the length 0, and
 *   the user data reaches the write function as it was given.
 * - A write function that reports a failure in its n-th call is called n
 *   times, and the generation returns ARGX_ERROR_WRITE_FAILED.
 * - With a size that is not 0, the array gets the first bytes of the text,
 *   size - 1 at most, and a '\0' behind them; with the size 0 it gets
 *   nothing, and may be NULL. No other byte is written. The length that is
 *   told is the one of the whole text, and a text that is cut is a success.
 * - When an allocation of argx_help_format() fails, the array holds the
 *   empty string, or is untouched with the size 0, and the length is 0.
 *
 * A behaviour that the help and the usage line share is tested for both:
 * they are the two kinds of text of this file.
 *
 * Every test uses the test allocator ("../support/test_allocator.h"). Its
 * state and the allocator that is set are shared by all the tests of this
 * executable. A test starts with an install, destroys what it created, sets
 * the default allocator back before it ends, and ends with check_clean().
 */
#include <argx/argx.h>

#include "../support/test_allocator.h"

#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* The size of the text that a sink holds. */
#define SINK_SIZE ((size_t)1024)

/*
 * The bytes before the array that a format function is given, and the
 * least number of bytes behind the text: they must stay as they were.
 */
#define GUARD_SIZE ((size_t)16)

/* The size of the area that holds the guard bytes and the array. */
#define AREA_SIZE ((size_t)1024)

/*
 * The byte that fills the area before a call. No text of this file has
 * it.
 */
#define SENTINEL ((char)0x7E)

/*
 * The value of a length before a call: no text of this file is that
 * long.
 */
#define LENGTH_UNSET ((size_t)54321)

/* The two texts that the library generates. */
typedef enum text_kind
{
    TEXT_HELP, /* argx_help_write(), argx_help_format() */
    TEXT_USAGE /* argx_help_write_usage(), argx_help_format_usage() */
} text_kind_t;

/* What the write function of the tests has received. */
typedef struct sink
{
    char text[SINK_SIZE]; /* the parts, joined in the order of the calls */
    size_t length;        /* the number of bytes of text */
    size_t calls;         /* the number of calls, the failed one included */
    size_t empty_calls;   /* the calls with the length 0 or without a text */
    size_t fail_at;       /* the call that reports a failure; 0: none */
} sink_t;

/* Starts an empty sink that reports no failure. */
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
    if (length > (sizeof sink->text - sink->length))
    {
        return false;
    }

    memcpy(sink->text + sink->length, text, length);
    sink->length += length;
    return true;
}

/*
 * The end of every test: the test allocator has no live block and saw no
 * violation of the allocator contract.
 */
static void check_clean(void)
{
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_violations(), 0);
}

/* Adds a subcommand with a description; it must succeed. */
static void add_sub(argx_command_t *command, const char *name, const char *description)
{
    argx_command_t *sub = NULL;

    NAOS_REQUIRE_TRUE(argx_command_add_subcommand(command, name, &sub) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(sub != NULL);
    NAOS_REQUIRE_TRUE(argx_command_set_description(sub, description) == ARGX_ERROR_NONE);
}

/*
 * Adds a flag that must succeed. short_name is '\0' for none; description
 * is its description, or NULL to set none.
 */
static void add_flag(argx_command_t *command, char short_name, const char *long_name,
                     const char *description)
{
    argx_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(argx_command_add_flag(command, short_name, long_name, &option) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(option != NULL);
    if (description != NULL)
    {
        NAOS_REQUIRE_TRUE(argx_option_set_description(option, description) == ARGX_ERROR_NONE);
    }
}

/*
 * Adds an option with a value of strings and the display name "FILE"; it
 * must succeed. short_name is '\0' for none. default_value is its default
 * value, or NULL for an option that is required.
 */
static void add_file(argx_command_t *command, char short_name, const char *long_name,
                     const char *default_value, const char *description)
{
    argx_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(argx_command_add_value(command, short_name, long_name, argx_type_string(),
                                             &option) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(option != NULL);
    NAOS_REQUIRE_TRUE(argx_option_set_value_name(option, "FILE") == ARGX_ERROR_NONE);
    if (default_value != NULL)
    {
        NAOS_REQUIRE_TRUE(argx_option_set_default(option, default_value) == ARGX_ERROR_NONE);
    }
    else
    {
        NAOS_REQUIRE_TRUE(argx_option_set_required(option, true) == ARGX_ERROR_NONE);
    }
    NAOS_REQUIRE_TRUE(argx_option_set_description(option, description) == ARGX_ERROR_NONE);
}

/*
 * Installs the test allocator and creates the schema of the tests: a root
 * with a description, two subcommands and four options, validated. With
 * the width 40 its help has all the parts of a help, lines that are broken,
 * an item whose description starts on the next line, and annotations; its
 * usage line has two lines.
 */
static argx_command_t *create_schema(void)
{
    argx_command_t *root = NULL;

    argx_test_allocator_install();
    NAOS_REQUIRE_TRUE(argx_command_create("tool", &root) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(root != NULL);
    NAOS_REQUIRE_TRUE(argx_command_set_description(
                          root, "Shows how the text of a help leaves the library, part by part.") ==
                      ARGX_ERROR_NONE);

    add_sub(root, "add", "Add a thing");
    add_sub(root, "remove", "Remove a thing\nfor ever");

    add_flag(root, 'v', "verbose", "Say more about every step of the work");
    add_file(root, 'o', "output", "out.txt", "Write the result to FILE");
    add_file(root, '\0', "config", NULL, "Read the settings from FILE");
    add_flag(root, '\0', "quiet", NULL);

    NAOS_REQUIRE_TRUE(argx_command_validate(root, NULL, NULL, NULL) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_is_validated(root));
    return root;
}

/* Creates the help options of the tests: the width 40. */
static argx_help_options_t *create_narrow(void)
{
    argx_help_options_t *options = NULL;

    NAOS_REQUIRE_TRUE(argx_help_options_create(&options) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(options != NULL);
    argx_help_options_set_width(options, 40);
    return options;
}

/*
 * Destroys the help options and the schema, and ends the test: nothing
 * stays allocated.
 */
static void destroy_schema(argx_command_t *root, argx_help_options_t *options)
{
    argx_help_options_destroy(options);
    argx_command_destroy(root);
    argx_test_allocator_uninstall();
    check_clean();
}

/* The usage line of the schema with the width 40. */
static const char narrow_usage[] = "Usage: tool [OPTIONS] --config <FILE>\n"
                                   "       [COMMAND]\n";

/* The help of the schema with the width 40. */
static const char narrow_help[] = "Usage: tool [OPTIONS] --config <FILE>\n"
                                  "       [COMMAND]\n"
                                  "\n"
                                  "Shows how the text of a help leaves the\n"
                                  "library, part by part.\n"
                                  "\n"
                                  "Commands:\n"
                                  "  add               Add a thing\n"
                                  "  remove            Remove a thing\n"
                                  "\n"
                                  "Options:\n"
                                  "  -v, --verbose     Say more about every\n"
                                  "                    step of the work\n"
                                  "  -o, --output <FILE>\n"
                                  "                    Write the result to\n"
                                  "                    FILE [default:\n"
                                  "                    out.txt]\n"
                                  "      --config <FILE>\n"
                                  "                    Read the settings\n"
                                  "                    from FILE [required]\n"
                                  "      --quiet\n";

/* The usage line of the schema with the default settings. */
static const char default_usage[] = "Usage: tool [OPTIONS] --config <FILE> [COMMAND]\n";

/* Has the text of the kind passed to a write function. */
static argx_error_t write_text(text_kind_t kind, const argx_command_t *command,
                               const argx_help_options_t *options,
                               argx_help_write_fn_t write_fn, void *user_data)
{
    if (kind == TEXT_USAGE)
    {
        return argx_help_write_usage(command, options, write_fn, user_data);
    }
    return argx_help_write(command, options, write_fn, user_data);
}

/* Has the text of the kind put into an array. */
static argx_error_t format_text(text_kind_t kind, const argx_command_t *command,
                                const argx_help_options_t *options, char *buffer, size_t size,
                                size_t *length)
{
    if (kind == TEXT_USAGE)
    {
        return argx_help_format_usage(command, options, buffer, size, length);
    }
    return argx_help_format(command, options, buffer, size, length);
}

/*
 * Has the whole text of the kind written to sink; it must succeed, with a
 * call or more and no call without a text.
 */
static void collect(text_kind_t kind, const argx_command_t *command,
                    const argx_help_options_t *options, sink_t *sink)
{
    sink_init(sink);
    NAOS_REQUIRE_TRUE(write_text(kind, command, options, sink_write, sink) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(sink->calls > 0);
    NAOS_REQUIRE_TRUE(sink->length > 0);
    NAOS_REQUIRE_TRUE(sink->length < (AREA_SIZE - (2 * GUARD_SIZE)));
    NAOS_REQUIRE_TRUE(memchr(sink->text, SENTINEL, sink->length) == NULL);
}

/* How check_format() calls a format function. */
typedef struct format_call
{
    size_t size;      /* the size that is given */
    bool null_buffer; /* whether NULL is given as the array; only with the size 0 */
    bool with_length; /* whether a place for the length is given */
} format_call_t;

/*
 * Has the text of the kind put into an array of call.size bytes that
 * stands in the middle of an area of sentinel bytes, and checks all of the
 * area: the array holds the first bytes of the text that fit and a '\0'
 * behind them, or nothing with the size 0; every other byte of the area,
 * before the array, behind the '\0' and behind the array, is as it was.
 * The call succeeds, and the length, when it is asked for, is the length of
 * the whole text. Nothing stays allocated.
 *
 * text and text_length are the whole text that is expected.
 */
static void check_format(text_kind_t kind, const argx_command_t *command,
                         const argx_help_options_t *options, const char *text, size_t text_length,
                         format_call_t call)
{
    char area[AREA_SIZE];
    char want[AREA_SIZE];
    size_t length = LENGTH_UNSET;
    size_t live = argx_test_allocator_live();
    argx_error_t error;

    NAOS_REQUIRE_TRUE(call.size <= (AREA_SIZE - (2 * GUARD_SIZE)));
    NAOS_REQUIRE_TRUE(!call.null_buffer || (call.size == 0));

    memset(area, SENTINEL, sizeof area);
    memset(want, SENTINEL, sizeof want);
    if (call.size != 0)
    {
        size_t written = (text_length < (call.size - 1)) ? text_length : (call.size - 1);

        memcpy(want + GUARD_SIZE, text, written);
        want[GUARD_SIZE + written] = '\0';
    }

    error = format_text(kind, command, options, call.null_buffer ? NULL : (area + GUARD_SIZE),
                        call.size, call.with_length ? &length : NULL);

    NAOS_CHECK_TRUE(error == ARGX_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(length, call.with_length ? text_length : LENGTH_UNSET);
    NAOS_CHECK_MEM_EQ(area, sizeof area, want, sizeof want);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), live);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_violations(), 0);
}

/*
 * Checks the format function of the kind with the size given: with and
 * without a place for the length, and for the size 0 also with NULL as the
 * array. The text that is expected is the one that a write function gets.
 */
static void check_size(text_kind_t kind, const argx_command_t *command,
                       const argx_help_options_t *options, size_t size)
{
    sink_t sink;
    format_call_t call;
    int with_length;

    collect(kind, command, options, &sink);

    for (with_length = 0; with_length < 2; ++with_length)
    {
        call.size = size;
        call.null_buffer = false;
        call.with_length = with_length != 0;
        check_format(kind, command, options, sink.text, sink.length, call);

        if (size == 0)
        {
            call.null_buffer = true;
            check_format(kind, command, options, sink.text, sink.length, call);
        }
    }
}

/* The length of the text of the kind, as a write function gets it. */
static size_t text_length(text_kind_t kind, const argx_command_t *command,
                          const argx_help_options_t *options)
{
    sink_t sink;

    collect(kind, command, options, &sink);
    return sink.length;
}

/*
 * Runs check_size() for both kinds, with the narrow help options and with
 * none, with the size that the length of the text and delta give: the
 * length + delta, or delta alone when from_length is false.
 */
static void check_sizes(bool from_length, size_t delta)
{
    argx_command_t *root = create_schema();
    argx_help_options_t *narrow = create_narrow();
    const argx_help_options_t *settings[2];
    size_t setting;
    int kind;

    settings[0] = narrow;
    settings[1] = NULL;

    for (setting = 0; setting < 2; ++setting)
    {
        for (kind = TEXT_HELP; kind <= TEXT_USAGE; ++kind)
        {
            size_t base =
                from_length ? text_length((text_kind_t)kind, root, settings[setting]) : 0;

            check_size((text_kind_t)kind, root, settings[setting], base + delta);
        }
    }

    destroy_schema(root, narrow);
}

/* ---- The texts ----------------------------------------------------------- */

NAOS_TEST(format_gives_the_help_as_a_string)
{
    argx_command_t *root = create_schema();
    argx_help_options_t *narrow = create_narrow();
    char buffer[SINK_SIZE];
    size_t length = LENGTH_UNSET;

    memset(buffer, SENTINEL, sizeof buffer);
    NAOS_CHECK_TRUE(argx_help_format(root, narrow, buffer, sizeof buffer, &length) ==
                    ARGX_ERROR_NONE);

    NAOS_CHECK_UINT_EQ(length, strlen(narrow_help));
    NAOS_REQUIRE_TRUE(memchr(buffer, '\0', sizeof buffer) != NULL);
    NAOS_CHECK_MEM_EQ(buffer, strlen(buffer), narrow_help, strlen(narrow_help));

    destroy_schema(root, narrow);
}

NAOS_TEST(format_usage_gives_the_usage_line_as_a_string)
{
    argx_command_t *root = create_schema();
    argx_help_options_t *narrow = create_narrow();
    char buffer[SINK_SIZE];
    size_t length = LENGTH_UNSET;

    memset(buffer, SENTINEL, sizeof buffer);
    NAOS_CHECK_TRUE(argx_help_format_usage(root, narrow, buffer, sizeof buffer, &length) ==
                    ARGX_ERROR_NONE);

    NAOS_CHECK_UINT_EQ(length, strlen(narrow_usage));
    NAOS_REQUIRE_TRUE(memchr(buffer, '\0', sizeof buffer) != NULL);
    NAOS_CHECK_MEM_EQ(buffer, strlen(buffer), narrow_usage, strlen(narrow_usage));

    /* Without help options the settings have their default values. */
    length = LENGTH_UNSET;
    memset(buffer, SENTINEL, sizeof buffer);
    NAOS_CHECK_TRUE(argx_help_format_usage(root, NULL, buffer, sizeof buffer, &length) ==
                    ARGX_ERROR_NONE);

    NAOS_CHECK_UINT_EQ(length, strlen(default_usage));
    NAOS_REQUIRE_TRUE(memchr(buffer, '\0', sizeof buffer) != NULL);
    NAOS_CHECK_MEM_EQ(buffer, strlen(buffer), default_usage, strlen(default_usage));

    destroy_schema(root, narrow);
}

/* ---- The write function -------------------------------------------------- */

NAOS_TEST(parts_of_the_write_function_joined_are_the_text_of_the_buffer)
{
    argx_command_t *root = create_schema();
    argx_help_options_t *narrow = create_narrow();
    const argx_help_options_t *settings[2];
    size_t setting;
    int kind;

    settings[0] = narrow;
    settings[1] = NULL;

    for (setting = 0; setting < 2; ++setting)
    {
        for (kind = TEXT_HELP; kind <= TEXT_USAGE; ++kind)
        {
            sink_t sink;
            char buffer[SINK_SIZE];
            size_t length = LENGTH_UNSET;

            collect((text_kind_t)kind, root, settings[setting], &sink);
            /* The text comes in several parts. */
            NAOS_CHECK_TRUE(sink.calls > 1);

            memset(buffer, SENTINEL, sizeof buffer);
            NAOS_CHECK_TRUE(format_text((text_kind_t)kind, root, settings[setting], buffer,
                                        sizeof buffer, &length) == ARGX_ERROR_NONE);

            NAOS_CHECK_UINT_EQ(length, sink.length);
            NAOS_REQUIRE_TRUE(memchr(buffer, '\0', sizeof buffer) != NULL);
            NAOS_CHECK_MEM_EQ(buffer, strlen(buffer), sink.text, sink.length);
        }
    }

    /* The parts are the texts that are expected. */
    {
        sink_t sink;

        collect(TEXT_HELP, root, narrow, &sink);
        NAOS_CHECK_MEM_EQ(sink.text, sink.length, narrow_help, strlen(narrow_help));
        collect(TEXT_USAGE, root, narrow, &sink);
        NAOS_CHECK_MEM_EQ(sink.text, sink.length, narrow_usage, strlen(narrow_usage));
    }

    destroy_schema(root, narrow);
}

/* What the write function of the test of the user data has seen. */
typedef struct probe
{
    const void *self;  /* the pointer that was given as the user data */
    size_t calls;      /* the number of calls */
    size_t mismatches; /* the calls whose user data was another pointer */
} probe_t;

/*
 * Counts the call in the probe that user_data points to, and whether
 * user_data is the address that the test wrote into the probe.
 */
static bool probe_write(void *user_data, const char *text, size_t length)
{
    probe_t *probe = user_data;

    (void)text;
    (void)length;
    ++probe->calls;
    if (probe->self != user_data)
    {
        ++probe->mismatches;
    }
    return true;
}

/*
 * Takes every part, and reports a failure when the user data is not
 * NULL.
 */
static bool null_user_data_write(void *user_data, const char *text, size_t length)
{
    (void)text;
    (void)length;
    return user_data == NULL;
}

NAOS_TEST(user_data_reaches_the_write_function_unchanged)
{
    argx_command_t *root = create_schema();
    argx_help_options_t *narrow = create_narrow();
    int kind;

    for (kind = TEXT_HELP; kind <= TEXT_USAGE; ++kind)
    {
        probe_t probe;

        probe.self = &probe;
        probe.calls = 0;
        probe.mismatches = 0;

        NAOS_CHECK_TRUE(write_text((text_kind_t)kind, root, narrow, probe_write, &probe) ==
                        ARGX_ERROR_NONE);
        NAOS_CHECK_TRUE(probe.calls > 1);
        NAOS_CHECK_UINT_EQ(probe.mismatches, 0);
        NAOS_CHECK_TRUE(probe.self == &probe);

        /* NULL is passed on as NULL. */
        NAOS_CHECK_TRUE(write_text((text_kind_t)kind, root, narrow, null_user_data_write, NULL) ==
                        ARGX_ERROR_NONE);
    }

    destroy_schema(root, narrow);
}

NAOS_TEST(write_function_is_never_called_with_the_length_0)
{
    argx_command_t *root = create_schema();
    argx_help_options_t *narrow = create_narrow();
    const argx_help_options_t *settings[2];
    size_t setting;
    int kind;

    settings[0] = narrow;
    settings[1] = NULL;

    for (setting = 0; setting < 2; ++setting)
    {
        for (kind = TEXT_HELP; kind <= TEXT_USAGE; ++kind)
        {
            sink_t sink;

            collect((text_kind_t)kind, root, settings[setting], &sink);
            NAOS_CHECK_UINT_EQ(sink.empty_calls, 0);
        }
    }

    destroy_schema(root, narrow);
}

NAOS_TEST(failure_in_the_nth_call_stops_the_generation_after_n_calls)
{
    argx_command_t *root = create_schema();
    argx_help_options_t *narrow = create_narrow();
    size_t live = argx_test_allocator_live();
    int kind;

    for (kind = TEXT_HELP; kind <= TEXT_USAGE; ++kind)
    {
        sink_t whole;
        size_t n;

        collect((text_kind_t)kind, root, narrow, &whole);

        for (n = 1; n <= whole.calls; ++n)
        {
            sink_t sink;

            sink_init(&sink);
            sink.fail_at = n;

            NAOS_CHECK_TRUE(write_text((text_kind_t)kind, root, narrow, sink_write, &sink) ==
                            ARGX_ERROR_WRITE_FAILED);

            /* The failed call is the last one. */
            NAOS_CHECK_UINT_EQ(sink.calls, n);
            NAOS_CHECK_UINT_EQ(sink.empty_calls, 0);
            /* What was taken before is the start of the text. */
            NAOS_CHECK_TRUE(sink.length < whole.length);
            NAOS_CHECK_MEM_EQ(sink.text, sink.length, whole.text, sink.length);
            NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), live);
            NAOS_CHECK_UINT_EQ(argx_test_allocator_violations(), 0);
        }

        /* A failure that would come behind the last call is never seen. */
        {
            sink_t sink;

            sink_init(&sink);
            sink.fail_at = whole.calls + 1;
            NAOS_CHECK_TRUE(write_text((text_kind_t)kind, root, narrow, sink_write, &sink) ==
                            ARGX_ERROR_NONE);
            NAOS_CHECK_UINT_EQ(sink.calls, whole.calls);
            NAOS_CHECK_MEM_EQ(sink.text, sink.length, whole.text, whole.length);
        }
    }

    NAOS_CHECK_TRUE(argx_command_is_validated(root));
    destroy_schema(root, narrow);
}

/* ---- The buffer ---------------------------------------------------------- */

NAOS_TEST(buffer_that_fits_exactly_holds_the_whole_text)
{
    /* The length of the text and the '\0'. */
    check_sizes(true, 1);
}

NAOS_TEST(buffer_with_room_to_spare_is_written_up_to_the_terminator_only)
{
    check_sizes(true, 40);
}

NAOS_TEST(buffer_one_byte_short_loses_the_last_byte_and_succeeds)
{
    /* The length of the text: no room for the '\0' behind the last byte. */
    check_sizes(true, 0);
}

NAOS_TEST(buffer_cut_in_the_middle_holds_the_start_of_the_text)
{
    check_sizes(false, 2);
    check_sizes(false, 10);
    check_sizes(false, 60);
}

NAOS_TEST(buffer_of_size_1_holds_the_empty_string)
{
    check_sizes(false, 1);
}

NAOS_TEST(size_0_writes_nothing_and_takes_null)
{
    check_sizes(false, 0);
}

NAOS_TEST(two_calls_get_the_whole_text)
{
    argx_command_t *root = create_schema();
    argx_help_options_t *narrow = create_narrow();
    int kind;

    for (kind = TEXT_HELP; kind <= TEXT_USAGE; ++kind)
    {
        const char *expected = (kind == TEXT_HELP) ? narrow_help : narrow_usage;
        char buffer[SINK_SIZE];
        size_t length = LENGTH_UNSET;
        size_t second = LENGTH_UNSET;

        /* The first call tells the length; the text was cut. */
        NAOS_CHECK_TRUE(format_text((text_kind_t)kind, root, narrow, NULL, 0, &length) ==
                        ARGX_ERROR_NONE);
        NAOS_CHECK_UINT_EQ(length, strlen(expected));
        NAOS_REQUIRE_TRUE((length + 1) <= sizeof buffer);

        /* The second call, with the length + 1 bytes, gets all of it. */
        memset(buffer, SENTINEL, sizeof buffer);
        NAOS_CHECK_TRUE(format_text((text_kind_t)kind, root, narrow, buffer, length + 1, &second) ==
                        ARGX_ERROR_NONE);
        NAOS_CHECK_UINT_EQ(second, length);
        NAOS_CHECK_TRUE(second < (length + 1));
        NAOS_CHECK_TRUE(buffer[length] == '\0');
        NAOS_CHECK_MEM_EQ(buffer, strlen(buffer), expected, strlen(expected));
    }

    destroy_schema(root, narrow);
}

/* ---- Memory -------------------------------------------------------------- */

NAOS_TEST(format_leaves_nothing_allocated_and_format_usage_allocates_nothing)
{
    argx_command_t *root = create_schema();
    argx_help_options_t *narrow = create_narrow();
    size_t live = argx_test_allocator_live();
    size_t sizes[4];
    size_t index;

    sizes[0] = 0;
    sizes[1] = 1;
    sizes[2] = 30;
    sizes[3] = SINK_SIZE;

    for (index = 0; index < 4; ++index)
    {
        char buffer[SINK_SIZE];
        size_t length = LENGTH_UNSET;
        size_t requests = argx_test_allocator_requests();

        NAOS_CHECK_TRUE(argx_help_format_usage(root, narrow, buffer, sizes[index], &length) ==
                        ARGX_ERROR_NONE);
        NAOS_CHECK_UINT_EQ(length, strlen(narrow_usage));
        NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), requests);
        NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), live);

        length = LENGTH_UNSET;
        NAOS_CHECK_TRUE(argx_help_format(root, narrow, buffer, sizes[index], &length) ==
                        ARGX_ERROR_NONE);
        NAOS_CHECK_UINT_EQ(length, strlen(narrow_help));
        /* The help of this schema needs memory, and gives it back. */
        NAOS_CHECK_TRUE(argx_test_allocator_requests() > requests);
        NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), live);
    }

    NAOS_CHECK_UINT_EQ(argx_test_allocator_violations(), 0);
    destroy_schema(root, narrow);
}

/*
 * Makes the first allocation of argx_help_format() fail, with an array of
 * call.size bytes in the middle of an area of sentinel bytes. The call
 * returns ARGX_ERROR_OUT_OF_MEMORY; the array holds the empty string, or
 * is untouched with the size 0; no byte outside the array is written; the
 * length, when it is asked for, is 0; nothing stays allocated.
 */
static void check_out_of_memory(const argx_command_t *command,
                                const argx_help_options_t *options, format_call_t call)
{
    char area[AREA_SIZE];
    char want[AREA_SIZE];
    size_t length = LENGTH_UNSET;
    size_t live = argx_test_allocator_live();
    argx_error_t error;

    NAOS_REQUIRE_TRUE(call.size <= (AREA_SIZE - (2 * GUARD_SIZE)));
    NAOS_REQUIRE_TRUE(!call.null_buffer || (call.size == 0));

    memset(area, SENTINEL, sizeof area);
    memset(want, SENTINEL, sizeof want);

    argx_test_allocator_fail_at(argx_test_allocator_requests() + 1);
    error = argx_help_format(command, options, call.null_buffer ? NULL : (area + GUARD_SIZE),
                             call.size, call.with_length ? &length : NULL);
    argx_test_allocator_fail_at(0);

    NAOS_CHECK_TRUE(error == ARGX_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_UINT_EQ(length, call.with_length ? (size_t)0 : LENGTH_UNSET);

    if (call.size != 0)
    {
        /*
         * The empty string. The bytes of the array behind its '\0' are not
         * specified: the start of the text may have been there.
         */
        NAOS_CHECK_TRUE(area[GUARD_SIZE] == '\0');
    }
    NAOS_CHECK_MEM_EQ(area, GUARD_SIZE, want, GUARD_SIZE);
    NAOS_CHECK_MEM_EQ(area + GUARD_SIZE + call.size, sizeof area - GUARD_SIZE - call.size,
                      want + GUARD_SIZE + call.size, sizeof want - GUARD_SIZE - call.size);

    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), live);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_violations(), 0);
}

NAOS_TEST(format_out_of_memory_empties_the_buffer_and_the_length)
{
    argx_command_t *root = create_schema();
    argx_help_options_t *narrow = create_narrow();
    size_t whole = strlen(narrow_help);
    format_call_t call;
    int with_length;

    for (with_length = 0; with_length < 2; ++with_length)
    {
        call.with_length = with_length != 0;
        call.null_buffer = false;

        /*
         * Room for all of the text: its start was written before the
         * allocation failed.
         */
        call.size = whole + 1;
        check_out_of_memory(root, narrow, call);
        /* Room for a part of it. */
        call.size = 20;
        check_out_of_memory(root, narrow, call);
        call.size = 1;
        check_out_of_memory(root, narrow, call);

        /* The size 0: nothing is written. */
        call.size = 0;
        check_out_of_memory(root, narrow, call);
        call.null_buffer = true;
        check_out_of_memory(root, narrow, call);
    }

    /*
     * The schema and the help options are as they were: the next call
     * succeeds and gives the whole text.
     */
    {
        char buffer[SINK_SIZE];
        size_t length = LENGTH_UNSET;

        NAOS_CHECK_TRUE(argx_command_is_validated(root));
        memset(buffer, SENTINEL, sizeof buffer);
        NAOS_CHECK_TRUE(argx_help_format(root, narrow, buffer, sizeof buffer, &length) ==
                        ARGX_ERROR_NONE);
        NAOS_CHECK_UINT_EQ(length, whole);
        NAOS_REQUIRE_TRUE(memchr(buffer, '\0', sizeof buffer) != NULL);
        NAOS_CHECK_MEM_EQ(buffer, strlen(buffer), narrow_help, whole);
    }

    destroy_schema(root, narrow);
}

NAOS_SUITE(help_output,
    NAOS_ADD_TEST(format_gives_the_help_as_a_string),
    NAOS_ADD_TEST(format_usage_gives_the_usage_line_as_a_string),
    NAOS_ADD_TEST(parts_of_the_write_function_joined_are_the_text_of_the_buffer),
    NAOS_ADD_TEST(user_data_reaches_the_write_function_unchanged),
    NAOS_ADD_TEST(write_function_is_never_called_with_the_length_0),
    NAOS_ADD_TEST(failure_in_the_nth_call_stops_the_generation_after_n_calls),
    NAOS_ADD_TEST(buffer_that_fits_exactly_holds_the_whole_text),
    NAOS_ADD_TEST(buffer_with_room_to_spare_is_written_up_to_the_terminator_only),
    NAOS_ADD_TEST(buffer_one_byte_short_loses_the_last_byte_and_succeeds),
    NAOS_ADD_TEST(buffer_cut_in_the_middle_holds_the_start_of_the_text),
    NAOS_ADD_TEST(buffer_of_size_1_holds_the_empty_string),
    NAOS_ADD_TEST(size_0_writes_nothing_and_takes_null),
    NAOS_ADD_TEST(two_calls_get_the_whole_text),
    NAOS_ADD_TEST(format_leaves_nothing_allocated_and_format_usage_allocates_nothing),
    NAOS_ADD_TEST(format_out_of_memory_empties_the_buffer_and_the_length)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(help_output)
)
