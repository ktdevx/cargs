/*
 * SPDX-FileCopyrightText: 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * Tests of the public parsing API for wide arguments: argx_parse_wide()
 * of <argx/parse.h>.
 *
 * A wide command line gives what the same command line in UTF-8 gives with
 * argx_parse(): the selected commands, the values, and every field of a
 * parse error. The tests parse both and compare the two results, and they
 * also compare the result of the wide command line with the bytes that are
 * expected, so that the two entries cannot be wrong in the same way
 * unnoticed.
 *
 * A wide argument with a sequence that cannot be converted to UTF-8 is the
 * parse error ARGX_PARSE_ERROR_INVALID_ENCODING. A wide string is UTF-16
 * where WCHAR_MAX <= 0xFFFF and UTF-32 elsewhere, so the units that cannot
 * be converted differ between the two: they are chosen with the same
 * condition as in the library, and every test runs checks in both.
 *
 * argx_parse() does not look at the bytes of its arguments: bytes that
 * are not UTF-8 are a value like any other.
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
#include <stdint.h>
#include <string.h>

/* The number of arguments of an argv array. */
#define ARGC(argv) ((int)(sizeof(argv) / sizeof((argv)[0])))

/* The bytes of U+FFFD, which replaces a unit that cannot be converted. */
#define REPLACEMENT "\xEF\xBF\xBD"

/*
 * Characters of 2, 3 and 4 bytes in UTF-8: U+00E9, U+3042 and U+1F600. The
 * wide ones are written as universal character names, which the compiler
 * turns into the units of its wchar_t: U+1F600 is a pair of surrogates
 * where wchar_t has 16 bits.
 */
#define W_2 L"\u00E9"
#define W_3 L"\u3042"
#define W_4 L"\U0001F600"
#define U_2 "\xC3\xA9"
#define U_3 "\xE3\x81\x82"
#define U_4 "\xF0\x9F\x98\x80"

/*
 * The units that cannot be converted, each of them when it stands between
 * two letters.
 */
#if WCHAR_MAX <= 0xFFFF
/* UTF-16: a surrogate that is not part of a pair. */
static const wchar_t bad_units[] = {
    (wchar_t)0xD800, /* the first high surrogate, with no low surrogate after it */
    (wchar_t)0xDBFF, /* the last high surrogate */
    (wchar_t)0xDC00, /* the first low surrogate, with no high surrogate before it */
    (wchar_t)0xDFFF  /* the last low surrogate */
};
#else
/*
 * UTF-32: a value in the range of the surrogates, a value above U+10FFFF,
 * and a negative value (a large one where wchar_t has no sign).
 */
static const wchar_t bad_units[] = {
    (wchar_t)0xD800,
    (wchar_t)0xDFFF,
    (wchar_t)0x110000,
    (wchar_t)-1
};
#endif

#define BAD_UNIT_COUNT (sizeof(bad_units) / sizeof(bad_units[0]))

/* One of them, for the tests that need only one. */
#define BAD_UNIT (bad_units[0])

/*
 * The end of every test: the test allocator has no live block and saw no
 * violation of the allocator contract.
 */
static void check_clean(void)
{
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_violations(), 0);
}

/* ---- The schema of the tests --------------------------------------------- */

/*
 * tool [-v|--verbose] [-d|--debug]... [-c|--color|--no-color] [-n|--num INT]
 *      [-o|--name STRING]
 *   run [-t|--tag STRING] [FILE]...
 *
 * --color is on at first. --tag has the default value "none".
 */
typedef struct fixture
{
    argx_command_t *root;
    argx_option_t *verbose;
    argx_option_t *debug;
    argx_option_t *color;
    argx_option_t *num;
    argx_option_t *name;
    argx_command_t *run;
    argx_option_t *tag;
    argx_positional_t *file;
} fixture_t;

/* Installs the test allocator and creates the schema. */
static void set_up(fixture_t *fixture)
{
    argx_test_allocator_install();

    NAOS_REQUIRE_TRUE(argx_command_create("tool", &fixture->root) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_flag(fixture->root, 'v', "verbose", &fixture->verbose) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_count(fixture->root, 'd', "debug", &fixture->debug) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_negatable(fixture->root, 'c', "color", &fixture->color) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_option_set_initial(fixture->color, true) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_value(fixture->root, 'n', "num", argx_type_int(), &fixture->num) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_value(fixture->root, 'o', "name", argx_type_string(),
                                             &fixture->name) == ARGX_ERROR_NONE);

    NAOS_REQUIRE_TRUE(argx_command_add_subcommand(fixture->root, "run", &fixture->run) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_value(fixture->run, 't', "tag", argx_type_string(),
                                             &fixture->tag) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_option_set_default(fixture->tag, "none") == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_positional(fixture->run, "FILE", argx_type_string(),
                                                  ARGX_ARITY_ZERO_OR_MORE,
                                                  &fixture->file) == ARGX_ERROR_NONE);

    NAOS_REQUIRE_TRUE(argx_command_validate(fixture->root, NULL, NULL, NULL) == ARGX_ERROR_NONE);
}

/* Destroys the schema and ends the test: nothing stays allocated. */
static void tear_down(fixture_t *fixture)
{
    argx_command_destroy(fixture->root);
    argx_test_allocator_uninstall();
    check_clean();
}

/* ---- Parsing -------------------------------------------------------------- */

/*
 * Parses the wide argv, which must match the schema. Returns the result,
 * which the caller destroys.
 */
