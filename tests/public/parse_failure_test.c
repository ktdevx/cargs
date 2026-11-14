/*
 * Tests of the failure of an allocation in a parse: clargs_parse() and
 * clargs_parse_wide() of <clargs/parse.h>.
 *
 * Each test takes one parse and makes its first, second, ... allocation
 * request fail in turn, in the shape that docs/conventions.md gives under
 * the failure of every allocation in turn. The operations are:
 *
 * 1. a parse that succeeds, with two levels of subcommands, the four kinds
 *    of options, values that are replaced, a positional that takes any
 *    number of values, and default values,
 * 2. parses that end in a parse error, whose message is allocated last,
 * 3. a parse that gives every value its default value,
 * 4. parses through the wide entry.
 *
 * Whichever request fails, the parse must return
 * CLARGS_ERROR_OUT_OF_MEMORY, write NULL to the place of the result, keep
 * no block of its own, and have cleaned up every value that it converted:
 * the values of a user-defined value type are counted. A result of an
 * earlier parse must be as it was. The same parse of the same schema must
 * then do what it does when no request fails, and the schema and the value
 * types must be destroyed without a block left and without a violation of
 * the allocator contract.
 *
 * The schema is built again for each request to fail, after the reset of
 * the counts, so that the counts start when nothing is allocated. Only the
 * requests of the parse count: the request to fail is set just before it.
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

/*
 * A number with more characters than the conversion of the built-in
 * floating-point type copies without an allocation of its own: a
 * conversion that can itself run out of memory.
 */
#define LONG_FLOAT "0." "1234567890" "1234567890" "1234567890" "1234567890" \
                   "1234567890" "1234567890" "1234567890"

/* Characters of 2, 3 and 4 bytes in UTF-8: U+00E9, U+3042 and U+1F600. */
#define W_2 L"\u00E9"
#define W_3 L"\u3042"
#define W_4 L"\U0001F600"
#define U_2 "\xC3\xA9"
#define U_3 "\xE3\x81\x82"
#define U_4 "\xF0\x9F\x98\x80"

/* The bytes of U+FFFD, which replaces a unit that cannot be converted. */
#define REPLACEMENT "\xEF\xBF\xBD"

/* The most allocation requests that a parse is given to fail in. */
#define REQUEST_LIMIT 1000

/*
 * The end of every test: the test allocator has no live block and saw no
 * violation of the allocator contract.
 */
static void check_clean(void)
{
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
}

/* ---- The user-defined value type ---------------------------------------- */

/*
 * The value of the user-defined value type "word": the text that the
 * conversion was given, not a copy of it, and the length of the text.
 */
typedef struct word {
    const char *text;
    size_t length;
} word_t;

/*
 * The user data of the value type "word". It counts the conversions that
 * succeeded and the cleanups, and tells whether the text "bad" is
 * rejected.
 */
typedef struct word_log {
    size_t converted;
    size_t released;
    bool reject;
} word_log_t;

static clargs_error_t word_convert(const char *text, void *value, void *context)
{
    word_log_t *log = context;
    word_t word;

    if (log->reject && strcmp(text, "bad") == 0) {
        return CLARGS_ERROR_INVALID_VALUE;
    }
    word.text = text;
    word.length = strlen(text);
    memcpy(value, &word, sizeof(word));
    log->converted++;
    return CLARGS_ERROR_NONE;
}

/* Reads the value: its text must still be valid when it is cleaned up. */
static void word_release(void *value, void *context)
{
    word_log_t *log = context;
    const word_t *word = value;

    NAOS_CHECK_UINT_EQ(strlen(word->text), word->length);
    log->released++;
}

/* ---- The schema --------------------------------------------------------- */

/*
 * The tool of every test:
 *
 *   tool [-v|--verbose] [-d|--debug]... [-c|--[no-]color]
 *        [--config STRING, "tool.cfg"] [-j|--jobs INT, "2"]
 *     remote [-q|--quiet] [-n|--name WORD, "origin"]
 *       add [-f|--force] [-x|--extra]... [--[no-]fetch] [-t|--tag WORD]
 *           [--ratio FLOAT, "0.5"] [--mode red|green|blue, "red"]
 *           [--label WORD, "none"] NAME [URL...]
 *     init [--owner WORD, "nobody"] [--template WORD, "bad"] [DIR, "."]
 *
 * NAME, URL and DIR are of the value type "word". The conversion takes
 * "bad" until log.reject is set, which is done after the validation.
 */
