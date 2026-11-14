/*
 * Tests of the public parsing API for positionals: argx_parse() of
 * <argx/parse.h>, read back with the argx_result_*() functions.
 *
 * A schema is built the way a consumer does it, validated, and parsed with
 * arguments written as an argv array. The tests cover how the arguments
 * that are not options are assigned to the positionals of a command (one
 * that takes one argument, one that may take one, one that takes any
 * number), that the place of the options among them does not matter, the
 * terminator of the options "--", the arguments that are not options
 * although they start with a '-' (a lone "-", the form of a negative
 * number), and the parse errors of a positional. They also cover the
 * subcommands: an argument that is not an option selects a subcommand of a
 * command that has subcommands, to any depth, the options are those of the
 * current command only, and what is read of a command that was not
 * selected.
 *
 * No test relies on a default value or on a required positional that is
 * missing: a positional that is required is always given.
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
 * The end of every test: the test allocator has no live block and saw no
 * violation of the allocator contract.
 */
static void check_clean(void)
{
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_violations(), 0);
}

/* Installs the test allocator and creates the root of a schema. */
static argx_command_t *create_root(const char *name)
{
    argx_command_t *root = NULL;

    argx_test_allocator_install();
    NAOS_REQUIRE_TRUE(argx_command_create(name, &root) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(root != NULL);
    return root;
}

/* Adds a positional to command. */
static argx_positional_t *add_positional(argx_command_t *command, const char *name,
                                         const argx_type_t *type, argx_arity_t arity)
{
    argx_positional_t *positional = NULL;

    NAOS_REQUIRE_TRUE(argx_command_add_positional(command, name, type, arity, &positional) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(positional != NULL);
    return positional;
}

static void validate(argx_command_t *root)
{
    NAOS_REQUIRE_TRUE(argx_command_validate(root, NULL, NULL, NULL) == ARGX_ERROR_NONE);
}

/* Destroys the schema and ends the test: nothing stays allocated. */
static void destroy_root(argx_command_t *root)
{
    argx_command_destroy(root);
    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * Parses argv, which must match the schema. Returns the result, which the
 * caller destroys.
 */
static argx_result_t *parse_ok(const argx_command_t *root, int argc, char *const *argv)
{
    argx_result_t *result = NULL;

    NAOS_REQUIRE_TRUE(argx_parse(root, argc, argv, &result) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(result != NULL);
    NAOS_CHECK_INT_EQ(argx_result_error(result), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_result_error_kind(result), ARGX_PARSE_ERROR_NONE);
    NAOS_CHECK_TRUE(argx_result_error_message(result) == NULL);
    return result;
}

/*
 * Parses argv, which must have a parse error of the kind, at the argument
 * with the index, whose value is that argument, found while command was
 * the current command, with the message. Returns the result.
 */
static argx_result_t *parse_bad_in(const argx_command_t *root, int argc, char *const *argv,
                                   argx_parse_error_kind_t kind, size_t index,
                                   const argx_command_t *command, const char *message)
{
    argx_result_t *result = NULL;

    NAOS_REQUIRE_TRUE(argx_parse(root, argc, argv, &result) == ARGX_ERROR_INVALID_COMMAND_LINE);
    NAOS_REQUIRE_TRUE(result != NULL);
    NAOS_CHECK_INT_EQ(argx_result_error(result), ARGX_ERROR_INVALID_COMMAND_LINE);
    NAOS_CHECK_INT_EQ(argx_result_error_kind(result), kind);
    NAOS_CHECK_INT_EQ(argx_result_error_source(result), ARGX_SOURCE_COMMAND_LINE);
    NAOS_CHECK_UINT_EQ(argx_result_error_index(result), index);
    NAOS_REQUIRE_TRUE(argx_result_error_argument(result) != NULL);
    NAOS_CHECK_STR_EQ(argx_result_error_argument(result), argv[index]);
    NAOS_REQUIRE_TRUE(argx_result_error_value(result) != NULL);
    NAOS_CHECK_STR_EQ(argx_result_error_value(result), argv[index]);
    NAOS_CHECK_TRUE(argx_result_error_command(result) == command);
    NAOS_CHECK_TRUE(argx_result_error_option(result) == NULL);
    NAOS_CHECK_INT_EQ(argx_result_error_short_name(result), '\0');
    NAOS_REQUIRE_TRUE(argx_result_error_message(result) != NULL);
    NAOS_CHECK_STR_EQ(argx_result_error_message(result), message);
    return result;
}

/*
 * The same, for an error that was found while the root was the current
 * command.
 */
static argx_result_t *parse_bad(const argx_command_t *root, int argc, char *const *argv,
                                argx_parse_error_kind_t kind, size_t index, const char *message)
{
    return parse_bad_in(root, argc, argv, kind, index, root, message);
}

/*
 * Parses argv, which must have an option that command, the current command
 * at the argument with the index, does not have, with the message. Returns
 * the result.
 */
static argx_result_t *parse_unknown_option(const argx_command_t *root, int argc, char *const *argv,
                                           size_t index, const argx_command_t *command,
                                           const char *message)
{
    argx_result_t *result = NULL;

    NAOS_REQUIRE_TRUE(argx_parse(root, argc, argv, &result) == ARGX_ERROR_INVALID_COMMAND_LINE);
    NAOS_REQUIRE_TRUE(result != NULL);
    NAOS_CHECK_INT_EQ(argx_result_error(result), ARGX_ERROR_INVALID_COMMAND_LINE);
    NAOS_CHECK_INT_EQ(argx_result_error_kind(result), ARGX_PARSE_ERROR_UNKNOWN_OPTION);
    NAOS_CHECK_INT_EQ(argx_result_error_source(result), ARGX_SOURCE_COMMAND_LINE);
    NAOS_CHECK_UINT_EQ(argx_result_error_index(result), index);
    NAOS_REQUIRE_TRUE(argx_result_error_argument(result) != NULL);
    NAOS_CHECK_STR_EQ(argx_result_error_argument(result), argv[index]);
    NAOS_CHECK_TRUE(argx_result_error_value(result) == NULL);
    NAOS_CHECK_TRUE(argx_result_error_command(result) == command);
    NAOS_CHECK_TRUE(argx_result_error_option(result) == NULL);
    NAOS_CHECK_TRUE(argx_result_error_positional(result) == NULL);
    NAOS_REQUIRE_TRUE(argx_result_error_message(result) != NULL);
    NAOS_CHECK_STR_EQ(argx_result_error_message(result), message);
    return result;
}

/*
 * Checks that positional has count values, given on the command line when
 * count is not 0 and none otherwise.
 */
static void check_count(const argx_result_t *result, const argx_positional_t *positional, size_t count)
{
    NAOS_CHECK_UINT_EQ(argx_result_positional_count(result, positional), count);
    NAOS_CHECK_INT_EQ(argx_result_positional_source(result, positional),
                      (count == 0) ? ARGX_SOURCE_NONE : ARGX_SOURCE_COMMAND_LINE);
}

/* Checks that the value at index of positional, a string, is expected. */
static void check_string(const argx_result_t *result, const argx_positional_t *positional, size_t index,
                         const char *expected)
{
    const char *text = NULL;

    NAOS_REQUIRE_TRUE(argx_result_positional_string(result, positional, index, &text));
    NAOS_REQUIRE_TRUE(text != NULL);
    NAOS_CHECK_STR_EQ(text, expected);
}

/* ---- The schema "copy": options and positionals of each arity ----------- */

typedef struct copy_fixture
{
    argx_command_t *root;
    argx_option_t *verbose;  /* -v, --verbose: a flag */
    argx_option_t *out;      /* -o, --out: a string */
    argx_positional_t *src;  /* SRC: one string */
    argx_positional_t *dst;  /* DST: none or one string */
    argx_positional_t *rest; /* REST: any number of strings */
} copy_fixture_t;

static void set_up_copy(copy_fixture_t *fixture)
{
    argx_command_t *root = create_root("copy");

    fixture->root = root;
    NAOS_REQUIRE_TRUE(argx_command_add_flag(root, 'v', "verbose", &fixture->verbose) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_value(root, 'o', "out", argx_type_string(), &fixture->out) ==
                      ARGX_ERROR_NONE);
    fixture->src = add_positional(root, "SRC", argx_type_string(), ARGX_ARITY_REQUIRED);
    fixture->dst = add_positional(root, "DST", argx_type_string(), ARGX_ARITY_OPTIONAL);
    fixture->rest = add_positional(root, "REST", argx_type_string(), ARGX_ARITY_ZERO_OR_MORE);
    validate(root);
}

/*
 * Checks that result holds "a" for SRC, "b" for DST, "c" and "d" for REST,
 * --verbose and --out=x.
 */
static void check_copy_abcd(const argx_result_t *result, const copy_fixture_t *fixture)
{
    const char *text = NULL;

    check_count(result, fixture->src, 1);
    check_string(result, fixture->src, 0, "a");
    check_count(result, fixture->dst, 1);
    check_string(result, fixture->dst, 0, "b");
    check_count(result, fixture->rest, 2);
    check_string(result, fixture->rest, 0, "c");
    check_string(result, fixture->rest, 1, "d");
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture->verbose));
    NAOS_REQUIRE_TRUE(argx_result_option_string(result, fixture->out, &text));
    NAOS_CHECK_STR_EQ(text, "x");
}

/* ---- The assignment ------------------------------------------------------ */

NAOS_TEST(one_argument_goes_to_the_first_positional)
{
    copy_fixture_t fixture;
    char *argv[] = { "copy", "a" };
    argx_result_t *result;
    const char *text = "unchanged";

    set_up_copy(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);

    check_count(result, fixture.src, 1);
    check_string(result, fixture.src, 0, "a");
    check_count(result, fixture.dst, 0);
    check_count(result, fixture.rest, 0);
    NAOS_CHECK_FALSE(argx_result_positional_string(result, fixture.src, 1, &text));
    NAOS_CHECK_FALSE(argx_result_positional_string(result, fixture.dst, 0, &text));
    NAOS_CHECK_STR_EQ(text, "unchanged");
    NAOS_CHECK_UINT_EQ(argx_result_command_count(result), 1);
    NAOS_CHECK_TRUE(argx_result_command(result) == fixture.root);

    argx_result_destroy(result);
    destroy_root(fixture.root);
}

NAOS_TEST(the_second_argument_goes_to_the_optional_positional)
{
    copy_fixture_t fixture;
    char *argv[] = { "copy", "a", "b" };
    argx_result_t *result;

    set_up_copy(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);

    check_count(result, fixture.src, 1);
    check_string(result, fixture.src, 0, "a");
    check_count(result, fixture.dst, 1);
    check_string(result, fixture.dst, 0, "b");
    check_count(result, fixture.rest, 0);

    argx_result_destroy(result);
    destroy_root(fixture.root);
}

NAOS_TEST(the_arguments_after_them_go_to_the_positional_that_takes_any_number_in_their_order)
{
    copy_fixture_t fixture;
    char *argv[] = { "copy", "a", "b", "c", "d", "e", "c" };
    argx_result_t *result;

    set_up_copy(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);

    check_count(result, fixture.src, 1);
    check_string(result, fixture.src, 0, "a");
    check_count(result, fixture.dst, 1);
    check_string(result, fixture.dst, 0, "b");
    check_count(result, fixture.rest, 4);
    check_string(result, fixture.rest, 0, "c");
    check_string(result, fixture.rest, 1, "d");
    check_string(result, fixture.rest, 2, "e");
    check_string(result, fixture.rest, 3, "c");

    argx_result_destroy(result);
    destroy_root(fixture.root);
}

NAOS_TEST(a_positional_that_takes_one_or_more_takes_every_argument_and_converts_each)
{
    argx_command_t *root = create_root("sum");
    argx_positional_t *level = add_positional(root, "LEVEL", argx_type_int(), ARGX_ARITY_REQUIRED);
    argx_positional_t *nums = add_positional(root, "NUMS", argx_type_float(), ARGX_ARITY_ONE_OR_MORE);
    char *argv[] = { "sum", "7", "1.5", "2", "0.25" };
    argx_result_t *result;
    int64_t integer = 0;
    double number = 0.0;

    validate(root);
    result = parse_ok(root, ARGC(argv), argv);

    check_count(result, level, 1);
    NAOS_REQUIRE_TRUE(argx_result_positional_int(result, level, 0, &integer));
    NAOS_CHECK_TRUE(integer == 7);
    check_count(result, nums, 3);
    NAOS_REQUIRE_TRUE(argx_result_positional_float(result, nums, 0, &number));
    NAOS_CHECK_TRUE(number == 1.5);
    NAOS_REQUIRE_TRUE(argx_result_positional_float(result, nums, 1, &number));
    NAOS_CHECK_TRUE(number == 2.0);
    NAOS_REQUIRE_TRUE(argx_result_positional_float(result, nums, 2, &number));
    NAOS_CHECK_TRUE(number == 0.25);
    NAOS_CHECK_FALSE(argx_result_positional_float(result, nums, 3, &number));

    argx_result_destroy(result);
    destroy_root(root);
}

NAOS_TEST(no_argument_leaves_every_positional_without_a_value)
{
    argx_command_t *root = create_root("list");
    argx_positional_t *dir = add_positional(root, "DIR", argx_type_string(), ARGX_ARITY_OPTIONAL);
    argx_positional_t *rest = add_positional(root, "REST", argx_type_string(), ARGX_ARITY_ZERO_OR_MORE);
    char *argv[] = { "list" };
    argx_result_t *result;

    validate(root);
    result = parse_ok(root, ARGC(argv), argv);

    check_count(result, dir, 0);
    check_count(result, rest, 0);
    NAOS_CHECK_TRUE(argx_result_positional_value(result, dir, 0) == NULL);

    argx_result_destroy(result);
    destroy_root(root);
}

/* ---- Options among the positionals -------------------------------------- */

NAOS_TEST(options_before_the_positionals_do_not_change_the_assignment)
{
    copy_fixture_t fixture;
    char *argv[] = { "copy", "-v", "--out", "x", "a", "b", "c", "d" };
    argx_result_t *result;

    set_up_copy(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);
    check_copy_abcd(result, &fixture);

    argx_result_destroy(result);
    destroy_root(fixture.root);
}

NAOS_TEST(options_between_the_positionals_do_not_change_the_assignment)
{
    copy_fixture_t fixture;
    char *argv[] = { "copy", "a", "-v", "b", "c", "-o", "x", "d" };
    char *other[] = { "copy", "a", "--out=x", "b", "--verbose", "c", "d" };
    argx_result_t *result;

    set_up_copy(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);
    check_copy_abcd(result, &fixture);
    argx_result_destroy(result);

    result = parse_ok(fixture.root, ARGC(other), other);
    check_copy_abcd(result, &fixture);
    argx_result_destroy(result);

    destroy_root(fixture.root);
}

NAOS_TEST(options_after_the_positionals_do_not_change_the_assignment)
{
    copy_fixture_t fixture;
    char *argv[] = { "copy", "a", "b", "c", "d", "-vox" };
    argx_result_t *result;

    set_up_copy(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);
    check_copy_abcd(result, &fixture);

    argx_result_destroy(result);
    destroy_root(fixture.root);
}

NAOS_TEST(the_value_of_an_option_is_not_a_positional)
{
    copy_fixture_t fixture;
    char *argv[] = { "copy", "--out", "a", "-o", "b", "c" };
    argx_result_t *result;
    const char *text = NULL;

    set_up_copy(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);

    check_count(result, fixture.src, 1);
    check_string(result, fixture.src, 0, "c");
    check_count(result, fixture.dst, 0);
    check_count(result, fixture.rest, 0);
    NAOS_REQUIRE_TRUE(argx_result_option_string(result, fixture.out, &text));
    NAOS_CHECK_STR_EQ(text, "b");

    argx_result_destroy(result);
    destroy_root(fixture.root);
}

/* ---- The terminator ------------------------------------------------------ */

NAOS_TEST(after_the_terminator_every_argument_is_a_value_and_the_terminator_is_none)
{
    copy_fixture_t fixture;
    char *argv[] = { "copy", "a", "--", "-v", "--", "--out=x", "--bogus", "-o" };
    argx_result_t *result;

    set_up_copy(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);

    check_count(result, fixture.src, 1);
    check_string(result, fixture.src, 0, "a");
    check_count(result, fixture.dst, 1);
    check_string(result, fixture.dst, 0, "-v");
    check_count(result, fixture.rest, 4);
    check_string(result, fixture.rest, 0, "--");
    check_string(result, fixture.rest, 1, "--out=x");
    check_string(result, fixture.rest, 2, "--bogus");
    check_string(result, fixture.rest, 3, "-o");
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.verbose));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.out), ARGX_SOURCE_NONE);

    argx_result_destroy(result);
    destroy_root(fixture.root);
}