static argx_result_t *parse_wide_ok(const fixture_t *fixture, int argc, wchar_t *const *argv)
{
    argx_result_t *result = NULL;

    NAOS_REQUIRE_TRUE(argx_parse_wide(fixture->root, argc, argv, &result) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(result != NULL);
    NAOS_CHECK_INT_EQ(argx_result_error(result), ARGX_ERROR_NONE);
    return result;
}

/*
 * Checks that text is expected: NULL, or a string with the same
 * characters.
 */
static void check_text(const char *text, const char *expected)
{
    if (expected == NULL)
    {
        NAOS_CHECK_TRUE(text == NULL);
    }
    else
    {
        NAOS_REQUIRE_TRUE(text != NULL);
        NAOS_CHECK_STR_EQ(text, expected);
    }
}

/* Checks that option has the string expected as its value. */
static void check_option_string(const argx_result_t *result, const argx_option_t *option,
                                const char *expected)
{
    const char *text = NULL;

    NAOS_REQUIRE_TRUE(argx_result_option_string(result, option, &text));
    NAOS_REQUIRE_TRUE(text != NULL);
    NAOS_CHECK_STR_EQ(text, expected);
}

/* Checks that the value of positional at index is the string expected. */
static void check_positional_string(const argx_result_t *result, const argx_positional_t *positional,
                                    size_t index, const char *expected)
{
    const char *text = NULL;

    NAOS_REQUIRE_TRUE(argx_result_positional_string(result, positional, index, &text));
    NAOS_REQUIRE_TRUE(text != NULL);
    NAOS_CHECK_STR_EQ(text, expected);
}

/* ---- Comparing two results ----------------------------------------------- */

/* The option with a string as its value reads the same from both. */
static void check_same_string(const argx_result_t *wide, const argx_result_t *narrow,
                              const argx_option_t *option)
{
    const char *wide_text = NULL;
    const char *narrow_text = NULL;
    const bool wide_has = argx_result_option_string(wide, option, &wide_text);
    const bool narrow_has = argx_result_option_string(narrow, option, &narrow_text);

    NAOS_CHECK_TRUE(wide_has == narrow_has);
    check_text(wide_text, narrow_text);
    NAOS_CHECK_INT_EQ(argx_result_option_source(wide, option), argx_result_option_source(narrow, option));
}

/*
 * wide, a result of argx_parse_wide(), reads the same as narrow, a result
 * of argx_parse(): the selected commands, every option and positional of
 * the schema, and every field of the parse error.
 */
static void check_same_results(const fixture_t *fixture, const argx_result_t *wide,
                               const argx_result_t *narrow)
{
    int64_t wide_number = 0;
    int64_t narrow_number = 0;
    bool wide_has;
    bool narrow_has;
    size_t count;
    size_t i;

    /* The selected commands. */
    count = argx_result_command_count(narrow);
    NAOS_REQUIRE_TRUE(argx_result_command_count(wide) == count);
    for (i = 0; i < count; ++i)
    {
        NAOS_CHECK_TRUE(argx_result_command_at(wide, i) == argx_result_command_at(narrow, i));
    }
    NAOS_CHECK_TRUE(argx_result_command(wide) == argx_result_command(narrow));

    /* The options. */
    NAOS_CHECK_TRUE(argx_result_flag(wide, fixture->verbose) == argx_result_flag(narrow, fixture->verbose));
    NAOS_CHECK_INT_EQ(argx_result_option_source(wide, fixture->verbose),
                      argx_result_option_source(narrow, fixture->verbose));
    NAOS_CHECK_UINT_EQ(argx_result_count(wide, fixture->debug), argx_result_count(narrow, fixture->debug));
    NAOS_CHECK_INT_EQ(argx_result_option_source(wide, fixture->debug),
                      argx_result_option_source(narrow, fixture->debug));
    NAOS_CHECK_TRUE(argx_result_flag(wide, fixture->color) == argx_result_flag(narrow, fixture->color));
    NAOS_CHECK_INT_EQ(argx_result_option_source(wide, fixture->color),
                      argx_result_option_source(narrow, fixture->color));

    wide_has = argx_result_option_int(wide, fixture->num, &wide_number);
    narrow_has = argx_result_option_int(narrow, fixture->num, &narrow_number);
    NAOS_CHECK_TRUE(wide_has == narrow_has);
    NAOS_CHECK_TRUE(wide_number == narrow_number);
    NAOS_CHECK_INT_EQ(argx_result_option_source(wide, fixture->num),
                      argx_result_option_source(narrow, fixture->num));

    check_same_string(wide, narrow, fixture->name);
    check_same_string(wide, narrow, fixture->tag);

    /* The positional. */
    count = argx_result_positional_count(narrow, fixture->file);
    NAOS_REQUIRE_TRUE(argx_result_positional_count(wide, fixture->file) == count);
    NAOS_CHECK_INT_EQ(argx_result_positional_source(wide, fixture->file),
                      argx_result_positional_source(narrow, fixture->file));
    for (i = 0; i < count; ++i)
    {
        const char *narrow_text = NULL;

        NAOS_REQUIRE_TRUE(argx_result_positional_string(narrow, fixture->file, i, &narrow_text));
        check_positional_string(wide, fixture->file, i, narrow_text);
    }

    /* The parse error. */
    NAOS_CHECK_INT_EQ(argx_result_error(wide), argx_result_error(narrow));
    NAOS_CHECK_INT_EQ(argx_result_error_kind(wide), argx_result_error_kind(narrow));
    check_text(argx_result_error_argument(wide), argx_result_error_argument(narrow));
    check_text(argx_result_error_value(wide), argx_result_error_value(narrow));
    NAOS_CHECK_UINT_EQ(argx_result_error_index(wide), argx_result_error_index(narrow));
    NAOS_CHECK_INT_EQ(argx_result_error_source(wide), argx_result_error_source(narrow));
    NAOS_CHECK_TRUE(argx_result_error_command(wide) == argx_result_error_command(narrow));
    NAOS_CHECK_TRUE(argx_result_error_option(wide) == argx_result_error_option(narrow));
    NAOS_CHECK_TRUE(argx_result_error_positional(wide) == argx_result_error_positional(narrow));
    NAOS_CHECK_INT_EQ(argx_result_error_short_name(wide), argx_result_error_short_name(narrow));
    check_text(argx_result_error_message(wide), argx_result_error_message(narrow));
}

/*
 * Parses the wide argv and the same command line in UTF-8, argv, which has
 * as many arguments. Both parses return expected and give results that
 * read the same. Returns the result of the wide argv, which the caller
 * destroys.
 */
static argx_result_t *parse_both(const fixture_t *fixture, int argc, wchar_t *const *wide_argv,
                                 char *const *argv, argx_error_t expected)
{
    argx_result_t *wide = NULL;
    argx_result_t *narrow = NULL;

    NAOS_REQUIRE_TRUE(argx_parse_wide(fixture->root, argc, wide_argv, &wide) == expected);
    NAOS_REQUIRE_TRUE(wide != NULL);
    NAOS_REQUIRE_TRUE(argx_parse(fixture->root, argc, argv, &narrow) == expected);
    NAOS_REQUIRE_TRUE(narrow != NULL);

    check_same_results(fixture, wide, narrow);
    argx_result_destroy(narrow);
    return wide;
}

/* ---- A wide argument that cannot be converted ---------------------------- */

/*
 * Checks every field of the parse error of result, which is that of the
 * argument at index, argument in UTF-8 with U+FFFD for each unit that
 * could not be converted, found while command was the current command.
 */
static void check_invalid_encoding(const argx_result_t *result, const char *argument, size_t index,
                                   const argx_command_t *command, const char *message)
{
    NAOS_CHECK_INT_EQ(argx_result_error(result), ARGX_ERROR_INVALID_COMMAND_LINE);
    NAOS_CHECK_INT_EQ(argx_result_error_kind(result), ARGX_PARSE_ERROR_INVALID_ENCODING);
    check_text(argx_result_error_argument(result), argument);
    check_text(argx_result_error_value(result), argument);
    NAOS_CHECK_UINT_EQ(argx_result_error_index(result), index);
    NAOS_CHECK_INT_EQ(argx_result_error_source(result), ARGX_SOURCE_COMMAND_LINE);
    NAOS_CHECK_TRUE(argx_result_error_command(result) == command);
    NAOS_CHECK_TRUE(argx_result_error_option(result) == NULL);
    NAOS_CHECK_TRUE(argx_result_error_positional(result) == NULL);
    NAOS_CHECK_INT_EQ(argx_result_error_short_name(result), '\0');
    check_text(argx_result_error_message(result), message);
}

/*
 * Parses the wide argv, which must have a parse error. Returns the result,
 * which the caller destroys.
 */
static argx_result_t *parse_wide_error(const fixture_t *fixture, int argc, wchar_t *const *argv)
{
    argx_result_t *result = NULL;

    NAOS_REQUIRE_TRUE(argx_parse_wide(fixture->root, argc, argv, &result) ==
                      ARGX_ERROR_INVALID_COMMAND_LINE);
    NAOS_REQUIRE_TRUE(result != NULL);
    return result;
}

/*
 * Parses the wide argv, which must have the error of an argument that
 * cannot be converted, and checks every field of the error.
 */
static void expect_invalid_encoding(const fixture_t *fixture, int argc, wchar_t *const *argv,
                                    const char *argument, size_t index, const argx_command_t *command,
                                    const char *message)
{
    argx_result_t *result = parse_wide_error(fixture, argc, argv);

    check_invalid_encoding(result, argument, index, command, message);
    argx_result_destroy(result);
}

/* ---- The same as the command line in UTF-8 ------------------------------- */

NAOS_TEST(a_command_line_without_arguments_gives_an_empty_result)
{
    fixture_t fixture;
    wchar_t *wide_argv[] = { L"tool" };
    char *argv[] = { "tool" };
    argx_result_t *result = NULL;

    set_up(&fixture);

    /* argc is 0: argv is not read. */
    NAOS_REQUIRE_TRUE(argx_parse_wide(fixture.root, 0, NULL, &result) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(result != NULL);
    NAOS_CHECK_INT_EQ(argx_result_error(result), ARGX_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(argx_result_command_count(result), 1);
    NAOS_CHECK_TRUE(argx_result_command(result) == fixture.root);
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.verbose));
    argx_result_destroy(result);

    /* The name of the program alone. */
    result = parse_both(&fixture, ARGC(wide_argv), wide_argv, argv, ARGX_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(argx_result_command_count(result), 1);
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.verbose));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.name), ARGX_SOURCE_NONE);
    argx_result_destroy(result);

    tear_down(&fixture);
}

