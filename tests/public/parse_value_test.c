/*
 * Tests of the public parsing API for the values that a command line did
 * not give: argx_parse() of <argx/parse.h>, read back with the
 * argx_result_*() functions.
 *
 * A schema is built the way a consumer does it, validated, and parsed with
 * arguments written as an argv array. The tests cover what is done after
 * the last argument was read without a parse error: the check that what is
 * required was given (an option that takes a value, a positional that takes
 * one argument or one or more), which of several missing ones is reported,
 * the default values of the options and positionals that were not given,
 * the source of every value, what an option or a positional without a value
 * reads as, what the options that take no value read as when they were not
 * given, that the commands that were not selected are neither checked nor
 * given their default values, and that a result with a parse error holds no
 * default value.
 *
 * They also cover the conversion of the values by their value types: the
 * values of the five kinds of value types, of options and of positionals,
 * read through both ways of reading; that a value type is given the text as
 * it was written; that every value that was converted is cleaned up exactly
 * once, counted with a user-defined value type; and what a parse returns
 * when the conversion of a user-defined value type fails.
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

static argx_command_t *add_subcommand(argx_command_t *command, const char *name)
{
    argx_command_t *subcommand = NULL;

    NAOS_REQUIRE_TRUE(argx_command_add_subcommand(command, name, &subcommand) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(subcommand != NULL);
    return subcommand;
}

/*
 * Adds an option that takes a value to command. default_value may be
 * NULL: the option has no default value then.
 */
static argx_option_t *add_value(argx_command_t *command, char short_name, const char *long_name,
                                const argx_type_t *type, const char *default_value)
{
    argx_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(argx_command_add_value(command, short_name, long_name, type, &option) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(option != NULL);
    if (default_value != NULL)
    {
        NAOS_REQUIRE_TRUE(argx_option_set_default(option, default_value) == ARGX_ERROR_NONE);
    }
    return option;
}

/* Adds an option that takes a value and must be given to command. */
static argx_option_t *add_required(argx_command_t *command, char short_name, const char *long_name,
                                   const argx_type_t *type)
{
    argx_option_t *option = add_value(command, short_name, long_name, type, NULL);

    NAOS_REQUIRE_TRUE(argx_option_set_required(option, true) == ARGX_ERROR_NONE);
    return option;
}

/*
 * Adds a positional to command. default_value may be NULL: the positional
 * has no default value then.
 */
static argx_positional_t *add_positional(argx_command_t *command, const char *name,
                                         const argx_type_t *type, argx_arity_t arity,
                                         const char *default_value)
{
    argx_positional_t *positional = NULL;

    NAOS_REQUIRE_TRUE(argx_command_add_positional(command, name, type, arity, &positional) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(positional != NULL);
    if (default_value != NULL)
    {
        NAOS_REQUIRE_TRUE(argx_positional_set_default(positional, default_value) == ARGX_ERROR_NONE);
    }
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
 * Parses argv, which must have a parse error of the kind with the message.
 * Returns the result, which the caller destroys.
 */
static argx_result_t *parse_bad(const argx_command_t *root, int argc, char *const *argv,
                                argx_parse_error_kind_t kind, const char *message)
{
    argx_result_t *result = NULL;

    NAOS_REQUIRE_TRUE(argx_parse(root, argc, argv, &result) == ARGX_ERROR_INVALID_COMMAND_LINE);
    NAOS_REQUIRE_TRUE(result != NULL);
    NAOS_CHECK_INT_EQ(argx_result_error(result), ARGX_ERROR_INVALID_COMMAND_LINE);
    NAOS_CHECK_INT_EQ(argx_result_error_kind(result), kind);
    NAOS_REQUIRE_TRUE(argx_result_error_message(result) != NULL);
    NAOS_CHECK_STR_EQ(argx_result_error_message(result), message);
    return result;
}

/*
 * Checks that option, of the value type string, has the value expected
 * from source, through both ways of reading it.
 */
static void check_option_string(const argx_result_t *result, const argx_option_t *option,
                                const char *expected, argx_source_t source)
{
    const char *value = NULL;
    const void *area;

    NAOS_CHECK_INT_EQ(argx_result_option_source(result, option), source);
    NAOS_REQUIRE_TRUE(argx_result_option_string(result, option, &value));
    NAOS_REQUIRE_TRUE(value != NULL);
    NAOS_CHECK_STR_EQ(value, expected);
    area = argx_result_option_value(result, option);
    NAOS_REQUIRE_TRUE(area != NULL);
    NAOS_CHECK_TRUE(*(const char *const *)area == value);
}

/* The same for an option of the value type int. */
static void check_option_int(const argx_result_t *result, const argx_option_t *option,
                             int64_t expected, argx_source_t source)
{
    int64_t value = 0;

    NAOS_CHECK_INT_EQ(argx_result_option_source(result, option), source);
    NAOS_REQUIRE_TRUE(argx_result_option_int(result, option, &value));
    NAOS_CHECK_TRUE(value == expected);
}

/* Checks that option, of the value type string, has no value. */
static void check_option_none(const argx_result_t *result, const argx_option_t *option)
{
    const char *value = "untouched";

    NAOS_CHECK_INT_EQ(argx_result_option_source(result, option), ARGX_SOURCE_NONE);
    NAOS_CHECK_FALSE(argx_result_option_string(result, option, &value));
    NAOS_CHECK_STR_EQ(value, "untouched");
    NAOS_CHECK_TRUE(argx_result_option_value(result, option) == NULL);
}

/*
 * Checks that positional, of the value type string, has the one value
 * expected from source.
 */
static void check_positional_string(const argx_result_t *result, const argx_positional_t *positional,
                                    const char *expected, argx_source_t source)
{
    const char *value = NULL;

    NAOS_CHECK_INT_EQ(argx_result_positional_source(result, positional), source);
    NAOS_CHECK_UINT_EQ(argx_result_positional_count(result, positional), 1);
    NAOS_REQUIRE_TRUE(argx_result_positional_string(result, positional, 0, &value));
    NAOS_REQUIRE_TRUE(value != NULL);
    NAOS_CHECK_STR_EQ(value, expected);
}

/* Checks that positional, of the value type string, has no value. */
static void check_positional_none(const argx_result_t *result, const argx_positional_t *positional)
{
    const char *value = "untouched";

    NAOS_CHECK_INT_EQ(argx_result_positional_source(result, positional), ARGX_SOURCE_NONE);
    NAOS_CHECK_UINT_EQ(argx_result_positional_count(result, positional), 0);
    NAOS_CHECK_FALSE(argx_result_positional_string(result, positional, 0, &value));
    NAOS_CHECK_STR_EQ(value, "untouched");
    NAOS_CHECK_TRUE(argx_result_positional_value(result, positional, 0) == NULL);
}

/* ---- Default values ----------------------------------------------------- */

NAOS_TEST(an_option_and_a_positional_that_were_not_given_have_their_default_values)
{
    argx_command_t *root = create_root("tool");
    argx_option_t *name = add_value(root, 'n', "name", argx_type_string(), "anon");
    argx_option_t *jobs = add_value(root, 'j', "jobs", argx_type_int(), "4");
    argx_option_t *empty = add_value(root, '\0', "empty", argx_type_string(), "");
    argx_positional_t *file =
        add_positional(root, "FILE", argx_type_string(), ARGX_ARITY_OPTIONAL, "out.txt");
    char *argv[] = { "tool" };
    argx_result_t *result;

    validate(root);
    result = parse_ok(root, ARGC(argv), argv);
    check_option_string(result, name, "anon", ARGX_SOURCE_DEFAULT);
    check_option_int(result, jobs, 4, ARGX_SOURCE_DEFAULT);
    check_option_string(result, empty, "", ARGX_SOURCE_DEFAULT);
    check_positional_string(result, file, "out.txt", ARGX_SOURCE_DEFAULT);
    argx_result_destroy(result);

    /*
     * A command line without any argument, not even the name of the
     * program, is one on which nothing was given.
     */
    result = parse_ok(root, 0, NULL);
    check_option_string(result, name, "anon", ARGX_SOURCE_DEFAULT);
    check_option_int(result, jobs, 4, ARGX_SOURCE_DEFAULT);
    check_positional_string(result, file, "out.txt", ARGX_SOURCE_DEFAULT);
    argx_result_destroy(result);

    destroy_root(root);
}

NAOS_TEST(what_was_given_on_the_command_line_is_the_value_and_not_the_default)
{
    argx_command_t *root = create_root("tool");
    argx_option_t *name = add_value(root, 'n', "name", argx_type_string(), "anon");
    argx_option_t *jobs = add_value(root, 'j', "jobs", argx_type_int(), "4");
    argx_positional_t *file =
        add_positional(root, "FILE", argx_type_string(), ARGX_ARITY_OPTIONAL, "out.txt");
    char *all[] = { "tool", "--name", "eve", "-j8", "in.txt" };
    char *some[] = { "tool", "--jobs=4" };
    argx_result_t *result;

    validate(root);
    result = parse_ok(root, ARGC(all), all);
    check_option_string(result, name, "eve", ARGX_SOURCE_COMMAND_LINE);
    check_option_int(result, jobs, 8, ARGX_SOURCE_COMMAND_LINE);
    check_positional_string(result, file, "in.txt", ARGX_SOURCE_COMMAND_LINE);
    argx_result_destroy(result);

    /*
     * A value that equals the default value still comes from the command
     * line; the others are defaults.
     */
    result = parse_ok(root, ARGC(some), some);
    check_option_string(result, name, "anon", ARGX_SOURCE_DEFAULT);
    check_option_int(result, jobs, 4, ARGX_SOURCE_COMMAND_LINE);
    check_positional_string(result, file, "out.txt", ARGX_SOURCE_DEFAULT);
    argx_result_destroy(result);

    destroy_root(root);
}

NAOS_TEST(an_option_and_a_positional_without_a_default_that_were_not_given_have_no_value)
{
    argx_command_t *root = create_root("tool");
    argx_option_t *name = add_value(root, 'n', "name", argx_type_string(), NULL);
    argx_option_t *jobs = add_value(root, 'j', "jobs", argx_type_int(), NULL);
    argx_positional_t *file =
        add_positional(root, "FILE", argx_type_string(), ARGX_ARITY_OPTIONAL, NULL);
    char *argv[] = { "tool" };
    int64_t number = 77;
    argx_result_t *result;

    validate(root);
    result = parse_ok(root, ARGC(argv), argv);
    check_option_none(result, name);
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, jobs), ARGX_SOURCE_NONE);
    NAOS_CHECK_FALSE(argx_result_option_int(result, jobs, &number));
    NAOS_CHECK_TRUE(number == 77);
    NAOS_CHECK_TRUE(argx_result_option_value(result, jobs) == NULL);
    check_positional_none(result, file);
    argx_result_destroy(result);

    destroy_root(root);
}

NAOS_TEST(a_positional_that_takes_any_number_has_no_value_when_none_was_given)
{
    argx_command_t *root = create_root("tool");
    argx_positional_t *first =
        add_positional(root, "FIRST", argx_type_string(), ARGX_ARITY_OPTIONAL, "one");
    argx_positional_t *rest =
        add_positional(root, "REST", argx_type_string(), ARGX_ARITY_ZERO_OR_MORE, NULL);
    char *argv[] = { "tool" };
    argx_result_t *result;

    validate(root);
    result = parse_ok(root, ARGC(argv), argv);
    check_positional_string(result, first, "one", ARGX_SOURCE_DEFAULT);
    check_positional_none(result, rest);
    argx_result_destroy(result);

    destroy_root(root);
}

NAOS_TEST(the_options_that_take_no_value_read_as_their_initial_values_when_they_were_not_given)
{
    argx_command_t *root = create_root("tool");
    argx_option_t *force = NULL;
    argx_option_t *verbose = NULL;
    argx_option_t *color = NULL;
    argx_option_t *pager = NULL;
    char *none[] = { "tool" };
    char *given[] = { "tool", "--force", "-vv", "--no-color", "--pager" };
    argx_result_t *result;

    NAOS_REQUIRE_TRUE(argx_command_add_flag(root, 'f', "force", &force) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_count(root, 'v', "verbose", &verbose) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_negatable(root, '\0', "color", &color) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_option_set_initial(color, true) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_negatable(root, '\0', "pager", &pager) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_option_set_initial(pager, false) == ARGX_ERROR_NONE);
    validate(root);

    result = parse_ok(root, ARGC(none), none);
    NAOS_CHECK_FALSE(argx_result_flag(result, force));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, force), ARGX_SOURCE_DEFAULT);
    NAOS_CHECK_UINT_EQ(argx_result_count(result, verbose), 0);
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, verbose), ARGX_SOURCE_DEFAULT);
    NAOS_CHECK_TRUE(argx_result_flag(result, color));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, color), ARGX_SOURCE_DEFAULT);
    NAOS_CHECK_FALSE(argx_result_flag(result, pager));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, pager), ARGX_SOURCE_DEFAULT);
    argx_result_destroy(result);

    result = parse_ok(root, ARGC(given), given);
    NAOS_CHECK_TRUE(argx_result_flag(result, force));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, force), ARGX_SOURCE_COMMAND_LINE);
    NAOS_CHECK_UINT_EQ(argx_result_count(result, verbose), 2);
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, verbose), ARGX_SOURCE_COMMAND_LINE);
    NAOS_CHECK_FALSE(argx_result_flag(result, color));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, color), ARGX_SOURCE_COMMAND_LINE);
    NAOS_CHECK_TRUE(argx_result_flag(result, pager));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, pager), ARGX_SOURCE_COMMAND_LINE);
    argx_result_destroy(result);

    destroy_root(root);
}