typedef struct fixture {
    word_log_t log;
    clargs_type_t *word;
    clargs_type_t *color_type;
    clargs_command_t *root;
    clargs_option_t *verbose;
    clargs_option_t *debug;
    clargs_option_t *color;
    clargs_option_t *config;
    clargs_option_t *jobs;
    clargs_command_t *remote;
    clargs_option_t *quiet;
    clargs_option_t *name;
    clargs_command_t *add;
    clargs_option_t *force;
    clargs_option_t *extra;
    clargs_option_t *fetch;
    clargs_option_t *tag;
    clargs_option_t *ratio;
    clargs_option_t *mode;
    clargs_option_t *label;
    clargs_positional_t *item;
    clargs_positional_t *urls;
    clargs_command_t *init;
    clargs_option_t *owner;
    clargs_option_t *template_name;
    clargs_positional_t *dir;
} fixture_t;

static clargs_command_t *add_subcommand(clargs_command_t *command, const char *name)
{
    clargs_command_t *subcommand = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_subcommand(command, name, &subcommand) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(subcommand != NULL);
    return subcommand;
}

static clargs_option_t *add_flag(clargs_command_t *command, char short_name, const char *long_name)
{
    clargs_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_flag(command, short_name, long_name, &option) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(option != NULL);
    return option;
}

static clargs_option_t *add_count(clargs_command_t *command, char short_name, const char *long_name)
{
    clargs_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_count(command, short_name, long_name, &option) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(option != NULL);
    return option;
}

static clargs_option_t *add_negatable(clargs_command_t *command, char short_name, const char *long_name)
{
    clargs_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_negatable(command, short_name, long_name, &option) ==
                      CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(option != NULL);
    return option;
}

/*
 * Adds an option that takes a value to command. default_value may be
 * NULL: the option has no default value then.
 */
static clargs_option_t *add_value(clargs_command_t *command, char short_name, const char *long_name,
                                  const clargs_type_t *type, const char *default_value)
{
    clargs_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_value(command, short_name, long_name, type, &option) ==
                      CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(option != NULL);
    if (default_value != NULL) {
        NAOS_REQUIRE_TRUE(clargs_option_set_default(option, default_value) == CLARGS_ERROR_NONE);
    }
    return option;
}

/*
 * Adds a positional to command. default_value may be NULL: the positional
 * has no default value then.
 */
static clargs_positional_t *add_positional(clargs_command_t *command, const char *name,
                                           const clargs_type_t *type, clargs_arity_t arity,
                                           const char *default_value)
{
    clargs_positional_t *positional = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_positional(command, name, type, arity, &positional) ==
                      CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(positional != NULL);
    if (default_value != NULL) {
        NAOS_REQUIRE_TRUE(clargs_positional_set_default(positional, default_value) == CLARGS_ERROR_NONE);
    }
    return positional;
}

/*
 * Creates the value types and the schema, and validates it. The test
 * allocator is installed, and no request is set to fail.
 */
