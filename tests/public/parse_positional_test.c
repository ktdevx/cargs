/*
 * Tests of the public parsing API for positionals: clargs_parse() of
 * <clargs/parse.h>, read back with the clargs_result_*() functions.
 *
 * A schema is built the way a consumer does it, validated, and parsed with
 * arguments written as an argv array. The tests cover how the arguments
 * that are not options are assigned to the positionals of a command (one
 * that takes one argument, one that may take one, one that takes any
 * number), that the place of the options among them does not matter, the
 * terminator of the options "--", the arguments that are not options
 * although they start with a '-' (a lone "-", the form of a negative
 * number), and the parse errors of a positional.
 *
 * No test relies on a default value or on a required positional that is
 * missing: a positional that is required is always given.
 *
 * Every test uses the test allocator ("../support/test_allocator.h"). Its
 * state and the allocator that is set are shared by all the tests of this
 * executable. A test starts with an install, destroys what it created, sets
 * the default allocator back before it ends, and ends with check_clean().
 */
#include <clargs/clargs.h>

#include "../support/test_allocator.h"

#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* The number of arguments of an argv array. */
#define ARGC(argv) ((int)(sizeof(argv) / sizeof((argv)[0])))

/* The end of every test: the test allocator has no live block and saw no
 * violation of the allocator contract. */
static void check_clean(void)
{
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
}