NAOS_TEST(the_terminator_as_the_first_argument_makes_the_options_values)
{
    copy_fixture_t fixture;
    char *argv[] = { "copy", "--", "--verbose" };
    argx_result_t *result;

    set_up_copy(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);

    check_count(result, fixture.src, 1);
    check_string(result, fixture.src, 0, "--verbose");
    check_count(result, fixture.dst, 0);
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.verbose));

    argx_result_destroy(result);
    destroy_root(fixture.root);
}

NAOS_TEST(the_terminator_as_the_last_argument_gives_no_value)
{
    copy_fixture_t fixture;
    char *argv[] = { "copy", "-v", "a", "--" };
    argx_result_t *result;

    set_up_copy(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);

    check_count(result, fixture.src, 1);
    check_string(result, fixture.src, 0, "a");
    check_count(result, fixture.dst, 0);
    check_count(result, fixture.rest, 0);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.verbose));

    argx_result_destroy(result);
    destroy_root(fixture.root);
}

NAOS_TEST(options_before_the_terminator_are_options)
{
    copy_fixture_t fixture;
    char *argv[] = { "copy", "-v", "--", "-v" };
    argx_result_t *result;

    set_up_copy(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);

    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.verbose));
    check_count(result, fixture.src, 1);
    check_string(result, fixture.src, 0, "-v");

    argx_result_destroy(result);
    destroy_root(fixture.root);
}