static void set_up(fixture_t *fixture)
{
    const char *const colors[] = { "red", "green", "blue" };

    fixture->log.converted = 0;
    fixture->log.released = 0;
    fixture->log.reject = false;
    fixture->word = NULL;
    fixture->color_type = NULL;
    fixture->root = NULL;

    NAOS_REQUIRE_TRUE(clargs_type_create("word", sizeof(word_t), word_convert, word_release, &fixture->log,
                                         &fixture->word) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_type_create_choice(colors, sizeof(colors) / sizeof(colors[0]),
                                                &fixture->color_type) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_command_create("tool", &fixture->root) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(fixture->root != NULL);

    fixture->verbose = add_flag(fixture->root, 'v', "verbose");
    fixture->debug = add_count(fixture->root, 'd', "debug");
    fixture->color = add_negatable(fixture->root, 'c', "color");
    fixture->config = add_value(fixture->root, '\0', "config", clargs_type_string(), "tool.cfg");
    fixture->jobs = add_value(fixture->root, 'j', "jobs", clargs_type_int(), "2");

    fixture->remote = add_subcommand(fixture->root, "remote");
    fixture->quiet = add_flag(fixture->remote, 'q', "quiet");
    fixture->name = add_value(fixture->remote, 'n', "name", fixture->word, "origin");

    fixture->add = add_subcommand(fixture->remote, "add");
    fixture->force = add_flag(fixture->add, 'f', "force");
    fixture->extra = add_count(fixture->add, 'x', "extra");
    fixture->fetch = add_negatable(fixture->add, '\0', "fetch");
    fixture->tag = add_value(fixture->add, 't', "tag", fixture->word, NULL);
    fixture->ratio = add_value(fixture->add, '\0', "ratio", clargs_type_float(), "0.5");
    fixture->mode = add_value(fixture->add, '\0', "mode", fixture->color_type, "red");
    fixture->label = add_value(fixture->add, '\0', "label", fixture->word, "none");
    fixture->item = add_positional(fixture->add, "NAME", fixture->word, CLARGS_ARITY_REQUIRED, NULL);
    fixture->urls = add_positional(fixture->add, "URL", fixture->word, CLARGS_ARITY_ZERO_OR_MORE, NULL);

    fixture->init = add_subcommand(fixture->root, "init");
    fixture->owner = add_value(fixture->init, '\0', "owner", fixture->word, "nobody");
    fixture->template_name = add_value(fixture->init, '\0', "template", fixture->word, "bad");
    fixture->dir = add_positional(fixture->init, "DIR", fixture->word, CLARGS_ARITY_OPTIONAL, ".");

    NAOS_REQUIRE_TRUE(clargs_command_validate(fixture->root, NULL, NULL, NULL) == CLARGS_ERROR_NONE);
    /* The validation converted the default values and cleaned them up. */
    NAOS_CHECK_UINT_EQ(fixture->log.released, fixture->log.converted);
}

/*
 * The number of values of the value type "word" that were converted and
 * not cleaned up yet.
 */
static size_t words_held(const fixture_t *fixture)
{
    return fixture->log.converted - fixture->log.released;
}

/*
 * Destroys the schema and the value types: they can still be destroyed,
 * and nothing stays allocated.
 */
static void tear_down(fixture_t *fixture)
{
    clargs_command_destroy(fixture->root);
    clargs_type_destroy(fixture->color_type);
    clargs_type_destroy(fixture->word);
    NAOS_CHECK_UINT_EQ(words_held(fixture), 0);
    check_clean();
}

/* ---- Reading a result --------------------------------------------------- */

static void check_option_word(const clargs_result_t *result, const clargs_option_t *option,
                              const char *expected, clargs_source_t source)
{
    const word_t *word = clargs_result_option_value(result, option);

    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option), source);
    NAOS_REQUIRE_TRUE(word != NULL);
    NAOS_REQUIRE_TRUE(word->text != NULL);
    NAOS_CHECK_STR_EQ(word->text, expected);
}

static void check_positional_word(const clargs_result_t *result, const clargs_positional_t *positional,
                                  size_t index, const char *expected)
{
    const word_t *word = clargs_result_positional_value(result, positional, index);

    NAOS_REQUIRE_TRUE(word != NULL);
    NAOS_REQUIRE_TRUE(word->text != NULL);
    NAOS_CHECK_STR_EQ(word->text, expected);
}

static void check_option_string(const clargs_result_t *result, const clargs_option_t *option,
                                const char *expected, clargs_source_t source)
{
    const char *value = NULL;

    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option), source);
    NAOS_REQUIRE_TRUE(clargs_result_option_string(result, option, &value));
    NAOS_REQUIRE_TRUE(value != NULL);
    NAOS_CHECK_STR_EQ(value, expected);
}

static void check_option_int(const clargs_result_t *result, const clargs_option_t *option,
                             int64_t expected, clargs_source_t source)
{
    int64_t value = 0;

    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option), source);
    NAOS_REQUIRE_TRUE(clargs_result_option_int(result, option, &value));
    NAOS_CHECK_TRUE(value == expected);
}

/*
 * Checks the selected commands of result: count of them, the last of which
 * is last.
 */
static void check_commands(const clargs_result_t *result, const fixture_t *fixture, size_t count,
                           const clargs_command_t *last)
{
    NAOS_CHECK_UINT_EQ(clargs_result_command_count(result), count);
    NAOS_CHECK_TRUE(clargs_result_command_at(result, 0) == fixture->root);
    NAOS_CHECK_TRUE(clargs_result_command(result) == last);
}

/* Checks the parse error of result. */
static void check_error(const clargs_result_t *result, clargs_parse_error_kind_t kind, const char *message)
{
    NAOS_CHECK_INT_EQ(clargs_result_error(result), CLARGS_ERROR_INVALID_COMMAND_LINE);
    NAOS_CHECK_INT_EQ(clargs_result_error_kind(result), kind);
    NAOS_REQUIRE_TRUE(clargs_result_error_message(result) != NULL);
    NAOS_CHECK_STR_EQ(clargs_result_error_message(result), message);
}