/* Installs the test allocator and creates the root of a schema. */
static clargs_command_t *create_root(const char *name)
{
    clargs_command_t *root = NULL;

    clargs_test_allocator_install();
    NAOS_REQUIRE_TRUE(clargs_command_create(name, &root) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(root != NULL);
    return root;
}

/* Adds a positional to command. */
static clargs_positional_t *add_positional(clargs_command_t *command, const char *name,
                                           const clargs_type_t *type, clargs_arity_t arity)
{
    clargs_positional_t *positional = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_positional(command, name, type, arity, &positional) ==
                      CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(positional != NULL);
    return positional;
}

static void validate(clargs_command_t *root)
{
    NAOS_REQUIRE_TRUE(clargs_command_validate(root, NULL, NULL, NULL) == CLARGS_ERROR_NONE);
}

/* Destroys the schema and ends the test: nothing stays allocated. */
static void destroy_root(clargs_command_t *root)
{
    clargs_command_destroy(root);
    clargs_test_allocator_uninstall();
    check_clean();
}

/* Parses argv, which must match the schema. Returns the result, which the
 * caller destroys. */
static clargs_result_t *parse_ok(const clargs_command_t *root, int argc, char *const *argv)
{
    clargs_result_t *result = NULL;

    NAOS_REQUIRE_TRUE(clargs_parse(root, argc, argv, &result) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(result != NULL);
    NAOS_CHECK_INT_EQ(clargs_result_error(result), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_result_error_kind(result), CLARGS_PARSE_ERROR_NONE);
    NAOS_CHECK_TRUE(clargs_result_error_message(result) == NULL);
    return result;
}

/* Parses argv, which must have a parse error of the kind, at the argument
 * with the index, whose value is that argument, with the message. Returns
 * the result. */
static clargs_result_t *parse_bad(const clargs_command_t *root, int argc, char *const *argv,
                                  clargs_parse_error_kind_t kind, size_t index, const char *message)
{
    clargs_result_t *result = NULL;

    NAOS_REQUIRE_TRUE(clargs_parse(root, argc, argv, &result) == CLARGS_ERROR_INVALID_COMMAND_LINE);
    NAOS_REQUIRE_TRUE(result != NULL);
    NAOS_CHECK_INT_EQ(clargs_result_error(result), CLARGS_ERROR_INVALID_COMMAND_LINE);
    NAOS_CHECK_INT_EQ(clargs_result_error_kind(result), kind);
    NAOS_CHECK_INT_EQ(clargs_result_error_source(result), CLARGS_SOURCE_COMMAND_LINE);
    NAOS_CHECK_UINT_EQ(clargs_result_error_index(result), index);
    NAOS_REQUIRE_TRUE(clargs_result_error_argument(result) != NULL);
    NAOS_CHECK_STR_EQ(clargs_result_error_argument(result), argv[index]);
    NAOS_REQUIRE_TRUE(clargs_result_error_value(result) != NULL);
    NAOS_CHECK_STR_EQ(clargs_result_error_value(result), argv[index]);
    NAOS_CHECK_TRUE(clargs_result_error_command(result) == root);
    NAOS_CHECK_TRUE(clargs_result_error_option(result) == NULL);
    NAOS_CHECK_INT_EQ(clargs_result_error_short_name(result), '\0');
    NAOS_REQUIRE_TRUE(clargs_result_error_message(result) != NULL);
    NAOS_CHECK_STR_EQ(clargs_result_error_message(result), message);
    return result;
}

/* Checks that positional has count values, given on the command line when
 * count is not 0 and none otherwise. */
static void check_count(const clargs_result_t *result, const clargs_positional_t *positional, size_t count)
{
    NAOS_CHECK_UINT_EQ(clargs_result_positional_count(result, positional), count);
    NAOS_CHECK_INT_EQ(clargs_result_positional_source(result, positional),
                      count == 0 ? CLARGS_SOURCE_NONE : CLARGS_SOURCE_COMMAND_LINE);
}

/* Checks that the value at index of positional, a string, is expected. */
static void check_string(const clargs_result_t *result, const clargs_positional_t *positional, size_t index,
                         const char *expected)
{
    const char *text = NULL;

    NAOS_REQUIRE_TRUE(clargs_result_positional_string(result, positional, index, &text));
    NAOS_REQUIRE_TRUE(text != NULL);
    NAOS_CHECK_STR_EQ(text, expected);
}

/* ---- The schema "copy": options and positionals of each arity ----------- */

typedef struct copy_fixture {
    clargs_command_t *root;
    clargs_option_t *verbose;  /* -v, --verbose: a flag */
    clargs_option_t *out;      /* -o, --out: a string */
    clargs_positional_t *src;  /* SRC: one string */
    clargs_positional_t *dst;  /* DST: none or one string */
    clargs_positional_t *rest; /* REST: any number of strings */
} copy_fixture_t;

static void set_up_copy(copy_fixture_t *fixture)
{
    clargs_command_t *root = create_root("copy");

    fixture->root = root;
    NAOS_REQUIRE_TRUE(clargs_command_add_flag(root, 'v', "verbose", &fixture->verbose) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_command_add_value(root, 'o', "out", clargs_type_string(), &fixture->out) ==
                      CLARGS_ERROR_NONE);
    fixture->src = add_positional(root, "SRC", clargs_type_string(), CLARGS_ARITY_REQUIRED);
    fixture->dst = add_positional(root, "DST", clargs_type_string(), CLARGS_ARITY_OPTIONAL);
    fixture->rest = add_positional(root, "REST", clargs_type_string(), CLARGS_ARITY_ZERO_OR_MORE);
    validate(root);
}

/* Checks that result holds "a" for SRC, "b" for DST, "c" and "d" for REST,
 * --verbose and --out=x. */
static void check_copy_abcd(const clargs_result_t *result, const copy_fixture_t *fixture)
{
    const char *text = NULL;

    check_count(result, fixture->src, 1);
    check_string(result, fixture->src, 0, "a");
    check_count(result, fixture->dst, 1);
    check_string(result, fixture->dst, 0, "b");
    check_count(result, fixture->rest, 2);
    check_string(result, fixture->rest, 0, "c");
    check_string(result, fixture->rest, 1, "d");
    NAOS_CHECK_TRUE(clargs_result_flag(result, fixture->verbose));
    NAOS_REQUIRE_TRUE(clargs_result_option_string(result, fixture->out, &text));
    NAOS_CHECK_STR_EQ(text, "x");
}

/* ---- The assignment ------------------------------------------------------ */

NAOS_TEST(one_argument_goes_to_the_first_positional)
{
    copy_fixture_t fixture;
    char *argv[] = { "copy", "a" };
    clargs_result_t *result;
    const char *text = "unchanged";

    set_up_copy(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);

    check_count(result, fixture.src, 1);
    check_string(result, fixture.src, 0, "a");
    check_count(result, fixture.dst, 0);
    check_count(result, fixture.rest, 0);
    NAOS_CHECK_FALSE(clargs_result_positional_string(result, fixture.src, 1, &text));
    NAOS_CHECK_FALSE(clargs_result_positional_string(result, fixture.dst, 0, &text));
    NAOS_CHECK_STR_EQ(text, "unchanged");
    NAOS_CHECK_UINT_EQ(clargs_result_command_count(result), 1);
    NAOS_CHECK_TRUE(clargs_result_command(result) == fixture.root);

    clargs_result_destroy(result);
    destroy_root(fixture.root);
}

NAOS_TEST(the_second_argument_goes_to_the_optional_positional)
{
    copy_fixture_t fixture;
    char *argv[] = { "copy", "a", "b" };
    clargs_result_t *result;

    set_up_copy(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);

    check_count(result, fixture.src, 1);
    check_string(result, fixture.src, 0, "a");
    check_count(result, fixture.dst, 1);
    check_string(result, fixture.dst, 0, "b");
    check_count(result, fixture.rest, 0);

    clargs_result_destroy(result);
    destroy_root(fixture.root);
}

NAOS_TEST(the_arguments_after_them_go_to_the_positional_that_takes_any_number_in_their_order)
{
    copy_fixture_t fixture;
    char *argv[] = { "copy", "a", "b", "c", "d", "e", "c" };
    clargs_result_t *result;

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

    clargs_result_destroy(result);
    destroy_root(fixture.root);
}

NAOS_TEST(a_positional_that_takes_one_or_more_takes_every_argument_and_converts_each)
{
    clargs_command_t *root = create_root("sum");
    clargs_positional_t *level = add_positional(root, "LEVEL", clargs_type_int(), CLARGS_ARITY_REQUIRED);
    clargs_positional_t *nums = add_positional(root, "NUMS", clargs_type_float(), CLARGS_ARITY_ONE_OR_MORE);
    char *argv[] = { "sum", "7", "1.5", "2", "0.25" };
    clargs_result_t *result;
    int64_t integer = 0;
    double number = 0.0;

    validate(root);
    result = parse_ok(root, ARGC(argv), argv);

    check_count(result, level, 1);
    NAOS_REQUIRE_TRUE(clargs_result_positional_int(result, level, 0, &integer));
    NAOS_CHECK_TRUE(integer == 7);
    check_count(result, nums, 3);
    NAOS_REQUIRE_TRUE(clargs_result_positional_float(result, nums, 0, &number));
    NAOS_CHECK_TRUE(number == 1.5);
    NAOS_REQUIRE_TRUE(clargs_result_positional_float(result, nums, 1, &number));
    NAOS_CHECK_TRUE(number == 2.0);
    NAOS_REQUIRE_TRUE(clargs_result_positional_float(result, nums, 2, &number));
    NAOS_CHECK_TRUE(number == 0.25);
    NAOS_CHECK_FALSE(clargs_result_positional_float(result, nums, 3, &number));

    clargs_result_destroy(result);
    destroy_root(root);
}

NAOS_TEST(no_argument_leaves_every_positional_without_a_value)
{
    clargs_command_t *root = create_root("list");
    clargs_positional_t *dir = add_positional(root, "DIR", clargs_type_string(), CLARGS_ARITY_OPTIONAL);
    clargs_positional_t *rest = add_positional(root, "REST", clargs_type_string(), CLARGS_ARITY_ZERO_OR_MORE);
    char *argv[] = { "list" };
    clargs_result_t *result;

    validate(root);
    result = parse_ok(root, ARGC(argv), argv);

    check_count(result, dir, 0);
    check_count(result, rest, 0);
    NAOS_CHECK_TRUE(clargs_result_positional_value(result, dir, 0) == NULL);

    clargs_result_destroy(result);
    destroy_root(root);
}

/* ---- Options among the positionals -------------------------------------- */

NAOS_TEST(options_before_the_positionals_do_not_change_the_assignment)
{
    copy_fixture_t fixture;
    char *argv[] = { "copy", "-v", "--out", "x", "a", "b", "c", "d" };
    clargs_result_t *result;

    set_up_copy(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);
    check_copy_abcd(result, &fixture);

    clargs_result_destroy(result);
    destroy_root(fixture.root);
}

NAOS_TEST(options_between_the_positionals_do_not_change_the_assignment)
{
    copy_fixture_t fixture;
    char *argv[] = { "copy", "a", "-v", "b", "c", "-o", "x", "d" };
    char *other[] = { "copy", "a", "--out=x", "b", "--verbose", "c", "d" };
    clargs_result_t *result;

    set_up_copy(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);
    check_copy_abcd(result, &fixture);
    clargs_result_destroy(result);

    result = parse_ok(fixture.root, ARGC(other), other);
    check_copy_abcd(result, &fixture);
    clargs_result_destroy(result);

    destroy_root(fixture.root);
}

NAOS_TEST(options_after_the_positionals_do_not_change_the_assignment)
{
    copy_fixture_t fixture;
    char *argv[] = { "copy", "a", "b", "c", "d", "-vox" };
    clargs_result_t *result;

    set_up_copy(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);
    check_copy_abcd(result, &fixture);

    clargs_result_destroy(result);
    destroy_root(fixture.root);
}

NAOS_TEST(the_value_of_an_option_is_not_a_positional)
{
    copy_fixture_t fixture;
    char *argv[] = { "copy", "--out", "a", "-o", "b", "c" };
    clargs_result_t *result;
    const char *text = NULL;

    set_up_copy(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);

    check_count(result, fixture.src, 1);
    check_string(result, fixture.src, 0, "c");
    check_count(result, fixture.dst, 0);
    check_count(result, fixture.rest, 0);
    NAOS_REQUIRE_TRUE(clargs_result_option_string(result, fixture.out, &text));
    NAOS_CHECK_STR_EQ(text, "b");

    clargs_result_destroy(result);
    destroy_root(fixture.root);
}

/* ---- The terminator ------------------------------------------------------ */

NAOS_TEST(after_the_terminator_every_argument_is_a_value_and_the_terminator_is_none)
{
    copy_fixture_t fixture;
    char *argv[] = { "copy", "a", "--", "-v", "--", "--out=x", "--bogus", "-o" };
    clargs_result_t *result;

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
    NAOS_CHECK_FALSE(clargs_result_flag(result, fixture.verbose));
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, fixture.out), CLARGS_SOURCE_NONE);

    clargs_result_destroy(result);
    destroy_root(fixture.root);
}

