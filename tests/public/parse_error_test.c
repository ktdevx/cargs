/*
 * Tests of the public parsing API for the parse errors: what a result of
 * cline_parse() of <cline/parse.h> tells about a command line that does
 * not match the schema, read back with the cline_result_error_*()
 * functions.
 *
 * This is the one place where every kind of parse error is checked field by
 * field: what the parse returns, cline_result_error(), the kind, the
 * argument, the value, the index, the source, the command, the option, the
 * positional, the letter of a bundle and the text of the message. The other
 * tests of the parsing check the kind and the message where they need them.
 * The error of a wide argument that cannot be converted needs
 * cline_parse_wide() and is not here.
 *
 * The tests also cover which of several errors is reported, what a result
 * with a parse error still holds, what a result without one returns from
 * the functions that read an error, and that the strings of an error do not
 * point into the argv of the caller.
 *
 * Every test uses the test allocator ("../support/test_allocator.h"). Its
 * state and the allocator that is set are shared by all the tests of this
 * executable. A test starts with an install, destroys what it created, sets
 * the default allocator back before it ends, and ends with check_clean().
 */
#include <cline/cline.h>

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
    NAOS_CHECK_UINT_EQ(cline_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_violations(), 0);
}

/* ---- Building a schema --------------------------------------------------- */

/* Installs the test allocator and creates the root of a schema. */
static cline_command_t *create_root(const char *name)
{
    cline_command_t *root = NULL;

    cline_test_allocator_install();
    NAOS_REQUIRE_TRUE(cline_command_create(name, &root) == CLINE_ERROR_NONE);
    NAOS_REQUIRE_TRUE(root != NULL);
    return root;
}

static cline_command_t *add_subcommand(cline_command_t *command, const char *name)
{
    cline_command_t *subcommand = NULL;

    NAOS_REQUIRE_TRUE(cline_command_add_subcommand(command, name, &subcommand) == CLINE_ERROR_NONE);
    NAOS_REQUIRE_TRUE(subcommand != NULL);
    return subcommand;
}

static cline_option_t *add_flag(cline_command_t *command, char short_name, const char *long_name)
{
    cline_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(cline_command_add_flag(command, short_name, long_name, &option) == CLINE_ERROR_NONE);
    NAOS_REQUIRE_TRUE(option != NULL);
    return option;
}

static cline_option_t *add_count(cline_command_t *command, char short_name, const char *long_name)
{
    cline_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(cline_command_add_count(command, short_name, long_name, &option) == CLINE_ERROR_NONE);
    NAOS_REQUIRE_TRUE(option != NULL);
    return option;
}

/* Adds a negatable flag with the initial state initial to command. */
static cline_option_t *add_negatable(cline_command_t *command, char short_name, const char *long_name,
                                      bool initial)
{
    cline_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(cline_command_add_negatable(command, short_name, long_name, &option) ==
                      CLINE_ERROR_NONE);
    NAOS_REQUIRE_TRUE(option != NULL);
    NAOS_REQUIRE_TRUE(cline_option_set_initial(option, initial) == CLINE_ERROR_NONE);
    return option;
}

/*
 * Adds an option that takes a value to command. default_value may be
 * NULL: the option has no default value then.
 */
static cline_option_t *add_value(cline_command_t *command, char short_name, const char *long_name,
                                  const cline_type_t *type, const char *default_value)
{
    cline_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(cline_command_add_value(command, short_name, long_name, type, &option) ==
                      CLINE_ERROR_NONE);
    NAOS_REQUIRE_TRUE(option != NULL);
    if (default_value != NULL)
    {
        NAOS_REQUIRE_TRUE(cline_option_set_default(option, default_value) == CLINE_ERROR_NONE);
    }
    return option;
}

/* Adds an option that takes a value and must be given to command. */
static cline_option_t *add_required(cline_command_t *command, char short_name, const char *long_name,
                                     const cline_type_t *type)
{
    cline_option_t *option = add_value(command, short_name, long_name, type, NULL);

    NAOS_REQUIRE_TRUE(cline_option_set_required(option, true) == CLINE_ERROR_NONE);
    return option;
}

/*
 * Adds a positional to command. default_value may be NULL: the positional
 * has no default value then.
 */
static cline_positional_t *add_positional(cline_command_t *command, const char *name,
                                           const cline_type_t *type, cline_arity_t arity,
                                           const char *default_value)
{
    cline_positional_t *positional = NULL;

    NAOS_REQUIRE_TRUE(cline_command_add_positional(command, name, type, arity, &positional) ==
                      CLINE_ERROR_NONE);
    NAOS_REQUIRE_TRUE(positional != NULL);
    if (default_value != NULL)
    {
        NAOS_REQUIRE_TRUE(cline_positional_set_default(positional, default_value) == CLINE_ERROR_NONE);
    }
    return positional;
}

static void validate(cline_command_t *root)
{
    NAOS_REQUIRE_TRUE(cline_command_validate(root, NULL, NULL, NULL) == CLINE_ERROR_NONE);
}

/* Destroys the schema and ends the test: nothing stays allocated. */
static void destroy_root(cline_command_t *root)
{
    cline_command_destroy(root);
    cline_test_allocator_uninstall();
    check_clean();
}

/* ---- What a parse error is expected to be -------------------------------- */

/*
 * Every field of a parse error. A string that is NULL is expected to read
 * as NULL.
 */
typedef struct expected
{
    cline_parse_error_kind_t kind;
    const char *argument;
    const char *value;
    size_t index;
    cline_source_t source;
    const cline_command_t *command;
    const cline_option_t *option;
    const cline_positional_t *positional;
    char short_name;
    const char *message;
} expected_t;

/*
 * An error of the kind at the argument with the index of the command line,
 * found while command was the current command. It has no value, no option,
 * no positional and no letter: a test sets the ones that its kind has.
 */
static expected_t on_command_line(cline_parse_error_kind_t kind, const char *argument, size_t index,
                                  const cline_command_t *command, const char *message)
{
    expected_t expected;

    expected.kind = kind;
    expected.argument = argument;
    expected.value = NULL;
    expected.index = index;
    expected.source = CLINE_SOURCE_COMMAND_LINE;
    expected.command = command;
    expected.option = NULL;
    expected.positional = NULL;
    expected.short_name = '\0';
    expected.message = message;
    return expected;
}

/*
 * An error of the kind that is not at an argument: its source is source,
 * CLINE_SOURCE_NONE for something required that is missing and
 * CLINE_SOURCE_DEFAULT for a default value, and command is the command of
 * the option or the positional, which a test sets.
 */