/* ---- What is required --------------------------------------------------- */

NAOS_TEST(a_required_option_that_was_not_given_is_a_parse_error)
{
    argx_command_t *root = create_root("tool");
    argx_option_t *output = add_required(root, 'o', "output", argx_type_string());
    argx_option_t *force = NULL;
    char *missing[] = { "tool", "--force" };
    char *by_long[] = { "tool", "--output", "a.txt" };
    char *by_short[] = { "tool", "-oa.txt" };
    argx_result_t *result;

    NAOS_REQUIRE_TRUE(argx_command_add_flag(root, 'f', "force", &force) == ARGX_ERROR_NONE);
    validate(root);

    result = parse_bad(root, ARGC(missing), missing, ARGX_PARSE_ERROR_MISSING_OPTION,
                       "missing required option '--output'");
    NAOS_CHECK_TRUE(argx_result_error_option(result) == output);
    check_option_none(result, output);
    /* What was read before the end of the command line is still there. */
    NAOS_CHECK_TRUE(argx_result_flag(result, force));
    argx_result_destroy(result);

    result = parse_ok(root, ARGC(by_long), by_long);
    check_option_string(result, output, "a.txt", ARGX_SOURCE_COMMAND_LINE);
    argx_result_destroy(result);

    result = parse_ok(root, ARGC(by_short), by_short);
    check_option_string(result, output, "a.txt", ARGX_SOURCE_COMMAND_LINE);
    argx_result_destroy(result);

    destroy_root(root);
}

NAOS_TEST(a_missing_required_option_without_a_long_name_is_named_by_its_short_name)
{
    argx_command_t *root = create_root("tool");
    argx_option_t *output = add_required(root, 'o', NULL, argx_type_string());
    char *argv[] = { "tool" };
    argx_result_t *result;

    validate(root);
    result = parse_bad(root, ARGC(argv), argv, ARGX_PARSE_ERROR_MISSING_OPTION,
                       "missing required option '-o'");
    NAOS_CHECK_TRUE(argx_result_error_option(result) == output);
    argx_result_destroy(result);

    destroy_root(root);
}

NAOS_TEST(a_required_positional_that_was_not_given_is_a_parse_error)
{
    argx_command_t *root = create_root("tool");
    argx_positional_t *file =
        add_positional(root, "FILE", argx_type_string(), ARGX_ARITY_REQUIRED, NULL);
    argx_positional_t *more =
        add_positional(root, "MORE", argx_type_string(), ARGX_ARITY_ZERO_OR_MORE, NULL);
    char *missing[] = { "tool" };
    char *given[] = { "tool", "a.txt" };
    argx_result_t *result;

    validate(root);
    result = parse_bad(root, ARGC(missing), missing, ARGX_PARSE_ERROR_MISSING_POSITIONAL,
                       "missing required argument 'FILE'");
    NAOS_CHECK_TRUE(argx_result_error_positional(result) == file);
    check_positional_none(result, file);
    argx_result_destroy(result);

    result = parse_ok(root, ARGC(given), given);
    check_positional_string(result, file, "a.txt", ARGX_SOURCE_COMMAND_LINE);
    check_positional_none(result, more);
    argx_result_destroy(result);

    destroy_root(root);
}

NAOS_TEST(a_positional_that_takes_one_or_more_without_a_value_is_a_parse_error)
{
    argx_command_t *root = create_root("tool");
    argx_positional_t *files =
        add_positional(root, "FILES", argx_type_string(), ARGX_ARITY_ONE_OR_MORE, NULL);
    char *missing[] = { "tool", "--" };
    char *given[] = { "tool", "a", "b" };
    argx_result_t *result;

    validate(root);
    result = parse_bad(root, ARGC(missing), missing, ARGX_PARSE_ERROR_MISSING_POSITIONAL,
                       "missing required argument 'FILES'");
    NAOS_CHECK_TRUE(argx_result_error_positional(result) == files);
    argx_result_destroy(result);

    result = parse_ok(root, ARGC(given), given);
    NAOS_CHECK_UINT_EQ(argx_result_positional_count(result, files), 2);
    NAOS_CHECK_INT_EQ(argx_result_positional_source(result, files), ARGX_SOURCE_COMMAND_LINE);
    argx_result_destroy(result);

    destroy_root(root);
}