NAOS_TEST(the_terminator_as_the_first_argument_makes_the_options_values)
{
    copy_fixture_t fixture;
    char *argv[] = { "copy", "--", "--verbose" };
    clargs_result_t *result;

    set_up_copy(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);

    check_count(result, fixture.src, 1);
    check_string(result, fixture.src, 0, "--verbose");
    check_count(result, fixture.dst, 0);
    NAOS_CHECK_FALSE(clargs_result_flag(result, fixture.verbose));

    clargs_result_destroy(result);
    destroy_root(fixture.root);
}

NAOS_TEST(the_terminator_as_the_last_argument_gives_no_value)
{
    copy_fixture_t fixture;
    char *argv[] = { "copy", "-v", "a", "--" };
    clargs_result_t *result;

    set_up_copy(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);

    check_count(result, fixture.src, 1);
    check_string(result, fixture.src, 0, "a");
    check_count(result, fixture.dst, 0);
    check_count(result, fixture.rest, 0);
    NAOS_CHECK_TRUE(clargs_result_flag(result, fixture.verbose));

    clargs_result_destroy(result);
    destroy_root(fixture.root);
}

NAOS_TEST(options_before_the_terminator_are_options)
{
    copy_fixture_t fixture;
    char *argv[] = { "copy", "-v", "--", "-v" };
    clargs_result_t *result;

    set_up_copy(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);

    NAOS_CHECK_TRUE(clargs_result_flag(result, fixture.verbose));
    check_count(result, fixture.src, 1);
    check_string(result, fixture.src, 0, "-v");

    clargs_result_destroy(result);
    destroy_root(fixture.root);
}