NAOS_TEST(ascii_arguments_give_the_same_result)
{
    fixture_t fixture;
    wchar_t *wide_argv[] = { L"tool", L"-vdd",  L"--no-color", L"--num=42", L"-o",   L"-x",
                             L"run",  L"--tag", L"--",         L"a.txt",    L"--",   L"-v",
                             L"" };
    char *argv[] = { "tool", "-vdd", "--no-color", "--num=42", "-o", "-x", "run", "--tag", "--", "a.txt",
                     "--",   "-v",   "" };
    argx_result_t *result;
    int64_t number = 0;

    set_up(&fixture);
    result = parse_both(&fixture, ARGC(wide_argv), wide_argv, argv, ARGX_ERROR_NONE);

    NAOS_REQUIRE_TRUE(argx_result_command_count(result) == 2);
    NAOS_CHECK_TRUE(argx_result_command_at(result, 0) == fixture.root);
    NAOS_CHECK_TRUE(argx_result_command_at(result, 1) == fixture.run);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.verbose));
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.debug), 2);
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.color));
    NAOS_REQUIRE_TRUE(argx_result_option_int(result, fixture.num, &number));
    NAOS_CHECK_TRUE(number == 42);
    check_option_string(result, fixture.name, "-x");
    check_option_string(result, fixture.tag, "--");
    NAOS_REQUIRE_TRUE(argx_result_positional_count(result, fixture.file) == 3);
    check_positional_string(result, fixture.file, 0, "a.txt");
    check_positional_string(result, fixture.file, 1, "-v");
    check_positional_string(result, fixture.file, 2, "");
    argx_result_destroy(result);

    tear_down(&fixture);
}