/*
 * Of several required things that are missing, the first is reported: the
 * commands from the root on, the options of a command before its
 * positionals, each in the order in which they were added.
 */
NAOS_TEST(the_first_missing_required_thing_is_reported)
{
    argx_command_t *root = create_root("tool");
    argx_option_t *token = add_required(root, 't', "token", argx_type_string());
    argx_command_t *copy = add_subcommand(root, "copy");
    /*
     * The positionals are added before the options, to tell the order of
     * the report from the order of the definitions.
     */
    argx_positional_t *source =
        add_positional(copy, "SOURCE", argx_type_string(), ARGX_ARITY_REQUIRED, NULL);
    argx_positional_t *target =
        add_positional(copy, "TARGET", argx_type_string(), ARGX_ARITY_REQUIRED, NULL);
    argx_option_t *mode = add_required(copy, 'm', "mode", argx_type_string());
    argx_option_t *owner = add_required(copy, 'w', "owner", argx_type_string());
    char *nothing[] = { "tool", "copy" };
    char *with_token[] = { "tool", "-tx", "copy" };
    char *with_mode[] = { "tool", "-tx", "copy", "--owner", "o", "--mode", "m" };
    char *with_owner[] = { "tool", "-tx", "copy", "--owner", "o" };
    char *with_source[] = { "tool", "-tx", "copy", "-mm", "-wo", "a" };
    char *complete[] = { "tool", "-tx", "copy", "a", "-mm", "b", "-wo" };
    argx_result_t *result;

    validate(root);

    /* The root comes before its subcommand. */
    result = parse_bad(root, ARGC(nothing), nothing, ARGX_PARSE_ERROR_MISSING_OPTION,
                       "missing required option '--token'");
    NAOS_CHECK_TRUE(argx_result_error_option(result) == token);
    NAOS_CHECK_TRUE(argx_result_error_command(result) == root);
    argx_result_destroy(result);

    /* The options come before the positionals, in their order. */
    result = parse_bad(root, ARGC(with_token), with_token, ARGX_PARSE_ERROR_MISSING_OPTION,
                       "missing required option '--mode'");
    NAOS_CHECK_TRUE(argx_result_error_option(result) == mode);
    NAOS_CHECK_TRUE(argx_result_error_command(result) == copy);
    argx_result_destroy(result);

    result = parse_bad(root, ARGC(with_owner), with_owner, ARGX_PARSE_ERROR_MISSING_OPTION,
                       "missing required option '--mode'");
    NAOS_CHECK_TRUE(argx_result_error_option(result) == mode);
    argx_result_destroy(result);

    /* The positionals in their order. */
    result = parse_bad(root, ARGC(with_mode), with_mode, ARGX_PARSE_ERROR_MISSING_POSITIONAL,
                       "missing required argument 'SOURCE'");
    NAOS_CHECK_TRUE(argx_result_error_positional(result) == source);
    NAOS_CHECK_TRUE(argx_result_error_command(result) == copy);
    argx_result_destroy(result);

    result = parse_bad(root, ARGC(with_source), with_source, ARGX_PARSE_ERROR_MISSING_POSITIONAL,
                       "missing required argument 'TARGET'");
    NAOS_CHECK_TRUE(argx_result_error_positional(result) == target);
    argx_result_destroy(result);

    result = parse_ok(root, ARGC(complete), complete);
    check_option_string(result, owner, "o", ARGX_SOURCE_COMMAND_LINE);
    check_positional_string(result, target, "b", ARGX_SOURCE_COMMAND_LINE);
    argx_result_destroy(result);

    destroy_root(root);
}

/* ---- The commands that were and were not selected ----------------------- */

/*
 * A tool with the subcommands "build", which has something required, a
 * default value and a negatable flag for each initial state, and "clean".
 */
typedef struct tool_fixture
{
    argx_command_t *root;
    argx_option_t *config;      /* --config of tool, default "tool.cfg" */
    argx_command_t *build;
    argx_option_t *target;      /* --target of build, required */
    argx_option_t *jobs;        /* --jobs of build, default "2" */
    argx_option_t *color;       /* --color of build, initially on */
    argx_option_t *cache;       /* --cache of build, initially off */
    argx_positional_t *source;  /* SOURCE of build, required */
    argx_positional_t *output;  /* OUTPUT of build, optional, default "a.out" */
    argx_command_t *clean;
    argx_option_t *depth;       /* --depth of clean, default "1" */
} tool_fixture_t;

static void set_up_tool(tool_fixture_t *fixture)
{
    fixture->root = create_root("tool");
    fixture->config = add_value(fixture->root, 'c', "config", argx_type_string(), "tool.cfg");
    fixture->build = add_subcommand(fixture->root, "build");
    fixture->target = add_required(fixture->build, 't', "target", argx_type_string());
    fixture->jobs = add_value(fixture->build, 'j', "jobs", argx_type_int(), "2");
    fixture->color = NULL;
    fixture->cache = NULL;
    NAOS_REQUIRE_TRUE(argx_command_add_negatable(fixture->build, '\0', "color", &fixture->color) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_option_set_initial(fixture->color, true) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_negatable(fixture->build, '\0', "cache", &fixture->cache) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_option_set_initial(fixture->cache, false) == ARGX_ERROR_NONE);
    fixture->source =
        add_positional(fixture->build, "SOURCE", argx_type_string(), ARGX_ARITY_REQUIRED, NULL);
    fixture->output =
        add_positional(fixture->build, "OUTPUT", argx_type_string(), ARGX_ARITY_OPTIONAL, "a.out");
    fixture->clean = add_subcommand(fixture->root, "clean");
    fixture->depth = add_value(fixture->clean, 'd', "depth", argx_type_int(), "1");
    validate(fixture->root);
}

/* Checks that nothing of "build" was checked or filled in. */
static void check_build_not_selected(const argx_result_t *result, const tool_fixture_t *fixture)
{
    int64_t number = 77;

    check_option_none(result, fixture->target);
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture->jobs), ARGX_SOURCE_NONE);
    NAOS_CHECK_FALSE(argx_result_option_int(result, fixture->jobs, &number));
    NAOS_CHECK_TRUE(number == 77);
    NAOS_CHECK_TRUE(argx_result_option_value(result, fixture->jobs) == NULL);
    /* A negatable flag reads as its initial state, which nothing gave. */
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture->color));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture->color), ARGX_SOURCE_NONE);
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture->cache));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture->cache), ARGX_SOURCE_NONE);
    check_positional_none(result, fixture->source);
    check_positional_none(result, fixture->output);
}

NAOS_TEST(a_command_that_was_not_selected_is_not_checked_and_gets_no_default_value)
{
    tool_fixture_t fixture;
    char *sibling[] = { "tool", "clean" };
    char *parent[] = { "tool" };
    argx_result_t *result;

    set_up_tool(&fixture);

    /*
     * "build" has a required option and a required positional, and "clean"
     * was selected: nothing is missing.
     */
    result = parse_ok(fixture.root, ARGC(sibling), sibling);
    NAOS_CHECK_TRUE(argx_result_command(result) == fixture.clean);
    check_option_string(result, fixture.config, "tool.cfg", ARGX_SOURCE_DEFAULT);
    check_option_int(result, fixture.depth, 1, ARGX_SOURCE_DEFAULT);
    check_build_not_selected(result, &fixture);
    argx_result_destroy(result);

    /* No subcommand was selected: neither is checked or filled in. */
    result = parse_ok(fixture.root, ARGC(parent), parent);
    NAOS_CHECK_TRUE(argx_result_command(result) == fixture.root);
    check_option_string(result, fixture.config, "tool.cfg", ARGX_SOURCE_DEFAULT);
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.depth), ARGX_SOURCE_NONE);
    NAOS_CHECK_TRUE(argx_result_option_value(result, fixture.depth) == NULL);
    check_build_not_selected(result, &fixture);
    argx_result_destroy(result);

    destroy_root(fixture.root);
}

NAOS_TEST(every_selected_command_is_checked_and_gets_its_default_values)
{
    tool_fixture_t fixture;
    char *complete[] = { "tool", "build", "--target", "x86", "main.c" };
    char *missing[] = { "tool", "build", "main.c" };
    argx_result_t *result;

    set_up_tool(&fixture);

    result = parse_ok(fixture.root, ARGC(complete), complete);
    check_option_string(result, fixture.config, "tool.cfg", ARGX_SOURCE_DEFAULT);
    check_option_string(result, fixture.target, "x86", ARGX_SOURCE_COMMAND_LINE);
    check_option_int(result, fixture.jobs, 2, ARGX_SOURCE_DEFAULT);
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.color));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.color), ARGX_SOURCE_DEFAULT);
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.cache));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.cache), ARGX_SOURCE_DEFAULT);
    check_positional_string(result, fixture.source, "main.c", ARGX_SOURCE_COMMAND_LINE);
    check_positional_string(result, fixture.output, "a.out", ARGX_SOURCE_DEFAULT);
    /* "clean" was not selected. */
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.depth), ARGX_SOURCE_NONE);
    NAOS_CHECK_TRUE(argx_result_option_value(result, fixture.depth) == NULL);
    argx_result_destroy(result);

    result = parse_bad(fixture.root, ARGC(missing), missing, ARGX_PARSE_ERROR_MISSING_OPTION,
                       "missing required option '--target'");
    NAOS_CHECK_TRUE(argx_result_error_command(result) == fixture.build);
    argx_result_destroy(result);

    destroy_root(fixture.root);
}