/* ---- Arguments that start with '-' and are not options ------------------ */

NAOS_TEST(a_lone_dash_and_the_empty_string_are_values)
{
    copy_fixture_t fixture;
    char *argv[] = { "copy", "-", "", "-", "-v" };
    clargs_result_t *result;

    set_up_copy(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);

    check_count(result, fixture.src, 1);
    check_string(result, fixture.src, 0, "-");
    check_count(result, fixture.dst, 1);
    check_string(result, fixture.dst, 0, "");
    check_count(result, fixture.rest, 1);
    check_string(result, fixture.rest, 0, "-");
    NAOS_CHECK_TRUE(clargs_result_flag(result, fixture.verbose));

    clargs_result_destroy(result);
    destroy_root(fixture.root);
}

NAOS_TEST(a_negative_number_is_a_value_for_a_command_without_a_digit_as_a_short_name)
{
    clargs_command_t *root = create_root("calc");
    clargs_option_t *verbose = NULL;
    clargs_positional_t *level;
    clargs_positional_t *nums;
    char *argv[] = { "calc", "-1", "-.5", "-v", "-2.5", "-10" };
    clargs_result_t *result;
    int64_t integer = 0;
    double number = 0.0;

    NAOS_REQUIRE_TRUE(clargs_command_add_flag(root, 'v', NULL, &verbose) == CLARGS_ERROR_NONE);
    level = add_positional(root, "LEVEL", clargs_type_int(), CLARGS_ARITY_REQUIRED);
    nums = add_positional(root, "NUMS", clargs_type_float(), CLARGS_ARITY_ZERO_OR_MORE);
    validate(root);
    result = parse_ok(root, ARGC(argv), argv);

    check_count(result, level, 1);
    NAOS_REQUIRE_TRUE(clargs_result_positional_int(result, level, 0, &integer));
    NAOS_CHECK_TRUE(integer == -1);
    check_count(result, nums, 3);
    NAOS_REQUIRE_TRUE(clargs_result_positional_float(result, nums, 0, &number));
    NAOS_CHECK_TRUE(number == -0.5);
    NAOS_REQUIRE_TRUE(clargs_result_positional_float(result, nums, 1, &number));
    NAOS_CHECK_TRUE(number == -2.5);
    NAOS_REQUIRE_TRUE(clargs_result_positional_float(result, nums, 2, &number));
    NAOS_CHECK_TRUE(number == -10.0);
    NAOS_CHECK_TRUE(clargs_result_flag(result, verbose));

    clargs_result_destroy(result);
    destroy_root(root);
}