/* ---- Arguments that start with '-' and are not options ------------------ */

NAOS_TEST(a_lone_dash_and_the_empty_string_are_values)
{
    copy_fixture_t fixture;
    char *argv[] = { "copy", "-", "", "-", "-v" };
    argx_result_t *result;

    set_up_copy(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);

    check_count(result, fixture.src, 1);
    check_string(result, fixture.src, 0, "-");
    check_count(result, fixture.dst, 1);
    check_string(result, fixture.dst, 0, "");
    check_count(result, fixture.rest, 1);
    check_string(result, fixture.rest, 0, "-");
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.verbose));

    argx_result_destroy(result);
    destroy_root(fixture.root);
}

NAOS_TEST(a_negative_number_is_a_value_for_a_command_without_a_digit_as_a_short_name)
{
    argx_command_t *root = create_root("calc");
    argx_option_t *verbose = NULL;
    argx_positional_t *level;
    argx_positional_t *nums;
    char *argv[] = { "calc", "-1", "-.5", "-v", "-2.5", "-10" };
    argx_result_t *result;
    int64_t integer = 0;
    double number = 0.0;

    NAOS_REQUIRE_TRUE(argx_command_add_flag(root, 'v', NULL, &verbose) == ARGX_ERROR_NONE);
    level = add_positional(root, "LEVEL", argx_type_int(), ARGX_ARITY_REQUIRED);
    nums = add_positional(root, "NUMS", argx_type_float(), ARGX_ARITY_ZERO_OR_MORE);
    validate(root);
    result = parse_ok(root, ARGC(argv), argv);

    check_count(result, level, 1);
    NAOS_REQUIRE_TRUE(argx_result_positional_int(result, level, 0, &integer));
    NAOS_CHECK_TRUE(integer == -1);
    check_count(result, nums, 3);
    NAOS_REQUIRE_TRUE(argx_result_positional_float(result, nums, 0, &number));
    NAOS_CHECK_TRUE(number == -0.5);
    NAOS_REQUIRE_TRUE(argx_result_positional_float(result, nums, 1, &number));
    NAOS_CHECK_TRUE(number == -2.5);
    NAOS_REQUIRE_TRUE(argx_result_positional_float(result, nums, 2, &number));
    NAOS_CHECK_TRUE(number == -10.0);
    NAOS_CHECK_TRUE(argx_result_flag(result, verbose));

    argx_result_destroy(result);
    destroy_root(root);
}

NAOS_TEST(a_negative_number_is_an_option_for_a_command_with_a_digit_as_a_short_name)
{
    argx_command_t *root = create_root("digit");
    argx_option_t *one = NULL;
    argx_positional_t *items;
    char *argv[] = { "digit", "a", "-1", "b", "--", "-1" };
    argx_result_t *result;

    NAOS_REQUIRE_TRUE(argx_command_add_flag(root, '1', NULL, &one) == ARGX_ERROR_NONE);
    items = add_positional(root, "ITEMS", argx_type_string(), ARGX_ARITY_ZERO_OR_MORE);
    validate(root);
    result = parse_ok(root, ARGC(argv), argv);

    NAOS_CHECK_TRUE(argx_result_flag(result, one));
    check_count(result, items, 3);
    check_string(result, items, 0, "a");
    check_string(result, items, 1, "b");
    check_string(result, items, 2, "-1");

    argx_result_destroy(result);
    destroy_root(root);
}

/* ---- One argument too many ---------------------------------------------- */