/* ---- A result with a parse error ---------------------------------------- */

NAOS_TEST(a_result_with_a_parse_error_has_no_default_value)
{
    tool_fixture_t fixture;
    char *unknown[] = { "tool", "build", "-tx86", "main.c", "--bogus" };
    char *missing[] = { "tool", "build", "main.c" };
    int64_t number = 77;
    argx_result_t *result;

    set_up_tool(&fixture);

    /* An error of the scan. */
    result = parse_bad(fixture.root, ARGC(unknown), unknown, ARGX_PARSE_ERROR_UNKNOWN_OPTION,
                       "unknown option '--bogus'");
    check_option_none(result, fixture.config);
    check_option_string(result, fixture.target, "x86", ARGX_SOURCE_COMMAND_LINE);
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.jobs), ARGX_SOURCE_NONE);
    NAOS_CHECK_FALSE(argx_result_option_int(result, fixture.jobs, &number));
    NAOS_CHECK_TRUE(number == 77);
    check_positional_string(result, fixture.source, "main.c", ARGX_SOURCE_COMMAND_LINE);
    check_positional_none(result, fixture.output);
    /* The options that take no value read as when they were not given. */
    NAOS_CHECK_TRUE(argx_result_flag(result, fixture.color));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.color), ARGX_SOURCE_DEFAULT);
    NAOS_CHECK_FALSE(argx_result_flag(result, fixture.cache));
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.cache), ARGX_SOURCE_DEFAULT);
    argx_result_destroy(result);

    /*
     * Something required is missing: the default values of the commands
     * before it, and of the same command, are not applied either.
     */
    result = parse_bad(fixture.root, ARGC(missing), missing, ARGX_PARSE_ERROR_MISSING_OPTION,
                       "missing required option '--target'");
    check_option_none(result, fixture.config);
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.jobs), ARGX_SOURCE_NONE);
    NAOS_CHECK_TRUE(argx_result_option_value(result, fixture.jobs) == NULL);
    check_positional_string(result, fixture.source, "main.c", ARGX_SOURCE_COMMAND_LINE);
    check_positional_none(result, fixture.output);
    argx_result_destroy(result);

    destroy_root(fixture.root);
}

/*
 * A value type that takes every text until it is told to reject the text
 * "bad". Its value is the text. The validation of a schema converts the
 * default values with it; the switch is thrown afterwards.
 */
typedef struct picky
{
    bool reject;
} picky_t;

static argx_error_t picky_convert(const char *text, void *value, void *context)
{
    const picky_t *picky = context;

    if (picky->reject && (strcmp(text, "bad") == 0))
    {
        return ARGX_ERROR_INVALID_VALUE;
    }
    memcpy(value, &text, sizeof(text));
    return ARGX_ERROR_NONE;
}

/*
 * The validation checked the default values, so this happens only with a
 * value type that does not give the same answer twice.
 */
NAOS_TEST(a_default_value_that_its_type_does_not_take_is_a_parse_error_without_default_values)
{
    picky_t picky = { false };
    argx_type_t *type = NULL;
    argx_command_t *root = create_root("tool");
    argx_option_t *first;
    argx_option_t *second;
    argx_option_t *third;
    char *argv[] = { "tool" };
    argx_result_t *result;

    NAOS_REQUIRE_TRUE(argx_type_create("word", sizeof(const char *), picky_convert, NULL, &picky,
                                       &type) == ARGX_ERROR_NONE);
    first = add_value(root, '\0', "first", argx_type_string(), "good");
    second = add_value(root, '\0', "second", type, "bad");
    third = add_value(root, '\0', "third", argx_type_string(), "late");
    validate(root);

    result = parse_ok(root, ARGC(argv), argv);
    check_option_string(result, first, "good", ARGX_SOURCE_DEFAULT);
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, second), ARGX_SOURCE_DEFAULT);
    NAOS_CHECK_TRUE(argx_result_option_value(result, second) != NULL);
    argx_result_destroy(result);

    picky.reject = true;
    result = parse_bad(root, ARGC(argv), argv, ARGX_PARSE_ERROR_INVALID_VALUE,
                       "invalid default value 'bad' for option '--second' (expected word)");
    NAOS_CHECK_INT_EQ(argx_result_error_source(result), ARGX_SOURCE_DEFAULT);
    NAOS_CHECK_TRUE(argx_result_error_option(result) == second);
    /* The default value that was applied before the error is taken back. */
    check_option_none(result, first);
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, second), ARGX_SOURCE_NONE);
    NAOS_CHECK_TRUE(argx_result_option_value(result, second) == NULL);
    check_option_none(result, third);
    argx_result_destroy(result);

    argx_command_destroy(root);
    argx_type_destroy(type);
    argx_test_allocator_uninstall();
    check_clean();
}

/* ---- The lack of memory ------------------------------------------------- */

NAOS_TEST(a_parse_that_cannot_allocate_a_default_value_returns_no_result_and_leaves_nothing_allocated)
{
    tool_fixture_t fixture;
    char *argv[] = { "tool", "build", "--target", "x86", "main.c" };
    size_t live;
    size_t before;
    size_t requests;
    size_t nth;
    argx_result_t *result;

    set_up_tool(&fixture);
    live = argx_test_allocator_live();

    /*
     * How many requests a parse that succeeds makes. The last ones are
     * those of the default values.
     */
    before = argx_test_allocator_requests();
    result = parse_ok(fixture.root, ARGC(argv), argv);
    requests = argx_test_allocator_requests() - before;
    argx_result_destroy(result);
    NAOS_REQUIRE_TRUE(requests > 0);

    /* Each of them fails in turn. */
    for (nth = 1; nth <= requests; nth++)
    {
        argx_error_t error;

        result = NULL;
        argx_test_allocator_fail_at(argx_test_allocator_requests() + nth);
        error = argx_parse(fixture.root, ARGC(argv), argv, &result);
        argx_test_allocator_fail_at(0);

        NAOS_CHECK_INT_EQ(error, ARGX_ERROR_OUT_OF_MEMORY);
        NAOS_CHECK_TRUE(result == NULL);
        argx_result_destroy(result);
        NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), live);
    }

    /* The schema is as it was: the same parse succeeds again. */
    result = parse_ok(fixture.root, ARGC(argv), argv);
    check_option_string(result, fixture.config, "tool.cfg", ARGX_SOURCE_DEFAULT);
    check_option_int(result, fixture.jobs, 2, ARGX_SOURCE_DEFAULT);
    check_positional_string(result, fixture.output, "a.out", ARGX_SOURCE_DEFAULT);
    argx_result_destroy(result);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), live);

    destroy_root(fixture.root);
}

/* ---- The value types ---------------------------------------------------- */

/*
 * The value of the user-defined value type "word": the text that the
 * conversion was given, not a copy of it, and the length of the text.
 */
typedef struct word
{
    const char *text;
    size_t length;
} word_t;

/*
 * The user data of the value type "word". It counts the conversions that
 * succeeded and the cleanups, keeps the text of the last conversion that
 * was requested and of the last value that was cleaned up, and tells what
 * the conversion of the text "bad" returns.
 */
typedef struct word_log
{
    size_t converted;
    size_t released;
    argx_error_t failure; /* ARGX_ERROR_NONE: "bad" is taken like every text */
    char last_text[32];
    char last_released[32];
} word_log_t;

static void keep_text(char *buffer, size_t size, const char *text)
{
    size_t length = strlen(text);

    if (length >= size)
    {
        length = size - 1;
    }
    memcpy(buffer, text, length);
    buffer[length] = '\0';
}

static argx_error_t word_convert(const char *text, void *value, void *context)
{
    word_log_t *log = context;
    word_t word;

    keep_text(log->last_text, sizeof(log->last_text), text);
    if ((log->failure != ARGX_ERROR_NONE) && (strcmp(text, "bad") == 0))
    {
        return log->failure;
    }
    word.text = text;
    word.length = strlen(text);
    memcpy(value, &word, sizeof(word));
    log->converted++;
    return ARGX_ERROR_NONE;
}

/*
 * Reads the text of the value: the text must still be valid when its value
 * is cleaned up.
 */
static void word_release(void *value, void *context)
{
    word_log_t *log = context;
    const word_t *word = value;

    keep_text(log->last_released, sizeof(log->last_released), word->text);
    log->released++;
}

/*
 * Creates the value type "word" with log, which it puts in its first
 * state. Call it after create_root(), which resets the test allocator.
 */