NAOS_TEST(characters_of_two_three_and_four_bytes_are_read_as_utf8)
{
    fixture_t fixture;
    wchar_t *wide_argv[] = { L"tool",
                             L"--name=" W_2 L"t" W_3 L"-" W_4,
                             L"run",
                             L"-t" W_4 W_4,
                             W_2,
                             W_3 L".txt",
                             L"x" W_4,
                             W_2 W_3 W_4 };
    char *argv[] = { "tool",
                     "--name=" U_2 "t" U_3 "-" U_4,
                     "run",
                     "-t" U_4 U_4,
                     U_2,
                     U_3 ".txt",
                     "x" U_4,
                     U_2 U_3 U_4 };
    argx_result_t *result;

    set_up(&fixture);
    result = parse_both(&fixture, ARGC(wide_argv), wide_argv, argv, ARGX_ERROR_NONE);

    check_option_string(result, fixture.name, U_2 "t" U_3 "-" U_4);
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.name), ARGX_SOURCE_COMMAND_LINE);
    check_option_string(result, fixture.tag, U_4 U_4);
    NAOS_REQUIRE_TRUE(argx_result_positional_count(result, fixture.file) == 4);
    check_positional_string(result, fixture.file, 0, U_2);
    check_positional_string(result, fixture.file, 1, U_3 ".txt");
    check_positional_string(result, fixture.file, 2, "x" U_4);
    check_positional_string(result, fixture.file, 3, U_2 U_3 U_4);
    argx_result_destroy(result);

    tear_down(&fixture);
}

NAOS_TEST(a_value_taken_from_the_next_argument_is_read_as_utf8)
{
    fixture_t fixture;
    wchar_t *wide_argv[] = { L"tool", L"--name", W_3 W_2, L"-o", W_4, L"run", L"--tag", W_2 };
    char *argv[] = { "tool", "--name", U_3 U_2, "-o", U_4, "run", "--tag", U_2 };
    argx_result_t *result;

    set_up(&fixture);
    result = parse_both(&fixture, ARGC(wide_argv), wide_argv, argv, ARGX_ERROR_NONE);

    /* The second value of --name replaced the first. */
    check_option_string(result, fixture.name, U_4);
    check_option_string(result, fixture.tag, U_2);
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.tag), ARGX_SOURCE_COMMAND_LINE);
    NAOS_CHECK_UINT_EQ(argx_result_positional_count(result, fixture.file), 0);
    argx_result_destroy(result);

    tear_down(&fixture);
}

NAOS_TEST(a_value_that_was_not_given_has_its_default_value)
{
    fixture_t fixture;
    wchar_t *wide_argv[] = { L"tool", L"run", W_2 };
    char *argv[] = { "tool", "run", U_2 };
    argx_result_t *result;

    set_up(&fixture);
    result = parse_both(&fixture, ARGC(wide_argv), wide_argv, argv, ARGX_ERROR_NONE);

    check_option_string(result, fixture.tag, "none");
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.tag), ARGX_SOURCE_DEFAULT);
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.name), ARGX_SOURCE_NONE);
    argx_result_destroy(result);

    tear_down(&fixture);
}

NAOS_TEST(parse_errors_are_the_same_and_their_strings_are_utf8)
{
    fixture_t fixture;
    wchar_t *unknown_option_wide[] = { L"tool", L"-v", L"--" W_2 W_4 L"=1", L"run" };
    char *unknown_option[] = { "tool", "-v", "--" U_2 U_4 "=1", "run" };
    wchar_t *unknown_subcommand_wide[] = { L"tool", W_3 W_4 };
    char *unknown_subcommand[] = { "tool", U_3 U_4 };
    wchar_t *invalid_value_wide[] = { L"tool", L"-vn", W_2 W_3 };
    char *invalid_value[] = { "tool", "-vn", U_2 U_3 };
    wchar_t *unexpected_value_wide[] = { L"tool", L"--verbose=" W_4 };
    char *unexpected_value[] = { "tool", "--verbose=" U_4 };
    wchar_t *missing_value_wide[] = { L"tool", L"run", W_2, L"--tag" };
    char *missing_value[] = { "tool", "run", U_2, "--tag" };
    argx_result_t *result;

    set_up(&fixture);

    result = parse_both(&fixture, ARGC(unknown_option_wide), unknown_option_wide, unknown_option,
                        ARGX_ERROR_INVALID_COMMAND_LINE);
    NAOS_CHECK_INT_EQ(argx_result_error_kind(result), ARGX_PARSE_ERROR_UNKNOWN_OPTION);
    check_text(argx_result_error_argument(result), "--" U_2 U_4 "=1");
    NAOS_CHECK_UINT_EQ(argx_result_error_index(result), 2);
    check_text(argx_result_error_message(result), "unknown option '--" U_2 U_4 "'");
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.verbose));
    argx_result_destroy(result);

    result = parse_both(&fixture, ARGC(unknown_subcommand_wide), unknown_subcommand_wide, unknown_subcommand,
                        ARGX_ERROR_INVALID_COMMAND_LINE);
    NAOS_CHECK_INT_EQ(argx_result_error_kind(result), ARGX_PARSE_ERROR_UNKNOWN_SUBCOMMAND);
    check_text(argx_result_error_argument(result), U_3 U_4);
    check_text(argx_result_error_value(result), U_3 U_4);
    check_text(argx_result_error_message(result), "unknown command '" U_3 U_4 "'");
    argx_result_destroy(result);

    result = parse_both(&fixture, ARGC(invalid_value_wide), invalid_value_wide, invalid_value,
                        ARGX_ERROR_INVALID_COMMAND_LINE);
    NAOS_CHECK_INT_EQ(argx_result_error_kind(result), ARGX_PARSE_ERROR_INVALID_VALUE);
    check_text(argx_result_error_argument(result), U_2 U_3);
    check_text(argx_result_error_value(result), U_2 U_3);
    NAOS_CHECK_UINT_EQ(argx_result_error_index(result), 2);
    NAOS_CHECK_TRUE(argx_result_error_option(result) == fixture.num);
    NAOS_CHECK_INT_EQ(argx_result_error_short_name(result), 'n');
    check_text(argx_result_error_message(result),
               "invalid value '" U_2 U_3 "' for option '-n' (expected integer)");
    argx_result_destroy(result);

    result = parse_both(&fixture, ARGC(unexpected_value_wide), unexpected_value_wide, unexpected_value,
                        ARGX_ERROR_INVALID_COMMAND_LINE);
    NAOS_CHECK_INT_EQ(argx_result_error_kind(result), ARGX_PARSE_ERROR_UNEXPECTED_VALUE);
    check_text(argx_result_error_value(result), U_4);
    argx_result_destroy(result);

    result = parse_both(&fixture, ARGC(missing_value_wide), missing_value_wide, missing_value,
                        ARGX_ERROR_INVALID_COMMAND_LINE);
    NAOS_CHECK_INT_EQ(argx_result_error_kind(result), ARGX_PARSE_ERROR_MISSING_VALUE);
    NAOS_CHECK_TRUE(argx_result_error_command(result) == fixture.run);
    NAOS_REQUIRE_TRUE(argx_result_positional_count(result, fixture.file) == 1);
    check_positional_string(result, fixture.file, 0, U_2);
    argx_result_destroy(result);

    tear_down(&fixture);
}