NAOS_TEST(an_argument_after_the_last_positional_is_a_parse_error)
{
    argx_command_t *root = create_root("pair");
    argx_option_t *verbose = NULL;
    argx_positional_t *src;
    argx_positional_t *dst;
    char *argv[] = { "pair", "a", "b", "extra", "-v", "more" };
    argx_result_t *result;

    NAOS_REQUIRE_TRUE(argx_command_add_flag(root, 'v', NULL, &verbose) == ARGX_ERROR_NONE);
    src = add_positional(root, "SRC", argx_type_string(), ARGX_ARITY_REQUIRED);
    dst = add_positional(root, "DST", argx_type_string(), ARGX_ARITY_OPTIONAL);
    validate(root);
    result = parse_bad(root, ARGC(argv), argv, ARGX_PARSE_ERROR_UNEXPECTED_POSITIONAL, 3,
                       "unexpected argument 'extra'");

    NAOS_CHECK_TRUE(argx_result_error_positional(result) == NULL);
    /* What was read before the error is there; the scan stopped at it. */
    check_count(result, src, 1);
    check_string(result, src, 0, "a");
    check_count(result, dst, 1);
    check_string(result, dst, 0, "b");
    NAOS_CHECK_FALSE(argx_result_flag(result, verbose));

    argx_result_destroy(result);
    destroy_root(root);
}

NAOS_TEST(an_argument_for_a_command_without_positionals_is_a_parse_error)
{
    argx_command_t *root = create_root("bare");
    argx_option_t *verbose = NULL;
    char *argv[] = { "bare", "-v", "extra" };
    argx_result_t *result;

    NAOS_REQUIRE_TRUE(argx_command_add_flag(root, 'v', NULL, &verbose) == ARGX_ERROR_NONE);
    validate(root);
    result = parse_bad(root, ARGC(argv), argv, ARGX_PARSE_ERROR_UNEXPECTED_POSITIONAL, 2,
                       "unexpected argument 'extra'");

    NAOS_CHECK_TRUE(argx_result_error_positional(result) == NULL);
    NAOS_CHECK_TRUE(argx_result_flag(result, verbose));

    argx_result_destroy(result);
    destroy_root(root);
}

NAOS_TEST(an_argument_too_many_after_the_terminator_is_a_parse_error_and_the_terminator_is_not)
{
    argx_command_t *root = create_root("bare");
    char *only[] = { "bare", "--" };
    char *argv[] = { "bare", "--", "--" };
    char *dash[] = { "bare", "-" };
    char *empty[] = { "bare", "" };
    char *negative[] = { "bare", "-1" };
    argx_result_t *result;

    validate(root);
    result = parse_ok(root, ARGC(only), only);
    argx_result_destroy(result);

    result = parse_bad(root, ARGC(argv), argv, ARGX_PARSE_ERROR_UNEXPECTED_POSITIONAL, 2,
                       "unexpected argument '--'");
    argx_result_destroy(result);

    result = parse_bad(root, ARGC(dash), dash, ARGX_PARSE_ERROR_UNEXPECTED_POSITIONAL, 1,
                       "unexpected argument '-'");
    argx_result_destroy(result);

    result = parse_bad(root, ARGC(empty), empty, ARGX_PARSE_ERROR_UNEXPECTED_POSITIONAL, 1,
                       "unexpected argument ''");
    argx_result_destroy(result);

    result = parse_bad(root, ARGC(negative), negative, ARGX_PARSE_ERROR_UNEXPECTED_POSITIONAL, 1,
                       "unexpected argument '-1'");
    argx_result_destroy(result);

    destroy_root(root);
}

/* ---- A value that the value type does not take -------------------------- */

NAOS_TEST(a_value_that_the_type_of_a_positional_does_not_take_is_a_parse_error)
{
    argx_command_t *root = create_root("repeat");
    argx_option_t *verbose = NULL;
    argx_positional_t *text;
    argx_positional_t *count;
    char *argv[] = { "repeat", "word", "-v", "x", "5" };
    argx_result_t *result;

    NAOS_REQUIRE_TRUE(argx_command_add_flag(root, 'v', NULL, &verbose) == ARGX_ERROR_NONE);
    text = add_positional(root, "TEXT", argx_type_string(), ARGX_ARITY_REQUIRED);
    count = add_positional(root, "COUNT", argx_type_int(), ARGX_ARITY_ZERO_OR_MORE);
    validate(root);
    result = parse_bad(root, ARGC(argv), argv, ARGX_PARSE_ERROR_INVALID_VALUE, 3,
                       "invalid value 'x' for argument 'COUNT' (expected integer)");

    NAOS_CHECK_TRUE(argx_result_error_positional(result) == count);
    check_count(result, text, 1);
    check_string(result, text, 0, "word");
    check_count(result, count, 0);
    NAOS_CHECK_TRUE(argx_result_flag(result, verbose));

    argx_result_destroy(result);
    destroy_root(root);
}

NAOS_TEST(a_wrong_value_after_good_ones_keeps_the_good_ones)
{
    argx_command_t *root = create_root("sum");
    argx_positional_t *nums = add_positional(root, "NUMS", argx_type_int(), ARGX_ARITY_ONE_OR_MORE);
    char *argv[] = { "sum", "1", "2", "--", "-x", "3" };
    argx_result_t *result;
    int64_t integer = 0;

    validate(root);
    result = parse_bad(root, ARGC(argv), argv, ARGX_PARSE_ERROR_INVALID_VALUE, 4,
                       "invalid value '-x' for argument 'NUMS' (expected integer)");

    NAOS_CHECK_TRUE(argx_result_error_positional(result) == nums);
    check_count(result, nums, 2);
    NAOS_REQUIRE_TRUE(argx_result_positional_int(result, nums, 1, &integer));
    NAOS_CHECK_TRUE(integer == 2);

    argx_result_destroy(result);
    destroy_root(root);
}

/* ---- The schema "tool": subcommands, two levels deep --------------------- */

/*
 * tool [-v|--verbose] [--config FILE]
 *   status
 *   remote [-v|--verbose]...
 *     add [-f|--force] [-1] [-o|--out FILE] NAME [URLS]...
 *     remove [--all] NAME
 */
typedef struct tool_fixture
{
    argx_command_t *root;
    argx_option_t *verbose;        /* tool -v, --verbose: a flag */
    argx_option_t *config;         /* tool --config: a string */
    argx_command_t *status;        /* tool status: nothing */
    argx_command_t *remote;        /* tool remote */
    argx_option_t *remote_verbose; /* tool remote -v, --verbose: a count */
    argx_command_t *add;           /* tool remote add */
    argx_option_t *force;          /* tool remote add -f, --force: a flag */
    argx_option_t *one;            /* tool remote add -1: a flag */
    argx_option_t *out;            /* tool remote add -o, --out: a string */
    argx_positional_t *name;       /* tool remote add NAME: one string */
    argx_positional_t *urls;       /* tool remote add URLS: any number of strings */
    argx_command_t *remove;        /* tool remote remove */
    argx_option_t *all;            /* tool remote remove --all: a flag */
    argx_positional_t *removed;    /* tool remote remove NAME: one string */
} tool_fixture_t;