static argx_type_t *create_word_type(word_log_t *log)
{
    argx_type_t *type = NULL;

    log->converted = 0;
    log->released = 0;
    log->failure = ARGX_ERROR_NONE;
    log->last_text[0] = '\0';
    log->last_released[0] = '\0';
    NAOS_REQUIRE_TRUE(argx_type_create("word", sizeof(word_t), word_convert, word_release, log, &type) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(type != NULL);
    return type;
}

/*
 * Creates the value type of the choices "red", "green" and "blue". Call it
 * after create_root().
 */
static argx_type_t *create_color_type(void)
{
    const char *const colors[] = { "red", "green", "blue" };
    argx_type_t *type = NULL;

    NAOS_REQUIRE_TRUE(argx_type_create_choice(colors, sizeof(colors) / sizeof(colors[0]), &type) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(type != NULL);
    return type;
}

/*
 * Checks that area, the area of a value of the value type "word", holds
 * the text expected.
 */
static void check_word(const void *area, const char *expected)
{
    const word_t *word = area;

    NAOS_REQUIRE_TRUE(area != NULL);
    NAOS_REQUIRE_TRUE(word->text != NULL);
    NAOS_CHECK_STR_EQ(word->text, expected);
    NAOS_CHECK_UINT_EQ(word->length, strlen(expected));
}

/*
 * Checks that the value at index of positional, of the value type string,
 * is expected, through both ways of reading it.
 */
static void check_positional_string_at(const argx_result_t *result, const argx_positional_t *positional,
                                       size_t index, const char *expected)
{
    const char *value = NULL;
    const void *area;

    NAOS_REQUIRE_TRUE(argx_result_positional_string(result, positional, index, &value));
    NAOS_REQUIRE_TRUE(value != NULL);
    NAOS_CHECK_STR_EQ(value, expected);
    area = argx_result_positional_value(result, positional, index);
    NAOS_REQUIRE_TRUE(area != NULL);
    NAOS_CHECK_TRUE(*(const char *const *)area == value);
}

/* The same for the value type int. */
static void check_positional_int_at(const argx_result_t *result, const argx_positional_t *positional,
                                    size_t index, int64_t expected)
{
    int64_t value = 0;
    const void *area;

    NAOS_REQUIRE_TRUE(argx_result_positional_int(result, positional, index, &value));
    NAOS_CHECK_TRUE(value == expected);
    area = argx_result_positional_value(result, positional, index);
    NAOS_REQUIRE_TRUE(area != NULL);
    NAOS_CHECK_TRUE(*(const int64_t *)area == value);
}

/* The same for the value type float. */
static void check_positional_float_at(const argx_result_t *result, const argx_positional_t *positional,
                                      size_t index, double expected)
{
    double value = 0.0;
    const void *area;

    NAOS_REQUIRE_TRUE(argx_result_positional_float(result, positional, index, &value));
    NAOS_CHECK_TRUE(value == expected);
    area = argx_result_positional_value(result, positional, index);
    NAOS_REQUIRE_TRUE(area != NULL);
    NAOS_CHECK_TRUE(*(const double *)area == value);
}

/* The same for a value type of choices. */
static void check_positional_choice_at(const argx_result_t *result, const argx_positional_t *positional,
                                       size_t index, size_t expected)
{
    size_t value = 99;
    const void *area;

    NAOS_REQUIRE_TRUE(argx_result_positional_choice(result, positional, index, &value));
    NAOS_CHECK_UINT_EQ(value, expected);
    area = argx_result_positional_value(result, positional, index);
    NAOS_REQUIRE_TRUE(area != NULL);
    NAOS_CHECK_TRUE(*(const size_t *)area == value);
}

/*
 * Checks that positional has count values, all from the command line, and
 * none after them.
 */
static void check_positional_given(const argx_result_t *result, const argx_positional_t *positional,
                                   size_t count)
{
    NAOS_CHECK_UINT_EQ(argx_result_positional_count(result, positional), count);
    NAOS_CHECK_INT_EQ(argx_result_positional_source(result, positional), ARGX_SOURCE_COMMAND_LINE);
    NAOS_CHECK_TRUE(argx_result_positional_value(result, positional, count) == NULL);
}

NAOS_TEST(an_option_of_every_value_type_reads_the_same_through_both_ways)
{
    word_log_t log;
    argx_command_t *root = create_root("tool");
    argx_type_t *color_type = create_color_type();
    argx_type_t *word_type = create_word_type(&log);
    argx_option_t *text = add_value(root, 't', "text", argx_type_string(), NULL);
    argx_option_t *num = add_value(root, 'n', "num", argx_type_int(), NULL);
    argx_option_t *ratio = add_value(root, 'r', "ratio", argx_type_float(), NULL);
    argx_option_t *color = add_value(root, 'c', "color", color_type, NULL);
    argx_option_t *word = add_value(root, 'w', "word", word_type, NULL);
    char *argv[] = { "tool", "--text", "as written", "--num", "-42", "--ratio=1.5", "-cgreen",
                     "-w", "two words" };
    int64_t number = 0;
    double real = 0.0;
    size_t index = 99;
    const void *area;
    argx_result_t *result;

    validate(root);
    result = parse_ok(root, ARGC(argv), argv);

    /* string: the text as it was written. */
    check_option_string(result, text, "as written", ARGX_SOURCE_COMMAND_LINE);

    NAOS_CHECK_INT_EQ(argx_result_option_source(result, num), ARGX_SOURCE_COMMAND_LINE);
    NAOS_REQUIRE_TRUE(argx_result_option_int(result, num, &number));
    NAOS_CHECK_TRUE(number == -42);
    area = argx_result_option_value(result, num);
    NAOS_REQUIRE_TRUE(area != NULL);
    NAOS_CHECK_TRUE(*(const int64_t *)area == number);

    NAOS_CHECK_INT_EQ(argx_result_option_source(result, ratio), ARGX_SOURCE_COMMAND_LINE);
    NAOS_REQUIRE_TRUE(argx_result_option_float(result, ratio, &real));
    NAOS_CHECK_TRUE(real == 1.5);
    area = argx_result_option_value(result, ratio);
    NAOS_REQUIRE_TRUE(area != NULL);
    NAOS_CHECK_TRUE(*(const double *)area == real);

    NAOS_CHECK_INT_EQ(argx_result_option_source(result, color), ARGX_SOURCE_COMMAND_LINE);
    NAOS_REQUIRE_TRUE(argx_result_option_choice(result, color, &index));
    NAOS_CHECK_UINT_EQ(index, 1);
    area = argx_result_option_value(result, color);
    NAOS_REQUIRE_TRUE(area != NULL);
    NAOS_CHECK_TRUE(*(const size_t *)area == index);

    /* A user-defined value type is read through the area of the value. */
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, word), ARGX_SOURCE_COMMAND_LINE);
    check_word(argx_result_option_value(result, word), "two words");

    argx_result_destroy(result);
    argx_command_destroy(root);
    argx_type_destroy(word_type);
    argx_type_destroy(color_type);
    argx_test_allocator_uninstall();
    NAOS_CHECK_UINT_EQ(log.released, log.converted);
    check_clean();
}

NAOS_TEST(a_positional_of_every_value_type_reads_the_same_through_both_ways)
{
    word_log_t log;
    argx_command_t *root = create_root("tool");
    argx_type_t *color_type = create_color_type();
    argx_type_t *word_type = create_word_type(&log);
    argx_positional_t *text =
        add_positional(root, "TEXT", argx_type_string(), ARGX_ARITY_REQUIRED, NULL);
    argx_positional_t *num = add_positional(root, "NUM", argx_type_int(), ARGX_ARITY_REQUIRED, NULL);
    argx_positional_t *ratio =
        add_positional(root, "RATIO", argx_type_float(), ARGX_ARITY_REQUIRED, NULL);
    argx_positional_t *color = add_positional(root, "COLOR", color_type, ARGX_ARITY_REQUIRED, NULL);
    argx_positional_t *word = add_positional(root, "WORD", word_type, ARGX_ARITY_OPTIONAL, NULL);
    char *argv[] = { "tool", "as written", "-7", "-0.25", "blue", "two words" };
    argx_result_t *result;

    validate(root);
    result = parse_ok(root, ARGC(argv), argv);

    check_positional_given(result, text, 1);
    check_positional_string_at(result, text, 0, "as written");
    check_positional_given(result, num, 1);
    check_positional_int_at(result, num, 0, -7);
    check_positional_given(result, ratio, 1);
    check_positional_float_at(result, ratio, 0, -0.25);
    check_positional_given(result, color, 1);
    check_positional_choice_at(result, color, 0, 2);
    check_positional_given(result, word, 1);
    check_word(argx_result_positional_value(result, word, 0), "two words");

    argx_result_destroy(result);
    argx_command_destroy(root);
    argx_type_destroy(word_type);
    argx_type_destroy(color_type);
    argx_test_allocator_uninstall();
    NAOS_CHECK_UINT_EQ(log.released, log.converted);
    check_clean();
}

/*
 * One subcommand for each value type, with a positional of that type that
 * takes any number of values.
 */
