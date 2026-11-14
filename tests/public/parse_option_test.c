/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * Tests of the public parsing API for options: argx_parse() of
 * <argx/parse.h>, read back with the argx_result_*() functions.
 *
 * A schema is built the way a consumer does it, validated, and parsed with
 * arguments written as an argv array. The tests cover the entry (no
 * argument, the program name alone, repeated parses, the copy of the
 * arguments) and the long options: the forms "--name", "--name=value",
 * "--name value" and "--no-name", the arguments that are taken as a value,
 * options that are given more than once, and the parse errors of an option.
 * They cover the short options as well: a bundle of them, the two ways in
 * which the last one takes a value, the parse errors in a bundle, and the
 * rule for an argument with the form of a negative number.
 *
 * No test relies on a default value or on a required option.
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

/*
 * The schema of the tests: the command "tool" with one option of each kind
 * and more.
 */
typedef struct fixture
{
    argx_command_t *root;
    argx_option_t *verbose; /* -v, --verbose: a flag */
    argx_option_t *help;    /* --help: a flag */
    argx_option_t *debug;   /* -d, --debug: a count */
    argx_option_t *color;   /* --color, --no-color: a negatable flag, initially off */
    argx_option_t *pager;   /* --pager, --no-pager: a negatable flag, initially on */
    argx_option_t *name;    /* --name: a string */
    argx_option_t *out;     /* -o, --out: a string */
    argx_option_t *num;     /* --num: an integer */
} fixture_t;

/*
 * The end of every test: the test allocator has no live block and saw no
 * violation of the allocator contract.
 */
static void check_clean(void)
{
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_violations(), 0);
}

/* Installs the test allocator, builds the schema and validates it. */
static void set_up(fixture_t *fixture)
{
    argx_command_t *root = NULL;

    argx_test_allocator_install();
    NAOS_REQUIRE_TRUE(argx_command_create("tool", &root) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(root != NULL);
    fixture->root = root;

    NAOS_REQUIRE_TRUE(argx_command_add_flag(root, 'v', "verbose", &fixture->verbose) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_flag(root, '\0', "help", &fixture->help) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_count(root, 'd', "debug", &fixture->debug) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_negatable(root, '\0', "color", &fixture->color) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_negatable(root, '\0', "pager", &fixture->pager) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_option_set_initial(fixture->pager, true) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_value(root, '\0', "name", argx_type_string(), &fixture->name) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_value(root, 'o', "out", argx_type_string(), &fixture->out) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_value(root, '\0', "num", argx_type_int(), &fixture->num) ==
                      ARGX_ERROR_NONE);

    NAOS_REQUIRE_TRUE(argx_command_validate(root, NULL, NULL, NULL) == ARGX_ERROR_NONE);
}

/* Destroys the schema and ends the test: nothing stays allocated. */
static void tear_down(fixture_t *fixture)
{
    argx_command_destroy(fixture->root);
    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * Parses argv and returns the result, which the caller destroys. The parse
 * must return expected, which is not ARGX_ERROR_OUT_OF_MEMORY, and the
 * result must report the same.
 */
static argx_result_t *parse(const fixture_t *fixture, int argc, char *const *argv, argx_error_t expected)
{
    argx_result_t *result = NULL;

    NAOS_REQUIRE_TRUE(argx_parse(fixture->root, argc, argv, &result) == expected);
    NAOS_REQUIRE_TRUE(result != NULL);
    NAOS_CHECK_INT_EQ(argx_result_error(result), expected);
    return result;
}

static argx_result_t *parse_ok(const fixture_t *fixture, int argc, char *const *argv)
{
    argx_result_t *result = parse(fixture, argc, argv, ARGX_ERROR_NONE);

    NAOS_CHECK_INT_EQ(argx_result_error_kind(result), ARGX_PARSE_ERROR_NONE);
    NAOS_CHECK_TRUE(argx_result_error_message(result) == NULL);
    return result;
}

/*
 * Parses argv, which must have a parse error of the kind, at the argument
 * with the index, with the message. Returns the result.
 */
static argx_result_t *parse_bad(const fixture_t *fixture, int argc, char *const *argv,
                                argx_parse_error_kind_t kind, size_t index, const char *message)
{
    argx_result_t *result = parse(fixture, argc, argv, ARGX_ERROR_INVALID_COMMAND_LINE);

    NAOS_CHECK_INT_EQ(argx_result_error_kind(result), kind);
    NAOS_CHECK_INT_EQ(argx_result_error_source(result), ARGX_SOURCE_COMMAND_LINE);
    NAOS_CHECK_UINT_EQ(argx_result_error_index(result), index);
    NAOS_REQUIRE_TRUE(argx_result_error_argument(result) != NULL);
    NAOS_CHECK_STR_EQ(argx_result_error_argument(result), argv[index]);
    NAOS_CHECK_TRUE(argx_result_error_command(result) == fixture->root);
    NAOS_CHECK_TRUE(argx_result_error_positional(result) == NULL);
    NAOS_CHECK_INT_EQ(argx_result_error_short_name(result), '\0');
    NAOS_REQUIRE_TRUE(argx_result_error_message(result) != NULL);
    NAOS_CHECK_STR_EQ(argx_result_error_message(result), message);
    return result;
}

/* The value of a string option of result, which must have one. */
static const char *string_of(const argx_result_t *result, const argx_option_t *option)
{
    const char *text = NULL;

    NAOS_REQUIRE_TRUE(argx_result_option_string(result, option, &text));
    NAOS_REQUIRE_TRUE(text != NULL);
    return text;
}

/* Checks that an option that takes a value has none in result. */
static void check_no_value(const argx_result_t *result, const argx_option_t *option)
{
    NAOS_CHECK_TRUE(argx_result_option_value(result, option) == NULL);
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, option), ARGX_SOURCE_NONE);
}

/* Checks that nothing was given in result. */
static void check_nothing_given(const fixture_t *fixture, const argx_result_t *result)
{
    NAOS_CHECK_UINT_EQ(argx_result_command_count(result), 1);
    NAOS_CHECK_TRUE(argx_result_command(result) == fixture->root);

    NAOS_CHECK_FALSE(argx_result_flag(result, fixture->verbose));
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture->help));
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture->debug), 0);
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture->color));
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture->pager));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture->verbose), ARGX_SOURCE_DEFAULT);
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture->debug), ARGX_SOURCE_DEFAULT);
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture->color), ARGX_SOURCE_DEFAULT);
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture->pager), ARGX_SOURCE_DEFAULT);
    check_no_value(result, fixture->name);
    check_no_value(result, fixture->out);
    check_no_value(result, fixture->num);
}

/* ---- The entry ---------------------------------------------------------- */

NAOS_TEST(no_argument_at_all_is_a_command_line_on_which_nothing_was_given)
{
    fixture_t fixture;
    argx_result_t *result;

    set_up(&fixture);
    result = parse_ok(&fixture, 0, NULL);

    check_nothing_given(&fixture, result);

    argx_result_destroy(result);
    tear_down(&fixture);
}

NAOS_TEST(the_program_name_alone_is_a_command_line_on_which_nothing_was_given)
{
    fixture_t fixture;
    char *argv[] = { "tool" };
    argx_result_t *result;

    set_up(&fixture);
    result = parse_ok(&fixture, ARGC(argv), argv);

    check_nothing_given(&fixture, result);

    argx_result_destroy(result);
    tear_down(&fixture);
}

NAOS_TEST(the_program_name_is_not_parsed)
{
    fixture_t fixture;
    char *argv[] = { "--verbose" };
    char *unknown[] = { "--bogus", "--debug" };
    argx_result_t *result;

    set_up(&fixture);

    result = parse_ok(&fixture, ARGC(argv), argv);
    check_nothing_given(&fixture, result);
    argx_result_destroy(result);

    result = parse_ok(&fixture, ARGC(unknown), unknown);
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.debug), 1);
    argx_result_destroy(result);

    tear_down(&fixture);
}

NAOS_TEST(argv_can_be_the_argv_of_main_with_its_null_at_the_end)
{
    fixture_t fixture;
    char *argv[] = { "tool", "--verbose", NULL };
    argx_result_t *result;

    set_up(&fixture);
    result = parse_ok(&fixture, ARGC(argv) - 1, argv);

    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.verbose));

    argx_result_destroy(result);
    tear_down(&fixture);
}