static argx_command_t *add_subcommand(argx_command_t *command, const char *name)
{
    argx_command_t *subcommand = NULL;

    NAOS_REQUIRE_TRUE(argx_command_add_subcommand(command, name, &subcommand) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(subcommand != NULL);
    return subcommand;
}

static void set_up_tool(tool_fixture_t *fixture)
{
    argx_command_t *root = create_root("tool");

    fixture->root = root;
    NAOS_REQUIRE_TRUE(argx_command_add_flag(root, 'v', "verbose", &fixture->verbose) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_value(root, '\0', "config", argx_type_string(),
                                             &fixture->config) == ARGX_ERROR_NONE);
    fixture->status = add_subcommand(root, "status");
    fixture->remote = add_subcommand(root, "remote");
    NAOS_REQUIRE_TRUE(argx_command_add_count(fixture->remote, 'v', "verbose", &fixture->remote_verbose) ==
                      ARGX_ERROR_NONE);
    fixture->add = add_subcommand(fixture->remote, "add");
    NAOS_REQUIRE_TRUE(argx_command_add_flag(fixture->add, 'f', "force", &fixture->force) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_flag(fixture->add, '1', NULL, &fixture->one) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_value(fixture->add, 'o', "out", argx_type_string(),
                                             &fixture->out) == ARGX_ERROR_NONE);
    fixture->name = add_positional(fixture->add, "NAME", argx_type_string(), ARGX_ARITY_REQUIRED);
    fixture->urls = add_positional(fixture->add, "URLS", argx_type_string(), ARGX_ARITY_ZERO_OR_MORE);
    fixture->remove = add_subcommand(fixture->remote, "remove");
    NAOS_REQUIRE_TRUE(argx_command_add_flag(fixture->remove, '\0', "all", &fixture->all) ==
                      ARGX_ERROR_NONE);
    fixture->removed = add_positional(fixture->remove, "NAME", argx_type_string(), ARGX_ARITY_REQUIRED);
    validate(root);
}

/*
 * Checks that what result holds for the options and the positionals of
 * "tool remote add" is what a command that was not selected reads as.
 */
static void check_add_not_selected(const argx_result_t *result, const tool_fixture_t *fixture)
{
    const char *text = "unchanged";

    NAOS_CHECK_FALSE(argx_result_flag(result, fixture->force));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture->force), ARGX_SOURCE_NONE);
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture->one));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture->one), ARGX_SOURCE_NONE);
    NAOS_CHECK_FALSE(argx_result_option_string(result, fixture->out, &text));
    NAOS_CHECK_STR_EQ(text, "unchanged");
    NAOS_CHECK_TRUE(argx_result_option_value(result, fixture->out) == NULL);
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture->out), ARGX_SOURCE_NONE);
    check_count(result, fixture->name, 0);
    NAOS_CHECK_FALSE(argx_result_positional_string(result, fixture->name, 0, &text));
    NAOS_CHECK_STR_EQ(text, "unchanged");
    NAOS_CHECK_TRUE(argx_result_positional_value(result, fixture->name, 0) == NULL);
    check_count(result, fixture->urls, 0);
}

/* ---- The selection of a subcommand -------------------------------------- */

NAOS_TEST(an_argument_that_is_not_an_option_selects_a_subcommand)
{
    tool_fixture_t fixture;
    char *argv[] = { "tool", "status" };
    argx_result_t *result;

    set_up_tool(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);

    NAOS_REQUIRE_TRUE(argx_result_command_count(result) == 2);
    NAOS_CHECK_TRUE(argx_result_command_at(result, 0) == fixture.root);
    NAOS_CHECK_TRUE(argx_result_command_at(result, 1) == fixture.status);
    NAOS_CHECK_TRUE(argx_result_command(result) == fixture.status);

    argx_result_destroy(result);
    destroy_root(fixture.root);
}

NAOS_TEST(a_subcommand_of_a_subcommand_is_selected_in_the_same_way)
{
    tool_fixture_t fixture;
    char *argv[] = { "tool", "remote", "remove", "origin" };
    argx_result_t *result;

    set_up_tool(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);

    NAOS_REQUIRE_TRUE(argx_result_command_count(result) == 3);
    NAOS_CHECK_TRUE(argx_result_command_at(result, 0) == fixture.root);
    NAOS_CHECK_TRUE(argx_result_command_at(result, 1) == fixture.remote);
    NAOS_CHECK_TRUE(argx_result_command_at(result, 2) == fixture.remove);
    NAOS_CHECK_TRUE(argx_result_command(result) == fixture.remove);
    check_count(result, fixture.removed, 1);
    check_string(result, fixture.removed, 0, "origin");
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.all));

    argx_result_destroy(result);
    destroy_root(fixture.root);
}

NAOS_TEST(the_options_and_positionals_of_the_last_subcommand_are_read_after_its_name)
{
    tool_fixture_t fixture;
    char *argv[] = { "tool", "remote", "add", "-f", "origin", "--out=x", "u1", "-1", "--", "add", "-f" };
    argx_result_t *result;
    const char *text = NULL;

    set_up_tool(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);

    NAOS_REQUIRE_TRUE(argx_result_command_count(result) == 3);
    NAOS_CHECK_TRUE(argx_result_command(result) == fixture.add);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.force));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.force), ARGX_SOURCE_COMMAND_LINE);
    /* "add" has a digit as a short name, the commands before it have not. */
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.one));
    NAOS_REQUIRE_TRUE(argx_result_option_string(result, fixture.out, &text));
    NAOS_CHECK_STR_EQ(text, "x");
    check_count(result, fixture.name, 1);
    check_string(result, fixture.name, 0, "origin");
    /* A name of a subcommand is a value for a command that has none. */
    check_count(result, fixture.urls, 3);
    check_string(result, fixture.urls, 0, "u1");
    check_string(result, fixture.urls, 1, "add");
    check_string(result, fixture.urls, 2, "-f");

    argx_result_destroy(result);
    destroy_root(fixture.root);
}

NAOS_TEST(the_digits_that_are_short_names_are_those_of_the_current_command)
{
    argx_command_t *root = create_root("num");
    argx_option_t *two = NULL;
    argx_command_t *neg;
    argx_positional_t *number;
    char *argv[] = { "num", "-2", "neg", "-2" };
    argx_result_t *result;
    int64_t integer = 0;

    NAOS_REQUIRE_TRUE(argx_command_add_flag(root, '2', NULL, &two) == ARGX_ERROR_NONE);
    neg = add_subcommand(root, "neg");
    number = add_positional(neg, "N", argx_type_int(), ARGX_ARITY_REQUIRED);
    validate(root);
    result = parse_ok(root, ARGC(argv), argv);

    NAOS_CHECK_TRUE(argx_result_flag(result, two));
    NAOS_CHECK_TRUE(argx_result_command(result) == neg);
    check_count(result, number, 1);
    NAOS_REQUIRE_TRUE(argx_result_positional_int(result, number, 0, &integer));
    NAOS_CHECK_TRUE(integer == -2);

    argx_result_destroy(result);
    destroy_root(root);
}

NAOS_TEST(a_command_line_that_names_no_subcommand_ends_in_the_command_it_reached)
{
    tool_fixture_t fixture;
    char *none[] = { "tool" };
    char *options[] = { "tool", "-v", "--config", "remote" };
    char *remote[] = { "tool", "remote", "-v" };
    char *terminator[] = { "tool", "remote", "--" };
    argx_result_t *result;
    const char *text = NULL;

    set_up_tool(&fixture);

    result = parse_ok(fixture.root, ARGC(none), none);
    NAOS_CHECK_UINT_EQ(argx_result_command_count(result), 1);
    NAOS_CHECK_TRUE(argx_result_command(result) == fixture.root);
    argx_result_destroy(result);

    /* The value of an option is not a name of a subcommand. */
    result = parse_ok(fixture.root, ARGC(options), options);
    NAOS_CHECK_UINT_EQ(argx_result_command_count(result), 1);
    NAOS_CHECK_TRUE(argx_result_command(result) == fixture.root);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.verbose));
    NAOS_REQUIRE_TRUE(argx_result_option_string(result, fixture.config, &text));
    NAOS_CHECK_STR_EQ(text, "remote");
    argx_result_destroy(result);

    result = parse_ok(fixture.root, ARGC(remote), remote);
    NAOS_REQUIRE_TRUE(argx_result_command_count(result) == 2);
    NAOS_CHECK_TRUE(argx_result_command_at(result, 1) == fixture.remote);
    NAOS_CHECK_TRUE(argx_result_command(result) == fixture.remote);
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.remote_verbose), 1);
    argx_result_destroy(result);

    result = parse_ok(fixture.root, ARGC(terminator), terminator);
    NAOS_CHECK_UINT_EQ(argx_result_command_count(result), 2);
    NAOS_CHECK_TRUE(argx_result_command(result) == fixture.remote);
    argx_result_destroy(result);

    destroy_root(fixture.root);
}