NAOS_TEST(a_positional_that_takes_several_values_of_every_value_type_reads_each_in_its_order)
{
    word_log_t log;
    argx_command_t *root = create_root("tool");
    argx_type_t *color_type = create_color_type();
    argx_type_t *word_type = create_word_type(&log);
    argx_positional_t *texts = add_positional(add_subcommand(root, "texts"), "ITEM", argx_type_string(),
                                              ARGX_ARITY_ZERO_OR_MORE, NULL);
    argx_positional_t *nums = add_positional(add_subcommand(root, "nums"), "ITEM", argx_type_int(),
                                             ARGX_ARITY_ONE_OR_MORE, NULL);
    argx_positional_t *ratios = add_positional(add_subcommand(root, "ratios"), "ITEM", argx_type_float(),
                                               ARGX_ARITY_ZERO_OR_MORE, NULL);
    argx_positional_t *colors = add_positional(add_subcommand(root, "colors"), "ITEM", color_type,
                                               ARGX_ARITY_ONE_OR_MORE, NULL);
    argx_positional_t *words =
        add_positional(add_subcommand(root, "words"), "ITEM", word_type, ARGX_ARITY_ZERO_OR_MORE, NULL);
    char *text_argv[] = { "tool", "texts", "b", "", "a", "b" };
    char *num_argv[] = { "tool", "nums", "3", "-1", "+2" };
    char *ratio_argv[] = { "tool", "ratios", "1.5", "-2e3" };
    char *color_argv[] = { "tool", "colors", "blue", "red", "blue", "green" };
    char *word_argv[] = { "tool", "words", "x", "y z", "x" };
    const char *string = "untouched";
    int64_t number = 77;
    double real = 7.5;
    size_t index = 99;
    size_t converted;
    argx_result_t *result;

    validate(root);

    result = parse_ok(root, ARGC(text_argv), text_argv);
    check_positional_given(result, texts, 4);
    check_positional_string_at(result, texts, 0, "b");
    check_positional_string_at(result, texts, 1, "");
    check_positional_string_at(result, texts, 2, "a");
    check_positional_string_at(result, texts, 3, "b");
    NAOS_CHECK_FALSE(argx_result_positional_string(result, texts, 4, &string));
    NAOS_CHECK_STR_EQ(string, "untouched");
    argx_result_destroy(result);

    result = parse_ok(root, ARGC(num_argv), num_argv);
    check_positional_given(result, nums, 3);
    check_positional_int_at(result, nums, 0, 3);
    check_positional_int_at(result, nums, 1, -1);
    check_positional_int_at(result, nums, 2, 2);
    NAOS_CHECK_FALSE(argx_result_positional_int(result, nums, 3, &number));
    NAOS_CHECK_TRUE(number == 77);
    argx_result_destroy(result);

    result = parse_ok(root, ARGC(ratio_argv), ratio_argv);
    check_positional_given(result, ratios, 2);
    check_positional_float_at(result, ratios, 0, 1.5);
    check_positional_float_at(result, ratios, 1, -2000.0);
    NAOS_CHECK_FALSE(argx_result_positional_float(result, ratios, 2, &real));
    NAOS_CHECK_TRUE(real == 7.5);
    argx_result_destroy(result);

    result = parse_ok(root, ARGC(color_argv), color_argv);
    check_positional_given(result, colors, 4);
    check_positional_choice_at(result, colors, 0, 2);
    check_positional_choice_at(result, colors, 1, 0);
    check_positional_choice_at(result, colors, 2, 2);
    check_positional_choice_at(result, colors, 3, 1);
    NAOS_CHECK_FALSE(argx_result_positional_choice(result, colors, 4, &index));
    NAOS_CHECK_UINT_EQ(index, 99);
    argx_result_destroy(result);

    converted = log.converted;
    result = parse_ok(root, ARGC(word_argv), word_argv);
    check_positional_given(result, words, 3);
    check_word(argx_result_positional_value(result, words, 0), "x");
    check_word(argx_result_positional_value(result, words, 1), "y z");
    check_word(argx_result_positional_value(result, words, 2), "x");
    /* Each value has its own area. */
    NAOS_CHECK_TRUE(argx_result_positional_value(result, words, 0) !=
                    argx_result_positional_value(result, words, 2));
    NAOS_CHECK_UINT_EQ(log.converted - converted, 3);
    argx_result_destroy(result);
    NAOS_CHECK_UINT_EQ(log.released, log.converted);

    argx_command_destroy(root);
    argx_type_destroy(word_type);
    argx_type_destroy(color_type);
    argx_test_allocator_uninstall();
    check_clean();
}

/* ---- The text that a value type is given --------------------------------- */

NAOS_TEST(a_value_type_is_given_the_text_as_it_was_written)
{
    word_log_t log;
    argx_command_t *root = create_root("tool");
    argx_type_t *word_type = create_word_type(&log);
    argx_option_t *word = add_value(root, 'w', "word", word_type, NULL);
    argx_option_t *text = add_value(root, 't', "text", argx_type_string(), NULL);
    argx_positional_t *items = add_positional(root, "ITEM", word_type, ARGX_ARITY_ZERO_OR_MORE, NULL);
    char *separate[] = { "tool", "--text", "  both  ", "--word", " lead" };
    char *attached[] = { "tool", "--text= ", "--word=trail ", "-t\ttab\n" };
    char *bundled[] = { "tool", "-w lead and trail " };
    char *plain[] = { "tool", " lead", "trail ", " ", "\ttab\n", "" };
    argx_result_t *result;

    validate(root);

    result = parse_ok(root, ARGC(separate), separate);
    check_option_string(result, text, "  both  ", ARGX_SOURCE_COMMAND_LINE);
    check_word(argx_result_option_value(result, word), " lead");
    NAOS_CHECK_STR_EQ(log.last_text, " lead");
    argx_result_destroy(result);

    result = parse_ok(root, ARGC(attached), attached);
    check_option_string(result, text, "\ttab\n", ARGX_SOURCE_COMMAND_LINE);
    check_word(argx_result_option_value(result, word), "trail ");
    NAOS_CHECK_STR_EQ(log.last_text, "trail ");
    argx_result_destroy(result);

    result = parse_ok(root, ARGC(bundled), bundled);
    check_word(argx_result_option_value(result, word), " lead and trail ");
    NAOS_CHECK_STR_EQ(log.last_text, " lead and trail ");
    argx_result_destroy(result);

    result = parse_ok(root, ARGC(plain), plain);
    check_positional_given(result, items, 5);
    check_word(argx_result_positional_value(result, items, 0), " lead");
    check_word(argx_result_positional_value(result, items, 1), "trail ");
    check_word(argx_result_positional_value(result, items, 2), " ");
    check_word(argx_result_positional_value(result, items, 3), "\ttab\n");
    check_word(argx_result_positional_value(result, items, 4), "");
    NAOS_CHECK_STR_EQ(log.last_text, "");
    argx_result_destroy(result);

    argx_command_destroy(root);
    argx_type_destroy(word_type);
    argx_test_allocator_uninstall();
    NAOS_CHECK_UINT_EQ(log.released, log.converted);
    check_clean();
}

/*
 * The built-in value types take no white space. A value with a space in
 * front or behind is not trimmed for them, so they do not take it.
 */
NAOS_TEST(white_space_around_a_value_is_not_removed_for_its_value_type)
{
    argx_command_t *root = create_root("tool");
    argx_type_t *color_type = create_color_type();
    argx_option_t *num = add_value(root, 'n', "num", argx_type_int(), NULL);
    argx_option_t *ratio = add_value(root, 'r', "ratio", argx_type_float(), NULL);
    argx_option_t *color = add_value(root, 'c', "color", color_type, NULL);
    argx_positional_t *count = add_positional(root, "COUNT", argx_type_int(), ARGX_ARITY_OPTIONAL, NULL);
    char *lead[] = { "tool", "--num", " 5" };
    char *trail[] = { "tool", "--num=5 " };
    char *real[] = { "tool", "-r 1.5" };
    char *choice[] = { "tool", "--color", "red " };
    char *plain[] = { "tool", " 5" };
    char *exact[] = { "tool", "--num", "5", "-r1.5", "--color=red", "5" };
    int64_t number = 77;
    argx_result_t *result;

    validate(root);

    result = parse_bad(root, ARGC(lead), lead, ARGX_PARSE_ERROR_INVALID_VALUE,
                       "invalid value ' 5' for option '--num' (expected integer)");
    NAOS_REQUIRE_TRUE(argx_result_error_value(result) != NULL);
    NAOS_CHECK_STR_EQ(argx_result_error_value(result), " 5");
    NAOS_CHECK_TRUE(argx_result_error_option(result) == num);
    NAOS_CHECK_FALSE(argx_result_option_int(result, num, &number));
    NAOS_CHECK_TRUE(number == 77);
    argx_result_destroy(result);

    result = parse_bad(root, ARGC(trail), trail, ARGX_PARSE_ERROR_INVALID_VALUE,
                       "invalid value '5 ' for option '--num' (expected integer)");
    NAOS_REQUIRE_TRUE(argx_result_error_value(result) != NULL);
    NAOS_CHECK_STR_EQ(argx_result_error_value(result), "5 ");
    argx_result_destroy(result);

    result = parse_bad(root, ARGC(real), real, ARGX_PARSE_ERROR_INVALID_VALUE,
                       "invalid value ' 1.5' for option '-r' (expected float)");
    NAOS_CHECK_TRUE(argx_result_error_option(result) == ratio);
    argx_result_destroy(result);

    result = parse_bad(root, ARGC(choice), choice, ARGX_PARSE_ERROR_INVALID_VALUE,
                       "invalid value 'red ' for option '--color' (expected choice)");
    NAOS_CHECK_TRUE(argx_result_error_option(result) == color);
    argx_result_destroy(result);

    result = parse_bad(root, ARGC(plain), plain, ARGX_PARSE_ERROR_INVALID_VALUE,
                       "invalid value ' 5' for argument 'COUNT' (expected integer)");
    NAOS_CHECK_TRUE(argx_result_error_positional(result) == count);
    NAOS_CHECK_UINT_EQ(argx_result_positional_count(result, count), 0);
    argx_result_destroy(result);

    /* The same values without the spaces are taken. */
    result = parse_ok(root, ARGC(exact), exact);
    check_option_int(result, num, 5, ARGX_SOURCE_COMMAND_LINE);
    check_positional_int_at(result, count, 0, 5);
    argx_result_destroy(result);

    argx_command_destroy(root);
    argx_type_destroy(color_type);
    argx_test_allocator_uninstall();
    check_clean();
}