NAOS_TEST(two_parses_with_one_schema_give_the_same_result)
{
    fixture_t fixture;
    char *argv[] = { "tool", "--verbose", "--debug", "--debug", "--no-pager", "--name", "x", "--num=7" };
    char *other[] = { "tool", "--color", "--out=file" };
    argx_result_t *results[3];
    int64_t number = 0;
    size_t i;

    set_up(&fixture);
    results[0] = parse_ok(&fixture, ARGC(argv), argv);
    results[1] = parse_ok(&fixture, ARGC(other), other);
    results[2] = parse_ok(&fixture, ARGC(argv), argv);
    NAOS_CHECK_TRUE(results[0] != results[2]);

    /*
     * The first and the third parse read the same; the parse between them
     * left nothing behind.
     */
    for (i = 0; i < 3; i += 2)
    {
        NAOS_CHECK_TRUE(argx_result_flag(results[i], fixture.verbose));
        NAOS_CHECK_UINT_EQ(argx_result_count(results[i], fixture.debug), 2);
        NAOS_CHECK_FALSE(argx_result_flag(results[i], fixture.pager));
        NAOS_CHECK_FALSE(argx_result_flag(results[i], fixture.color));
        NAOS_CHECK_INT_EQ(argx_result_option_source(results[i], fixture.color), ARGX_SOURCE_DEFAULT);
        NAOS_CHECK_STR_EQ(string_of(results[i], fixture.name), "x");
        NAOS_REQUIRE_TRUE(argx_result_option_int(results[i], fixture.num, &number));
        NAOS_CHECK_INT_EQ(number, 7);
        check_no_value(results[i], fixture.out);
    }

    NAOS_CHECK_TRUE(argx_result_flag(results[1], fixture.color));
    NAOS_CHECK_STR_EQ(string_of(results[1], fixture.out), "file");
    NAOS_CHECK_FALSE(argx_result_flag(results[1], fixture.verbose));
    NAOS_CHECK_UINT_EQ(argx_result_count(results[1], fixture.debug), 0);
    check_no_value(results[1], fixture.name);

    /* The schema is still validated: a parse does not change it. */
    NAOS_CHECK_TRUE(argx_command_is_validated(fixture.root));
    NAOS_CHECK_UINT_EQ(argx_command_option_count(fixture.root), 8);

    argx_result_destroy(results[0]);
    argx_result_destroy(results[1]);
    argx_result_destroy(results[2]);
    tear_down(&fixture);
}

NAOS_TEST(the_result_does_not_change_when_argv_is_changed_after_the_parse)
{
    fixture_t fixture;
    char name[] = "--name";
    char value[] = "alice";
    char attached[] = "--out=file";
    char wrong[] = "--bogus";
    char *argv[4];
    argx_result_t *result;

    argv[0] = "tool";
    argv[1] = name;
    argv[2] = value;
    argv[3] = attached;

    set_up(&fixture);

    result = parse_ok(&fixture, 4, argv);
    /* The parse wrote nothing to argv. */
    NAOS_CHECK_STR_EQ(name, "--name");
    NAOS_CHECK_STR_EQ(value, "alice");
    NAOS_CHECK_STR_EQ(attached, "--out=file");
    NAOS_CHECK_TRUE(argv[1] == name);
    NAOS_CHECK_TRUE(argv[2] == value);
    NAOS_CHECK_TRUE(argv[3] == attached);

    memset(value, 'X', sizeof(value) - 1);
    memset(attached, 'Y', sizeof(attached) - 1);
    argv[2] = NULL;
    argv[3] = NULL;
    NAOS_CHECK_STR_EQ(string_of(result, fixture.name), "alice");
    NAOS_CHECK_STR_EQ(string_of(result, fixture.out), "file");
    NAOS_CHECK_TRUE(string_of(result, fixture.name) != value);
    argx_result_destroy(result);

    /* The argument of a parse error is a copy too. */
    argv[1] = wrong;
    result = parse(&fixture, 2, argv, ARGX_ERROR_INVALID_COMMAND_LINE);
    memset(wrong, 'Z', sizeof(wrong) - 1);
    NAOS_REQUIRE_TRUE(argx_result_error_argument(result) != NULL);
    NAOS_CHECK_STR_EQ(argx_result_error_argument(result), "--bogus");
    NAOS_CHECK_STR_EQ(argx_result_error_message(result), "unknown option '--bogus'");
    argx_result_destroy(result);

    tear_down(&fixture);
}

/* ---- The forms of a long option ----------------------------------------- */

NAOS_TEST(a_long_name_gives_a_flag)
{
    fixture_t fixture;
    char *argv[] = { "tool", "--verbose" };
    argx_result_t *result;

    set_up(&fixture);
    result = parse_ok(&fixture, ARGC(argv), argv);

    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.verbose));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.verbose), ARGX_SOURCE_COMMAND_LINE);
    /* The other options are as they were. */
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.help));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.help), ARGX_SOURCE_DEFAULT);
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.debug), 0);
    check_no_value(result, fixture.name);

    argx_result_destroy(result);
    tear_down(&fixture);
}

NAOS_TEST(a_value_is_attached_with_an_equals_sign)
{
    fixture_t fixture;
    char *argv[] = { "tool", "--name=value", "--num=-12" };
    argx_result_t *result;
    int64_t number = 0;

    set_up(&fixture);
    result = parse_ok(&fixture, ARGC(argv), argv);

    NAOS_CHECK_STR_EQ(string_of(result, fixture.name), "value");
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.name), ARGX_SOURCE_COMMAND_LINE);
    NAOS_REQUIRE_TRUE(argx_result_option_int(result, fixture.num, &number));
    NAOS_CHECK_INT_EQ(number, -12);

    argx_result_destroy(result);
    tear_down(&fixture);
}

NAOS_TEST(a_value_is_taken_from_the_next_argument)
{
    fixture_t fixture;
    char *argv[] = { "tool", "--name", "value", "--verbose", "--num", "42" };
    argx_result_t *result;
    int64_t number = 0;

    set_up(&fixture);
    result = parse_ok(&fixture, ARGC(argv), argv);

    NAOS_CHECK_STR_EQ(string_of(result, fixture.name), "value");
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.name), ARGX_SOURCE_COMMAND_LINE);
    /* The scan goes on after the value. */
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.verbose));
    NAOS_REQUIRE_TRUE(argx_result_option_int(result, fixture.num, &number));
    NAOS_CHECK_INT_EQ(number, 42);

    argx_result_destroy(result);
    tear_down(&fixture);
}

NAOS_TEST(an_attached_value_can_be_empty)
{
    fixture_t fixture;
    char *argv[] = { "tool", "--name=", "--verbose" };
    argx_result_t *result;

    set_up(&fixture);
    result = parse_ok(&fixture, ARGC(argv), argv);

    NAOS_CHECK_STR_EQ(string_of(result, fixture.name), "");
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.name), ARGX_SOURCE_COMMAND_LINE);
    /* The next argument was not taken as the value. */
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.verbose));

    argx_result_destroy(result);
    tear_down(&fixture);
}

NAOS_TEST(a_value_from_the_next_argument_can_be_empty)
{
    fixture_t fixture;
    char *argv[] = { "tool", "--name", "" };
    argx_result_t *result;

    set_up(&fixture);
    result = parse_ok(&fixture, ARGC(argv), argv);

    NAOS_CHECK_STR_EQ(string_of(result, fixture.name), "");

    argx_result_destroy(result);
    tear_down(&fixture);
}

NAOS_TEST(the_name_ends_at_the_first_equals_sign)
{
    fixture_t fixture;
    char *argv[] = { "tool", "--name=a=b", "--out==" };
    argx_result_t *result;

    set_up(&fixture);
    result = parse_ok(&fixture, ARGC(argv), argv);

    NAOS_CHECK_STR_EQ(string_of(result, fixture.name), "a=b");
    NAOS_CHECK_STR_EQ(string_of(result, fixture.out), "=");

    argx_result_destroy(result);
    tear_down(&fixture);
}