/* Checks that result has no parse error. */
static void check_success(const clargs_result_t *result)
{
    NAOS_CHECK_INT_EQ(clargs_result_error(result), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_result_error_kind(result), CLARGS_PARSE_ERROR_NONE);
    NAOS_CHECK_TRUE(clargs_result_error_message(result) == NULL);
}

/* ---- Failing every request of a parse ----------------------------------- */

/*
 * What a parse that completed must have given. held is set to the number
 * of values of the value type "word" that the result must hold.
 */
typedef void (*check_fn_t)(const fixture_t *fixture, const clargs_result_t *result, size_t *held);

/*
 * One parse: a command line for one of the two entries, what the parse
 * returns when no request fails, and the check of its result.
 */
typedef struct parse_case {
    int argc;
    char *const *argv;          /* NULL: the parse goes through the wide entry */
    wchar_t *const *wide_argv;  /* used when argv is NULL */
    bool reject;                /* whether the value type "word" rejects "bad" */
    clargs_error_t outcome;     /* CLARGS_ERROR_NONE or CLARGS_ERROR_INVALID_COMMAND_LINE */
    check_fn_t check_fn;
    size_t least;               /* the least number of requests that the parse must make */
} parse_case_t;

static clargs_error_t run_parse(const fixture_t *fixture, const parse_case_t *parse_case,
                                clargs_result_t **out)
{
    if (parse_case->argv != NULL) {
        return clargs_parse(fixture->root, parse_case->argc, parse_case->argv, out);
    }
    return clargs_parse_wide(fixture->root, parse_case->argc, parse_case->wide_argv, out);
}

/*
 * The parse of parse_case with no request to fail: it returns what it is
 * expected to, and gives the result that is expected. The result is
 * destroyed, and its values are cleaned up with it.
 */
static void check_parse_completes(const fixture_t *fixture, const parse_case_t *parse_case, size_t live)
{
    const size_t held_before = words_held(fixture);
    clargs_result_t *result = NULL;
    size_t held = 0;

    NAOS_CHECK_INT_EQ(run_parse(fixture, parse_case, &result), parse_case->outcome);
    NAOS_REQUIRE_TRUE(result != NULL);
    NAOS_CHECK_INT_EQ(clargs_result_error(result), parse_case->outcome);
    parse_case->check_fn(fixture, result, &held);
    NAOS_CHECK_UINT_EQ(words_held(fixture), held_before + held);

    clargs_result_destroy(result);
    NAOS_CHECK_UINT_EQ(words_held(fixture), held_before);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);
}

/*
 * Checks other, the result of the parse of "tool" alone that was made
 * before the parse that failed: it is as it was.
 */
static void check_other(const fixture_t *fixture, const clargs_result_t *other)
{
    check_success(other);
    check_commands(other, fixture, 1, fixture->root);
    NAOS_CHECK_FALSE(clargs_result_flag(other, fixture->verbose));
    check_option_string(other, fixture->config, "tool.cfg", CLARGS_SOURCE_DEFAULT);
    check_option_int(other, fixture->jobs, 2, CLARGS_SOURCE_DEFAULT);
}

/*
 * Makes the first, second, ... request of the parse of parse_case fail in
 * turn, until the parse completes without reaching the request to fail.
 */