NAOS_TEST(a_negative_number_is_an_option_for_a_command_with_a_digit_as_a_short_name)
{
    clargs_command_t *root = create_root("digit");
    clargs_option_t *one = NULL;
    clargs_positional_t *items;
    char *argv[] = { "digit", "a", "-1", "b", "--", "-1" };
    clargs_result_t *result;

    NAOS_REQUIRE_TRUE(clargs_command_add_flag(root, '1', NULL, &one) == CLARGS_ERROR_NONE);
    items = add_positional(root, "ITEMS", clargs_type_string(), CLARGS_ARITY_ZERO_OR_MORE);
    validate(root);
    result = parse_ok(root, ARGC(argv), argv);

    NAOS_CHECK_TRUE(clargs_result_flag(result, one));
    check_count(result, items, 3);
    check_string(result, items, 0, "a");
    check_string(result, items, 1, "b");
    check_string(result, items, 2, "-1");

    clargs_result_destroy(result);
    destroy_root(root);
}

/* ---- One argument too many ---------------------------------------------- */

NAOS_TEST(an_argument_after_the_last_positional_is_a_parse_error)
{
    clargs_command_t *root = create_root("pair");
    clargs_option_t *verbose = NULL;
    clargs_positional_t *src;
    clargs_positional_t *dst;
    char *argv[] = { "pair", "a", "b", "extra", "-v", "more" };
    clargs_result_t *result;

    NAOS_REQUIRE_TRUE(clargs_command_add_flag(root, 'v', NULL, &verbose) == CLARGS_ERROR_NONE);
    src = add_positional(root, "SRC", clargs_type_string(), CLARGS_ARITY_REQUIRED);
    dst = add_positional(root, "DST", clargs_type_string(), CLARGS_ARITY_OPTIONAL);
    validate(root);
    result = parse_bad(root, ARGC(argv), argv, CLARGS_PARSE_ERROR_UNEXPECTED_POSITIONAL, 3,
                       "unexpected argument 'extra'");

    NAOS_CHECK_TRUE(clargs_result_error_positional(result) == NULL);
    /* What was read before the error is there; the scan stopped at it. */
    check_count(result, src, 1);
    check_string(result, src, 0, "a");
    check_count(result, dst, 1);
    check_string(result, dst, 0, "b");
    NAOS_CHECK_FALSE(clargs_result_flag(result, verbose));

    clargs_result_destroy(result);
    destroy_root(root);
}