NAOS_TEST(a_value_is_passed_to_the_value_type_as_it_was_written)
{
    fixture_t fixture;
    char *argv[] = { "tool", "--name= a b ", "--out", " --x=y " };
    argx_result_t *result;

    set_up(&fixture);
    result = parse_ok(&fixture, ARGC(argv), argv);

    NAOS_CHECK_STR_EQ(string_of(result, fixture.name), " a b ");
    NAOS_CHECK_STR_EQ(string_of(result, fixture.out), " --x=y ");

    argx_result_destroy(result);
    tear_down(&fixture);
}

NAOS_TEST(the_long_name_of_a_negatable_flag_switches_it_on)
{
    fixture_t fixture;
    char *argv[] = { "tool", "--color", "--pager" };
    argx_result_t *result;

    set_up(&fixture);
    result = parse_ok(&fixture, ARGC(argv), argv);

    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.color));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.color), ARGX_SOURCE_COMMAND_LINE);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.pager));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.pager), ARGX_SOURCE_COMMAND_LINE);

    argx_result_destroy(result);
    tear_down(&fixture);
}

NAOS_TEST(the_negated_name_of_a_negatable_flag_switches_it_off)
{
    fixture_t fixture;
    char *argv[] = { "tool", "--no-color", "--no-pager" };
    argx_result_t *result;

    set_up(&fixture);
    result = parse_ok(&fixture, ARGC(argv), argv);

    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.color));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.color), ARGX_SOURCE_COMMAND_LINE);
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.pager));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.pager), ARGX_SOURCE_COMMAND_LINE);

    argx_result_destroy(result);
    tear_down(&fixture);
}

/* ---- Names that do not match -------------------------------------------- */

NAOS_TEST(a_name_in_another_case_is_an_unknown_option)
{
    fixture_t fixture;
    char *argv[] = { "tool", "--Verbose" };
    char *upper[] = { "tool", "--debug", "--NAME=x" };
    argx_result_t *result;

    set_up(&fixture);

    result = parse_bad(&fixture, ARGC(argv), argv, ARGX_PARSE_ERROR_UNKNOWN_OPTION, 1,
                       "unknown option '--Verbose'");
    NAOS_CHECK_TRUE(argx_result_error_value(result) == NULL);
    NAOS_CHECK_TRUE(argx_result_error_option(result) == NULL);
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.verbose));
    argx_result_destroy(result);

    /* The message names the option without its value. */
    result = parse_bad(&fixture, ARGC(upper), upper, ARGX_PARSE_ERROR_UNKNOWN_OPTION, 2,
                       "unknown option '--NAME'");
    NAOS_CHECK_TRUE(argx_result_error_value(result) == NULL);
    NAOS_CHECK_TRUE(argx_result_error_option(result) == NULL);
    check_no_value(result, fixture.name);
    argx_result_destroy(result);

    tear_down(&fixture);
}

NAOS_TEST(a_part_of_a_name_and_a_longer_name_are_unknown_options)
{
    fixture_t fixture;
    char *part[] = { "tool", "--verb" };
    char *longer[] = { "tool", "--verbosely" };
    char *part_with_value[] = { "tool", "--nam=x" };
    char *negated_part[] = { "tool", "--no-col" };
    char *negated_flag[] = { "tool", "--no-verbose" };
    char *empty_name[] = { "tool", "--=x" };
    argx_result_t *result;

    set_up(&fixture);

    result = parse_bad(&fixture, ARGC(part), part, ARGX_PARSE_ERROR_UNKNOWN_OPTION, 1,
                       "unknown option '--verb'");
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.verbose));
    argx_result_destroy(result);

    result = parse_bad(&fixture, ARGC(longer), longer, ARGX_PARSE_ERROR_UNKNOWN_OPTION, 1,
                       "unknown option '--verbosely'");
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.verbose));
    argx_result_destroy(result);

    result = parse_bad(&fixture, ARGC(part_with_value), part_with_value, ARGX_PARSE_ERROR_UNKNOWN_OPTION,
                       1, "unknown option '--nam'");
    check_no_value(result, fixture.name);
    argx_result_destroy(result);

    result = parse_bad(&fixture, ARGC(negated_part), negated_part, ARGX_PARSE_ERROR_UNKNOWN_OPTION, 1,
                       "unknown option '--no-col'");
    argx_result_destroy(result);

    /* Only a negatable flag has a negated name. */
    result = parse_bad(&fixture, ARGC(negated_flag), negated_flag, ARGX_PARSE_ERROR_UNKNOWN_OPTION, 1,
                       "unknown option '--no-verbose'");
    argx_result_destroy(result);

    result = parse_bad(&fixture, ARGC(empty_name), empty_name, ARGX_PARSE_ERROR_UNKNOWN_OPTION, 1,
                       "unknown option '--'");
    argx_result_destroy(result);

    tear_down(&fixture);
}

/* ---- Arguments that are taken as a value -------------------------------- */

NAOS_TEST(the_next_argument_is_the_value_whatever_it_looks_like)
{
    fixture_t fixture;
    char *bundle[] = { "tool", "--name", "-x", "--debug" };
    char *terminator[] = { "tool", "--name", "--", "--debug" };
    char *option[] = { "tool", "--name", "--verbose", "--debug" };
    char *unknown[] = { "tool", "--name", "--bogus=1", "--debug" };
    char *number[] = { "tool", "--num", "-5", "--debug" };
    argx_result_t *result;
    int64_t value = 0;

    set_up(&fixture);

    result = parse_ok(&fixture, ARGC(bundle), bundle);
    NAOS_CHECK_STR_EQ(string_of(result, fixture.name), "-x");
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.debug), 1);
    argx_result_destroy(result);

    result = parse_ok(&fixture, ARGC(terminator), terminator);
    NAOS_CHECK_STR_EQ(string_of(result, fixture.name), "--");
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.debug), 1);
    argx_result_destroy(result);

    /* The argument is not read as an option: the flag is not given. */
    result = parse_ok(&fixture, ARGC(option), option);
    NAOS_CHECK_STR_EQ(string_of(result, fixture.name), "--verbose");
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.verbose));
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.debug), 1);
    argx_result_destroy(result);

    result = parse_ok(&fixture, ARGC(unknown), unknown);
    NAOS_CHECK_STR_EQ(string_of(result, fixture.name), "--bogus=1");
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.debug), 1);
    argx_result_destroy(result);

    result = parse_ok(&fixture, ARGC(number), number);
    NAOS_REQUIRE_TRUE(argx_result_option_int(result, fixture.num, &value));
    NAOS_CHECK_INT_EQ(value, -5);
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.debug), 1);
    argx_result_destroy(result);

    tear_down(&fixture);
}

/* ---- Options that are given more than once ------------------------------ */

NAOS_TEST(a_flag_that_is_given_twice_is_given)
{
    fixture_t fixture;
    char *argv[] = { "tool", "--verbose", "--verbose" };
    argx_result_t *result;

    set_up(&fixture);
    result = parse_ok(&fixture, ARGC(argv), argv);

    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.verbose));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.verbose), ARGX_SOURCE_COMMAND_LINE);

    argx_result_destroy(result);
    tear_down(&fixture);
}

NAOS_TEST(a_count_is_the_number_of_times_it_was_given)
{
    fixture_t fixture;
    char *once[] = { "tool", "--debug" };
    char *thrice[] = { "tool", "--debug", "--verbose", "--debug", "--debug" };
    argx_result_t *result;

    set_up(&fixture);

    result = parse_ok(&fixture, ARGC(once), once);
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.debug), 1);
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.debug), ARGX_SOURCE_COMMAND_LINE);
    argx_result_destroy(result);

    result = parse_ok(&fixture, ARGC(thrice), thrice);
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.debug), 3);
    argx_result_destroy(result);

    tear_down(&fixture);
}

NAOS_TEST(a_negatable_flag_has_the_state_that_was_given_last)
{
    fixture_t fixture;
    char *on_off[] = { "tool", "--color", "--no-color" };
    char *off_on[] = { "tool", "--no-color", "--color" };
    char *on_off_on[] = { "tool", "--pager", "--no-pager", "--verbose", "--pager" };
    argx_result_t *result;

    set_up(&fixture);

    result = parse_ok(&fixture, ARGC(on_off), on_off);
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.color));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.color), ARGX_SOURCE_COMMAND_LINE);
    argx_result_destroy(result);

    result = parse_ok(&fixture, ARGC(off_on), off_on);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.color));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.color), ARGX_SOURCE_COMMAND_LINE);
    argx_result_destroy(result);

    result = parse_ok(&fixture, ARGC(on_off_on), on_off_on);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.pager));
    argx_result_destroy(result);

    tear_down(&fixture);
}