NAOS_TEST(the_strings_of_a_result_stay_valid_after_the_wide_argv_was_overwritten)
{
    fixture_t fixture;
    wchar_t name[] = L"--name=" W_3;
    wchar_t file[] = W_2 L"-" W_4;
    wchar_t run[] = L"run";
    wchar_t tool[] = L"tool";
    wchar_t *argv[4];
    argx_result_t *result;
    size_t i;

    argv[0] = tool;
    argv[1] = name;
    argv[2] = run;
    argv[3] = file;

    set_up(&fixture);
    result = parse_wide_ok(&fixture, ARGC(argv), argv);

    for (i = 0; name[i] != L'\0'; ++i)
    {
        name[i] = L'#';
    }
    for (i = 0; file[i] != L'\0'; ++i)
    {
        file[i] = L'#';
    }
    argv[1] = NULL;
    argv[3] = NULL;

    check_option_string(result, fixture.name, U_3);
    NAOS_REQUIRE_TRUE(argx_result_positional_count(result, fixture.file) == 1);
    check_positional_string(result, fixture.file, 0, U_2 "-" U_4);
    argx_result_destroy(result);

    tear_down(&fixture);
}

NAOS_TEST(the_same_wide_command_line_gives_the_same_result_twice)
{
    fixture_t fixture;
    wchar_t *argv[] = { L"tool", L"-d", L"--name", W_4, L"run", W_3 };
    argx_result_t *first;
    argx_result_t *second;

    set_up(&fixture);
    first = parse_wide_ok(&fixture, ARGC(argv), argv);
    second = parse_wide_ok(&fixture, ARGC(argv), argv);

    check_same_results(&fixture, first, second);
    NAOS_CHECK_UINT_EQ(argx_result_count(second, fixture.debug), 1);
    check_option_string(second, fixture.name, U_4);
    argx_result_destroy(first);
    argx_result_destroy(second);

    tear_down(&fixture);
}

/* ---- An argument that cannot be converted -------------------------------- */

NAOS_TEST(every_unit_that_cannot_be_converted_is_a_parse_error)
{
    fixture_t fixture;
    wchar_t bad[4];
    wchar_t *argv[] = { L"tool", L"-v", L"run", L"first", NULL, L"last" };
    size_t i;

    bad[0] = L'a';
    bad[2] = L'b';
    bad[3] = L'\0';
    argv[4] = bad;

    set_up(&fixture);
    for (i = 0; i < BAD_UNIT_COUNT; ++i)
    {
        argx_result_t *result;

        bad[1] = bad_units[i];
        result = parse_wide_error(&fixture, ARGC(argv), argv);
        check_invalid_encoding(result, "a" REPLACEMENT "b", 4, fixture.run,
                               "invalid character encoding in argument 'a" REPLACEMENT "b'");

        /*
         * What was read before the error is in the result; the argument
         * itself and what follows it are not, and no default value is.
         */
        NAOS_REQUIRE_TRUE(argx_result_command_count(result) == 2);
        NAOS_CHECK_TRUE(argx_result_command(result) == fixture.run);
        NAOS_CHECK_TRUE(argx_result_flag(result, fixture.verbose));
        NAOS_REQUIRE_TRUE(argx_result_positional_count(result, fixture.file) == 1);
        check_positional_string(result, fixture.file, 0, "first");
        NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.tag), ARGX_SOURCE_NONE);
        argx_result_destroy(result);
    }

    tear_down(&fixture);
}

#if WCHAR_MAX <= 0xFFFF

/*
 * UTF-16: surrogates are converted only as a pair of a high and a low
 * one.
 */
static void check_units_of_this_width(const fixture_t *fixture_in)
{
    const fixture_t fixture = *fixture_in;
    wchar_t pair[] = { L'x', (wchar_t)0xD83D, (wchar_t)0xDE00, L'\0' };
    wchar_t reversed[] = { L'x', (wchar_t)0xDE00, (wchar_t)0xD83D, L'\0' };
    wchar_t two_high[] = { (wchar_t)0xD83D, (wchar_t)0xD83D, (wchar_t)0xDE00, L'\0' };
    wchar_t high_at_the_end[] = { L'x', (wchar_t)0xD83D, L'\0' };
    wchar_t *argv[] = { L"tool", L"run", NULL };
    argx_result_t *result;

    argv[2] = pair;
    result = parse_wide_ok(&fixture, ARGC(argv), argv);
    NAOS_REQUIRE_TRUE(argx_result_positional_count(result, fixture.file) == 1);
    check_positional_string(result, fixture.file, 0, "x" U_4);
    argx_result_destroy(result);

    argv[2] = reversed;
    expect_invalid_encoding(&fixture, ARGC(argv), argv, "x" REPLACEMENT REPLACEMENT, 2, fixture.run,
                            "invalid character encoding in argument 'x" REPLACEMENT REPLACEMENT "'");

    argv[2] = two_high;
    expect_invalid_encoding(&fixture, ARGC(argv), argv, REPLACEMENT U_4, 2, fixture.run,
                            "invalid character encoding in argument '" REPLACEMENT U_4 "'");

    argv[2] = high_at_the_end;
    expect_invalid_encoding(&fixture, ARGC(argv), argv, "x" REPLACEMENT, 2, fixture.run,
                            "invalid character encoding in argument 'x" REPLACEMENT "'");
}