static void fail_every_request(const parse_case_t *parse_case)
{
    char *other_argv[] = { "tool" };
    bool completed = false;
    size_t n;

    clargs_test_allocator_install();

    for (n = 1; n <= REQUEST_LIMIT; ++n) {
        fixture_t fixture;
        clargs_result_t *other = NULL;
        clargs_result_t *result;
        clargs_error_t error;
        size_t live;
        size_t before;
        size_t made;
        size_t held;

        /* Nothing is allocated here, so the counts can start again. */
        clargs_test_allocator_reset();

        /* The schema, and a result of it that is alive during the parse. */
        set_up(&fixture);
        fixture.log.reject = parse_case->reject;
        NAOS_REQUIRE_TRUE(clargs_parse(fixture.root, ARGC(other_argv), other_argv, &other) ==
                          CLARGS_ERROR_NONE);
        NAOS_REQUIRE_TRUE(other != NULL);
        live = clargs_test_allocator_live();
        held = words_held(&fixture);

        /*
         * The operation: only its requests count. The place of the result
         * holds a pointer that is not NULL.
         */
        result = other;
        before = clargs_test_allocator_requests();
        clargs_test_allocator_fail_at(before + n);
        error = run_parse(&fixture, parse_case, &result);
        made = clargs_test_allocator_requests() - before;
        clargs_test_allocator_fail_at(0);

        if (error != CLARGS_ERROR_OUT_OF_MEMORY && made < n) {
            size_t expected = 0;

            /*
             * The parse ended before the request to fail was reached: every
             * allocation point has failed once, n - 1 of them.
             */
            NAOS_CHECK_UINT_EQ(made, n - 1);
            NAOS_CHECK_TRUE(made >= parse_case->least);
            NAOS_CHECK_INT_EQ(error, parse_case->outcome);
            NAOS_REQUIRE_TRUE(result != NULL);
            NAOS_REQUIRE_TRUE(result != other);
            parse_case->check_fn(&fixture, result, &expected);
            NAOS_CHECK_UINT_EQ(words_held(&fixture), held + expected);
            clargs_result_destroy(result);
            NAOS_CHECK_UINT_EQ(words_held(&fixture), held);
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);

            check_other(&fixture, other);
            clargs_result_destroy(other);
            tear_down(&fixture);
            completed = true;
            break;
        }

        /*
         * The n-th request failed, the parse stopped there and reported
         * it, and it wrote NULL to the place of the result.
         */
        NAOS_CHECK_INT_EQ(error, CLARGS_ERROR_OUT_OF_MEMORY);
        NAOS_CHECK_UINT_EQ(made, n);
        NAOS_CHECK_TRUE(result == NULL);

        /*
         * Nothing of the parse stays allocated, and every value that it
         * converted was cleaned up.
         */
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);
        NAOS_CHECK_UINT_EQ(words_held(&fixture), held);

        /*
         * What the failure must leave unchanged: the result of the earlier
         * parse, and the schema, with which the same parse now does what
         * it does when no request fails.
         */
        check_other(&fixture, other);
        check_parse_completes(&fixture, parse_case, live);

        /* Destroy what exists; nothing is left and nothing was violated. */
        clargs_result_destroy(other);
        tear_down(&fixture);
    }

    /* The loop ended because the parse completed, not at the limit. */
    NAOS_CHECK_TRUE(completed);

    clargs_test_allocator_uninstall();
    check_clean();
}

/* ---- Operation 1: a parse that succeeds --------------------------------- */

static void check_full(const fixture_t *fixture, const clargs_result_t *result, size_t *held)
{
    double ratio = 0.0;
    size_t mode = 99;

    check_success(result);
    check_commands(result, fixture, 3, fixture->add);
    NAOS_CHECK_TRUE(clargs_result_command_at(result, 1) == fixture->remote);
    NAOS_CHECK_TRUE(clargs_result_command_at(result, 2) == fixture->add);

    /* The root. */
    NAOS_CHECK_TRUE(clargs_result_flag(result, fixture->verbose));
    NAOS_CHECK_UINT_EQ(clargs_result_count(result, fixture->debug), 2);
    NAOS_CHECK_FALSE(clargs_result_flag(result, fixture->color));
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, fixture->color), CLARGS_SOURCE_COMMAND_LINE);
    check_option_string(result, fixture->config, "tool.cfg", CLARGS_SOURCE_DEFAULT);
    check_option_int(result, fixture->jobs, 4, CLARGS_SOURCE_COMMAND_LINE);

    /* remote. */
    NAOS_CHECK_TRUE(clargs_result_flag(result, fixture->quiet));
    check_option_word(result, fixture->name, "upstream", CLARGS_SOURCE_COMMAND_LINE);

    /* add: the tag was given three times, and the last one stays. */
    NAOS_CHECK_TRUE(clargs_result_flag(result, fixture->force));
    NAOS_CHECK_UINT_EQ(clargs_result_count(result, fixture->extra), 2);
    NAOS_CHECK_TRUE(clargs_result_flag(result, fixture->fetch));
    check_option_word(result, fixture->tag, "c", CLARGS_SOURCE_COMMAND_LINE);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, fixture->ratio), CLARGS_SOURCE_COMMAND_LINE);
    NAOS_REQUIRE_TRUE(clargs_result_option_float(result, fixture->ratio, &ratio));
    NAOS_CHECK_TRUE(ratio > 0.1234 && ratio < 0.1235);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, fixture->mode), CLARGS_SOURCE_DEFAULT);
    NAOS_REQUIRE_TRUE(clargs_result_option_choice(result, fixture->mode, &mode));
    NAOS_CHECK_UINT_EQ(mode, 0);
    check_option_word(result, fixture->label, "none", CLARGS_SOURCE_DEFAULT);

    NAOS_CHECK_UINT_EQ(clargs_result_positional_count(result, fixture->item), 1);
    check_positional_word(result, fixture->item, 0, "one");
    NAOS_CHECK_UINT_EQ(clargs_result_positional_count(result, fixture->urls), 6);
    check_positional_word(result, fixture->urls, 0, "u1");
    check_positional_word(result, fixture->urls, 1, "u2");
    check_positional_word(result, fixture->urls, 2, "u3");
    check_positional_word(result, fixture->urls, 3, "u4");
    check_positional_word(result, fixture->urls, 4, "u5");
    check_positional_word(result, fixture->urls, 5, "-u6");

    /* The name, the tag, the label, NAME and six of URL. */
    *held = 10;
}