NAOS_TEST(an_option_with_a_value_has_the_value_that_was_given_last)
{
    fixture_t fixture;
    char *argv[] = { "tool", "--out", "a", "--out", "b" };
    char *mixed[] = { "tool", "--out=a", "--out", "b", "--out=c" };
    argx_result_t *result;

    set_up(&fixture);

    result = parse_ok(&fixture, ARGC(argv), argv);
    NAOS_CHECK_STR_EQ(string_of(result, fixture.out), "b");
    argx_result_destroy(result);

    result = parse_ok(&fixture, ARGC(mixed), mixed);
    NAOS_CHECK_STR_EQ(string_of(result, fixture.out), "c");
    argx_result_destroy(result);

    tear_down(&fixture);
}

NAOS_TEST(every_value_of_an_option_is_converted_also_one_that_is_replaced)
{
    fixture_t fixture;
    char *argv[] = { "tool", "--num", "x", "--num", "5" };
    char *later[] = { "tool", "--num", "5", "--num=x" };
    argx_result_t *result;
    int64_t number = 0;

    set_up(&fixture);

    /* The argument of the error is the one that holds the value. */
    result = parse_bad(&fixture, ARGC(argv), argv, ARGX_PARSE_ERROR_INVALID_VALUE, 2,
                       "invalid value 'x' for option '--num' (expected integer)");
    NAOS_REQUIRE_TRUE(argx_result_error_value(result) != NULL);
    NAOS_CHECK_STR_EQ(argx_result_error_value(result), "x");
    NAOS_CHECK_TRUE(argx_result_error_option(result) == fixture.num);
    check_no_value(result, fixture.num);
    argx_result_destroy(result);

    /* The value that was converted before the error stays. */
    result = parse_bad(&fixture, ARGC(later), later, ARGX_PARSE_ERROR_INVALID_VALUE, 3,
                       "invalid value 'x' for option '--num' (expected integer)");
    NAOS_REQUIRE_TRUE(argx_result_error_value(result) != NULL);
    NAOS_CHECK_STR_EQ(argx_result_error_value(result), "x");
    NAOS_CHECK_TRUE(argx_result_error_option(result) == fixture.num);
    NAOS_REQUIRE_TRUE(argx_result_option_int(result, fixture.num, &number));
    NAOS_CHECK_INT_EQ(number, 5);
    argx_result_destroy(result);

    tear_down(&fixture);
}

/* ---- Parse errors ------------------------------------------------------- */

NAOS_TEST(an_option_with_a_value_at_the_end_has_no_value)
{
    fixture_t fixture;
    char *argv[] = { "tool", "--verbose", "--name" };
    argx_result_t *result;

    set_up(&fixture);
    result = parse_bad(&fixture, ARGC(argv), argv, ARGX_PARSE_ERROR_MISSING_VALUE, 2,
                       "option '--name' requires a value");

    NAOS_CHECK_TRUE(argx_result_error_value(result) == NULL);
    NAOS_CHECK_TRUE(argx_result_error_option(result) == fixture.name);
    check_no_value(result, fixture.name);
    /* What was read before the error can be read. */
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.verbose));

    argx_result_destroy(result);
    tear_down(&fixture);
}

NAOS_TEST(a_value_for_an_option_that_takes_none_is_an_error)
{
    fixture_t fixture;
    char *flag[] = { "tool", "--verbose=1" };
    char *count[] = { "tool", "--debug", "--debug=2" };
    char *negatable[] = { "tool", "--color=yes" };
    char *negated[] = { "tool", "--no-color=x" };
    char *empty[] = { "tool", "--verbose=" };
    argx_result_t *result;

    set_up(&fixture);

    result = parse_bad(&fixture, ARGC(flag), flag, ARGX_PARSE_ERROR_UNEXPECTED_VALUE, 1,
                       "option '--verbose' does not take a value");
    NAOS_REQUIRE_TRUE(argx_result_error_value(result) != NULL);
    NAOS_CHECK_STR_EQ(argx_result_error_value(result), "1");
    NAOS_CHECK_TRUE(argx_result_error_option(result) == fixture.verbose);
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.verbose));
    argx_result_destroy(result);

    result = parse_bad(&fixture, ARGC(count), count, ARGX_PARSE_ERROR_UNEXPECTED_VALUE, 2,
                       "option '--debug' does not take a value");
    NAOS_REQUIRE_TRUE(argx_result_error_value(result) != NULL);
    NAOS_CHECK_STR_EQ(argx_result_error_value(result), "2");
    NAOS_CHECK_TRUE(argx_result_error_option(result) == fixture.debug);
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.debug), 1);
    argx_result_destroy(result);

    result = parse_bad(&fixture, ARGC(negatable), negatable, ARGX_PARSE_ERROR_UNEXPECTED_VALUE, 1,
                       "option '--color' does not take a value");
    NAOS_REQUIRE_TRUE(argx_result_error_value(result) != NULL);
    NAOS_CHECK_STR_EQ(argx_result_error_value(result), "yes");
    NAOS_CHECK_TRUE(argx_result_error_option(result) == fixture.color);
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.color));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.color), ARGX_SOURCE_DEFAULT);
    argx_result_destroy(result);

    /* The message has the name as it was written. */
    result = parse_bad(&fixture, ARGC(negated), negated, ARGX_PARSE_ERROR_UNEXPECTED_VALUE, 1,
                       "option '--no-color' does not take a value");
    NAOS_REQUIRE_TRUE(argx_result_error_value(result) != NULL);
    NAOS_CHECK_STR_EQ(argx_result_error_value(result), "x");
    NAOS_CHECK_TRUE(argx_result_error_option(result) == fixture.color);
    argx_result_destroy(result);

    /* An empty value is a value. */
    result = parse_bad(&fixture, ARGC(empty), empty, ARGX_PARSE_ERROR_UNEXPECTED_VALUE, 1,
                       "option '--verbose' does not take a value");
    NAOS_REQUIRE_TRUE(argx_result_error_value(result) != NULL);
    NAOS_CHECK_STR_EQ(argx_result_error_value(result), "");
    argx_result_destroy(result);

    tear_down(&fixture);
}

NAOS_TEST(an_attached_value_that_its_type_does_not_take_is_an_error_at_its_argument)
{
    fixture_t fixture;
    char *argv[] = { "tool", "--verbose", "--num=x" };
    char *empty[] = { "tool", "--num=" };
    argx_result_t *result;

    set_up(&fixture);

    result = parse_bad(&fixture, ARGC(argv), argv, ARGX_PARSE_ERROR_INVALID_VALUE, 2,
                       "invalid value 'x' for option '--num' (expected integer)");
    NAOS_REQUIRE_TRUE(argx_result_error_value(result) != NULL);
    NAOS_CHECK_STR_EQ(argx_result_error_value(result), "x");
    NAOS_CHECK_TRUE(argx_result_error_option(result) == fixture.num);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.verbose));
    check_no_value(result, fixture.num);
    argx_result_destroy(result);

    result = parse_bad(&fixture, ARGC(empty), empty, ARGX_PARSE_ERROR_INVALID_VALUE, 1,
                       "invalid value '' for option '--num' (expected integer)");
    NAOS_REQUIRE_TRUE(argx_result_error_value(result) != NULL);
    NAOS_CHECK_STR_EQ(argx_result_error_value(result), "");
    argx_result_destroy(result);

    tear_down(&fixture);
}

NAOS_TEST(the_scan_stops_at_the_first_error)
{
    fixture_t fixture;
    char *argv[] = { "tool", "--verbose", "--bogus", "--debug", "--name", "x", "--other" };
    argx_result_t *result;

    set_up(&fixture);
    result = parse_bad(&fixture, ARGC(argv), argv, ARGX_PARSE_ERROR_UNKNOWN_OPTION, 2,
                       "unknown option '--bogus'");

    /* What comes before the error was read, what comes after it was not. */
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.verbose));
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.debug), 0);
    check_no_value(result, fixture.name);
    NAOS_CHECK_UINT_EQ(argx_result_command_count(result), 1);
    NAOS_CHECK_TRUE(argx_result_command(result) == fixture.root);

    argx_result_destroy(result);
    tear_down(&fixture);
}