/* ---- The options of the current command --------------------------------- */

NAOS_TEST(an_option_of_the_parent_is_read_before_the_name_of_the_subcommand)
{
    tool_fixture_t fixture;
    char *argv[] = { "tool", "--config=c", "-v", "remote", "add", "origin" };
    argx_result_t *result;
    const char *text = NULL;

    set_up_tool(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);

    NAOS_CHECK_TRUE(argx_result_command(result) == fixture.add);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.verbose));
    NAOS_REQUIRE_TRUE(argx_result_option_string(result, fixture.config, &text));
    NAOS_CHECK_STR_EQ(text, "c");
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.remote_verbose), 0);
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.remote_verbose), ARGX_SOURCE_DEFAULT);
    check_string(result, fixture.name, 0, "origin");

    argx_result_destroy(result);
    destroy_root(fixture.root);
}

NAOS_TEST(an_option_of_the_parent_after_the_name_of_the_subcommand_is_unknown)
{
    tool_fixture_t fixture;
    char *argv[] = { "tool", "-v", "remote", "--config", "c" };
    char *deep[] = { "tool", "remote", "add", "origin", "--config=c" };
    char *bundle[] = { "tool", "status", "-v" };
    argx_result_t *result;

    set_up_tool(&fixture);

    result = parse_unknown_option(fixture.root, ARGC(argv), argv, 3, fixture.remote,
                                  "unknown option '--config'");
    NAOS_CHECK_INT_EQ(argx_result_error_short_name(result), '\0');
    /* What was read before the error is there. */
    NAOS_CHECK_UINT_EQ(argx_result_command_count(result), 2);
    NAOS_CHECK_TRUE(argx_result_command(result) == fixture.remote);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.verbose));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.config), ARGX_SOURCE_NONE);
    argx_result_destroy(result);

    result = parse_unknown_option(fixture.root, ARGC(deep), deep, 4, fixture.add,
                                  "unknown option '--config'");
    NAOS_CHECK_UINT_EQ(argx_result_command_count(result), 3);
    check_string(result, fixture.name, 0, "origin");
    argx_result_destroy(result);

    result = parse_unknown_option(fixture.root, ARGC(bundle), bundle, 2, fixture.status,
                                  "unknown option '-v'");
    NAOS_CHECK_INT_EQ(argx_result_error_short_name(result), 'v');
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.verbose));
    argx_result_destroy(result);

    destroy_root(fixture.root);
}

NAOS_TEST(an_option_of_a_subcommand_before_its_name_is_unknown)
{
    tool_fixture_t fixture;
    char *argv[] = { "tool", "--force", "remote", "add", "origin" };
    char *middle[] = { "tool", "remote", "-f", "add", "origin" };
    argx_result_t *result;

    set_up_tool(&fixture);

    result = parse_unknown_option(fixture.root, ARGC(argv), argv, 1, fixture.root,
                                  "unknown option '--force'");
    NAOS_CHECK_UINT_EQ(argx_result_command_count(result), 1);
    argx_result_destroy(result);

    result = parse_unknown_option(fixture.root, ARGC(middle), middle, 2, fixture.remote,
                                  "unknown option '-f'");
    NAOS_CHECK_UINT_EQ(argx_result_command_count(result), 2);
    check_add_not_selected(result, &fixture);
    argx_result_destroy(result);

    destroy_root(fixture.root);
}

NAOS_TEST(an_option_name_of_the_parent_and_of_the_subcommand_is_the_option_of_the_current_command)
{
    tool_fixture_t fixture;
    char *both[] = { "tool", "-v", "remote", "-vv", "--verbose", "add", "origin" };
    char *child[] = { "tool", "remote", "--verbose" };
    char *parent[] = { "tool", "--verbose", "remote" };
    argx_result_t *result;

    set_up_tool(&fixture);

    result = parse_ok(fixture.root, ARGC(both), both);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.verbose));
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.remote_verbose), 3);
    NAOS_CHECK_TRUE(argx_result_command(result) == fixture.add);
    argx_result_destroy(result);

    result = parse_ok(fixture.root, ARGC(child), child);
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.verbose));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.verbose), ARGX_SOURCE_DEFAULT);
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.remote_verbose), 1);
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.remote_verbose),
                      ARGX_SOURCE_COMMAND_LINE);
    argx_result_destroy(result);

    result = parse_ok(fixture.root, ARGC(parent), parent);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.verbose));
    NAOS_CHECK_UINT_EQ(argx_result_command_count(result), 2);
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.remote_verbose), 0);
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.remote_verbose), ARGX_SOURCE_DEFAULT);
    argx_result_destroy(result);

    destroy_root(fixture.root);
}

/* ---- A command that was not selected ------------------------------------ */

NAOS_TEST(the_options_and_positionals_of_a_subcommand_that_was_not_selected_read_as_not_given)
{
    tool_fixture_t fixture;
    char *sibling[] = { "tool", "remote", "remove", "--all", "origin" };
    char *parent[] = { "tool", "-v" };
    argx_result_t *result;

    set_up_tool(&fixture);

    /* A sibling of the last command: "add" was not selected, "remove" was. */
    result = parse_ok(fixture.root, ARGC(sibling), sibling);
    NAOS_CHECK_TRUE(argx_result_command(result) == fixture.remove);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.all));
    check_string(result, fixture.removed, 0, "origin");
    check_add_not_selected(result, &fixture);
    argx_result_destroy(result);

    /* The commands below the last command. */
    result = parse_ok(fixture.root, ARGC(parent), parent);
    NAOS_CHECK_UINT_EQ(argx_result_command_count(result), 1);
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.remote_verbose), 0);
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.remote_verbose), ARGX_SOURCE_NONE);
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.all));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.all), ARGX_SOURCE_NONE);
    check_count(result, fixture.removed, 0);
    check_add_not_selected(result, &fixture);
    argx_result_destroy(result);

    destroy_root(fixture.root);
}

/* ---- The parse errors of a command that has subcommands ----------------- */