NAOS_TEST(a_parse_that_succeeds_fails_at_every_allocation_point)
{
    char *argv[] = { "tool",   "-vdd",   "--no-color", "--jobs=4", "remote", "-q",      "--name",
                     "upstream", "add",  "-f",         "-xx",      "--fetch", "--tag",  "a",
                     "-tb",    "--tag=c", "--ratio",   LONG_FLOAT, "one",    "u1",      "u2",
                     "u3",     "u4",     "u5",         "--",       "-u6" };
    parse_case_t parse_case;

    parse_case.argc = ARGC(argv);
    parse_case.argv = argv;
    parse_case.wide_argv = NULL;
    parse_case.reject = false;
    parse_case.outcome = CLARGS_ERROR_NONE;
    parse_case.check_fn = check_full;
    /*
     * The result, its arguments, three selected commands with their
     * options, the values, and the copy of the long number.
     */
    parse_case.least = 20;
    fail_every_request(&parse_case);
}

/* ---- Operation 2: parses that end in a parse error ---------------------- */

/*
 * What was read before each of the errors: the name, a tag that was
 * replaced, NAME and one of URL.
 */
static void check_read_before_error(const fixture_t *fixture, const clargs_result_t *result)
{
    check_commands(result, fixture, 3, fixture->add);
    NAOS_CHECK_TRUE(clargs_result_flag(result, fixture->verbose));
    check_option_word(result, fixture->name, "x", CLARGS_SOURCE_COMMAND_LINE);
    check_option_word(result, fixture->tag, "b", CLARGS_SOURCE_COMMAND_LINE);
    check_positional_word(result, fixture->item, 0, "one");
    NAOS_CHECK_UINT_EQ(clargs_result_positional_count(result, fixture->urls), 1);
    check_positional_word(result, fixture->urls, 0, "u1");
    /* A result with a parse error holds no default value. */
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, fixture->label), CLARGS_SOURCE_NONE);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, fixture->config), CLARGS_SOURCE_NONE);
}

static void check_unknown_option(const fixture_t *fixture, const clargs_result_t *result, size_t *held)
{
    check_error(result, CLARGS_PARSE_ERROR_UNKNOWN_OPTION, "unknown option '--bogus'");
    NAOS_CHECK_UINT_EQ(clargs_result_error_index(result), 12);
    check_read_before_error(fixture, result);
    *held = 4;
}

static void check_invalid_value(const fixture_t *fixture, const clargs_result_t *result, size_t *held)
{
    check_error(result, CLARGS_PARSE_ERROR_INVALID_VALUE,
                "invalid value 'bad' for argument 'URL' (expected word)");
    NAOS_CHECK_TRUE(clargs_result_error_positional(result) == fixture->urls);
    NAOS_CHECK_UINT_EQ(clargs_result_error_index(result), 12);
    check_read_before_error(fixture, result);
    *held = 4;
}

static void check_missing_positional(const fixture_t *fixture, const clargs_result_t *result, size_t *held)
{
    check_error(result, CLARGS_PARSE_ERROR_MISSING_POSITIONAL, "missing required argument 'NAME'");
    NAOS_CHECK_TRUE(clargs_result_error_positional(result) == fixture->item);
    NAOS_CHECK_INT_EQ(clargs_result_error_source(result), CLARGS_SOURCE_NONE);
    check_commands(result, fixture, 3, fixture->add);
    check_option_word(result, fixture->tag, "b", CLARGS_SOURCE_COMMAND_LINE);
    NAOS_CHECK_UINT_EQ(clargs_result_positional_count(result, fixture->item), 0);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, fixture->name), CLARGS_SOURCE_NONE);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, fixture->label), CLARGS_SOURCE_NONE);
    *held = 1;
}