/* ---- The cleanup of the values ------------------------------------------ */

/*
 * A tool whose values are all of the value type "word": the option --word,
 * the option --late, whose default value is "bad", and the positional ITEM,
 * which takes any number of values. The conversion takes "bad" until
 * log.failure is set, which is done after the validation.
 */
typedef struct word_fixture
{
    word_log_t log;
    argx_type_t *type;
    argx_command_t *root;
    argx_option_t *word;
    argx_option_t *late;
    argx_positional_t *items;
    size_t converted; /* log.converted after the validation */
} word_fixture_t;

static void set_up_words(word_fixture_t *fixture)
{
    fixture->root = create_root("tool");
    fixture->type = create_word_type(&fixture->log);
    fixture->word = add_value(fixture->root, 'w', "word", fixture->type, NULL);
    fixture->late = add_value(fixture->root, 'l', "late", fixture->type, "bad");
    fixture->items = add_positional(fixture->root, "ITEM", fixture->type, ARGX_ARITY_ZERO_OR_MORE, NULL);
    validate(fixture->root);
    /* The validation converted the default value and cleaned it up. */
    NAOS_CHECK_UINT_EQ(fixture->log.released, fixture->log.converted);
    fixture->converted = fixture->log.converted;
}

/*
 * The number of conversions that succeeded since the validation, and the
 * number of those values that were not cleaned up yet.
 */
static size_t words_converted(const word_fixture_t *fixture)
{
    return fixture->log.converted - fixture->converted;
}

static size_t words_held(const word_fixture_t *fixture)
{
    return fixture->log.converted - fixture->log.released;
}

static void tear_down_words(word_fixture_t *fixture)
{
    argx_command_destroy(fixture->root);
    argx_type_destroy(fixture->type);
    argx_test_allocator_uninstall();
    NAOS_CHECK_UINT_EQ(fixture->log.released, fixture->log.converted);
    check_clean();
}

NAOS_TEST(a_value_that_was_replaced_is_cleaned_up_once_and_the_last_one_with_the_result)
{
    word_fixture_t fixture;
    char *argv[] = { "tool", "--word", "a", "--late=l", "--word", "b", "-wc" };
    argx_result_t *result;

    set_up_words(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);

    /*
     * Four conversions; "a" and then "b" were replaced and cleaned up, and
     * the values of --word and --late are held.
     */
    NAOS_CHECK_UINT_EQ(words_converted(&fixture), 4);
    NAOS_CHECK_UINT_EQ(words_held(&fixture), 2);
    NAOS_CHECK_STR_EQ(fixture.log.last_released, "b");
    check_word(argx_result_option_value(result, fixture.word), "c");
    check_word(argx_result_option_value(result, fixture.late), "l");

    argx_result_destroy(result);
    NAOS_CHECK_UINT_EQ(words_converted(&fixture), 4);
    NAOS_CHECK_UINT_EQ(words_held(&fixture), 0);

    tear_down_words(&fixture);
}

NAOS_TEST(the_values_of_a_parse_that_ends_in_a_parse_error_are_cleaned_up_with_the_result)
{
    word_fixture_t fixture;
    char *argv[] = { "tool", "--word", "a", "x", "--word=b", "y", "--bogus", "z" };
    argx_result_t *result;

    set_up_words(&fixture);
    result = parse_bad(fixture.root, ARGC(argv), argv, ARGX_PARSE_ERROR_UNKNOWN_OPTION,
                       "unknown option '--bogus'");

    /*
     * Four conversions before the error and none after it; "a" was
     * replaced. No default value was converted.
     */
    NAOS_CHECK_UINT_EQ(words_converted(&fixture), 4);
    NAOS_CHECK_UINT_EQ(words_held(&fixture), 3);
    NAOS_CHECK_STR_EQ(fixture.log.last_released, "a");
    check_word(argx_result_option_value(result, fixture.word), "b");
    NAOS_CHECK_UINT_EQ(argx_result_positional_count(result, fixture.items), 2);
    check_word(argx_result_positional_value(result, fixture.items, 0), "x");
    check_word(argx_result_positional_value(result, fixture.items, 1), "y");
    NAOS_CHECK_TRUE(argx_result_option_value(result, fixture.late) == NULL);

    argx_result_destroy(result);
    NAOS_CHECK_UINT_EQ(words_converted(&fixture), 4);
    NAOS_CHECK_UINT_EQ(words_held(&fixture), 0);

    tear_down_words(&fixture);
}

NAOS_TEST(a_default_value_is_converted_once_and_cleaned_up_with_the_result)
{
    word_fixture_t fixture;
    char *argv[] = { "tool", "x" };
    argx_result_t *result;

    set_up_words(&fixture);
    result = parse_ok(fixture.root, ARGC(argv), argv);

    /* The value of ITEM and the default value of --late. */
    NAOS_CHECK_UINT_EQ(words_converted(&fixture), 2);
    NAOS_CHECK_UINT_EQ(words_held(&fixture), 2);
    NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.late), ARGX_SOURCE_DEFAULT);
    check_word(argx_result_option_value(result, fixture.late), "bad");
    NAOS_CHECK_STR_EQ(fixture.log.last_text, "bad");
    NAOS_CHECK_TRUE(argx_result_option_value(result, fixture.word) == NULL);

    argx_result_destroy(result);
    NAOS_CHECK_UINT_EQ(words_converted(&fixture), 2);
    NAOS_CHECK_UINT_EQ(words_held(&fixture), 0);

    /* A default value that was not applied is not converted. */
    {
        char *given[] = { "tool", "--late", "l" };

        result = parse_ok(fixture.root, ARGC(given), given);
        NAOS_CHECK_UINT_EQ(words_converted(&fixture), 3);
        NAOS_CHECK_INT_EQ(argx_result_option_source(result, fixture.late), ARGX_SOURCE_COMMAND_LINE);
        argx_result_destroy(result);
        NAOS_CHECK_UINT_EQ(words_held(&fixture), 0);
    }

    tear_down_words(&fixture);
}

/* ---- What a conversion that failed returns ------------------------------ */

/*
 * A conversion that fails with failure, which is not the lack of memory,
 * is the parse error of a value that its value type does not take: for the
 * value of an option, of a positional and for a default value.
 */