NAOS_TEST(a_name_that_is_not_a_subcommand_of_the_current_command_is_a_parse_error)
{
    tool_fixture_t fixture;
    char *argv[] = { "tool", "-v", "comit", "remote" };
    char *nested[] = { "tool", "remote", "-v", "status" };
    char *deeper[] = { "tool", "remote", "add", "origin" };
    argx_result_t *result;

    set_up_tool(&fixture);

    result = parse_bad(fixture.root, ARGC(argv), argv, ARGX_PARSE_ERROR_UNKNOWN_SUBCOMMAND, 2,
                       "unknown command 'comit'");
    NAOS_CHECK_TRUE(argx_result_error_positional(result) == NULL);
    /* What was read before the error is there; the scan stopped at it. */
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.verbose));
    NAOS_CHECK_UINT_EQ(argx_result_command_count(result), 1);
    NAOS_CHECK_TRUE(argx_result_command(result) == fixture.root);
    argx_result_destroy(result);

    /* A subcommand of the parent is not one of the current command. */
    result = parse_bad_in(fixture.root, ARGC(nested), nested, ARGX_PARSE_ERROR_UNKNOWN_SUBCOMMAND, 3,
                          fixture.remote, "unknown command 'status'");
    NAOS_CHECK_TRUE(argx_result_error_positional(result) == NULL);
    NAOS_CHECK_UINT_EQ(argx_result_command_count(result), 2);
    NAOS_CHECK_TRUE(argx_result_command(result) == fixture.remote);
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.remote_verbose), 1);
    argx_result_destroy(result);

    /*
     * The same name is a subcommand one level down. Its positional is
     * required, so it is given.
     */
    result = parse_ok(fixture.root, ARGC(deeper), deeper);
    NAOS_CHECK_TRUE(argx_result_command(result) == fixture.add);
    argx_result_destroy(result);

    destroy_root(fixture.root);
}

NAOS_TEST(the_name_of_a_subcommand_is_compared_as_a_whole_and_with_its_case)
{
    tool_fixture_t fixture;
    char *upper[] = { "tool", "Remote" };
    char *prefix[] = { "tool", "rem" };
    char *longer[] = { "tool", "remotes" };
    char *empty[] = { "tool", "" };
    char *dash[] = { "tool", "remote", "-" };
    char *negative[] = { "tool", "-1" };
    argx_result_t *result;

    set_up_tool(&fixture);

    result = parse_bad(fixture.root, ARGC(upper), upper, ARGX_PARSE_ERROR_UNKNOWN_SUBCOMMAND, 1,
                       "unknown command 'Remote'");
    argx_result_destroy(result);
    result = parse_bad(fixture.root, ARGC(prefix), prefix, ARGX_PARSE_ERROR_UNKNOWN_SUBCOMMAND, 1,
                       "unknown command 'rem'");
    argx_result_destroy(result);
    result = parse_bad(fixture.root, ARGC(longer), longer, ARGX_PARSE_ERROR_UNKNOWN_SUBCOMMAND, 1,
                       "unknown command 'remotes'");
    argx_result_destroy(result);
    result = parse_bad(fixture.root, ARGC(empty), empty, ARGX_PARSE_ERROR_UNKNOWN_SUBCOMMAND, 1,
                       "unknown command ''");
    argx_result_destroy(result);
    result = parse_bad_in(fixture.root, ARGC(dash), dash, ARGX_PARSE_ERROR_UNKNOWN_SUBCOMMAND, 2,
                          fixture.remote, "unknown command '-'");
    argx_result_destroy(result);
    /*
     * The form of a negative number is not an option of a command that has
     * no digit as a short name: it is a name, of no subcommand.
     */
    result = parse_bad(fixture.root, ARGC(negative), negative, ARGX_PARSE_ERROR_UNKNOWN_SUBCOMMAND, 1,
                       "unknown command '-1'");
    argx_result_destroy(result);

    destroy_root(fixture.root);
}

NAOS_TEST(an_argument_after_the_terminator_of_a_command_that_has_subcommands_is_one_too_many)
{
    tool_fixture_t fixture;
    char *argv[] = { "tool", "-v", "--", "remote", "add" };
    char *nested[] = { "tool", "remote", "--", "add", "origin" };
    char *unknown[] = { "tool", "--", "comit" };
    argx_result_t *result;

    set_up_tool(&fixture);

    /* The name of a subcommand is not one after the terminator. */
    result = parse_bad(fixture.root, ARGC(argv), argv, ARGX_PARSE_ERROR_UNEXPECTED_POSITIONAL, 3,
                       "unexpected argument 'remote'");
    NAOS_CHECK_TRUE(argx_result_error_positional(result) == NULL);
    NAOS_CHECK_UINT_EQ(argx_result_command_count(result), 1);
    NAOS_CHECK_TRUE(argx_result_command(result) == fixture.root);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.verbose));
    argx_result_destroy(result);

    result = parse_bad_in(fixture.root, ARGC(nested), nested, ARGX_PARSE_ERROR_UNEXPECTED_POSITIONAL, 3,
                          fixture.remote, "unexpected argument 'add'");
    NAOS_CHECK_TRUE(argx_result_error_positional(result) == NULL);
    NAOS_CHECK_UINT_EQ(argx_result_command_count(result), 2);
    NAOS_CHECK_TRUE(argx_result_command(result) == fixture.remote);
    check_add_not_selected(result, &fixture);
    argx_result_destroy(result);

    result = parse_bad(fixture.root, ARGC(unknown), unknown, ARGX_PARSE_ERROR_UNEXPECTED_POSITIONAL, 2,
                       "unexpected argument 'comit'");
    argx_result_destroy(result);

    destroy_root(fixture.root);
}

/* ---- The entry ----------------------------------------------------------- */

NAOS_TEST(the_values_are_copies_and_a_second_parse_gives_the_same_result)
{
    copy_fixture_t fixture;
    char first[] = "a";
    char second[] = "b";
    char *argv[3];
    argx_result_t *result;
    argx_result_t *again;

    argv[0] = "copy";
    argv[1] = first;
    argv[2] = second;
    set_up_copy(&fixture);
    result = parse_ok(fixture.root, 3, argv);
    again = parse_ok(fixture.root, 3, argv);
    first[0] = 'z';
    second[0] = 'z';

    check_string(result, fixture.src, 0, "a");
    check_string(result, fixture.dst, 0, "b");
    check_string(again, fixture.src, 0, "a");
    check_string(again, fixture.dst, 0, "b");

    argx_result_destroy(again);
    argx_result_destroy(result);
    destroy_root(fixture.root);
}

/* ---- A lack of memory --------------------------------------------------- */

/*
 * Parses argv once for each allocation of the parse, with that allocation
 * failing, and then once with none failing, which must return expected.
 */
static void check_every_allocation_failing(const argx_command_t *root, int argc, char *const *argv,
                                           argx_error_t expected)
{
    size_t nth;

    for (nth = 1; nth < 100; nth++)
    {
        const size_t live = argx_test_allocator_live();
        argx_result_t *result = NULL;
        argx_error_t error;

        argx_test_allocator_fail_at(argx_test_allocator_requests() + nth);
        error = argx_parse(root, argc, argv, &result);
        argx_test_allocator_fail_at(0);

        if (error != ARGX_ERROR_OUT_OF_MEMORY)
        {
            /* The parse made fewer than nth allocations. */
            NAOS_CHECK_INT_EQ(error, expected);
            NAOS_REQUIRE_TRUE(result != NULL);
            NAOS_CHECK_INT_EQ(argx_result_error(result), expected);
            argx_result_destroy(result);
            NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), live);
            /*
             * The copy of the arguments, the frame, the slots, and one
             * allocation at least for what the positionals are given.
             */
            NAOS_CHECK_TRUE(nth > 4);
            return;
        }
        NAOS_CHECK_TRUE(result == NULL);
        NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), live);
    }
    NAOS_FAIL("the parse never succeeded");
}