NAOS_TEST(an_argument_for_a_command_without_positionals_is_a_parse_error)
{
    clargs_command_t *root = create_root("bare");
    clargs_option_t *verbose = NULL;
    char *argv[] = { "bare", "-v", "extra" };
    clargs_result_t *result;

    NAOS_REQUIRE_TRUE(clargs_command_add_flag(root, 'v', NULL, &verbose) == CLARGS_ERROR_NONE);
    validate(root);
    result = parse_bad(root, ARGC(argv), argv, CLARGS_PARSE_ERROR_UNEXPECTED_POSITIONAL, 2,
                       "unexpected argument 'extra'");

    NAOS_CHECK_TRUE(clargs_result_error_positional(result) == NULL);
    NAOS_CHECK_TRUE(clargs_result_flag(result, verbose));

    clargs_result_destroy(result);
    destroy_root(root);
}

NAOS_TEST(an_argument_too_many_after_the_terminator_is_a_parse_error_and_the_terminator_is_not)
{
    clargs_command_t *root = create_root("bare");
    char *only[] = { "bare", "--" };
    char *argv[] = { "bare", "--", "--" };
    char *dash[] = { "bare", "-" };
    char *empty[] = { "bare", "" };
    char *negative[] = { "bare", "-1" };
    clargs_result_t *result;

    validate(root);
    result = parse_ok(root, ARGC(only), only);
    clargs_result_destroy(result);

    result = parse_bad(root, ARGC(argv), argv, CLARGS_PARSE_ERROR_UNEXPECTED_POSITIONAL, 2,
                       "unexpected argument '--'");
    clargs_result_destroy(result);

    result = parse_bad(root, ARGC(dash), dash, CLARGS_PARSE_ERROR_UNEXPECTED_POSITIONAL, 1,
                       "unexpected argument '-'");
    clargs_result_destroy(result);

    result = parse_bad(root, ARGC(empty), empty, CLARGS_PARSE_ERROR_UNEXPECTED_POSITIONAL, 1,
                       "unexpected argument ''");
    clargs_result_destroy(result);

    result = parse_bad(root, ARGC(negative), negative, CLARGS_PARSE_ERROR_UNEXPECTED_POSITIONAL, 1,
                       "unexpected argument '-1'");
    clargs_result_destroy(result);

    destroy_root(root);
}

/* ---- A value that the value type does not take -------------------------- */

NAOS_TEST(a_value_that_the_type_of_a_positional_does_not_take_is_a_parse_error)
{
    clargs_command_t *root = create_root("repeat");
    clargs_option_t *verbose = NULL;
    clargs_positional_t *text;
    clargs_positional_t *count;
    char *argv[] = { "repeat", "word", "-v", "x", "5" };
    clargs_result_t *result;

    NAOS_REQUIRE_TRUE(clargs_command_add_flag(root, 'v', NULL, &verbose) == CLARGS_ERROR_NONE);
    text = add_positional(root, "TEXT", clargs_type_string(), CLARGS_ARITY_REQUIRED);
    count = add_positional(root, "COUNT", clargs_type_int(), CLARGS_ARITY_ZERO_OR_MORE);
    validate(root);
    result = parse_bad(root, ARGC(argv), argv, CLARGS_PARSE_ERROR_INVALID_VALUE, 3,
                       "invalid value 'x' for argument 'COUNT' (expected integer)");

    NAOS_CHECK_TRUE(clargs_result_error_positional(result) == count);
    check_count(result, text, 1);
    check_string(result, text, 0, "word");
    check_count(result, count, 0);
    NAOS_CHECK_TRUE(clargs_result_flag(result, verbose));

    clargs_result_destroy(result);
    destroy_root(root);
}

NAOS_TEST(a_wrong_value_after_good_ones_keeps_the_good_ones)
{
    clargs_command_t *root = create_root("sum");
    clargs_positional_t *nums = add_positional(root, "NUMS", clargs_type_int(), CLARGS_ARITY_ONE_OR_MORE);
    char *argv[] = { "sum", "1", "2", "--", "-x", "3" };
    clargs_result_t *result;
    int64_t integer = 0;

    validate(root);
    result = parse_bad(root, ARGC(argv), argv, CLARGS_PARSE_ERROR_INVALID_VALUE, 4,
                       "invalid value '-x' for argument 'NUMS' (expected integer)");

    NAOS_CHECK_TRUE(clargs_result_error_positional(result) == nums);
    check_count(result, nums, 2);
    NAOS_REQUIRE_TRUE(clargs_result_positional_int(result, nums, 1, &integer));
    NAOS_CHECK_TRUE(integer == 2);

    clargs_result_destroy(result);
    destroy_root(root);
}