/* ---- Short options ------------------------------------------------------ */

/* The schema of the tests of the short options: the command "short". */
typedef struct short_fixture
{
    argx_command_t *root;
    argx_option_t *a;       /* -a: a flag */
    argx_option_t *b;       /* -b: a flag */
    argx_option_t *c;       /* -c: a flag */
    argx_option_t *verbose; /* -v, --verbose: a count */
    argx_option_t *color;   /* -C, --color, --no-color: a negatable flag, initially off */
    argx_option_t *out;     /* -o, --out: a string */
    argx_option_t *num;     /* -n, --num: an integer */
    argx_option_t *one;     /* -1: a flag; only with a digit */
} short_fixture_t;

/*
 * Installs the test allocator, builds the schema and validates it. The
 * option "-1" is added when with_digit is true.
 */
static void set_up_short(short_fixture_t *fixture, bool with_digit)
{
    argx_command_t *root = NULL;

    argx_test_allocator_install();
    NAOS_REQUIRE_TRUE(argx_command_create("short", &root) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(root != NULL);
    fixture->root = root;
    fixture->one = NULL;

    NAOS_REQUIRE_TRUE(argx_command_add_flag(root, 'a', NULL, &fixture->a) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_flag(root, 'b', NULL, &fixture->b) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_flag(root, 'c', NULL, &fixture->c) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_count(root, 'v', "verbose", &fixture->verbose) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_negatable(root, 'C', "color", &fixture->color) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_value(root, 'o', "out", argx_type_string(), &fixture->out) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_value(root, 'n', "num", argx_type_int(), &fixture->num) ==
                      ARGX_ERROR_NONE);
    if (with_digit)
    {
        NAOS_REQUIRE_TRUE(argx_command_add_flag(root, '1', NULL, &fixture->one) == ARGX_ERROR_NONE);
    }

    NAOS_REQUIRE_TRUE(argx_command_validate(root, NULL, NULL, NULL) == ARGX_ERROR_NONE);
}

/* Destroys the schema and ends the test: nothing stays allocated. */
static void tear_down_short(short_fixture_t *fixture)
{
    argx_command_destroy(fixture->root);
    argx_test_allocator_uninstall();
    check_clean();
}

/* Parses argv, which must match the schema. Returns the result. */
static argx_result_t *parse_short_ok(const short_fixture_t *fixture, int argc, char *const *argv)
{
    argx_result_t *result = NULL;

    NAOS_REQUIRE_TRUE(argx_parse(fixture->root, argc, argv, &result) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(result != NULL);
    NAOS_CHECK_INT_EQ(argx_result_error(result), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_result_error_kind(result), ARGX_PARSE_ERROR_NONE);
    NAOS_CHECK_TRUE(argx_result_error_message(result) == NULL);
    NAOS_CHECK_INT_EQ(argx_result_error_short_name(result), '\0');
    return result;
}

/*
 * Parses argv, which must have a parse error of the kind, at the argument
 * with the index, about the letter short_name, with the message. Returns
 * the result.
 */
static argx_result_t *parse_short_bad(const short_fixture_t *fixture, int argc, char *const *argv,
                                      argx_parse_error_kind_t kind, size_t index, char short_name,
                                      const char *message)
{
    argx_result_t *result = NULL;

    NAOS_REQUIRE_TRUE(argx_parse(fixture->root, argc, argv, &result) == ARGX_ERROR_INVALID_COMMAND_LINE);
    NAOS_REQUIRE_TRUE(result != NULL);
    NAOS_CHECK_INT_EQ(argx_result_error(result), ARGX_ERROR_INVALID_COMMAND_LINE);
    NAOS_CHECK_INT_EQ(argx_result_error_kind(result), kind);
    NAOS_CHECK_INT_EQ(argx_result_error_source(result), ARGX_SOURCE_COMMAND_LINE);
    NAOS_CHECK_UINT_EQ(argx_result_error_index(result), index);
    NAOS_REQUIRE_TRUE(argx_result_error_argument(result) != NULL);
    NAOS_CHECK_STR_EQ(argx_result_error_argument(result), argv[index]);
    NAOS_CHECK_TRUE(argx_result_error_command(result) == fixture->root);
    NAOS_CHECK_TRUE(argx_result_error_positional(result) == NULL);
    NAOS_CHECK_INT_EQ(argx_result_error_short_name(result), short_name);
    NAOS_REQUIRE_TRUE(argx_result_error_message(result) != NULL);
    NAOS_CHECK_STR_EQ(argx_result_error_message(result), message);
    return result;
}

NAOS_TEST(a_short_name_gives_a_flag)
{
    short_fixture_t fixture;
    char *argv[] = { "short", "-a" };
    argx_result_t *result;

    set_up_short(&fixture, false);
    result = parse_short_ok(&fixture, ARGC(argv), argv);

    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.a));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.a), ARGX_SOURCE_COMMAND_LINE);
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.b));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.b), ARGX_SOURCE_DEFAULT);
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.verbose), 0);

    argx_result_destroy(result);
    tear_down_short(&fixture);
}

NAOS_TEST(a_short_name_gives_a_count_by_one)
{
    short_fixture_t fixture;
    char *argv[] = { "short", "-v" };
    argx_result_t *result;

    set_up_short(&fixture, false);
    result = parse_short_ok(&fixture, ARGC(argv), argv);

    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.verbose), 1);
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.verbose), ARGX_SOURCE_COMMAND_LINE);

    argx_result_destroy(result);
    tear_down_short(&fixture);
}

NAOS_TEST(a_bundle_gives_each_of_its_options)
{
    short_fixture_t fixture;
    char *argv[] = { "short", "-abc" };
    char *other[] = { "short", "-ca", "-v" };
    argx_result_t *result;

    set_up_short(&fixture, false);

    result = parse_short_ok(&fixture, ARGC(argv), argv);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.a));
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.b));
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.c));
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.verbose), 0);
    argx_result_destroy(result);

    /* The scan goes on after a bundle. */
    result = parse_short_ok(&fixture, ARGC(other), other);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.a));
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.b));
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.c));
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.verbose), 1);
    argx_result_destroy(result);

    tear_down_short(&fixture);
}

NAOS_TEST(a_count_in_a_bundle_is_one_more_for_each_letter)
{
    short_fixture_t fixture;
    char *argv[] = { "short", "-vvv" };
    char *mixed[] = { "short", "-v", "--verbose", "-vv" };
    char *between[] = { "short", "-vava" };
    argx_result_t *result;

    set_up_short(&fixture, false);

    result = parse_short_ok(&fixture, ARGC(argv), argv);
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.verbose), 3);
    argx_result_destroy(result);

    /* The short name and the long name count together. */
    result = parse_short_ok(&fixture, ARGC(mixed), mixed);
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.verbose), 4);
    argx_result_destroy(result);

    result = parse_short_ok(&fixture, ARGC(between), between);
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.verbose), 2);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.a));
    argx_result_destroy(result);

    tear_down_short(&fixture);
}

NAOS_TEST(the_short_name_of_a_negatable_flag_switches_it_on)
{
    short_fixture_t fixture;
    char *argv[] = { "short", "-C" };
    char *after_off[] = { "short", "--no-color", "-aC" };
    char *before_off[] = { "short", "-C", "--no-color" };
    argx_result_t *result;

    set_up_short(&fixture, false);

    result = parse_short_ok(&fixture, ARGC(argv), argv);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.color));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.color), ARGX_SOURCE_COMMAND_LINE);
    argx_result_destroy(result);

    result = parse_short_ok(&fixture, ARGC(after_off), after_off);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.color));
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.a));
    argx_result_destroy(result);

    result = parse_short_ok(&fixture, ARGC(before_off), before_off);
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.color));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.color), ARGX_SOURCE_COMMAND_LINE);
    argx_result_destroy(result);

    tear_down_short(&fixture);
}