NAOS_TEST(a_parse_of_positionals_that_cannot_allocate_returns_no_result_and_leaves_nothing_allocated)
{
    copy_fixture_t fixture;
    char *argv[] = { "copy", "a", "-v", "b", "c", "--", "-d", "--" };
    char *extra[] = { "copy", "a", "b" };
    char *invalid[] = { "copy", "1", "x" };
    argx_command_t *pair;
    argx_command_t *nums;
    argx_result_t *result;

    set_up_copy(&fixture);
    check_every_allocation_failing(fixture.root, ARGC(argv), argv, ARGX_ERROR_NONE);

    /* After the failures, the same schema parses the same arguments. */
    result = parse_ok(fixture.root, ARGC(argv), argv);
    check_count(result, fixture.rest, 3);
    check_string(result, fixture.rest, 2, "--");
    argx_result_destroy(result);
    argx_command_destroy(fixture.root);

    NAOS_REQUIRE_TRUE(argx_command_create("pair", &pair) == ARGX_ERROR_NONE);
    (void)add_positional(pair, "ONLY", argx_type_string(), ARGX_ARITY_REQUIRED);
    validate(pair);
    check_every_allocation_failing(pair, ARGC(extra), extra, ARGX_ERROR_INVALID_COMMAND_LINE);
    argx_command_destroy(pair);

    NAOS_REQUIRE_TRUE(argx_command_create("nums", &nums) == ARGX_ERROR_NONE);
    (void)add_positional(nums, "NUMS", argx_type_int(), ARGX_ARITY_ONE_OR_MORE);
    validate(nums);
    check_every_allocation_failing(nums, ARGC(invalid), invalid, ARGX_ERROR_INVALID_COMMAND_LINE);
    argx_command_destroy(nums);

    argx_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(a_parse_of_subcommands_that_cannot_allocate_returns_no_result_and_leaves_nothing_allocated)
{
    tool_fixture_t fixture;
    char *argv[] = { "tool", "-v", "remote", "-v", "add", "-f", "origin", "--out", "x", "u1", "u2" };
    char *unknown[] = { "tool", "remote", "ad" };
    char *extra[] = { "tool", "remote", "--", "add" };
    argx_result_t *result;

    set_up_tool(&fixture);
    check_every_allocation_failing(fixture.root, ARGC(argv), argv, ARGX_ERROR_NONE);
    check_every_allocation_failing(fixture.root, ARGC(unknown), unknown, ARGX_ERROR_INVALID_COMMAND_LINE);
    check_every_allocation_failing(fixture.root, ARGC(extra), extra, ARGX_ERROR_INVALID_COMMAND_LINE);

    /* After the failures, the same schema parses the same arguments. */
    result = parse_ok(fixture.root, ARGC(argv), argv);
    NAOS_REQUIRE_TRUE(argx_result_command_count(result) == 3);
    NAOS_CHECK_TRUE(argx_result_command(result) == fixture.add);
    NAOS_CHECK_UINT_EQ(argx_result_count(result, fixture.remote_verbose), 1);
    check_count(result, fixture.urls, 2);
    check_string(result, fixture.urls, 1, "u2");
    argx_result_destroy(result);

    destroy_root(fixture.root);
}

/* ---- The suites ---------------------------------------------------------- */

NAOS_SUITE(assignment,
    NAOS_ADD_TEST(one_argument_goes_to_the_first_positional),
    NAOS_ADD_TEST(the_second_argument_goes_to_the_optional_positional),
    NAOS_ADD_TEST(the_arguments_after_them_go_to_the_positional_that_takes_any_number_in_their_order),
    NAOS_ADD_TEST(a_positional_that_takes_one_or_more_takes_every_argument_and_converts_each),
    NAOS_ADD_TEST(no_argument_leaves_every_positional_without_a_value),
    NAOS_ADD_TEST(the_values_are_copies_and_a_second_parse_gives_the_same_result)
)

NAOS_SUITE(options_among_positionals,
    NAOS_ADD_TEST(options_before_the_positionals_do_not_change_the_assignment),
    NAOS_ADD_TEST(options_between_the_positionals_do_not_change_the_assignment),
    NAOS_ADD_TEST(options_after_the_positionals_do_not_change_the_assignment),
    NAOS_ADD_TEST(the_value_of_an_option_is_not_a_positional)
)

NAOS_SUITE(terminator,
    NAOS_ADD_TEST(after_the_terminator_every_argument_is_a_value_and_the_terminator_is_none),
    NAOS_ADD_TEST(the_terminator_as_the_first_argument_makes_the_options_values),
    NAOS_ADD_TEST(the_terminator_as_the_last_argument_gives_no_value),
    NAOS_ADD_TEST(options_before_the_terminator_are_options)
)

NAOS_SUITE(not_options,
    NAOS_ADD_TEST(a_lone_dash_and_the_empty_string_are_values),
    NAOS_ADD_TEST(a_negative_number_is_a_value_for_a_command_without_a_digit_as_a_short_name),
    NAOS_ADD_TEST(a_negative_number_is_an_option_for_a_command_with_a_digit_as_a_short_name)
)

NAOS_SUITE(errors,
    NAOS_ADD_TEST(an_argument_after_the_last_positional_is_a_parse_error),
    NAOS_ADD_TEST(an_argument_for_a_command_without_positionals_is_a_parse_error),
    NAOS_ADD_TEST(an_argument_too_many_after_the_terminator_is_a_parse_error_and_the_terminator_is_not),
    NAOS_ADD_TEST(a_value_that_the_type_of_a_positional_does_not_take_is_a_parse_error),
    NAOS_ADD_TEST(a_wrong_value_after_good_ones_keeps_the_good_ones)
)

NAOS_SUITE(subcommands,
    NAOS_ADD_TEST(an_argument_that_is_not_an_option_selects_a_subcommand),
    NAOS_ADD_TEST(a_subcommand_of_a_subcommand_is_selected_in_the_same_way),
    NAOS_ADD_TEST(the_options_and_positionals_of_the_last_subcommand_are_read_after_its_name),
    NAOS_ADD_TEST(the_digits_that_are_short_names_are_those_of_the_current_command),
    NAOS_ADD_TEST(a_command_line_that_names_no_subcommand_ends_in_the_command_it_reached),
    NAOS_ADD_TEST(an_option_of_the_parent_is_read_before_the_name_of_the_subcommand),
    NAOS_ADD_TEST(an_option_of_the_parent_after_the_name_of_the_subcommand_is_unknown),
    NAOS_ADD_TEST(an_option_of_a_subcommand_before_its_name_is_unknown),
    NAOS_ADD_TEST(an_option_name_of_the_parent_and_of_the_subcommand_is_the_option_of_the_current_command),
    NAOS_ADD_TEST(the_options_and_positionals_of_a_subcommand_that_was_not_selected_read_as_not_given),
    NAOS_ADD_TEST(a_name_that_is_not_a_subcommand_of_the_current_command_is_a_parse_error),
    NAOS_ADD_TEST(the_name_of_a_subcommand_is_compared_as_a_whole_and_with_its_case),
    NAOS_ADD_TEST(an_argument_after_the_terminator_of_a_command_that_has_subcommands_is_one_too_many)
)

NAOS_SUITE(memory,
    NAOS_ADD_TEST(a_parse_of_positionals_that_cannot_allocate_returns_no_result_and_leaves_nothing_allocated),
    NAOS_ADD_TEST(a_parse_of_subcommands_that_cannot_allocate_returns_no_result_and_leaves_nothing_allocated)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(assignment),
    NAOS_ADD_SUITE(options_among_positionals),
    NAOS_ADD_SUITE(terminator),
    NAOS_ADD_SUITE(not_options),
    NAOS_ADD_SUITE(errors),
    NAOS_ADD_SUITE(subcommands),
    NAOS_ADD_SUITE(memory)
)