#else

/*
 * UTF-32: the values next to the ones that cannot be converted are
 * characters, and a pair of surrogates is not one.
 */
static void check_units_of_this_width(const fixture_t *fixture_in)
{
    const fixture_t fixture = *fixture_in;
    wchar_t valid[] = { (wchar_t)0xD7FF, (wchar_t)0xE000, (wchar_t)0x10FFFF, L'\0' };
    wchar_t surrogates[] = { (wchar_t)0xD83D, (wchar_t)0xDE00, L'\0' };
    wchar_t above[] = { (wchar_t)0x10FFFF, (wchar_t)0x110000, L'\0' };
    wchar_t *argv[] = { L"tool", L"run", NULL };
    argx_result_t *result;

    /* The last before and the first after the surrogates, and U+10FFFF. */
    argv[2] = valid;
    result = parse_wide_ok(&fixture, ARGC(argv), argv);
    NAOS_REQUIRE_TRUE(argx_result_positional_count(result, fixture.file) == 1);
    check_positional_string(result, fixture.file, 0, "\xED\x9F\xBF" "\xEE\x80\x80" "\xF4\x8F\xBF\xBF");
    argx_result_destroy(result);

    /*
     * A pair of surrogates is not a character in UTF-32: each of the two
     * cannot be converted.
     */
    argv[2] = surrogates;
    expect_invalid_encoding(&fixture, ARGC(argv), argv, REPLACEMENT REPLACEMENT, 2, fixture.run,
                            "invalid character encoding in argument '" REPLACEMENT REPLACEMENT "'");

    argv[2] = above;
    expect_invalid_encoding(&fixture, ARGC(argv), argv, "\xF4\x8F\xBF\xBF" REPLACEMENT, 2, fixture.run,
                            "invalid character encoding in argument '\xF4\x8F\xBF\xBF" REPLACEMENT "'");
}

#endif

NAOS_TEST(the_units_next_to_the_ones_that_cannot_be_converted_are_read_for_the_width_of_wchar_t)
{
    fixture_t fixture;

    set_up(&fixture);
    check_units_of_this_width(&fixture);
    tear_down(&fixture);
}

NAOS_TEST(an_option_with_a_unit_that_cannot_be_converted_is_not_looked_up)
{
    fixture_t fixture;
    wchar_t long_option[] = { L'-', L'-', L'n', L'a', L'm', L'e', L'=', L'x', L'\0', L'\0' };
    wchar_t bundle[] = { L'-', L'v', L'o', L'\0', L'\0' };
    wchar_t *argv[] = { L"tool", L"-d", NULL };
    argx_result_t *result;

    long_option[8] = BAD_UNIT;
    bundle[3] = BAD_UNIT;

    set_up(&fixture);

    /*
     * --name=x<bad>: the option is known, and it has no value all the
     * same.
     */
    argv[2] = long_option;
    result = parse_wide_error(&fixture, ARGC(argv), argv);
    check_invalid_encoding(result, "--name=x" REPLACEMENT, 2, fixture.root,
                           "invalid character encoding in argument '--name=x" REPLACEMENT "'");
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.name), ARGX_SOURCE_NONE);
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.debug), 1);
    argx_result_destroy(result);

    /*
     * -vo<bad>: no letter of the bundle is read, and the error has no
     * letter.
     */
    argv[2] = bundle;
    result = parse_wide_error(&fixture, ARGC(argv), argv);
    check_invalid_encoding(result, "-vo" REPLACEMENT, 2, fixture.root,
                           "invalid character encoding in argument '-vo" REPLACEMENT "'");
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.verbose));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.name), ARGX_SOURCE_NONE);
    argx_result_destroy(result);

    tear_down(&fixture);
}

NAOS_TEST(an_argument_that_would_be_taken_as_a_value_is_checked_before_it_is_taken)
{
    fixture_t fixture;
    wchar_t bad[] = { L'x', L'\0', L'\0' };
    wchar_t *long_argv[] = { L"tool", L"--name", NULL, L"run" };
    wchar_t *short_argv[] = { L"tool", L"-vo", NULL, L"run" };
    wchar_t *number_argv[] = { L"tool", L"-n", NULL };
    argx_result_t *result;

    bad[1] = BAD_UNIT;
    long_argv[2] = bad;
    short_argv[2] = bad;
    number_argv[2] = bad;

    set_up(&fixture);

    /*
     * The error is that of the argument, not of the option: it has no
     * option and no letter.
     */
    result = parse_wide_error(&fixture, ARGC(long_argv), long_argv);
    check_invalid_encoding(result, "x" REPLACEMENT, 2, fixture.root,
                           "invalid character encoding in argument 'x" REPLACEMENT "'");
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.name), ARGX_SOURCE_NONE);
    NAOS_CHECK_UINT_EQ(argx_result_command_count(result), 1);
    argx_result_destroy(result);

    result = parse_wide_error(&fixture, ARGC(short_argv), short_argv);
    check_invalid_encoding(result, "x" REPLACEMENT, 2, fixture.root,
                           "invalid character encoding in argument 'x" REPLACEMENT "'");
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.verbose));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.name), ARGX_SOURCE_NONE);
    argx_result_destroy(result);

    /*
     * The value type does not see the argument: the error is not that of
     * a value that is no integer.
     */
    expect_invalid_encoding(&fixture, ARGC(number_argv), number_argv, "x" REPLACEMENT, 2, fixture.root,
                            "invalid character encoding in argument 'x" REPLACEMENT "'");

    tear_down(&fixture);
}