/*
 * The default values that were applied before the one that failed are
 * taken back: the result holds no value.
 */
static void check_invalid_default(const fixture_t *fixture, const clargs_result_t *result, size_t *held)
{
    check_error(result, CLARGS_PARSE_ERROR_INVALID_VALUE,
                "invalid default value 'bad' for option '--template' (expected word)");
    NAOS_CHECK_TRUE(clargs_result_error_option(result) == fixture->template_name);
    NAOS_CHECK_INT_EQ(clargs_result_error_source(result), CLARGS_SOURCE_DEFAULT);
    check_commands(result, fixture, 2, fixture->init);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, fixture->config), CLARGS_SOURCE_NONE);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, fixture->jobs), CLARGS_SOURCE_NONE);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, fixture->owner), CLARGS_SOURCE_NONE);
    NAOS_CHECK_TRUE(clargs_result_option_value(result, fixture->owner) == NULL);
    NAOS_CHECK_TRUE(clargs_result_option_value(result, fixture->template_name) == NULL);
    NAOS_CHECK_UINT_EQ(clargs_result_positional_count(result, fixture->dir), 0);
    *held = 0;
}

NAOS_TEST(a_parse_that_ends_in_a_parse_error_fails_at_every_allocation_point)
{
    /*
     * An error of the scan, an error of a conversion, an error of the
     * check of what is required, and an error of a default value. The
     * message is the last allocation of each.
     */
    char *unknown[] = { "tool", "-v", "remote", "--name", "x",  "add",     "--tag",
                        "a",    "--tag", "b",   "one",    "u1", "--bogus", "u2" };
    char *invalid[] = { "tool", "-v", "remote", "--name", "x",  "add", "--tag",
                        "a",    "--tag", "b",   "one",    "u1", "bad", "u2" };
    char *missing[] = { "tool", "remote", "add", "--tag", "a", "-tb" };
    char *fallback[] = { "tool", "init" };
    parse_case_t parse_case;

    parse_case.wide_argv = NULL;
    parse_case.reject = true;
    parse_case.outcome = CLARGS_ERROR_INVALID_COMMAND_LINE;

    parse_case.argc = ARGC(unknown);
    parse_case.argv = unknown;
    parse_case.check_fn = check_unknown_option;
    /*
     * The result, its arguments, three selected commands, five values, the
     * values of URL, and the message.
     */
    parse_case.least = 12;
    fail_every_request(&parse_case);

    parse_case.argc = ARGC(invalid);
    parse_case.argv = invalid;
    parse_case.check_fn = check_invalid_value;
    parse_case.least = 12;
    fail_every_request(&parse_case);

    parse_case.argc = ARGC(missing);
    parse_case.argv = missing;
    parse_case.check_fn = check_missing_positional;
    /*
     * The result, its arguments, three selected commands, two values, and
     * the message.
     */
    parse_case.least = 7;
    fail_every_request(&parse_case);

    parse_case.argc = ARGC(fallback);
    parse_case.argv = fallback;
    parse_case.check_fn = check_invalid_default;
    /*
     * The result, its arguments, two selected commands, three default
     * values, and the message.
     */
    parse_case.least = 8;
    fail_every_request(&parse_case);
}

/* ---- Operation 3: a parse that applies default values ------------------- */

static void check_defaults(const fixture_t *fixture, const clargs_result_t *result, size_t *held)
{
    check_success(result);
    check_commands(result, fixture, 2, fixture->init);
    check_option_string(result, fixture->config, "tool.cfg", CLARGS_SOURCE_DEFAULT);
    check_option_int(result, fixture->jobs, 2, CLARGS_SOURCE_DEFAULT);
    check_option_word(result, fixture->owner, "nobody", CLARGS_SOURCE_DEFAULT);
    check_option_word(result, fixture->template_name, "bad", CLARGS_SOURCE_DEFAULT);
    NAOS_CHECK_INT_EQ(clargs_result_positional_source(result, fixture->dir), CLARGS_SOURCE_DEFAULT);
    NAOS_CHECK_UINT_EQ(clargs_result_positional_count(result, fixture->dir), 1);
    check_positional_word(result, fixture->dir, 0, ".");
    /* The owner, the template and DIR. */
    *held = 3;
}