static void check_failed_conversion_is_a_parse_error(argx_error_t failure)
{
    word_fixture_t fixture;
    char *option[] = { "tool", "--word", "a", "x", "--word", "bad", "y" };
    char *positional[] = { "tool", "--word", "a", "x", "bad", "y" };
    char *fallback[] = { "tool", "--word", "a", "x" };
    char *good[] = { "tool", "--word", "a", "x", "--late", "l" };
    argx_result_t *result;

    set_up_words(&fixture);
    fixture.log.failure = failure;

    result = parse_bad(fixture.root, ARGC(option), option, ARGX_PARSE_ERROR_INVALID_VALUE,
                       "invalid value 'bad' for option '--word' (expected word)");
    NAOS_CHECK_TRUE(argx_result_error_option(result) == fixture.word);
    NAOS_CHECK_TRUE(argx_result_error_positional(result) == NULL);
    NAOS_CHECK_INT_EQ(argx_result_error_source(result), ARGX_SOURCE_COMMAND_LINE);
    NAOS_CHECK_UINT_EQ(argx_result_error_index(result), 5);
    NAOS_REQUIRE_TRUE(argx_result_error_value(result) != NULL);
    NAOS_CHECK_STR_EQ(argx_result_error_value(result), "bad");
    /* The value that was there stays, and the parse stopped. */
    check_word(argx_result_option_value(result, fixture.word), "a");
    NAOS_CHECK_UINT_EQ(argx_result_positional_count(result, fixture.items), 1);
    NAOS_CHECK_UINT_EQ(words_converted(&fixture), 2);
    NAOS_CHECK_UINT_EQ(words_held(&fixture), 2);
    argx_result_destroy(result);
    NAOS_CHECK_UINT_EQ(words_held(&fixture), 0);

    result = parse_bad(fixture.root, ARGC(positional), positional, ARGX_PARSE_ERROR_INVALID_VALUE,
                       "invalid value 'bad' for argument 'ITEM' (expected word)");
    NAOS_CHECK_TRUE(argx_result_error_positional(result) == fixture.items);
    NAOS_CHECK_TRUE(argx_result_error_option(result) == NULL);
    NAOS_CHECK_UINT_EQ(argx_result_error_index(result), 4);
    NAOS_CHECK_UINT_EQ(argx_result_positional_count(result, fixture.items), 1);
    check_word(argx_result_positional_value(result, fixture.items, 0), "x");
    NAOS_CHECK_UINT_EQ(words_converted(&fixture), 4);
    NAOS_CHECK_UINT_EQ(words_held(&fixture), 2);
    argx_result_destroy(result);
    NAOS_CHECK_UINT_EQ(words_held(&fixture), 0);

    result = parse_bad(fixture.root, ARGC(fallback), fallback, ARGX_PARSE_ERROR_INVALID_VALUE,
                       "invalid default value 'bad' for option '--late' (expected word)");
    NAOS_CHECK_TRUE(argx_result_error_option(result) == fixture.late);
    NAOS_CHECK_INT_EQ(argx_result_error_source(result), ARGX_SOURCE_DEFAULT);
    NAOS_CHECK_TRUE(argx_result_option_value(result, fixture.late) == NULL);
    check_word(argx_result_option_value(result, fixture.word), "a");
    NAOS_CHECK_UINT_EQ(words_converted(&fixture), 6);
    NAOS_CHECK_UINT_EQ(words_held(&fixture), 2);
    argx_result_destroy(result);
    NAOS_CHECK_UINT_EQ(words_held(&fixture), 0);

    /*
     * Without the text that the value type does not take, the parse
     * succeeds.
     */
    result = parse_ok(fixture.root, ARGC(good), good);
    check_word(argx_result_option_value(result, fixture.late), "l");
    argx_result_destroy(result);

    tear_down_words(&fixture);
}

NAOS_TEST(a_conversion_that_reports_an_invalid_value_is_a_parse_error_with_a_result)
{
    check_failed_conversion_is_a_parse_error(ARGX_ERROR_INVALID_VALUE);
}

NAOS_TEST(a_conversion_that_reports_any_other_code_is_a_parse_error_with_a_result)
{
    check_failed_conversion_is_a_parse_error(ARGX_ERROR_NAME_CONFLICT);
    check_failed_conversion_is_a_parse_error(ARGX_ERROR_INVALID_COMMAND_LINE);
}

NAOS_TEST(a_conversion_that_reports_a_lack_of_memory_fails_the_parse_without_a_result)
{
    word_fixture_t fixture;
    char *option[] = { "tool", "--word", "a", "x", "--word", "bad", "y" };
    char *positional[] = { "tool", "--word", "a", "x", "bad", "y" };
    char *fallback[] = { "tool", "--word", "a", "x" };
    char *good[] = { "tool", "--word", "a", "x", "--late", "l" };
    char *const *argvs[3];
    int argcs[3];
    size_t expected[3];
    argx_result_t *other;
    size_t live;
    size_t i;

    argvs[0] = option;
    argcs[0] = ARGC(option);
    expected[0] = 2;
    argvs[1] = positional;
    argcs[1] = ARGC(positional);
    expected[1] = 2;
    argvs[2] = fallback;
    argcs[2] = ARGC(fallback);
    expected[2] = 2;

    set_up_words(&fixture);
    fixture.log.failure = ARGX_ERROR_OUT_OF_MEMORY;

    /* A result that is alive, to have a pointer that is not NULL. */
    other = parse_ok(fixture.root, ARGC(good), good);
    live = argx_test_allocator_live();

    for (i = 0; i < 3; i++)
    {
        size_t converted = fixture.log.converted;
        size_t held = words_held(&fixture);
        argx_result_t *result = other;
        argx_error_t error = argx_parse(fixture.root, argcs[i], argvs[i], &result);

        NAOS_CHECK_INT_EQ(error, ARGX_ERROR_OUT_OF_MEMORY);
        /* The place of the result was written to. */
        NAOS_CHECK_TRUE(result == NULL);
        /*
         * The values that were converted before the failure were cleaned
         * up, and nothing of the parse stays allocated.
         */
        NAOS_CHECK_STR_EQ(fixture.log.last_text, "bad");
        NAOS_CHECK_UINT_EQ(fixture.log.converted - converted, expected[i]);
        NAOS_CHECK_UINT_EQ(words_held(&fixture), held);
        NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), live);
    }

    /* The result of the other parse is as it was. */
    NAOS_CHECK_INT_EQ(argx_result_error(other), ARGX_ERROR_NONE);
    check_word(argx_result_option_value(other, fixture.word), "a");
    check_word(argx_result_option_value(other, fixture.late), "l");
    check_word(argx_result_positional_value(other, fixture.items, 0), "x");
    argx_result_destroy(other);
    NAOS_CHECK_UINT_EQ(words_held(&fixture), 0);

    tear_down_words(&fixture);
}

NAOS_SUITE(defaults,
    NAOS_ADD_TEST(an_option_and_a_positional_that_were_not_given_have_their_default_values),
    NAOS_ADD_TEST(what_was_given_on_the_command_line_is_the_value_and_not_the_default),
    NAOS_ADD_TEST(an_option_and_a_positional_without_a_default_that_were_not_given_have_no_value),
    NAOS_ADD_TEST(a_positional_that_takes_any_number_has_no_value_when_none_was_given),
    NAOS_ADD_TEST(the_options_that_take_no_value_read_as_their_initial_values_when_they_were_not_given)
)

NAOS_SUITE(required,
    NAOS_ADD_TEST(a_required_option_that_was_not_given_is_a_parse_error),
    NAOS_ADD_TEST(a_missing_required_option_without_a_long_name_is_named_by_its_short_name),
    NAOS_ADD_TEST(a_required_positional_that_was_not_given_is_a_parse_error),
    NAOS_ADD_TEST(a_positional_that_takes_one_or_more_without_a_value_is_a_parse_error),
    NAOS_ADD_TEST(the_first_missing_required_thing_is_reported)
)

NAOS_SUITE(commands,
    NAOS_ADD_TEST(a_command_that_was_not_selected_is_not_checked_and_gets_no_default_value),
    NAOS_ADD_TEST(every_selected_command_is_checked_and_gets_its_default_values)
)

NAOS_SUITE(parse_errors,
    NAOS_ADD_TEST(a_result_with_a_parse_error_has_no_default_value),
    NAOS_ADD_TEST(a_default_value_that_its_type_does_not_take_is_a_parse_error_without_default_values)
)

NAOS_SUITE(memory,
    NAOS_ADD_TEST(a_parse_that_cannot_allocate_a_default_value_returns_no_result_and_leaves_nothing_allocated)
)

NAOS_SUITE(value_types,
    NAOS_ADD_TEST(an_option_of_every_value_type_reads_the_same_through_both_ways),
    NAOS_ADD_TEST(a_positional_of_every_value_type_reads_the_same_through_both_ways),
    NAOS_ADD_TEST(a_positional_that_takes_several_values_of_every_value_type_reads_each_in_its_order),
    NAOS_ADD_TEST(a_value_type_is_given_the_text_as_it_was_written),
    NAOS_ADD_TEST(white_space_around_a_value_is_not_removed_for_its_value_type)
)

NAOS_SUITE(cleanup,
    NAOS_ADD_TEST(a_value_that_was_replaced_is_cleaned_up_once_and_the_last_one_with_the_result),
    NAOS_ADD_TEST(the_values_of_a_parse_that_ends_in_a_parse_error_are_cleaned_up_with_the_result),
    NAOS_ADD_TEST(a_default_value_is_converted_once_and_cleaned_up_with_the_result)
)

NAOS_SUITE(failed_conversions,
    NAOS_ADD_TEST(a_conversion_that_reports_an_invalid_value_is_a_parse_error_with_a_result),
    NAOS_ADD_TEST(a_conversion_that_reports_any_other_code_is_a_parse_error_with_a_result),
    NAOS_ADD_TEST(a_conversion_that_reports_a_lack_of_memory_fails_the_parse_without_a_result)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(defaults),
    NAOS_ADD_SUITE(required),
    NAOS_ADD_SUITE(commands),
    NAOS_ADD_SUITE(parse_errors),
    NAOS_ADD_SUITE(memory),
    NAOS_ADD_SUITE(value_types),
    NAOS_ADD_SUITE(cleanup),
    NAOS_ADD_SUITE(failed_conversions)
)