NAOS_TEST(a_short_option_takes_its_value_from_the_next_argument)
{
    short_fixture_t fixture;
    char *argv[] = { "short", "-o", "value", "-a" };
    char *bundle[] = { "short", "-abo", "value", "-c" };
    char *empty[] = { "short", "-o", "" };
    argx_result_t *result;

    set_up_short(&fixture, false);

    result = parse_short_ok(&fixture, ARGC(argv), argv);
    NAOS_CHECK_STR_EQ(string_of(result, fixture.out), "value");
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.out), ARGX_SOURCE_COMMAND_LINE);
    /* The scan goes on after the value. */
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.a));
    argx_result_destroy(result);

    result = parse_short_ok(&fixture, ARGC(bundle), bundle);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.a));
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.b));
    NAOS_CHECK_STR_EQ(string_of(result, fixture.out), "value");
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.c));
    argx_result_destroy(result);

    result = parse_short_ok(&fixture, ARGC(empty), empty);
    NAOS_CHECK_STR_EQ(string_of(result, fixture.out), "");
    argx_result_destroy(result);

    tear_down_short(&fixture);
}

NAOS_TEST(a_short_option_takes_the_rest_of_its_bundle_as_its_value)
{
    short_fixture_t fixture;
    char *argv[] = { "short", "-ovalue", "-a" };
    char *bundle[] = { "short", "-abovalue" };
    char *letters[] = { "short", "-oabc", "-n42" };
    char *equals[] = { "short", "-o=value" };
    char *dash[] = { "short", "-o-v", "-n-7" };
    char *spaces[] = { "short", "-o a b " };
    argx_result_t *result;
    int64_t number = 0;

    set_up_short(&fixture, false);

    result = parse_short_ok(&fixture, ARGC(argv), argv);
    NAOS_CHECK_STR_EQ(string_of(result, fixture.out), "value");
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.out), ARGX_SOURCE_COMMAND_LINE);
    /* The next argument was not taken as the value. */
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.a));
    argx_result_destroy(result);

    result = parse_short_ok(&fixture, ARGC(bundle), bundle);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.a));
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.b));
    NAOS_CHECK_STR_EQ(string_of(result, fixture.out), "value");
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.verbose), 0);
    argx_result_destroy(result);

    /* The rest is the value, not more options. */
    result = parse_short_ok(&fixture, ARGC(letters), letters);
    NAOS_CHECK_STR_EQ(string_of(result, fixture.out), "abc");
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.a));
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.b));
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.c));
    NAOS_REQUIRE_TRUE(argx_result_option_int(result, fixture.num, &number));
    NAOS_CHECK_INT_EQ(number, 42);
    argx_result_destroy(result);

    /* An '=' is a part of the value. */
    result = parse_short_ok(&fixture, ARGC(equals), equals);
    NAOS_CHECK_STR_EQ(string_of(result, fixture.out), "=value");
    argx_result_destroy(result);

    result = parse_short_ok(&fixture, ARGC(dash), dash);
    NAOS_CHECK_STR_EQ(string_of(result, fixture.out), "-v");
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.verbose), 0);
    NAOS_REQUIRE_TRUE(argx_result_option_int(result, fixture.num, &number));
    NAOS_CHECK_INT_EQ(number, -7);
    argx_result_destroy(result);

    result = parse_short_ok(&fixture, ARGC(spaces), spaces);
    NAOS_CHECK_STR_EQ(string_of(result, fixture.out), " a b ");
    argx_result_destroy(result);

    tear_down_short(&fixture);
}

NAOS_TEST(the_argument_after_a_short_option_is_the_value_whatever_it_looks_like)
{
    short_fixture_t fixture;
    char *terminator[] = { "short", "-o", "--", "-v" };
    char *number[] = { "short", "-o", "-1", "-v" };
    char *bundle[] = { "short", "-ao", "-bc", "-v" };
    char *unknown[] = { "short", "-o", "--bogus=1", "-v" };
    char *negative[] = { "short", "-n", "-5", "-v" };
    argx_result_t *result;
    int64_t value = 0;

    set_up_short(&fixture, false);

    result = parse_short_ok(&fixture, ARGC(terminator), terminator);
    NAOS_CHECK_STR_EQ(string_of(result, fixture.out), "--");
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.verbose), 1);
    argx_result_destroy(result);

    result = parse_short_ok(&fixture, ARGC(number), number);
    NAOS_CHECK_STR_EQ(string_of(result, fixture.out), "-1");
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.verbose), 1);
    argx_result_destroy(result);

    /* The argument is not read as a bundle: its flags are not given. */
    result = parse_short_ok(&fixture, ARGC(bundle), bundle);
    NAOS_CHECK_STR_EQ(string_of(result, fixture.out), "-bc");
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.a));
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.b));
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.c));
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.verbose), 1);
    argx_result_destroy(result);

    result = parse_short_ok(&fixture, ARGC(unknown), unknown);
    NAOS_CHECK_STR_EQ(string_of(result, fixture.out), "--bogus=1");
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.verbose), 1);
    argx_result_destroy(result);

    result = parse_short_ok(&fixture, ARGC(negative), negative);
    NAOS_REQUIRE_TRUE(argx_result_option_int(result, fixture.num, &value));
    NAOS_CHECK_INT_EQ(value, -5);
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.verbose), 1);
    argx_result_destroy(result);

    tear_down_short(&fixture);
}

NAOS_TEST(a_short_option_with_a_value_has_the_value_that_was_given_last)
{
    short_fixture_t fixture;
    char *argv[] = { "short", "-oa", "--out=b", "-o", "c" };
    char *long_last[] = { "short", "-o", "a", "--out", "b" };
    argx_result_t *result;

    set_up_short(&fixture, false);

    result = parse_short_ok(&fixture, ARGC(argv), argv);
    NAOS_CHECK_STR_EQ(string_of(result, fixture.out), "c");
    argx_result_destroy(result);

    result = parse_short_ok(&fixture, ARGC(long_last), long_last);
    NAOS_CHECK_STR_EQ(string_of(result, fixture.out), "b");
    argx_result_destroy(result);

    tear_down_short(&fixture);
}

NAOS_TEST(a_letter_that_is_no_short_name_is_an_unknown_option)
{
    short_fixture_t fixture;
    char *argv[] = { "short", "-ax", "-b" };
    char *alone[] = { "short", "-x" };
    char *first[] = { "short", "-v", "-xa" };
    char *upper[] = { "short", "-A" };
    char *equals[] = { "short", "-a=1" };
    char *dash[] = { "short", "-a-b" };
    argx_result_t *result;

    set_up_short(&fixture, false);

    /*
     * The argument of the error is the whole bundle; the letter is read
     * with argx_result_error_short_name().
     */
    result = parse_short_bad(&fixture, ARGC(argv), argv, ARGX_PARSE_ERROR_UNKNOWN_OPTION, 1, 'x',
                             "unknown option '-x'");
    NAOS_CHECK_TRUE(argx_result_error_value(result) == NULL);
    NAOS_CHECK_TRUE(argx_result_error_option(result) == NULL);
    /* The options before the letter were read, what comes after was not. */
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.a));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.a), ARGX_SOURCE_COMMAND_LINE);
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.b));
    argx_result_destroy(result);

    result = parse_short_bad(&fixture, ARGC(alone), alone, ARGX_PARSE_ERROR_UNKNOWN_OPTION, 1, 'x',
                             "unknown option '-x'");
    NAOS_CHECK_TRUE(argx_result_error_option(result) == NULL);
    argx_result_destroy(result);

    result = parse_short_bad(&fixture, ARGC(first), first, ARGX_PARSE_ERROR_UNKNOWN_OPTION, 2, 'x',
                             "unknown option '-x'");
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.verbose), 1);
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.a));
    argx_result_destroy(result);

    /* Upper and lower case are different. */
    result = parse_short_bad(&fixture, ARGC(upper), upper, ARGX_PARSE_ERROR_UNKNOWN_OPTION, 1, 'A',
                             "unknown option '-A'");
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.a));
    argx_result_destroy(result);

    /* After an option that takes no value, '=' and '-' are letters. */
    result = parse_short_bad(&fixture, ARGC(equals), equals, ARGX_PARSE_ERROR_UNKNOWN_OPTION, 1, '=',
                             "unknown option '-='");
    NAOS_CHECK_TRUE(argx_result_error_value(result) == NULL);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.a));
    argx_result_destroy(result);

    result = parse_short_bad(&fixture, ARGC(dash), dash, ARGX_PARSE_ERROR_UNKNOWN_OPTION, 1, '-',
                             "unknown option '--'");
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.a));
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.b));
    argx_result_destroy(result);

    tear_down_short(&fixture);
}