NAOS_TEST(a_parse_that_applies_default_values_fails_at_every_allocation_point)
{
    char *argv[] = { "tool", "init" };
    parse_case_t parse_case;

    parse_case.argc = ARGC(argv);
    parse_case.argv = argv;
    parse_case.wide_argv = NULL;
    parse_case.reject = false;
    parse_case.outcome = CLARGS_ERROR_NONE;
    parse_case.check_fn = check_defaults;
    /*
     * The result, its arguments, two selected commands, and five default
     * values, one of them in the array of the values of DIR.
     */
    parse_case.least = 9;
    fail_every_request(&parse_case);
}

/* ---- Operation 4: parses through the wide entry ------------------------- */

static void check_wide(const fixture_t *fixture, const clargs_result_t *result, size_t *held)
{
    check_success(result);
    check_commands(result, fixture, 3, fixture->add);
    NAOS_CHECK_TRUE(clargs_result_flag(result, fixture->verbose));
    check_option_word(result, fixture->name, U_2, CLARGS_SOURCE_COMMAND_LINE);
    check_option_word(result, fixture->tag, U_3, CLARGS_SOURCE_COMMAND_LINE);
    check_option_word(result, fixture->label, "none", CLARGS_SOURCE_DEFAULT);
    check_option_string(result, fixture->config, "tool.cfg", CLARGS_SOURCE_DEFAULT);
    check_positional_word(result, fixture->item, 0, U_4);
    NAOS_CHECK_UINT_EQ(clargs_result_positional_count(result, fixture->urls), 1);
    check_positional_word(result, fixture->urls, 0, "u1");
    /* The name, the tag, the label, NAME and one of URL. */
    *held = 5;
}

/*
 * The argument that cannot be converted is the fifth of the command line;
 * what was read before it stays.
 */
static void check_wide_invalid(const fixture_t *fixture, const clargs_result_t *result, size_t *held)
{
    check_error(result, CLARGS_PARSE_ERROR_INVALID_ENCODING,
                "invalid character encoding in argument 'a" REPLACEMENT "b'");
    NAOS_CHECK_UINT_EQ(clargs_result_error_index(result), 4);
    check_commands(result, fixture, 2, fixture->remote);
    NAOS_CHECK_TRUE(clargs_result_flag(result, fixture->verbose));
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, fixture->name), CLARGS_SOURCE_NONE);
    *held = 0;
}

NAOS_TEST(a_parse_of_wide_arguments_fails_at_every_allocation_point)
{
    wchar_t *argv[] = { L"tool", L"-v", L"remote", L"--name", W_2, L"add", (L"--tag=" W_3), W_4, L"u1" };
    wchar_t *invalid[] = { L"tool", L"-v", L"remote", L"--name", NULL, L"add" };
    wchar_t bad[4];
    parse_case_t parse_case;

    /*
     * A surrogate that is not part of a pair where a wide string is
     * UTF-16, and a value of the range of the surrogates where it is
     * UTF-32: it cannot be converted in either.
     */
    bad[0] = L'a';
    bad[1] = (wchar_t)0xD800;
    bad[2] = L'b';
    bad[3] = L'\0';
    invalid[4] = bad;

    parse_case.argv = NULL;
    parse_case.reject = false;

    parse_case.argc = ARGC(argv);
    parse_case.wide_argv = argv;
    parse_case.outcome = CLARGS_ERROR_NONE;
    parse_case.check_fn = check_wide;
    /*
     * The result, its arguments, three selected commands, and the
     * values.
     */
    parse_case.least = 12;
    fail_every_request(&parse_case);

    parse_case.argc = ARGC(invalid);
    parse_case.wide_argv = invalid;
    parse_case.outcome = CLARGS_ERROR_INVALID_COMMAND_LINE;
    parse_case.check_fn = check_wide_invalid;
    /* The result, its arguments, two selected commands, and the message. */
    parse_case.least = 5;
    fail_every_request(&parse_case);
}

NAOS_SUITE(failure,
    NAOS_ADD_TEST(a_parse_that_succeeds_fails_at_every_allocation_point),
    NAOS_ADD_TEST(a_parse_that_ends_in_a_parse_error_fails_at_every_allocation_point),
    NAOS_ADD_TEST(a_parse_that_applies_default_values_fails_at_every_allocation_point),
    NAOS_ADD_TEST(a_parse_of_wide_arguments_fails_at_every_allocation_point)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(failure)
)