NAOS_TEST(an_argument_after_the_terminator_is_checked_too)
{
    fixture_t fixture;
    wchar_t bad[] = { L'\0', L'\0' };
    wchar_t *argv[] = { L"tool", L"run", L"--", L"-v", NULL };
    argx_result_t *result;

    bad[0] = BAD_UNIT;
    argv[4] = bad;

    set_up(&fixture);
    result = parse_wide_error(&fixture, ARGC(argv), argv);
    check_invalid_encoding(result, REPLACEMENT, 4, fixture.run,
                           "invalid character encoding in argument '" REPLACEMENT "'");
    NAOS_REQUIRE_TRUE(argx_result_positional_count(result, fixture.file) == 1);
    check_positional_string(result, fixture.file, 0, "-v");
    argx_result_destroy(result);

    tear_down(&fixture);
}

NAOS_TEST(the_current_command_of_the_error_is_the_one_selected_before_the_argument)
{
    fixture_t fixture;
    wchar_t bad[] = { L'r', L'u', L'n', L'\0', L'\0' };
    wchar_t *argv[] = { L"tool", NULL, L"run" };

    bad[3] = BAD_UNIT;
    argv[1] = bad;

    set_up(&fixture);
    expect_invalid_encoding(&fixture, ARGC(argv), argv, "run" REPLACEMENT, 1, fixture.root,
                            "invalid character encoding in argument 'run" REPLACEMENT "'");
    tear_down(&fixture);
}

NAOS_TEST(of_two_arguments_that_cannot_be_converted_the_first_is_reported)
{
    fixture_t fixture;
    wchar_t first[] = { L'1', L'\0', L'\0' };
    wchar_t second[] = { L'2', L'\0', L'\0' };
    wchar_t *argv[] = { L"tool", L"run", L"a", NULL, L"b", NULL };

    first[1] = BAD_UNIT;
    second[1] = bad_units[BAD_UNIT_COUNT - 1];
    argv[3] = first;
    argv[5] = second;

    set_up(&fixture);
    expect_invalid_encoding(&fixture, ARGC(argv), argv, "1" REPLACEMENT, 3, fixture.run,
                            "invalid character encoding in argument '1" REPLACEMENT "'");
    tear_down(&fixture);
}

NAOS_TEST(an_error_of_an_earlier_argument_is_reported_instead)
{
    fixture_t fixture;
    wchar_t bad[] = { L'x', L'\0', L'\0' };
    wchar_t *unknown_option[] = { L"tool", L"--bogus", NULL };
    wchar_t *unknown_subcommand[] = { L"tool", L"-v", L"walk", NULL };
    wchar_t *invalid_value[] = { L"tool", L"--num", L"five", L"run", NULL };
    argx_result_t *result;

    bad[1] = BAD_UNIT;
    unknown_option[2] = bad;
    unknown_subcommand[3] = bad;
    invalid_value[4] = bad;

    set_up(&fixture);

    result = parse_wide_error(&fixture, ARGC(unknown_option), unknown_option);
    NAOS_CHECK_INT_EQ(argx_result_error_kind(result), ARGX_PARSE_ERROR_UNKNOWN_OPTION);
    NAOS_CHECK_UINT_EQ(argx_result_error_index(result), 1);
    check_text(argx_result_error_argument(result), "--bogus");
    check_text(argx_result_error_message(result), "unknown option '--bogus'");
    argx_result_destroy(result);

    result = parse_wide_error(&fixture, ARGC(unknown_subcommand), unknown_subcommand);
    NAOS_CHECK_INT_EQ(argx_result_error_kind(result), ARGX_PARSE_ERROR_UNKNOWN_SUBCOMMAND);
    NAOS_CHECK_UINT_EQ(argx_result_error_index(result), 2);
    check_text(argx_result_error_message(result), "unknown command 'walk'");
    argx_result_destroy(result);

    result = parse_wide_error(&fixture, ARGC(invalid_value), invalid_value);
    NAOS_CHECK_INT_EQ(argx_result_error_kind(result), ARGX_PARSE_ERROR_INVALID_VALUE);
    NAOS_CHECK_UINT_EQ(argx_result_error_index(result), 2);
    NAOS_CHECK_TRUE(argx_result_error_option(result) == fixture.num);
    check_text(argx_result_error_message(result),
               "invalid value 'five' for option '--num' (expected integer)");
    argx_result_destroy(result);

    tear_down(&fixture);
}

NAOS_TEST(an_error_is_reported_before_something_required_that_is_missing_is_looked_for)
{
    fixture_t fixture;
    argx_command_t *root = NULL;
    argx_option_t *key = NULL;
    wchar_t bad[] = { L'x', L'\0', L'\0' };
    wchar_t *argv[] = { L"tool", NULL };
    argx_result_t *result = NULL;

    bad[1] = BAD_UNIT;
    argv[1] = bad;

    /* tool --key STRING [FILE]..., where --key must be given. */
    argx_test_allocator_install();
    NAOS_REQUIRE_TRUE(argx_command_create("tool", &root) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_value(root, '\0', "key", argx_type_string(), &key) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_option_set_required(key, true) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_positional(root, "FILE", argx_type_string(),
                                                  ARGX_ARITY_ZERO_OR_MORE, &fixture.file) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_validate(root, NULL, NULL, NULL) == ARGX_ERROR_NONE);
    fixture.root = root;

    NAOS_REQUIRE_TRUE(argx_parse_wide(root, ARGC(argv), argv, &result) == ARGX_ERROR_INVALID_COMMAND_LINE);
    NAOS_REQUIRE_TRUE(result != NULL);
    check_invalid_encoding(result, "x" REPLACEMENT, 1, root,
                           "invalid character encoding in argument 'x" REPLACEMENT "'");
    NAOS_CHECK_UINT_EQ(argx_result_positional_count(result, fixture.file), 0);
    argx_result_destroy(result);

    tear_down(&fixture);
}