NAOS_TEST(a_short_option_with_a_value_at_the_end_has_no_value)
{
    short_fixture_t fixture;
    char *argv[] = { "short", "-o" };
    char *bundle[] = { "short", "-v", "-abo" };
    argx_result_t *result;

    set_up_short(&fixture, false);

    result = parse_short_bad(&fixture, ARGC(argv), argv, ARGX_PARSE_ERROR_MISSING_VALUE, 1, 'o',
                             "option '-o' requires a value");
    NAOS_CHECK_TRUE(argx_result_error_value(result) == NULL);
    NAOS_CHECK_TRUE(argx_result_error_option(result) == fixture.out);
    check_no_value(result, fixture.out);
    argx_result_destroy(result);

    result = parse_short_bad(&fixture, ARGC(bundle), bundle, ARGX_PARSE_ERROR_MISSING_VALUE, 2, 'o',
                             "option '-o' requires a value");
    NAOS_CHECK_TRUE(argx_result_error_value(result) == NULL);
    NAOS_CHECK_TRUE(argx_result_error_option(result) == fixture.out);
    check_no_value(result, fixture.out);
    /* What was read before the error can be read. */
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.verbose), 1);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.a));
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.b));
    argx_result_destroy(result);

    tear_down_short(&fixture);
}

NAOS_TEST(a_value_of_a_short_option_that_its_type_does_not_take_is_an_error_at_its_argument)
{
    short_fixture_t fixture;
    char *attached[] = { "short", "-v", "-anx", "-b" };
    char *next[] = { "short", "-v", "-an", "x", "-b" };
    char *alone[] = { "short", "-n", "--" };
    char *later[] = { "short", "-n5", "-n=6" };
    argx_result_t *result;
    int64_t number = 0;

    set_up_short(&fixture, false);

    /* A value in the bundle: the argument of the error is the bundle. */
    result = parse_short_bad(&fixture, ARGC(attached), attached, ARGX_PARSE_ERROR_INVALID_VALUE, 2, 'n',
                             "invalid value 'x' for option '-n' (expected integer)");
    NAOS_REQUIRE_TRUE(argx_result_error_value(result) != NULL);
    NAOS_CHECK_STR_EQ(argx_result_error_value(result), "x");
    NAOS_CHECK_TRUE(argx_result_error_option(result) == fixture.num);
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.verbose), 1);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.a));
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.b));
    check_no_value(result, fixture.num);
    argx_result_destroy(result);

    /*
     * A value in the next argument: the argument of the error is that one,
     * and the letter is still that of the option.
     */
    result = parse_short_bad(&fixture, ARGC(next), next, ARGX_PARSE_ERROR_INVALID_VALUE, 3, 'n',
                             "invalid value 'x' for option '-n' (expected integer)");
    NAOS_REQUIRE_TRUE(argx_result_error_value(result) != NULL);
    NAOS_CHECK_STR_EQ(argx_result_error_value(result), "x");
    NAOS_CHECK_TRUE(argx_result_error_value(result) == argx_result_error_argument(result));
    NAOS_CHECK_TRUE(argx_result_error_option(result) == fixture.num);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.a));
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.b));
    check_no_value(result, fixture.num);
    argx_result_destroy(result);

    result = parse_short_bad(&fixture, ARGC(alone), alone, ARGX_PARSE_ERROR_INVALID_VALUE, 2, 'n',
                             "invalid value '--' for option '-n' (expected integer)");
    NAOS_CHECK_TRUE(argx_result_error_option(result) == fixture.num);
    argx_result_destroy(result);

    /* The value that was converted before the error stays. */
    result = parse_short_bad(&fixture, ARGC(later), later, ARGX_PARSE_ERROR_INVALID_VALUE, 2, 'n',
                             "invalid value '=6' for option '-n' (expected integer)");
    NAOS_REQUIRE_TRUE(argx_result_error_value(result) != NULL);
    NAOS_CHECK_STR_EQ(argx_result_error_value(result), "=6");
    NAOS_REQUIRE_TRUE(argx_result_option_int(result, fixture.num, &number));
    NAOS_CHECK_INT_EQ(number, 5);
    argx_result_destroy(result);

    tear_down_short(&fixture);
}

/* ---- Arguments with the form of a negative number ----------------------- */

NAOS_TEST(a_digit_is_a_short_option_of_a_command_that_has_a_digit_as_a_short_name)
{
    short_fixture_t fixture;
    char *argv[] = { "short", "-1" };
    char *bundle[] = { "short", "-1a", "-v1" };
    char *other_digit[] = { "short", "-a", "-2" };
    char *fraction[] = { "short", "-.5" };
    argx_result_t *result;

    set_up_short(&fixture, true);

    result = parse_short_ok(&fixture, ARGC(argv), argv);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.one));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.one), ARGX_SOURCE_COMMAND_LINE);
    argx_result_destroy(result);

    result = parse_short_ok(&fixture, ARGC(bundle), bundle);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.one));
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.a));
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.verbose), 1);
    argx_result_destroy(result);

    /*
     * Every argument with the form of a negative number is a bundle then,
     * also one whose digit is no short name.
     */
    result = parse_short_bad(&fixture, ARGC(other_digit), other_digit, ARGX_PARSE_ERROR_UNKNOWN_OPTION, 2,
                             '2', "unknown option '-2'");
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.a));
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.one));
    argx_result_destroy(result);

    result = parse_short_bad(&fixture, ARGC(fraction), fraction, ARGX_PARSE_ERROR_UNKNOWN_OPTION, 1, '.',
                             "unknown option '-.'");
    argx_result_destroy(result);

    tear_down_short(&fixture);
}

NAOS_TEST(a_negative_number_is_not_a_bundle_of_a_command_that_has_no_digit_as_a_short_name)
{
    short_fixture_t fixture;
    char *argv[] = { "short", "-a", "-1", "-v" };
    char *more[] = { "-12", "-.5", "-1x", "-0", "-9a" };
    char *no_number[] = { "short", "-.", "-a" };
    char *letter_first[] = { "short", "-a1" };
    argx_result_t *result;
    size_t at;

    set_up_short(&fixture, false);

    /*
     * Not an unknown option: an argument that is not an option, which this
     * command, one without positionals, does not take.
     */
    result = parse_short_bad(&fixture, ARGC(argv), argv, ARGX_PARSE_ERROR_UNEXPECTED_POSITIONAL, 2, '\0',
                             "unexpected argument '-1'");
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.a));
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.verbose), 0);
    argx_result_destroy(result);

    /* The letters after the digit are not read as options. */
    for (at = 0; at < (sizeof(more) / sizeof(more[0])); at++)
    {
        char *one[2];

        one[0] = "short";
        one[1] = more[at];
        result = NULL;
        NAOS_REQUIRE_TRUE(argx_parse(fixture.root, 2, one, &result) == ARGX_ERROR_INVALID_COMMAND_LINE);
        NAOS_REQUIRE_TRUE(result != NULL);
        NAOS_CHECK_INT_EQ(argx_result_error_kind(result), ARGX_PARSE_ERROR_UNEXPECTED_POSITIONAL);
        NAOS_CHECK_INT_EQ(argx_result_error_short_name(result), '\0');
        NAOS_REQUIRE_TRUE(argx_result_error_value(result) != NULL);
        NAOS_CHECK_STR_EQ(argx_result_error_value(result), more[at]);
        NAOS_CHECK_FALSE(argx_result_flag(result, fixture.a));
        argx_result_destroy(result);
    }

    /* What has not the form of a negative number is a bundle. */
    result = parse_short_bad(&fixture, ARGC(no_number), no_number, ARGX_PARSE_ERROR_UNKNOWN_OPTION, 1, '.',
                             "unknown option '-.'");
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.a));
    argx_result_destroy(result);

    result = parse_short_bad(&fixture, ARGC(letter_first), letter_first, ARGX_PARSE_ERROR_UNKNOWN_OPTION,
                             1, '1', "unknown option '-1'");
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.a));
    argx_result_destroy(result);

    tear_down_short(&fixture);
}