/* ---- The entry ----------------------------------------------------------- */

NAOS_TEST(the_values_are_copies_and_a_second_parse_gives_the_same_result)
{
    copy_fixture_t fixture;
    char first[] = "a";
    char second[] = "b";
    char *argv[3];
    clargs_result_t *result;
    clargs_result_t *again;

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

    clargs_result_destroy(again);
    clargs_result_destroy(result);
    destroy_root(fixture.root);
}

/* ---- A lack of memory --------------------------------------------------- */

/* Parses argv once for each allocation of the parse, with that allocation
 * failing, and then once with none failing, which must return expected. */
static void check_every_allocation_failing(const clargs_command_t *root, int argc, char *const *argv,
                                           clargs_error_t expected)
{
    size_t nth;

    for (nth = 1; nth < 100; nth++) {
        const size_t live = clargs_test_allocator_live();
        clargs_result_t *result = NULL;
        clargs_error_t error;

        clargs_test_allocator_fail_at(clargs_test_allocator_requests() + nth);
        error = clargs_parse(root, argc, argv, &result);
        clargs_test_allocator_fail_at(0);

        if (error != CLARGS_ERROR_OUT_OF_MEMORY) {
            /* The parse made fewer than nth allocations. */
            NAOS_CHECK_INT_EQ(error, expected);
            NAOS_REQUIRE_TRUE(result != NULL);
            NAOS_CHECK_INT_EQ(clargs_result_error(result), expected);
            clargs_result_destroy(result);
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);
            /* The copy of the arguments, the frame, the slots, and one
             * allocation at least for what the positionals are given. */
            NAOS_CHECK_TRUE(nth > 4);
            return;
        }
        NAOS_CHECK_TRUE(result == NULL);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);
    }
    NAOS_FAIL("the parse never succeeded");
}

NAOS_TEST(a_parse_of_positionals_that_cannot_allocate_returns_no_result_and_leaves_nothing_allocated)
{
    copy_fixture_t fixture;
    char *argv[] = { "copy", "a", "-v", "b", "c", "--", "-d", "--" };
    char *extra[] = { "copy", "a", "b" };
    char *invalid[] = { "copy", "1", "x" };
    clargs_command_t *pair;
    clargs_command_t *nums;
    clargs_result_t *result;

    set_up_copy(&fixture);
    check_every_allocation_failing(fixture.root, ARGC(argv), argv, CLARGS_ERROR_NONE);

    /* After the failures, the same schema parses the same arguments. */
    result = parse_ok(fixture.root, ARGC(argv), argv);
    check_count(result, fixture.rest, 3);
    check_string(result, fixture.rest, 2, "--");
    clargs_result_destroy(result);
    clargs_command_destroy(fixture.root);

    NAOS_REQUIRE_TRUE(clargs_command_create("pair", &pair) == CLARGS_ERROR_NONE);
    (void)add_positional(pair, "ONLY", clargs_type_string(), CLARGS_ARITY_REQUIRED);
    validate(pair);
    check_every_allocation_failing(pair, ARGC(extra), extra, CLARGS_ERROR_INVALID_COMMAND_LINE);
    clargs_command_destroy(pair);

    NAOS_REQUIRE_TRUE(clargs_command_create("nums", &nums) == CLARGS_ERROR_NONE);
    (void)add_positional(nums, "NUMS", clargs_type_int(), CLARGS_ARITY_ONE_OR_MORE);
    validate(nums);
    check_every_allocation_failing(nums, ARGC(invalid), invalid, CLARGS_ERROR_INVALID_COMMAND_LINE);
    clargs_command_destroy(nums);

    clargs_test_allocator_uninstall();
    check_clean();
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

NAOS_SUITE(memory,
    NAOS_ADD_TEST(a_parse_of_positionals_that_cannot_allocate_returns_no_result_and_leaves_nothing_allocated)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(assignment),
    NAOS_ADD_SUITE(options_among_positionals),
    NAOS_ADD_SUITE(terminator),
    NAOS_ADD_SUITE(not_options),
    NAOS_ADD_SUITE(errors),
    NAOS_ADD_SUITE(memory)
)