static expected_t off_command_line(cline_parse_error_kind_t kind, cline_source_t source,
                                   const cline_command_t *command, const char *message)
{
    expected_t expected = on_command_line(kind, NULL, 0, command, message);

    expected.source = source;
    return expected;
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

/* Checks every field of the parse error of result. */
static void check_error(const cline_result_t *result, const expected_t *expected)
{
    NAOS_CHECK_INT_EQ(cline_result_error(result), CLINE_ERROR_INVALID_COMMAND_LINE);
    NAOS_CHECK_INT_EQ(cline_result_error_kind(result), expected->kind);
    check_text(cline_result_error_argument(result), expected->argument);
    check_text(cline_result_error_value(result), expected->value);
    NAOS_CHECK_UINT_EQ(cline_result_error_index(result), expected->index);
    NAOS_CHECK_INT_EQ(cline_result_error_source(result), expected->source);
    NAOS_CHECK_TRUE(cline_result_error_command(result) == expected->command);
    NAOS_CHECK_TRUE(cline_result_error_option(result) == expected->option);
    NAOS_CHECK_TRUE(cline_result_error_positional(result) == expected->positional);
    NAOS_CHECK_INT_EQ(cline_result_error_short_name(result), expected->short_name);
    check_text(cline_result_error_message(result), expected->message);
}

/*
 * Parses argv, which must have the parse error expected, and checks every
 * field of the error. Returns the result, which the caller destroys.
 */
static cline_result_t *parse_error(const cline_command_t *root, int argc, char *const *argv,
                                    const expected_t *expected)
{
    cline_result_t *result = NULL;

    NAOS_REQUIRE_TRUE(cline_parse(root, argc, argv, &result) == CLINE_ERROR_INVALID_COMMAND_LINE);
    NAOS_REQUIRE_TRUE(result != NULL);
    check_error(result, expected);
    return result;
}

/* The same for a test that reads nothing else from the result. */
static void expect_error(const cline_command_t *root, int argc, char *const *argv,
                         const expected_t *expected)
{
    cline_result_destroy(parse_error(root, argc, argv, expected));
}

/*
 * Parses argv, which must match the schema. Returns the result, which the
 * caller destroys.
 */
static cline_result_t *parse_ok(const cline_command_t *root, int argc, char *const *argv)
{
    cline_result_t *result = NULL;

    NAOS_REQUIRE_TRUE(cline_parse(root, argc, argv, &result) == CLINE_ERROR_NONE);
    NAOS_REQUIRE_TRUE(result != NULL);
    return result;
}

/* ---- The schema of most tests -------------------------------------------- */

/*
 * tool [-v|--verbose] [-d|--debug]... [-c|--color|--no-color] [-n|--num INT]
 *      [--output STRING]
 *   run [-f|--force] [-q|--quiet]... [-k|--cache|--no-cache] [-j|--jobs INT]
 *       [--name STRING] [-h|--help] SRC [COUNT]
 *   status [-a|--all]
 *   remote
 *     add NAME
 *
 * --color is on at first and --cache off. --output has the default value
 * "out.txt" and --name "anon". SRC is required; COUNT is optional and an
 * integer. status has no positional; tool and remote have subcommands.
 */
typedef struct fixture
{
    cline_command_t *root;
    cline_option_t *verbose;
    cline_option_t *debug;
    cline_option_t *color;
    cline_option_t *num;
    cline_option_t *output;
    cline_command_t *run;
    cline_option_t *force;
    cline_option_t *quiet;
    cline_option_t *cache;
    cline_option_t *jobs;
    cline_option_t *name;
    cline_option_t *help;
    cline_positional_t *src;
    cline_positional_t *count;
    cline_command_t *status;
    cline_option_t *all;
    cline_command_t *remote;
    cline_command_t *add;
    cline_positional_t *remote_name;
} fixture_t;

static void set_up(fixture_t *fixture)
{
    fixture->root = create_root("tool");
    fixture->verbose = add_flag(fixture->root, 'v', "verbose");
    fixture->debug = add_count(fixture->root, 'd', "debug");
    fixture->color = add_negatable(fixture->root, 'c', "color", true);
    fixture->num = add_value(fixture->root, 'n', "num", cline_type_int(), NULL);
    fixture->output = add_value(fixture->root, '\0', "output", cline_type_string(), "out.txt");

    fixture->run = add_subcommand(fixture->root, "run");
    fixture->force = add_flag(fixture->run, 'f', "force");
    fixture->quiet = add_count(fixture->run, 'q', "quiet");
    fixture->cache = add_negatable(fixture->run, 'k', "cache", false);
    fixture->jobs = add_value(fixture->run, 'j', "jobs", cline_type_int(), NULL);
    fixture->name = add_value(fixture->run, '\0', "name", cline_type_string(), "anon");
    fixture->help = add_flag(fixture->run, 'h', "help");
    fixture->src = add_positional(fixture->run, "SRC", cline_type_string(), CLINE_ARITY_REQUIRED, NULL);
    fixture->count = add_positional(fixture->run, "COUNT", cline_type_int(), CLINE_ARITY_OPTIONAL, NULL);

    fixture->status = add_subcommand(fixture->root, "status");
    fixture->all = add_flag(fixture->status, 'a', "all");

    fixture->remote = add_subcommand(fixture->root, "remote");
    fixture->add = add_subcommand(fixture->remote, "add");
    fixture->remote_name =
        add_positional(fixture->add, "NAME", cline_type_string(), CLINE_ARITY_REQUIRED, NULL);

    validate(fixture->root);
}

/* ---- The errors of the scan, kind by kind -------------------------------- */

NAOS_TEST(an_unknown_option_has_its_argument_and_no_value)
{
    fixture_t fixture;
    expected_t expected;

    set_up(&fixture);

    /*
     * A long option: the argument is the whole of it, the name of the
     * message ends before the '='.
     */
    {
        char *argv[] = { "tool", "-v", "run", "--bogus=1", "main.c" };

        expected = on_command_line(CLINE_PARSE_ERROR_UNKNOWN_OPTION, "--bogus=1", 3, fixture.run,
                                   "unknown option '--bogus'");
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    /*
     * An option of the parent is unknown after the name of the subcommand:
     * the command of the error is the current command.
     */
    {
        char *argv[] = { "tool", "run", "main.c", "--verbose" };

        expected = on_command_line(CLINE_PARSE_ERROR_UNKNOWN_OPTION, "--verbose", 3, fixture.run,
                                   "unknown option '--verbose'");
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    /*
     * A letter of a bundle: the argument is the whole bundle, and the
     * letter is told apart.
     */
    {
        char *argv[] = { "tool", "-v", "run", "-fqx", "main.c" };

        expected = on_command_line(CLINE_PARSE_ERROR_UNKNOWN_OPTION, "-fqx", 3, fixture.run,
                                   "unknown option '-x'");
        expected.short_name = 'x';
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    /* In the root command. */
    {
        char *argv[] = { "tool", "-d", "-dz", "run" };

        expected = on_command_line(CLINE_PARSE_ERROR_UNKNOWN_OPTION, "-dz", 2, fixture.root,
                                   "unknown option '-z'");
        expected.short_name = 'z';
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }

    destroy_root(fixture.root);
}

NAOS_TEST(an_unknown_subcommand_has_its_argument_as_its_value)
{
    fixture_t fixture;
    expected_t expected;

    set_up(&fixture);

    {
        char *argv[] = { "tool", "-v", "remote", "ad", "origin" };

        expected = on_command_line(CLINE_PARSE_ERROR_UNKNOWN_SUBCOMMAND, "ad", 3, fixture.remote,
                                   "unknown command 'ad'");
        expected.value = "ad";
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    {
        char *argv[] = { "tool", "-v", "-d", "comit" };

        expected = on_command_line(CLINE_PARSE_ERROR_UNKNOWN_SUBCOMMAND, "comit", 3, fixture.root,
                                   "unknown command 'comit'");
        expected.value = "comit";
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }

    destroy_root(fixture.root);
}

NAOS_TEST(a_missing_value_has_the_argument_of_the_option_and_the_option)
{
    fixture_t fixture;
    expected_t expected;

    set_up(&fixture);

    /* A long option that is the last argument. */
    {
        char *argv[] = { "tool", "run", "main.c", "--jobs" };

        expected = on_command_line(CLINE_PARSE_ERROR_MISSING_VALUE, "--jobs", 3, fixture.run,
                                   "option '--jobs' requires a value");
        expected.option = fixture.jobs;
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    /* The last letter of a bundle that is the last argument. */
    {
        char *argv[] = { "tool", "run", "main.c", "-fqj" };

        expected = on_command_line(CLINE_PARSE_ERROR_MISSING_VALUE, "-fqj", 3, fixture.run,
                                   "option '-j' requires a value");
        expected.option = fixture.jobs;
        expected.short_name = 'j';
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    /* In the root command. */
    {
        char *argv[] = { "tool", "-v", "--num" };

        expected = on_command_line(CLINE_PARSE_ERROR_MISSING_VALUE, "--num", 2, fixture.root,
                                   "option '--num' requires a value");
        expected.option = fixture.num;
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }

    destroy_root(fixture.root);
}

NAOS_TEST(an_unexpected_value_has_what_follows_the_equals_sign_and_the_option)
{
    fixture_t fixture;
    expected_t expected;

    set_up(&fixture);

    /* A flag. */
    {
        char *argv[] = { "tool", "run", "main.c", "--force=yes" };

        expected = on_command_line(CLINE_PARSE_ERROR_UNEXPECTED_VALUE, "--force=yes", 3, fixture.run,
                                   "option '--force' does not take a value");
        expected.value = "yes";
        expected.option = fixture.force;
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    /* A count. */
    {
        char *argv[] = { "tool", "-v", "--debug=2", "run" };

        expected = on_command_line(CLINE_PARSE_ERROR_UNEXPECTED_VALUE, "--debug=2", 2, fixture.root,
                                   "option '--debug' does not take a value");
        expected.value = "2";
        expected.option = fixture.debug;
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    /* A negatable flag, by its name. */
    {
        char *argv[] = { "tool", "run", "--cache=on", "main.c" };

        expected = on_command_line(CLINE_PARSE_ERROR_UNEXPECTED_VALUE, "--cache=on", 2, fixture.run,
                                   "option '--cache' does not take a value");
        expected.value = "on";
        expected.option = fixture.cache;
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    /*
     * A negatable flag, by its negated name: the message has the name as
     * it was written.
     */
    {
        char *argv[] = { "tool", "-v", "-d", "--no-color=never", "run" };

        expected = on_command_line(CLINE_PARSE_ERROR_UNEXPECTED_VALUE, "--no-color=never", 3, fixture.root,
                                   "option '--no-color' does not take a value");
        expected.value = "never";
        expected.option = fixture.color;
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    /*
     * An empty value is a value: the empty string, not NULL. Everything
     * after the first '=' is the value.
     */
    {
        char *argv[] = { "tool", "run", "--force=", "main.c" };

        expected = on_command_line(CLINE_PARSE_ERROR_UNEXPECTED_VALUE, "--force=", 2, fixture.run,
                                   "option '--force' does not take a value");
        expected.value = "";
        expected.option = fixture.force;
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    {
        char *argv[] = { "tool", "run", "main.c", "2", "--quiet=a=b" };

        expected = on_command_line(CLINE_PARSE_ERROR_UNEXPECTED_VALUE, "--quiet=a=b", 4, fixture.run,
                                   "option '--quiet' does not take a value");
        expected.value = "a=b";
        expected.option = fixture.quiet;
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }

    destroy_root(fixture.root);
}

NAOS_TEST(an_invalid_value_of_an_option_has_the_argument_that_holds_the_value)
{
    fixture_t fixture;
    expected_t expected;

    set_up(&fixture);

    /* --name=value: the argument is the whole of it. */
    {
        char *argv[] = { "tool", "run", "main.c", "--jobs=x" };

        expected = on_command_line(CLINE_PARSE_ERROR_INVALID_VALUE, "--jobs=x", 3, fixture.run,
                                   "invalid value 'x' for option '--jobs' (expected integer)");
        expected.value = "x";
        expected.option = fixture.jobs;
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    /* --name value: the argument and the index are those of the value. */
    {
        char *argv[] = { "tool", "run", "main.c", "--jobs", "many" };

        expected = on_command_line(CLINE_PARSE_ERROR_INVALID_VALUE, "many", 4, fixture.run,
                                   "invalid value 'many' for option '--jobs' (expected integer)");
        expected.value = "many";
        expected.option = fixture.jobs;
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    /* -nvalue: the argument is the whole of it, the name is the letter. */
    {
        char *argv[] = { "tool", "-v", "-nx", "run" };

        expected = on_command_line(CLINE_PARSE_ERROR_INVALID_VALUE, "-nx", 2, fixture.root,
                                   "invalid value 'x' for option '-n' (expected integer)");
        expected.value = "x";
        expected.option = fixture.num;
        expected.short_name = 'n';
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    /*
     * -n value: the argument and the index are those of the value; the
     * letter is still told.
     */
    {
        char *argv[] = { "tool", "-v", "-n", "x", "run" };

        expected = on_command_line(CLINE_PARSE_ERROR_INVALID_VALUE, "x", 3, fixture.root,
                                   "invalid value 'x' for option '-n' (expected integer)");
        expected.value = "x";
        expected.option = fixture.num;
        expected.short_name = 'n';
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    /*
     * The option that ends a bundle of several, with the value in the
     * bundle and in the next argument.
     */
    {
        char *argv[] = { "tool", "run", "-fqj1.5", "main.c" };

        expected = on_command_line(CLINE_PARSE_ERROR_INVALID_VALUE, "-fqj1.5", 2, fixture.run,
                                   "invalid value '1.5' for option '-j' (expected integer)");
        expected.value = "1.5";
        expected.option = fixture.jobs;
        expected.short_name = 'j';
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    {
        char *argv[] = { "tool", "run", "-fqj", "--force", "main.c" };

        expected = on_command_line(CLINE_PARSE_ERROR_INVALID_VALUE, "--force", 3, fixture.run,
                                   "invalid value '--force' for option '-j' (expected integer)");
        expected.value = "--force";
        expected.option = fixture.jobs;
        expected.short_name = 'j';
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    /* An empty value. */
    {
        char *argv[] = { "tool", "--num=", "run" };

        expected = on_command_line(CLINE_PARSE_ERROR_INVALID_VALUE, "--num=", 1, fixture.root,
                                   "invalid value '' for option '--num' (expected integer)");
        expected.value = "";
        expected.option = fixture.num;
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }

    destroy_root(fixture.root);
}

NAOS_TEST(an_invalid_value_of_a_positional_has_the_positional)
{
    fixture_t fixture;
    expected_t expected;

    set_up(&fixture);

    {
        char *argv[] = { "tool", "run", "main.c", "-f", "ten" };

        expected = on_command_line(CLINE_PARSE_ERROR_INVALID_VALUE, "ten", 4, fixture.run,
                                   "invalid value 'ten' for argument 'COUNT' (expected integer)");
        expected.value = "ten";
        expected.positional = fixture.count;
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    /* After the terminator, where the value looks like an option. */
    {
        char *argv[] = { "tool", "run", "--", "main.c", "--jobs" };

        expected = on_command_line(CLINE_PARSE_ERROR_INVALID_VALUE, "--jobs", 4, fixture.run,
                                   "invalid value '--jobs' for argument 'COUNT' (expected integer)");
        expected.value = "--jobs";
        expected.positional = fixture.count;
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }

    destroy_root(fixture.root);
}

NAOS_TEST(an_unexpected_positional_has_its_argument_as_its_value)
{
    fixture_t fixture;
    expected_t expected;

    set_up(&fixture);

    /* One more than the positionals of the command take. */
    {
        char *argv[] = { "tool", "run", "main.c", "2", "extra" };

        expected = on_command_line(CLINE_PARSE_ERROR_UNEXPECTED_POSITIONAL, "extra", 4, fixture.run,
                                   "unexpected argument 'extra'");
        expected.value = "extra";
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    /* A command that has no positional and no subcommand. */
    {
        char *argv[] = { "tool", "-v", "status", "--all", "now" };

        expected = on_command_line(CLINE_PARSE_ERROR_UNEXPECTED_POSITIONAL, "now", 4, fixture.status,
                                   "unexpected argument 'now'");
        expected.value = "now";
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    /*
     * After the terminator, in a command that has subcommands: the name of
     * a subcommand is not a name there.
     */
    {
        char *argv[] = { "tool", "-v", "--", "run", "main.c" };

        expected = on_command_line(CLINE_PARSE_ERROR_UNEXPECTED_POSITIONAL, "run", 3, fixture.root,
                                   "unexpected argument 'run'");
        expected.value = "run";
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    {
        char *argv[] = { "tool", "remote", "--", "--add" };

        expected = on_command_line(CLINE_PARSE_ERROR_UNEXPECTED_POSITIONAL, "--add", 3, fixture.remote,
                                   "unexpected argument '--add'");
        expected.value = "--add";
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }

    destroy_root(fixture.root);
}

/* ---- What is required and missing ---------------------------------------- */

/*
 * tool --token STRING
 *   push REMOTE --branch STRING -a STRING [--tag STRING]
 *   pull FILES...
 *
 * --token, --branch and -a are required; -a has no long name. REMOTE is
 * required and was added to push before its options. FILES takes one or
 * more.
 */
typedef struct required_fixture
{
    cline_command_t *root;
    cline_option_t *token;
    cline_command_t *push;
    cline_positional_t *remote;
    cline_option_t *branch;
    cline_option_t *area;
    cline_command_t *pull;
    cline_positional_t *files;
} required_fixture_t;

static void set_up_required(required_fixture_t *fixture)
{
    fixture->root = create_root("tool");
    fixture->push = add_subcommand(fixture->root, "push");
    fixture->remote =
        add_positional(fixture->push, "REMOTE", cline_type_string(), CLINE_ARITY_REQUIRED, NULL);
    fixture->branch = add_required(fixture->push, 'b', "branch", cline_type_string());
    fixture->area = add_required(fixture->push, 'a', NULL, cline_type_string());
    (void)add_value(fixture->push, '\0', "tag", cline_type_string(), NULL);
    fixture->pull = add_subcommand(fixture->root, "pull");
    fixture->files =
        add_positional(fixture->pull, "FILES", cline_type_string(), CLINE_ARITY_ONE_OR_MORE, NULL);
    /* The option of the root is added last: the root still comes first. */
    fixture->token = add_required(fixture->root, 't', "token", cline_type_string());
    validate(fixture->root);
}

NAOS_TEST(a_missing_option_has_no_position_and_the_option)
{
    required_fixture_t fixture;
    expected_t expected;

    set_up_required(&fixture);

    /*
     * An option with a long name is named by it, also when it has a short
     * name.
     */
    {
        char *argv[] = { "tool", "-t", "secret", "push", "-a", "x", "origin" };

        expected = off_command_line(CLINE_PARSE_ERROR_MISSING_OPTION, CLINE_SOURCE_NONE, fixture.push,
                                    "missing required option '--branch'");
        expected.option = fixture.branch;
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    /* An option that has a short name only. */
    {
        char *argv[] = { "tool", "-t", "secret", "push", "--branch", "main", "origin" };

        expected = off_command_line(CLINE_PARSE_ERROR_MISSING_OPTION, CLINE_SOURCE_NONE, fixture.push,
                                    "missing required option '-a'");
        expected.option = fixture.area;
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    /*
     * The command is that of the option, not the command that the command
     * line ends in.
     */
    {
        char *argv[] = { "tool", "pull", "a.txt" };

        expected = off_command_line(CLINE_PARSE_ERROR_MISSING_OPTION, CLINE_SOURCE_NONE, fixture.root,
                                    "missing required option '--token'");
        expected.option = fixture.token;
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }

    destroy_root(fixture.root);
}

NAOS_TEST(a_missing_positional_has_no_position_and_the_positional)
{
    required_fixture_t fixture;
    expected_t expected;

    set_up_required(&fixture);

    /* A positional that takes one argument. */
    {
        char *argv[] = { "tool", "--token=secret", "push", "-b", "main", "-ax" };

        expected = off_command_line(CLINE_PARSE_ERROR_MISSING_POSITIONAL, CLINE_SOURCE_NONE, fixture.push,
                                    "missing required argument 'REMOTE'");
        expected.positional = fixture.remote;
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    /* A positional that takes one or more. */
    {
        char *argv[] = { "tool", "--token=secret", "pull" };

        expected = off_command_line(CLINE_PARSE_ERROR_MISSING_POSITIONAL, CLINE_SOURCE_NONE, fixture.pull,
                                    "missing required argument 'FILES'");
        expected.positional = fixture.files;
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }

    destroy_root(fixture.root);
}

/* ---- A default value that its value type does not take ------------------- */

/*
 * A value type that takes every text but the one it is told to reject. Its
 * value is the text. The validation of a schema converts the default values
 * with it; a text is rejected only afterwards.
 */
typedef struct picky
{
    const char *rejected; /* NULL: every text is taken. */
} picky_t;

static cline_error_t picky_convert(const char *text, void *value, void *context)
{
    const picky_t *picky = context;

    if ((picky->rejected != NULL) && (strcmp(text, picky->rejected) == 0))
    {
        return CLINE_ERROR_INVALID_VALUE;
    }
    memcpy(value, &text, sizeof(text));
    return CLINE_ERROR_NONE;
}

NAOS_TEST(an_invalid_default_value_has_the_default_as_its_value_and_no_argument)
{
    picky_t picky = { NULL };
    cline_type_t *type = NULL;
    cline_command_t *root = create_root("tool");
    cline_command_t *make;
    cline_option_t *mode;
    cline_option_t *shape;
    cline_positional_t *target;
    char *argv[] = { "tool", "make", "-v" };
    expected_t expected;

    NAOS_REQUIRE_TRUE(cline_type_create("word", sizeof(const char *), picky_convert, NULL, &picky,
                                         &type) == CLINE_ERROR_NONE);
    mode = add_value(root, 'm', "mode", type, "bad-mode");
    make = add_subcommand(root, "make");
    (void)add_flag(make, 'v', NULL);
    shape = add_value(make, 's', NULL, type, "bad-shape");
    target = add_positional(make, "TARGET", type, CLINE_ARITY_OPTIONAL, "bad-target");
    validate(root);

    cline_result_destroy(parse_ok(root, ARGC(argv), argv));

    /*
     * An option. The command is that of the option, not the command that
     * the command line ends in.
     */
    picky.rejected = "bad-mode";
    expected = off_command_line(CLINE_PARSE_ERROR_INVALID_VALUE, CLINE_SOURCE_DEFAULT, root,
                                "invalid default value 'bad-mode' for option '--mode' (expected word)");
    expected.value = "bad-mode";
    expected.option = mode;
    expect_error(root, ARGC(argv), argv, &expected);

    /* An option that has a short name only. */
    picky.rejected = "bad-shape";
    expected = off_command_line(CLINE_PARSE_ERROR_INVALID_VALUE, CLINE_SOURCE_DEFAULT, make,
                                "invalid default value 'bad-shape' for option '-s' (expected word)");
    expected.value = "bad-shape";
    expected.option = shape;
    expect_error(root, ARGC(argv), argv, &expected);

    /* A positional. */
    picky.rejected = "bad-target";
    expected = off_command_line(CLINE_PARSE_ERROR_INVALID_VALUE, CLINE_SOURCE_DEFAULT, make,
                                "invalid default value 'bad-target' for argument 'TARGET' (expected word)");
    expected.value = "bad-target";
    expected.positional = target;
    expect_error(root, ARGC(argv), argv, &expected);

    cline_command_destroy(root);
    cline_type_destroy(type);
    cline_test_allocator_uninstall();
    check_clean();
}

/* ---- Which of several errors is reported --------------------------------- */

NAOS_TEST(of_two_errors_the_one_that_is_earlier_on_the_command_line_is_reported)
{
    fixture_t fixture;
    expected_t expected;

    set_up(&fixture);

    {
        char *argv[] = { "tool", "-v", "--bogus", "run", "--jobs=x", "main.c" };

        expected = on_command_line(CLINE_PARSE_ERROR_UNKNOWN_OPTION, "--bogus", 2, fixture.root,
                                   "unknown option '--bogus'");
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    /* The same two the other way round. */
    {
        char *argv[] = { "tool", "-v", "run", "--jobs=x", "--bogus", "main.c" };

        expected = on_command_line(CLINE_PARSE_ERROR_INVALID_VALUE, "--jobs=x", 3, fixture.run,
                                   "invalid value 'x' for option '--jobs' (expected integer)");
        expected.value = "x";
        expected.option = fixture.jobs;
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    /* Two in one bundle: the letter that comes first. */
    {
        char *argv[] = { "tool", "run", "-fxjy", "main.c" };

        expected = on_command_line(CLINE_PARSE_ERROR_UNKNOWN_OPTION, "-fxjy", 2, fixture.run,
                                   "unknown option '-x'");
        expected.short_name = 'x';
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    /*
     * An error on the command line comes before something required that
     * is missing: SRC of run was not given.
     */
    {
        char *argv[] = { "tool", "run", "-f", "--quiet=1" };

        expected = on_command_line(CLINE_PARSE_ERROR_UNEXPECTED_VALUE, "--quiet=1", 3, fixture.run,
                                   "option '--quiet' does not take a value");
        expected.value = "1";
        expected.option = fixture.quiet;
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }

    destroy_root(fixture.root);
}

NAOS_TEST(of_several_missing_things_the_first_of_the_nearest_command_is_reported)
{
    required_fixture_t fixture;
    expected_t expected;

    set_up_required(&fixture);

    /*
     * Everything is missing: the option of the root comes first, although
     * it was added last.
     */
    {
        char *argv[] = { "tool", "push" };

        expected = off_command_line(CLINE_PARSE_ERROR_MISSING_OPTION, CLINE_SOURCE_NONE, fixture.root,
                                    "missing required option '--token'");
        expected.option = fixture.token;
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    /*
     * In one command, the options come before the positionals, although
     * REMOTE was added first; of the options, the one added first.
     */
    {
        char *argv[] = { "tool", "-tsecret", "push" };

        expected = off_command_line(CLINE_PARSE_ERROR_MISSING_OPTION, CLINE_SOURCE_NONE, fixture.push,
                                    "missing required option '--branch'");
        expected.option = fixture.branch;
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    {
        char *argv[] = { "tool", "-tsecret", "push", "-bmain" };

        expected = off_command_line(CLINE_PARSE_ERROR_MISSING_OPTION, CLINE_SOURCE_NONE, fixture.push,
                                    "missing required option '-a'");
        expected.option = fixture.area;
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    {
        char *argv[] = { "tool", "-tsecret", "push", "-bmain", "-ax" };

        expected = off_command_line(CLINE_PARSE_ERROR_MISSING_POSITIONAL, CLINE_SOURCE_NONE, fixture.push,
                                    "missing required argument 'REMOTE'");
        expected.positional = fixture.remote;
        expect_error(fixture.root, ARGC(argv), argv, &expected);
    }
    /* Nothing is missing any more. */
    {
        char *argv[] = { "tool", "-tsecret", "push", "-bmain", "-ax", "origin" };

        cline_result_destroy(parse_ok(fixture.root, ARGC(argv), argv));
    }

    destroy_root(fixture.root);
}

/* ---- The result of a command line with a parse error --------------------- */

NAOS_TEST(a_result_with_a_parse_error_holds_what_was_read_before_the_error)
{
    fixture_t fixture;
    char *argv[] = { "tool", "-v", "-dd", "--no-color", "-n", "5", "run", "-f", "--cache",
                     "main.c", "-qq", "--jobs=3", "7", "--bogus", "--quiet", "--name=late" };
    expected_t expected;
    cline_result_t *result;
    const char *text = NULL;
    int64_t number = 0;

    set_up(&fixture);
    expected = on_command_line(CLINE_PARSE_ERROR_UNKNOWN_OPTION, "--bogus", 13, fixture.run,
                               "unknown option '--bogus'");
    result = parse_error(fixture.root, ARGC(argv), argv, &expected);

    /* The selected commands. */
    NAOS_REQUIRE_TRUE(cline_result_command_count(result) == 2);
    NAOS_CHECK_TRUE(cline_result_command_at(result, 0) == fixture.root);
    NAOS_CHECK_TRUE(cline_result_command_at(result, 1) == fixture.run);
    NAOS_CHECK_TRUE(cline_result_command(result) == fixture.run);

    /* The options of the root. */
    NAOS_CHECK_TRUE(cline_result_flag(result, fixture.verbose));
    NAOS_CHECK_INT_EQ(cline_result_option_source(result, fixture.verbose), CLINE_SOURCE_COMMAND_LINE);
    NAOS_CHECK_UINT_EQ(cline_result_count(result, fixture.debug), 2);
    NAOS_CHECK_INT_EQ(cline_result_option_source(result, fixture.debug), CLINE_SOURCE_COMMAND_LINE);
    NAOS_CHECK_FALSE(cline_result_flag(result, fixture.color));
    NAOS_CHECK_INT_EQ(cline_result_option_source(result, fixture.color), CLINE_SOURCE_COMMAND_LINE);
    NAOS_REQUIRE_TRUE(cline_result_option_int(result, fixture.num, &number));
    NAOS_CHECK_TRUE(number == 5);
    NAOS_CHECK_INT_EQ(cline_result_option_source(result, fixture.num), CLINE_SOURCE_COMMAND_LINE);

    /* The options and the positionals of run, up to the error. */
    NAOS_CHECK_TRUE(cline_result_flag(result, fixture.force));
    NAOS_CHECK_INT_EQ(cline_result_option_source(result, fixture.force), CLINE_SOURCE_COMMAND_LINE);
    NAOS_CHECK_TRUE(cline_result_flag(result, fixture.cache));
    NAOS_CHECK_INT_EQ(cline_result_option_source(result, fixture.cache), CLINE_SOURCE_COMMAND_LINE);
    NAOS_REQUIRE_TRUE(cline_result_option_int(result, fixture.jobs, &number));
    NAOS_CHECK_TRUE(number == 3);
    NAOS_CHECK_INT_EQ(cline_result_option_source(result, fixture.jobs), CLINE_SOURCE_COMMAND_LINE);
    NAOS_CHECK_UINT_EQ(cline_result_positional_count(result, fixture.src), 1);
    NAOS_REQUIRE_TRUE(cline_result_positional_string(result, fixture.src, 0, &text));
    NAOS_CHECK_STR_EQ(text, "main.c");
    NAOS_CHECK_INT_EQ(cline_result_positional_source(result, fixture.src), CLINE_SOURCE_COMMAND_LINE);
    NAOS_REQUIRE_TRUE(cline_result_positional_int(result, fixture.count, 0, &number));
    NAOS_CHECK_TRUE(number == 7);

    /*
     * What stands after the error was not read: --quiet was given twice,
     * and --name not at all.
     */
    NAOS_CHECK_UINT_EQ(cline_result_count(result, fixture.quiet), 2);
    NAOS_CHECK_INT_EQ(cline_result_option_source(result, fixture.name), CLINE_SOURCE_NONE);
    NAOS_CHECK_TRUE(cline_result_option_value(result, fixture.name) == NULL);

    cline_result_destroy(result);
    destroy_root(fixture.root);
}

NAOS_TEST(help_reads_as_given_when_something_required_is_missing)
{
    cline_command_t *root = create_root("tool");
    cline_option_t *help = add_flag(root, 'h', "help");
    cline_positional_t *file =
        add_positional(root, "FILE", cline_type_string(), CLINE_ARITY_REQUIRED, NULL);
    char *argv[] = { "tool", "--help" };
    expected_t expected;
    cline_result_t *result;

    validate(root);

    expected = off_command_line(CLINE_PARSE_ERROR_MISSING_POSITIONAL, CLINE_SOURCE_NONE, root,
                                "missing required argument 'FILE'");
    expected.positional = file;
    result = parse_error(root, ARGC(argv), argv, &expected);
    NAOS_CHECK_TRUE(cline_result_flag(result, help));
    NAOS_CHECK_INT_EQ(cline_result_option_source(result, help), CLINE_SOURCE_COMMAND_LINE);
    NAOS_CHECK_UINT_EQ(cline_result_positional_count(result, file), 0);
    cline_result_destroy(result);

    destroy_root(root);
}

NAOS_TEST(help_of_a_subcommand_reads_as_given_when_something_required_is_missing)
{
    fixture_t fixture;
    char *argv[] = { "tool", "run", "--help" };
    expected_t expected;
    cline_result_t *result;

    set_up(&fixture);

    expected = off_command_line(CLINE_PARSE_ERROR_MISSING_POSITIONAL, CLINE_SOURCE_NONE, fixture.run,
                                "missing required argument 'SRC'");
    expected.positional = fixture.src;
    result = parse_error(fixture.root, ARGC(argv), argv, &expected);
    NAOS_CHECK_TRUE(cline_result_flag(result, fixture.help));
    NAOS_CHECK_INT_EQ(cline_result_option_source(result, fixture.help), CLINE_SOURCE_COMMAND_LINE);
    NAOS_CHECK_TRUE(cline_result_command(result) == fixture.run);
    cline_result_destroy(result);

    destroy_root(fixture.root);
}

/*
 * Checks what was not given on a command line that selected tool and run
 * and has a parse error.
 */
static void check_nothing_was_applied(const cline_result_t *result, const fixture_t *fixture)
{
    const char *text = "untouched";
    int64_t number = 77;

    /* No default value: an option with one reads as one without. */
    NAOS_CHECK_INT_EQ(cline_result_option_source(result, fixture->output), CLINE_SOURCE_NONE);
    NAOS_CHECK_FALSE(cline_result_option_string(result, fixture->output, &text));
    NAOS_CHECK_STR_EQ(text, "untouched");
    NAOS_CHECK_TRUE(cline_result_option_value(result, fixture->output) == NULL);
    NAOS_CHECK_INT_EQ(cline_result_option_source(result, fixture->name), CLINE_SOURCE_NONE);
    NAOS_CHECK_FALSE(cline_result_option_string(result, fixture->name, &text));
    NAOS_CHECK_STR_EQ(text, "untouched");
    NAOS_CHECK_INT_EQ(cline_result_option_source(result, fixture->num), CLINE_SOURCE_NONE);
    NAOS_CHECK_FALSE(cline_result_option_int(result, fixture->num, &number));
    NAOS_CHECK_TRUE(number == 77);
    NAOS_CHECK_INT_EQ(cline_result_positional_source(result, fixture->count), CLINE_SOURCE_NONE);
    NAOS_CHECK_UINT_EQ(cline_result_positional_count(result, fixture->count), 0);

    /* The options that take no value read as their initial values. */
    NAOS_CHECK_FALSE(cline_result_flag(result, fixture->verbose));
    NAOS_CHECK_INT_EQ(cline_result_option_source(result, fixture->verbose), CLINE_SOURCE_DEFAULT);
    NAOS_CHECK_UINT_EQ(cline_result_count(result, fixture->debug), 0);
    NAOS_CHECK_INT_EQ(cline_result_option_source(result, fixture->debug), CLINE_SOURCE_DEFAULT);
    NAOS_CHECK_TRUE(cline_result_flag(result, fixture->color));
    NAOS_CHECK_INT_EQ(cline_result_option_source(result, fixture->color), CLINE_SOURCE_DEFAULT);
    NAOS_CHECK_FALSE(cline_result_flag(result, fixture->force));
    NAOS_CHECK_INT_EQ(cline_result_option_source(result, fixture->force), CLINE_SOURCE_DEFAULT);
    NAOS_CHECK_UINT_EQ(cline_result_count(result, fixture->quiet), 0);
    NAOS_CHECK_INT_EQ(cline_result_option_source(result, fixture->quiet), CLINE_SOURCE_DEFAULT);
    NAOS_CHECK_FALSE(cline_result_flag(result, fixture->cache));
    NAOS_CHECK_INT_EQ(cline_result_option_source(result, fixture->cache), CLINE_SOURCE_DEFAULT);

    /*
     * A command that was not selected has nothing, as in a result without
     * a parse error.
     */
    NAOS_CHECK_FALSE(cline_result_flag(result, fixture->all));
    NAOS_CHECK_INT_EQ(cline_result_option_source(result, fixture->all), CLINE_SOURCE_NONE);
    NAOS_CHECK_UINT_EQ(cline_result_positional_count(result, fixture->remote_name), 0);
}

NAOS_TEST(a_result_with_a_parse_error_has_no_default_value_and_the_initial_values)
{
    fixture_t fixture;
    char *scan[] = { "tool", "run", "main.c", "--bogus", "-v" };
    char *missing[] = { "tool", "run" };
    expected_t expected;
    cline_result_t *result;

    set_up(&fixture);

    /* An error of the scan. */
    expected = on_command_line(CLINE_PARSE_ERROR_UNKNOWN_OPTION, "--bogus", 3, fixture.run,
                               "unknown option '--bogus'");
    result = parse_error(fixture.root, ARGC(scan), scan, &expected);
    check_nothing_was_applied(result, &fixture);
    NAOS_CHECK_INT_EQ(cline_result_positional_source(result, fixture.src), CLINE_SOURCE_COMMAND_LINE);
    NAOS_CHECK_UINT_EQ(cline_result_positional_count(result, fixture.src), 1);
    cline_result_destroy(result);

    /* Something required is missing. */
    expected = off_command_line(CLINE_PARSE_ERROR_MISSING_POSITIONAL, CLINE_SOURCE_NONE, fixture.run,
                                "missing required argument 'SRC'");
    expected.positional = fixture.src;
    result = parse_error(fixture.root, ARGC(missing), missing, &expected);
    check_nothing_was_applied(result, &fixture);
    NAOS_CHECK_INT_EQ(cline_result_positional_source(result, fixture.src), CLINE_SOURCE_NONE);
    NAOS_CHECK_UINT_EQ(cline_result_positional_count(result, fixture.src), 0);
    cline_result_destroy(result);

    destroy_root(fixture.root);
}

/* ---- A result without a parse error -------------------------------------- */

NAOS_TEST(a_result_without_a_parse_error_reads_as_no_error)
{
    fixture_t fixture;
    char *given[] = { "tool", "-v", "-n", "5", "run", "-fj2", "main.c", "3" };
    char *nothing[] = { "tool" };
    cline_result_t *results[3];
    size_t index;

    set_up(&fixture);
    results[0] = parse_ok(fixture.root, ARGC(given), given);
    results[1] = parse_ok(fixture.root, ARGC(nothing), nothing);
    results[2] = parse_ok(fixture.root, 0, NULL);

    for (index = 0; index < (sizeof(results) / sizeof(results[0])); index++)
    {
        const cline_result_t *result = results[index];

        NAOS_CHECK_INT_EQ(cline_result_error(result), CLINE_ERROR_NONE);
        NAOS_CHECK_INT_EQ(cline_result_error_kind(result), CLINE_PARSE_ERROR_NONE);
        NAOS_CHECK_TRUE(cline_result_error_message(result) == NULL);
        NAOS_CHECK_TRUE(cline_result_error_argument(result) == NULL);
        NAOS_CHECK_TRUE(cline_result_error_value(result) == NULL);
        NAOS_CHECK_INT_EQ(cline_result_error_source(result), CLINE_SOURCE_NONE);
        NAOS_CHECK_UINT_EQ(cline_result_error_index(result), 0);
        NAOS_CHECK_TRUE(cline_result_error_command(result) == NULL);
        NAOS_CHECK_TRUE(cline_result_error_option(result) == NULL);
        NAOS_CHECK_TRUE(cline_result_error_positional(result) == NULL);
        NAOS_CHECK_INT_EQ(cline_result_error_short_name(result), '\0');
        cline_result_destroy(results[index]);
    }

    destroy_root(fixture.root);
}

NAOS_TEST(destroying_no_result_does_nothing)
{
    cline_test_allocator_install();
    cline_result_destroy(NULL);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_requests(), 0);
    cline_test_allocator_uninstall();
    check_clean();
}

/* ---- The strings of an error --------------------------------------------- */

NAOS_TEST(the_strings_of_an_error_stay_valid_after_argv_was_overwritten)
{
    fixture_t fixture;
    char program[] = "tool";
    char command[] = "run";
    char source[] = "main.c";
    char jobs[] = "--jobs=abc";
    char extra[] = "extra";
    char *argv[5];
    expected_t expected;
    cline_result_t *result;
    size_t index;

    set_up(&fixture);

    /* The value is a part of the argument. */
    argv[0] = program;
    argv[1] = command;
    argv[2] = source;
    argv[3] = jobs;
    argv[4] = extra;
    expected = on_command_line(CLINE_PARSE_ERROR_INVALID_VALUE, "--jobs=abc", 3, fixture.run,
                               "invalid value 'abc' for option '--jobs' (expected integer)");
    expected.value = "abc";
    expected.option = fixture.jobs;
    result = parse_error(fixture.root, ARGC(argv), argv, &expected);

    memset(program, '#', sizeof(program) - 1);
    memset(command, '#', sizeof(command) - 1);
    memset(source, '#', sizeof(source) - 1);
    memset(jobs, '#', sizeof(jobs) - 1);
    memset(extra, '#', sizeof(extra) - 1);
    for (index = 0; index < (sizeof(argv) / sizeof(argv[0])); index++)
    {
        argv[index] = NULL;
    }
    check_error(result, &expected);
    cline_result_destroy(result);

    /* The value is the argument, the next one of its option. */
    memcpy(program, "tool", sizeof(program));
    memcpy(command, "run", sizeof(command));
    memcpy(jobs, "-fqj", sizeof("-fqj"));
    memcpy(extra, "extra", sizeof(extra));
    argv[0] = program;
    argv[1] = command;
    argv[2] = jobs;
    argv[3] = extra;
    expected = on_command_line(CLINE_PARSE_ERROR_INVALID_VALUE, "extra", 3, fixture.run,
                               "invalid value 'extra' for option '-j' (expected integer)");
    expected.value = "extra";
    expected.option = fixture.jobs;
    expected.short_name = 'j';
    result = parse_error(fixture.root, 4, argv, &expected);

    memset(jobs, '#', sizeof(jobs) - 1);
    memset(extra, '#', sizeof(extra) - 1);
    argv[2] = NULL;
    argv[3] = NULL;
    check_error(result, &expected);
    cline_result_destroy(result);

    destroy_root(fixture.root);
}

NAOS_SUITE(scan_errors,
    NAOS_ADD_TEST(an_unknown_option_has_its_argument_and_no_value),
    NAOS_ADD_TEST(an_unknown_subcommand_has_its_argument_as_its_value),
    NAOS_ADD_TEST(a_missing_value_has_the_argument_of_the_option_and_the_option),
    NAOS_ADD_TEST(an_unexpected_value_has_what_follows_the_equals_sign_and_the_option),
    NAOS_ADD_TEST(an_invalid_value_of_an_option_has_the_argument_that_holds_the_value),
    NAOS_ADD_TEST(an_invalid_value_of_a_positional_has_the_positional),
    NAOS_ADD_TEST(an_unexpected_positional_has_its_argument_as_its_value)
)

NAOS_SUITE(resolve_errors,
    NAOS_ADD_TEST(a_missing_option_has_no_position_and_the_option),
    NAOS_ADD_TEST(a_missing_positional_has_no_position_and_the_positional),
    NAOS_ADD_TEST(an_invalid_default_value_has_the_default_as_its_value_and_no_argument)
)

NAOS_SUITE(first_error,
    NAOS_ADD_TEST(of_two_errors_the_one_that_is_earlier_on_the_command_line_is_reported),
    NAOS_ADD_TEST(of_several_missing_things_the_first_of_the_nearest_command_is_reported)
)

NAOS_SUITE(failed_results,
    NAOS_ADD_TEST(a_result_with_a_parse_error_holds_what_was_read_before_the_error),
    NAOS_ADD_TEST(help_reads_as_given_when_something_required_is_missing),
    NAOS_ADD_TEST(help_of_a_subcommand_reads_as_given_when_something_required_is_missing),
    NAOS_ADD_TEST(a_result_with_a_parse_error_has_no_default_value_and_the_initial_values)
)

NAOS_SUITE(no_error,
    NAOS_ADD_TEST(a_result_without_a_parse_error_reads_as_no_error),
    NAOS_ADD_TEST(destroying_no_result_does_nothing)
)

NAOS_SUITE(strings,
    NAOS_ADD_TEST(the_strings_of_an_error_stay_valid_after_argv_was_overwritten)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(scan_errors),
    NAOS_ADD_SUITE(resolve_errors),
    NAOS_ADD_SUITE(first_error),
    NAOS_ADD_SUITE(failed_results),
    NAOS_ADD_SUITE(no_error),
    NAOS_ADD_SUITE(strings)
)