/* ---- A lack of memory --------------------------------------------------- */

/*
 * Parses argv once for each allocation of the parse, with that allocation
 * failing, and then once with none failing, which must return expected.
 */
static void check_every_allocation_failing(const fixture_t *fixture, int argc, char *const *argv,
                                           argx_error_t expected)
{
    size_t nth;

    for (nth = 1; nth < 100; nth++)
    {
        const size_t live = argx_test_allocator_live();
        argx_result_t *result = NULL;
        argx_error_t error;

        argx_test_allocator_fail_at(argx_test_allocator_requests() + nth);
        error = argx_parse(fixture->root, argc, argv, &result);
        argx_test_allocator_fail_at(0);

        if (error != ARGX_ERROR_OUT_OF_MEMORY)
        {
            /* The parse made fewer than nth allocations. */
            NAOS_CHECK_INT_EQ(error, expected);
            NAOS_REQUIRE_TRUE(result != NULL);
            NAOS_CHECK_INT_EQ(argx_result_error(result), expected);
            argx_result_destroy(result);
            NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), live);
            NAOS_CHECK_TRUE(nth > 1);
            return;
        }
        NAOS_CHECK_TRUE(result == NULL);
        NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), live);
    }
    NAOS_FAIL("the parse never succeeded");
}

NAOS_TEST(a_parse_that_cannot_allocate_returns_no_result_and_leaves_nothing_allocated)
{
    fixture_t fixture;
    char *argv[] = { "tool", "--verbose", "--name", "a", "--name=b", "--num", "5", "--debug" };
    char *unknown[] = { "tool", "--name", "a", "--bogus" };
    char *invalid[] = { "tool", "--name", "a", "--num", "x" };
    char *bundles[] = { "tool", "-vd", "-ofile", "-do", "other", "-d" };
    char *unknown_letter[] = { "tool", "-oa", "-vx" };
    char *no_value[] = { "tool", "-oa", "-vo" };
    argx_result_t *sentinel = NULL;
    argx_result_t *result;

    set_up(&fixture);

    check_every_allocation_failing(&fixture, ARGC(argv), argv, ARGX_ERROR_NONE);
    check_every_allocation_failing(&fixture, ARGC(unknown), unknown, ARGX_ERROR_INVALID_COMMAND_LINE);
    check_every_allocation_failing(&fixture, ARGC(invalid), invalid, ARGX_ERROR_INVALID_COMMAND_LINE);
    check_every_allocation_failing(&fixture, ARGC(bundles), bundles, ARGX_ERROR_NONE);
    check_every_allocation_failing(&fixture, ARGC(unknown_letter), unknown_letter,
                                   ARGX_ERROR_INVALID_COMMAND_LINE);
    check_every_allocation_failing(&fixture, ARGC(no_value), no_value, ARGX_ERROR_INVALID_COMMAND_LINE);

    /* *out is set to NULL whatever it held. */
    result = parse_ok(&fixture, 0, NULL);
    sentinel = result;
    argx_test_allocator_fail_at(argx_test_allocator_requests() + 1);
    NAOS_CHECK_INT_EQ(argx_parse(fixture.root, ARGC(argv), argv, &sentinel), ARGX_ERROR_OUT_OF_MEMORY);
    argx_test_allocator_fail_at(0);
    NAOS_CHECK_TRUE(sentinel == NULL);
    argx_result_destroy(result);

    tear_down(&fixture);
}

NAOS_SUITE(entry,
    NAOS_ADD_TEST(no_argument_at_all_is_a_command_line_on_which_nothing_was_given),
    NAOS_ADD_TEST(the_program_name_alone_is_a_command_line_on_which_nothing_was_given),
    NAOS_ADD_TEST(the_program_name_is_not_parsed),
    NAOS_ADD_TEST(argv_can_be_the_argv_of_main_with_its_null_at_the_end),
    NAOS_ADD_TEST(two_parses_with_one_schema_give_the_same_result),
    NAOS_ADD_TEST(the_result_does_not_change_when_argv_is_changed_after_the_parse)
)

NAOS_SUITE(long_options,
    NAOS_ADD_TEST(a_long_name_gives_a_flag),
    NAOS_ADD_TEST(a_value_is_attached_with_an_equals_sign),
    NAOS_ADD_TEST(a_value_is_taken_from_the_next_argument),
    NAOS_ADD_TEST(an_attached_value_can_be_empty),
    NAOS_ADD_TEST(a_value_from_the_next_argument_can_be_empty),
    NAOS_ADD_TEST(the_name_ends_at_the_first_equals_sign),
    NAOS_ADD_TEST(a_value_is_passed_to_the_value_type_as_it_was_written),
    NAOS_ADD_TEST(the_long_name_of_a_negatable_flag_switches_it_on),
    NAOS_ADD_TEST(the_negated_name_of_a_negatable_flag_switches_it_off),
    NAOS_ADD_TEST(a_name_in_another_case_is_an_unknown_option),
    NAOS_ADD_TEST(a_part_of_a_name_and_a_longer_name_are_unknown_options),
    NAOS_ADD_TEST(the_next_argument_is_the_value_whatever_it_looks_like)
)

NAOS_SUITE(short_options,
    NAOS_ADD_TEST(a_short_name_gives_a_flag),
    NAOS_ADD_TEST(a_short_name_gives_a_count_by_one),
    NAOS_ADD_TEST(a_bundle_gives_each_of_its_options),
    NAOS_ADD_TEST(a_count_in_a_bundle_is_one_more_for_each_letter),
    NAOS_ADD_TEST(the_short_name_of_a_negatable_flag_switches_it_on),
    NAOS_ADD_TEST(a_short_option_takes_its_value_from_the_next_argument),
    NAOS_ADD_TEST(a_short_option_takes_the_rest_of_its_bundle_as_its_value),
    NAOS_ADD_TEST(the_argument_after_a_short_option_is_the_value_whatever_it_looks_like),
    NAOS_ADD_TEST(a_short_option_with_a_value_has_the_value_that_was_given_last),
    NAOS_ADD_TEST(a_letter_that_is_no_short_name_is_an_unknown_option),
    NAOS_ADD_TEST(a_short_option_with_a_value_at_the_end_has_no_value),
    NAOS_ADD_TEST(a_value_of_a_short_option_that_its_type_does_not_take_is_an_error_at_its_argument)
)

NAOS_SUITE(negative_numbers,
    NAOS_ADD_TEST(a_digit_is_a_short_option_of_a_command_that_has_a_digit_as_a_short_name),
    NAOS_ADD_TEST(a_negative_number_is_not_a_bundle_of_a_command_that_has_no_digit_as_a_short_name)
)

NAOS_SUITE(repeated,
    NAOS_ADD_TEST(a_flag_that_is_given_twice_is_given),
    NAOS_ADD_TEST(a_count_is_the_number_of_times_it_was_given),
    NAOS_ADD_TEST(a_negatable_flag_has_the_state_that_was_given_last),
    NAOS_ADD_TEST(an_option_with_a_value_has_the_value_that_was_given_last),
    NAOS_ADD_TEST(every_value_of_an_option_is_converted_also_one_that_is_replaced)
)

NAOS_SUITE(errors,
    NAOS_ADD_TEST(an_option_with_a_value_at_the_end_has_no_value),
    NAOS_ADD_TEST(a_value_for_an_option_that_takes_none_is_an_error),
    NAOS_ADD_TEST(an_attached_value_that_its_type_does_not_take_is_an_error_at_its_argument),
    NAOS_ADD_TEST(the_scan_stops_at_the_first_error)
)

NAOS_SUITE(memory,
    NAOS_ADD_TEST(a_parse_that_cannot_allocate_returns_no_result_and_leaves_nothing_allocated)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(entry),
    NAOS_ADD_SUITE(long_options),
    NAOS_ADD_SUITE(short_options),
    NAOS_ADD_SUITE(negative_numbers),
    NAOS_ADD_SUITE(repeated),
    NAOS_ADD_SUITE(errors),
    NAOS_ADD_SUITE(memory)
)