NAOS_TEST(a_name_of_the_program_that_cannot_be_converted_is_ignored)
{
    fixture_t fixture;
    wchar_t program[] = { L't', L'\0', L'l', L'\0' };
    wchar_t bad[] = { L'x', L'\0', L'\0' };
    wchar_t *alone[] = { NULL };
    wchar_t *valid_argv[] = { NULL, L"-v", L"run", W_2 };
    wchar_t *invalid_argv[] = { NULL, L"-v", L"run", NULL };
    argx_result_t *result;

    program[1] = BAD_UNIT;
    bad[1] = BAD_UNIT;
    alone[0] = program;
    valid_argv[0] = program;
    invalid_argv[0] = program;
    invalid_argv[3] = bad;

    set_up(&fixture);

    result = parse_wide_ok(&fixture, ARGC(alone), alone);
    NAOS_CHECK_INT_EQ(argx_result_error_kind(result), ARGX_PARSE_ERROR_NONE);
    NAOS_CHECK_TRUE(argx_result_error_argument(result) == NULL);
    argx_result_destroy(result);

    result = parse_wide_ok(&fixture, ARGC(valid_argv), valid_argv);
    NAOS_CHECK_INT_EQ(argx_result_error_kind(result), ARGX_PARSE_ERROR_NONE);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.verbose));
    NAOS_REQUIRE_TRUE(argx_result_positional_count(result, fixture.file) == 1);
    check_positional_string(result, fixture.file, 0, U_2);
    argx_result_destroy(result);

    /*
     * The first argument after the name of the program that cannot be
     * converted is still found.
     */
    expect_invalid_encoding(&fixture, ARGC(invalid_argv), invalid_argv, "x" REPLACEMENT, 3, fixture.run,
                            "invalid character encoding in argument 'x" REPLACEMENT "'");

    tear_down(&fixture);
}

/* ---- The bytes of a char argument ---------------------------------------- */

NAOS_TEST(bytes_that_are_not_utf8_are_a_value_as_they_are)
{
    fixture_t fixture;
    /*
     * A byte that no UTF-8 has, a lead byte with no byte after it, a byte
     * that continues nothing, a surrogate written as three bytes, and a
     * form that is longer than it need be.
     */
    char *argv[] = { "tool",         "--name=\xFF\xFE", "-o", "\xC3", "run", "--tag", "\x80\xBF",
                     "\xED\xA0\x80", "\xC0\xAF.txt" };
    argx_result_t *result = NULL;

    set_up(&fixture);
    NAOS_REQUIRE_TRUE(argx_parse(fixture.root, ARGC(argv), argv, &result) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(result != NULL);
    NAOS_CHECK_INT_EQ(argx_result_error(result), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_result_error_kind(result), ARGX_PARSE_ERROR_NONE);

    check_option_string(result, fixture.name, "\xC3");
    check_option_string(result, fixture.tag, "\x80\xBF");
    NAOS_REQUIRE_TRUE(argx_result_positional_count(result, fixture.file) == 2);
    check_positional_string(result, fixture.file, 0, "\xED\xA0\x80");
    check_positional_string(result, fixture.file, 1, "\xC0\xAF.txt");
    argx_result_destroy(result);

    tear_down(&fixture);
}

NAOS_TEST(bytes_that_are_not_utf8_are_in_a_parse_error_as_they_are)
{
    fixture_t fixture;
    char *argv[] = { "tool", "--\xFF=1" };
    argx_result_t *result = NULL;

    set_up(&fixture);
    NAOS_REQUIRE_TRUE(argx_parse(fixture.root, ARGC(argv), argv, &result) ==
                      ARGX_ERROR_INVALID_COMMAND_LINE);
    NAOS_REQUIRE_TRUE(result != NULL);
    NAOS_CHECK_INT_EQ(argx_result_error_kind(result), ARGX_PARSE_ERROR_UNKNOWN_OPTION);
    check_text(argx_result_error_argument(result), "--\xFF=1");
    check_text(argx_result_error_message(result), "unknown option '--\xFF'");
    argx_result_destroy(result);

    tear_down(&fixture);
}

NAOS_SUITE(same_as_utf8,
    NAOS_ADD_TEST(a_command_line_without_arguments_gives_an_empty_result),
    NAOS_ADD_TEST(ascii_arguments_give_the_same_result),
    NAOS_ADD_TEST(characters_of_two_three_and_four_bytes_are_read_as_utf8),
    NAOS_ADD_TEST(a_value_taken_from_the_next_argument_is_read_as_utf8),
    NAOS_ADD_TEST(a_value_that_was_not_given_has_its_default_value),
    NAOS_ADD_TEST(parse_errors_are_the_same_and_their_strings_are_utf8),
    NAOS_ADD_TEST(the_strings_of_a_result_stay_valid_after_the_wide_argv_was_overwritten),
    NAOS_ADD_TEST(the_same_wide_command_line_gives_the_same_result_twice)
)

NAOS_SUITE(invalid_encoding,
    NAOS_ADD_TEST(every_unit_that_cannot_be_converted_is_a_parse_error),
    NAOS_ADD_TEST(the_units_next_to_the_ones_that_cannot_be_converted_are_read_for_the_width_of_wchar_t),
    NAOS_ADD_TEST(an_option_with_a_unit_that_cannot_be_converted_is_not_looked_up),
    NAOS_ADD_TEST(an_argument_that_would_be_taken_as_a_value_is_checked_before_it_is_taken),
    NAOS_ADD_TEST(an_argument_after_the_terminator_is_checked_too),
    NAOS_ADD_TEST(the_current_command_of_the_error_is_the_one_selected_before_the_argument),
    NAOS_ADD_TEST(of_two_arguments_that_cannot_be_converted_the_first_is_reported),
    NAOS_ADD_TEST(an_error_of_an_earlier_argument_is_reported_instead),
    NAOS_ADD_TEST(an_error_is_reported_before_something_required_that_is_missing_is_looked_for),
    NAOS_ADD_TEST(a_name_of_the_program_that_cannot_be_converted_is_ignored)
)

NAOS_SUITE(char_arguments,
    NAOS_ADD_TEST(bytes_that_are_not_utf8_are_a_value_as_they_are),
    NAOS_ADD_TEST(bytes_that_are_not_utf8_are_in_a_parse_error_as_they_are)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(same_as_utf8),
    NAOS_ADD_SUITE(invalid_encoding),
    NAOS_ADD_SUITE(char_arguments)
)
