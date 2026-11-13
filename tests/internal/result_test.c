/*
 * Tests of the internal functions that build a result, "parse_internal.h",
 * and of the public functions that read and destroy one, <clargs/parse.h>.
 *
 * There is no test here that reads a field of a result: a result is built
 * with the internal functions and read with the functions that the parser
 * and the consumer use.
 *
 * Every test uses the test allocator ("../support/test_allocator.h"): its
 * count of requests shows how many allocations a step takes, its count of
 * live blocks shows that a destroyed result and a failed step leave nothing
 * behind, and its count of violations shows a call that the library must
 * never make.
 *
 * The state of the test allocator is shared by all the tests of this
 * executable. Every test starts with an install (which resets), destroys
 * what it created, sets the default allocator back before it ends, and ends
 * with check_clean().
 */
#include <clargs/clargs.h>

#include "parse_internal.h"

#include "../support/test_allocator.h"

#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* The end of every test: the test allocator has no live block and saw no
 * violation of the allocator contract. */
static void check_clean(void)
{
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
}

/* Creates a root with the name "app". The test allocator is installed. */
static clargs_command_t *new_root(void)
{
    clargs_command_t *root = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_create("app", &root) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(root != NULL);
    return root;
}

/* Installs the test allocator and creates a root with the name "app". */
static clargs_command_t *create_root(void)
{
    clargs_test_allocator_install();
    return new_root();
}

/* Destroys root and ends the test: nothing stays allocated. */
static void destroy_root(clargs_command_t *root)
{
    clargs_command_destroy(root);
    clargs_test_allocator_uninstall();
    check_clean();
}

/* Adds a subcommand, which must succeed, and returns it. */
static clargs_command_t *add_sub(clargs_command_t *command, const char *name)
{
    clargs_command_t *sub = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_subcommand(command, name, &sub) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(sub != NULL);
    return sub;
}

/* Adds one option of each kind to command: -f/--flag, -c/--color (a
 * negatable flag that starts on), -v/--verbose (a count) and -o/--out (a
 * string). All of them must succeed. */
static void add_four_options(clargs_command_t *command)
{
    clargs_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_flag(command, 'f', "flag", &option) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_command_add_negatable(command, 'c', "color", &option) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_option_set_initial(option, true) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_command_add_count(command, 'v', "verbose", &option) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_command_add_value(command, 'o', "out", clargs_type_string(), &option)
                      == CLARGS_ERROR_NONE);
}

/* Adds a required positional "input" and a positional "rest" that takes
 * any number of arguments to command. Both must succeed. */
static void add_two_positionals(clargs_command_t *command)
{
    clargs_positional_t *positional = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_positional(command, "input", clargs_type_string(),
                                                    CLARGS_ARITY_REQUIRED, &positional)
                      == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_command_add_positional(command, "rest", clargs_type_int(),
                                                    CLARGS_ARITY_ZERO_OR_MORE, &positional)
                      == CLARGS_ERROR_NONE);
}

/* Creates a result of root from argc and argv, which must succeed. */
static clargs_result_t *create_result(const clargs_command_t *root, int argc, char *const *argv)
{
    clargs_result_t *result = NULL;

    NAOS_REQUIRE_TRUE(clargs_result_create(root, argc, argv, &result) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(result != NULL);
    return result;
}

/* The number of requests that the creation of a result of root from argc
 * and argv takes. The result is destroyed again. */
static size_t requests_of_create(const clargs_command_t *root, int argc, char *const *argv)
{
    size_t before = clargs_test_allocator_requests();
    clargs_result_t *result = create_result(root, argc, argv);
    size_t requests = clargs_test_allocator_requests() - before;

    clargs_result_destroy(result);
    return requests;
}

/* ---- Destruction -------------------------------------------------------- */

NAOS_TEST(destroy_of_null_does_nothing)
{
    clargs_test_allocator_install();

    clargs_result_destroy(NULL);

    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 0);
    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(destroy_frees_everything_and_keeps_the_schema)
{
    char first[] = "app";
    char second[] = "--flag";
    char *argv[2];
    clargs_command_t *root = create_root();
    clargs_command_t *sub;
    clargs_result_t *result;
    size_t live_of_schema;

    argv[0] = first;
    argv[1] = second;
    /* A command with subcommands has no positionals. */
    add_four_options(root);
    sub = add_sub(root, "sub");
    add_four_options(sub);
    add_two_positionals(sub);
    live_of_schema = clargs_test_allocator_live();

    result = create_result(root, 2, argv);
    NAOS_REQUIRE_TRUE(clargs_result_push_command(result, sub) == CLARGS_ERROR_NONE);
    NAOS_CHECK_TRUE(clargs_test_allocator_live() > live_of_schema);

    clargs_result_destroy(result);

    /* Everything of the result is freed, and nothing of the schema. */
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live_of_schema);
    NAOS_CHECK_UINT_EQ(clargs_command_option_count(root), 4);
    NAOS_CHECK_UINT_EQ(clargs_command_positional_count(sub), 2);
    NAOS_CHECK_UINT_EQ(clargs_command_subcommand_count(root), 1);
    NAOS_CHECK_TRUE(clargs_command_subcommand_at(root, 0) == sub);
    NAOS_CHECK_STR_EQ(clargs_option_long_name(clargs_command_option_at(sub, 3)), "out");
    NAOS_CHECK_STR_EQ(clargs_positional_name(clargs_command_positional_at(sub, 1)), "rest");

    /* The schema can be used for another result. */
    result = create_result(root, 2, argv);
    clargs_result_destroy(result);

    destroy_root(root);
}

/* ---- The new result ----------------------------------------------------- */

NAOS_TEST(new_result_has_no_error_and_selects_the_root)
{
    clargs_command_t *root = create_root();
    clargs_result_t *result;

    add_sub(root, "sub");
    result = create_result(root, 0, NULL);

    NAOS_CHECK_INT_EQ(clargs_result_error(result), CLARGS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(clargs_result_command_count(result), 1);
    NAOS_CHECK_TRUE(clargs_result_command_at(result, 0) == root);
    NAOS_CHECK_TRUE(clargs_result_command(result) == root);
    NAOS_CHECK_UINT_EQ(clargs_result_first_invalid_arg(result), 0);

    clargs_result_destroy(result);
    destroy_root(root);
}

/* ---- The copy of the arguments ------------------------------------------ */

NAOS_TEST(no_argument_takes_no_copy)
{
    char only[] = "app";
    char *argv[1];
    clargs_command_t *root = create_root();
    size_t without;
    size_t with;

    argv[0] = only;

    /* The array is not read when argc is 0. */
    without = requests_of_create(root, 0, NULL);
    NAOS_CHECK_UINT_EQ(requests_of_create(root, 0, argv), without);

    /* The copy is the one allocation more. */
    with = requests_of_create(root, 1, argv);
    NAOS_CHECK_UINT_EQ(with, without + 1);

    destroy_root(root);
}

NAOS_TEST(one_argument_is_copied)
{
    char only[] = "app";
    char *argv[1];
    clargs_command_t *root = create_root();
    clargs_result_t *result;

    argv[0] = only;
    result = create_result(root, 1, argv);

    NAOS_CHECK_STR_EQ(clargs_result_arg(result, 0), "app");
    NAOS_CHECK_TRUE(clargs_result_arg(result, 0) != only);
    NAOS_CHECK_UINT_EQ(clargs_result_first_invalid_arg(result), 0);

    clargs_result_destroy(result);
    destroy_root(root);
}

NAOS_TEST(several_arguments_are_copied_in_one_allocation)
{
    char a0[] = "app";
    char a1[] = "--out=file name";
    char a2[] = "";
    char a3[] = "-";
    char a4[] = "a much longer argument, which is longer than a pointer, to move what follows";
    char a5[] = "last";
    char *argv[6];
    const char *expected[6] = {
        "app",
        "--out=file name",
        "",
        "-",
        "a much longer argument, which is longer than a pointer, to move what follows",
        "last"
    };
    clargs_command_t *root = create_root();
    clargs_result_t *result;
    size_t without;
    size_t i;

    argv[0] = a0;
    argv[1] = a1;
    argv[2] = a2;
    argv[3] = a3;
    argv[4] = a4;
    argv[5] = a5;

    /* However many arguments there are, the copy is one allocation. */
    without = requests_of_create(root, 0, NULL);
    NAOS_CHECK_UINT_EQ(requests_of_create(root, 6, argv), without + 1);

    result = create_result(root, 6, argv);
    for (i = 0; i < 6; ++i) {
        NAOS_CHECK_STR_EQ(clargs_result_arg(result, i), expected[i]);
        NAOS_CHECK_TRUE(clargs_result_arg(result, i) != argv[i]);
    }
    NAOS_CHECK_UINT_EQ(clargs_result_first_invalid_arg(result), 0);

    clargs_result_destroy(result);
    destroy_root(root);
}

NAOS_TEST(the_same_string_twice_is_copied_twice)
{
    char only[] = "same";
    char *argv[3];
    clargs_command_t *root = create_root();
    clargs_result_t *result;

    argv[0] = only;
    argv[1] = only;
    argv[2] = only;
    result = create_result(root, 3, argv);

    NAOS_CHECK_STR_EQ(clargs_result_arg(result, 0), "same");
    NAOS_CHECK_STR_EQ(clargs_result_arg(result, 1), "same");
    NAOS_CHECK_STR_EQ(clargs_result_arg(result, 2), "same");
    NAOS_CHECK_TRUE(clargs_result_arg(result, 0) != clargs_result_arg(result, 1));
    NAOS_CHECK_TRUE(clargs_result_arg(result, 1) != clargs_result_arg(result, 2));

    clargs_result_destroy(result);
    destroy_root(root);
}

NAOS_TEST(copy_does_not_change_when_the_arguments_change)
{
    char a0[] = "app";
    char a1[] = "--name=value";
    char a2[] = "input";
    char *argv[3];
    clargs_command_t *root = create_root();
    clargs_result_t *result;

    argv[0] = a0;
    argv[1] = a1;
    argv[2] = a2;
    result = create_result(root, 3, argv);

    /* The strings and the array are the caller's again after the call. */
    memset(a0, 'x', sizeof(a0) - 1);
    memset(a1, 'y', sizeof(a1) - 1);
    a2[0] = '\0';
    argv[0] = NULL;
    argv[1] = NULL;
    argv[2] = NULL;

    NAOS_CHECK_STR_EQ(clargs_result_arg(result, 0), "app");
    NAOS_CHECK_STR_EQ(clargs_result_arg(result, 1), "--name=value");
    NAOS_CHECK_STR_EQ(clargs_result_arg(result, 2), "input");

    clargs_result_destroy(result);
    destroy_root(root);
}

NAOS_TEST(bytes_are_copied_as_they_are)
{
    /* Bytes that are not UTF-8, a control character and a byte with the
     * highest bit set at the start and at the end. */
    char a0[] = { '\xff', 'a', '\x80', '\0' };
    char a1[] = { '\xc3', '\x28', '\t', '\x01', '\xfe', '\0' };
    char a2[] = { '\xe3', '\x81', '\x82', '\0' };
    char *argv[3];
    clargs_command_t *root = create_root();
    clargs_result_t *result;

    argv[0] = a0;
    argv[1] = a1;
    argv[2] = a2;
    result = create_result(root, 3, argv);

    NAOS_CHECK_MEM_EQ(clargs_result_arg(result, 0), sizeof(a0), a0, sizeof(a0));
    NAOS_CHECK_MEM_EQ(clargs_result_arg(result, 1), sizeof(a1), a1, sizeof(a1));
    NAOS_CHECK_MEM_EQ(clargs_result_arg(result, 2), sizeof(a2), a2, sizeof(a2));

    /* Bytes are not looked at: nothing is reported as invalid. */
    NAOS_CHECK_UINT_EQ(clargs_result_first_invalid_arg(result), 0);
    NAOS_CHECK_INT_EQ(clargs_result_error(result), CLARGS_ERROR_NONE);

    clargs_result_destroy(result);
    destroy_root(root);
}

/* ---- The selected commands ---------------------------------------------- */

NAOS_TEST(pushed_commands_are_read_in_their_order)
{
    clargs_command_t *root = create_root();
    clargs_command_t *other = add_sub(root, "other");
    clargs_command_t *remote = add_sub(root, "remote");
    clargs_command_t *add = add_sub(remote, "add");
    clargs_result_t *result = create_result(root, 0, NULL);

    NAOS_CHECK_TRUE(other != remote);

    NAOS_CHECK_INT_EQ(clargs_result_push_command(result, remote), CLARGS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(clargs_result_command_count(result), 2);
    NAOS_CHECK_TRUE(clargs_result_command_at(result, 0) == root);
    NAOS_CHECK_TRUE(clargs_result_command_at(result, 1) == remote);
    NAOS_CHECK_TRUE(clargs_result_command(result) == remote);

    NAOS_CHECK_INT_EQ(clargs_result_push_command(result, add), CLARGS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(clargs_result_command_count(result), 3);
    NAOS_CHECK_TRUE(clargs_result_command_at(result, 0) == root);
    NAOS_CHECK_TRUE(clargs_result_command_at(result, 1) == remote);
    NAOS_CHECK_TRUE(clargs_result_command_at(result, 2) == add);
    NAOS_CHECK_TRUE(clargs_result_command(result) == add);

    /* Pushing commands records no error. */
    NAOS_CHECK_INT_EQ(clargs_result_error(result), CLARGS_ERROR_NONE);

    clargs_result_destroy(result);
    destroy_root(root);
}

NAOS_TEST(many_nested_commands_are_kept)
{
    enum { DEPTH = 20 };
    clargs_command_t *commands[DEPTH + 1];
    clargs_command_t *root = create_root();
    clargs_result_t *result;
    size_t i;

    commands[0] = root;
    for (i = 1; i <= DEPTH; ++i) {
        commands[i] = add_sub(commands[i - 1], "sub");
    }

    result = create_result(root, 0, NULL);
    for (i = 1; i <= DEPTH; ++i) {
        NAOS_REQUIRE_TRUE(clargs_result_push_command(result, commands[i]) == CLARGS_ERROR_NONE);
    }

    NAOS_CHECK_UINT_EQ(clargs_result_command_count(result), DEPTH + 1);
    for (i = 0; i <= DEPTH; ++i) {
        NAOS_CHECK_TRUE(clargs_result_command_at(result, i) == commands[i]);
    }
    NAOS_CHECK_TRUE(clargs_result_command(result) == commands[DEPTH]);

    clargs_result_destroy(result);
    destroy_root(root);
}

/* The number of requests that pushing command, a subcommand of the root of
 * result, takes. */
static size_t requests_of_push(clargs_result_t *result, const clargs_command_t *command)
{
    size_t before = clargs_test_allocator_requests();

    NAOS_REQUIRE_TRUE(clargs_result_push_command(result, command) == CLARGS_ERROR_NONE);
    return clargs_test_allocator_requests() - before;
}

NAOS_TEST(slots_are_allocated_only_for_what_a_command_has)
{
    clargs_command_t *root = create_root();
    clargs_command_t *bare = add_sub(root, "bare");
    clargs_command_t *options = add_sub(root, "options");
    clargs_command_t *positionals = add_sub(root, "positionals");
    clargs_command_t *both = add_sub(root, "both");
    clargs_result_t *result;

    add_four_options(options);
    add_two_positionals(positionals);
    add_four_options(both);
    add_two_positionals(both);

    /* Each result has two selected commands, so the array of the selected
     * commands does not grow at the push: the requests are those of the
     * slots. */
    result = create_result(root, 0, NULL);
    NAOS_CHECK_UINT_EQ(requests_of_push(result, bare), 0);
    clargs_result_destroy(result);

    result = create_result(root, 0, NULL);
    NAOS_CHECK_UINT_EQ(requests_of_push(result, options), 1);
    clargs_result_destroy(result);

    result = create_result(root, 0, NULL);
    NAOS_CHECK_UINT_EQ(requests_of_push(result, positionals), 1);
    clargs_result_destroy(result);

    result = create_result(root, 0, NULL);
    NAOS_CHECK_UINT_EQ(requests_of_push(result, both), 2);
    clargs_result_destroy(result);

    destroy_root(root);
}

NAOS_TEST(slots_of_the_root_are_allocated_at_the_creation)
{
    clargs_command_t *bare;
    clargs_command_t *full;
    size_t requests_of_bare;

    clargs_test_allocator_install();
    bare = new_root();
    full = new_root();
    add_four_options(full);
    add_two_positionals(full);

    requests_of_bare = requests_of_create(bare, 0, NULL);
    NAOS_CHECK_UINT_EQ(requests_of_create(full, 0, NULL), requests_of_bare + 2);

    clargs_command_destroy(bare);
    destroy_root(full);
}

/* ---- Flags, counts and negatable flags ---------------------------------- */

/* The options that add_four_options() adds, by their index in the command. */
enum { FLAG = 0, COLOR = 1, VERBOSE = 2, OUT = 3 };

/* The option of command at index. */
static const clargs_option_t *option_of(const clargs_command_t *command, size_t index)
{
    return clargs_command_option_at(command, index);
}

NAOS_TEST(slots_start_as_not_given)
{
    clargs_command_t *root = create_root();
    clargs_option_t *plain = NULL;
    clargs_result_t *result;

    /* --color starts on; --plain keeps the initial state of the schema, off. */
    add_four_options(root);
    NAOS_REQUIRE_TRUE(clargs_command_add_negatable(root, 'p', "plain", &plain) == CLARGS_ERROR_NONE);
    result = create_result(root, 0, NULL);

    NAOS_CHECK_FALSE(clargs_result_flag(result, option_of(root, FLAG)));
    NAOS_CHECK_TRUE(clargs_result_flag(result, option_of(root, COLOR)));
    NAOS_CHECK_FALSE(clargs_result_flag(result, plain));
    NAOS_CHECK_UINT_EQ(clargs_result_count(result, option_of(root, VERBOSE)), 0);

    /* What a flag, a negatable flag and a count have when they are not
     * given comes from the schema; an option with a value has no value. */
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, FLAG)), CLARGS_SOURCE_DEFAULT);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, COLOR)), CLARGS_SOURCE_DEFAULT);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, plain), CLARGS_SOURCE_DEFAULT);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, VERBOSE)),
                      CLARGS_SOURCE_DEFAULT);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, OUT)), CLARGS_SOURCE_NONE);

    clargs_result_destroy(result);
    destroy_root(root);
}

NAOS_TEST(slots_of_a_pushed_command_start_as_not_given)
{
    clargs_command_t *root = create_root();
    clargs_command_t *sub = add_sub(root, "sub");
    clargs_result_t *result;

    add_four_options(sub);
    result = create_result(root, 0, NULL);
    NAOS_REQUIRE_TRUE(clargs_result_push_command(result, sub) == CLARGS_ERROR_NONE);

    NAOS_CHECK_FALSE(clargs_result_flag(result, option_of(sub, FLAG)));
    NAOS_CHECK_TRUE(clargs_result_flag(result, option_of(sub, COLOR)));
    NAOS_CHECK_UINT_EQ(clargs_result_count(result, option_of(sub, VERBOSE)), 0);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(sub, FLAG)), CLARGS_SOURCE_DEFAULT);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(sub, COLOR)), CLARGS_SOURCE_DEFAULT);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(sub, VERBOSE)), CLARGS_SOURCE_DEFAULT);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(sub, OUT)), CLARGS_SOURCE_NONE);

    clargs_result_destroy(result);
    destroy_root(root);
}

NAOS_TEST(flag_recorded_twice_is_given)
{
    clargs_command_t *root = create_root();
    clargs_result_t *result;

    add_four_options(root);
    result = create_result(root, 0, NULL);

    clargs_result_record_flag(result, option_of(root, FLAG));
    NAOS_CHECK_TRUE(clargs_result_flag(result, option_of(root, FLAG)));
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, FLAG)),
                      CLARGS_SOURCE_COMMAND_LINE);

    clargs_result_record_flag(result, option_of(root, FLAG));
    NAOS_CHECK_TRUE(clargs_result_flag(result, option_of(root, FLAG)));
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, FLAG)),
                      CLARGS_SOURCE_COMMAND_LINE);

    /* The other options are as they were. */
    NAOS_CHECK_TRUE(clargs_result_flag(result, option_of(root, COLOR)));
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, COLOR)), CLARGS_SOURCE_DEFAULT);
    NAOS_CHECK_UINT_EQ(clargs_result_count(result, option_of(root, VERBOSE)), 0);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, VERBOSE)),
                      CLARGS_SOURCE_DEFAULT);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, OUT)), CLARGS_SOURCE_NONE);
    NAOS_CHECK_INT_EQ(clargs_result_error(result), CLARGS_ERROR_NONE);

    clargs_result_destroy(result);
    destroy_root(root);
}

NAOS_TEST(count_is_the_number_of_records)
{
    clargs_command_t *root = create_root();
    clargs_result_t *result;
    size_t i;

    add_four_options(root);
    result = create_result(root, 0, NULL);

    clargs_result_record_count(result, option_of(root, VERBOSE));
    NAOS_CHECK_UINT_EQ(clargs_result_count(result, option_of(root, VERBOSE)), 1);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, VERBOSE)),
                      CLARGS_SOURCE_COMMAND_LINE);

    for (i = 2; i <= 40; ++i) {
        clargs_result_record_count(result, option_of(root, VERBOSE));
        NAOS_CHECK_UINT_EQ(clargs_result_count(result, option_of(root, VERBOSE)), i);
    }
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, VERBOSE)),
                      CLARGS_SOURCE_COMMAND_LINE);

    /* The other options are as they were. */
    NAOS_CHECK_FALSE(clargs_result_flag(result, option_of(root, FLAG)));
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, FLAG)), CLARGS_SOURCE_DEFAULT);
    NAOS_CHECK_TRUE(clargs_result_flag(result, option_of(root, COLOR)));
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, COLOR)), CLARGS_SOURCE_DEFAULT);

    clargs_result_destroy(result);
    destroy_root(root);
}

NAOS_TEST(negatable_flag_has_the_last_recorded_state)
{
    clargs_command_t *root = create_root();
    clargs_option_t *plain = NULL;
    clargs_result_t *result;

    add_four_options(root);
    NAOS_REQUIRE_TRUE(clargs_command_add_negatable(root, 'p', "plain", &plain) == CLARGS_ERROR_NONE);
    result = create_result(root, 0, NULL);

    /* --color starts on. */
    clargs_result_record_state(result, option_of(root, COLOR), false);
    NAOS_CHECK_FALSE(clargs_result_flag(result, option_of(root, COLOR)));
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, COLOR)),
                      CLARGS_SOURCE_COMMAND_LINE);
    clargs_result_record_state(result, option_of(root, COLOR), true);
    NAOS_CHECK_TRUE(clargs_result_flag(result, option_of(root, COLOR)));
    clargs_result_record_state(result, option_of(root, COLOR), true);
    NAOS_CHECK_TRUE(clargs_result_flag(result, option_of(root, COLOR)));
    clargs_result_record_state(result, option_of(root, COLOR), false);
    NAOS_CHECK_FALSE(clargs_result_flag(result, option_of(root, COLOR)));
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, COLOR)),
                      CLARGS_SOURCE_COMMAND_LINE);

    /* --plain starts off. The state that is recorded may be the initial
     * one: the source still tells that it was given. */
    clargs_result_record_state(result, plain, false);
    NAOS_CHECK_FALSE(clargs_result_flag(result, plain));
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, plain), CLARGS_SOURCE_COMMAND_LINE);
    clargs_result_record_state(result, plain, true);
    NAOS_CHECK_TRUE(clargs_result_flag(result, plain));

    /* The other options are as they were. */
    NAOS_CHECK_FALSE(clargs_result_flag(result, option_of(root, FLAG)));
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, FLAG)), CLARGS_SOURCE_DEFAULT);
    NAOS_CHECK_UINT_EQ(clargs_result_count(result, option_of(root, VERBOSE)), 0);

    clargs_result_destroy(result);
    destroy_root(root);
}

NAOS_TEST(options_of_each_selected_command_are_kept_apart)
{
    clargs_command_t *root = create_root();
    clargs_command_t *sub = add_sub(root, "sub");
    clargs_command_t *leaf = add_sub(sub, "leaf");
    clargs_result_t *result;

    /* The three commands have options of the same names. */
    add_four_options(root);
    add_four_options(sub);
    add_four_options(leaf);
    result = create_result(root, 0, NULL);
    NAOS_REQUIRE_TRUE(clargs_result_push_command(result, sub) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_result_push_command(result, leaf) == CLARGS_ERROR_NONE);

    clargs_result_record_flag(result, option_of(root, FLAG));
    clargs_result_record_count(result, option_of(sub, VERBOSE));
    clargs_result_record_count(result, option_of(sub, VERBOSE));
    clargs_result_record_state(result, option_of(leaf, COLOR), false);
    clargs_result_record_count(result, option_of(leaf, VERBOSE));

    NAOS_CHECK_TRUE(clargs_result_flag(result, option_of(root, FLAG)));
    NAOS_CHECK_TRUE(clargs_result_flag(result, option_of(root, COLOR)));
    NAOS_CHECK_UINT_EQ(clargs_result_count(result, option_of(root, VERBOSE)), 0);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, FLAG)),
                      CLARGS_SOURCE_COMMAND_LINE);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, VERBOSE)),
                      CLARGS_SOURCE_DEFAULT);

    NAOS_CHECK_FALSE(clargs_result_flag(result, option_of(sub, FLAG)));
    NAOS_CHECK_TRUE(clargs_result_flag(result, option_of(sub, COLOR)));
    NAOS_CHECK_UINT_EQ(clargs_result_count(result, option_of(sub, VERBOSE)), 2);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(sub, FLAG)), CLARGS_SOURCE_DEFAULT);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(sub, VERBOSE)),
                      CLARGS_SOURCE_COMMAND_LINE);

    NAOS_CHECK_FALSE(clargs_result_flag(result, option_of(leaf, FLAG)));
    NAOS_CHECK_FALSE(clargs_result_flag(result, option_of(leaf, COLOR)));
    NAOS_CHECK_UINT_EQ(clargs_result_count(result, option_of(leaf, VERBOSE)), 1);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(leaf, COLOR)),
                      CLARGS_SOURCE_COMMAND_LINE);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(leaf, OUT)), CLARGS_SOURCE_NONE);

    clargs_result_destroy(result);
    destroy_root(root);
}

NAOS_TEST(options_of_a_command_that_is_not_selected_read_as_not_given)
{
    clargs_command_t *root = create_root();
    clargs_command_t *remote = add_sub(root, "remote");
    clargs_command_t *add = add_sub(remote, "add");
    clargs_command_t *other = add_sub(root, "other");
    clargs_result_t *result;

    add_four_options(root);
    add_four_options(remote);
    add_four_options(add);
    add_four_options(other);
    result = create_result(root, 0, NULL);

    /* Only the root is selected: the options of its subcommand, of the
     * subcommand of that, and of the other subcommand have no source. */
    NAOS_CHECK_FALSE(clargs_result_flag(result, option_of(remote, FLAG)));
    NAOS_CHECK_TRUE(clargs_result_flag(result, option_of(remote, COLOR)));
    NAOS_CHECK_UINT_EQ(clargs_result_count(result, option_of(remote, VERBOSE)), 0);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(remote, FLAG)), CLARGS_SOURCE_NONE);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(remote, COLOR)), CLARGS_SOURCE_NONE);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(remote, VERBOSE)), CLARGS_SOURCE_NONE);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(remote, OUT)), CLARGS_SOURCE_NONE);
    NAOS_CHECK_FALSE(clargs_result_flag(result, option_of(add, FLAG)));
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(add, VERBOSE)), CLARGS_SOURCE_NONE);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(other, COLOR)), CLARGS_SOURCE_NONE);

    /* Once a command is selected, its options have slots. What is recorded
     * for the same names of the selected commands does not show in the
     * commands that are not selected. */
    NAOS_REQUIRE_TRUE(clargs_result_push_command(result, remote) == CLARGS_ERROR_NONE);
    clargs_result_record_flag(result, option_of(root, FLAG));
    clargs_result_record_flag(result, option_of(remote, FLAG));
    clargs_result_record_state(result, option_of(remote, COLOR), false);
    clargs_result_record_count(result, option_of(remote, VERBOSE));

    NAOS_CHECK_TRUE(clargs_result_flag(result, option_of(remote, FLAG)));
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(remote, FLAG)),
                      CLARGS_SOURCE_COMMAND_LINE);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(remote, OUT)), CLARGS_SOURCE_NONE);

    NAOS_CHECK_FALSE(clargs_result_flag(result, option_of(add, FLAG)));
    NAOS_CHECK_TRUE(clargs_result_flag(result, option_of(add, COLOR)));
    NAOS_CHECK_UINT_EQ(clargs_result_count(result, option_of(add, VERBOSE)), 0);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(add, FLAG)), CLARGS_SOURCE_NONE);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(add, COLOR)), CLARGS_SOURCE_NONE);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(add, VERBOSE)), CLARGS_SOURCE_NONE);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(add, OUT)), CLARGS_SOURCE_NONE);

    NAOS_CHECK_FALSE(clargs_result_flag(result, option_of(other, FLAG)));
    NAOS_CHECK_TRUE(clargs_result_flag(result, option_of(other, COLOR)));
    NAOS_CHECK_UINT_EQ(clargs_result_count(result, option_of(other, VERBOSE)), 0);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(other, FLAG)), CLARGS_SOURCE_NONE);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(other, COLOR)), CLARGS_SOURCE_NONE);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(other, VERBOSE)), CLARGS_SOURCE_NONE);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(other, OUT)), CLARGS_SOURCE_NONE);

    /* Asking about them is not an error of the result. */
    NAOS_CHECK_INT_EQ(clargs_result_error(result), CLARGS_ERROR_NONE);

    clargs_result_destroy(result);
    destroy_root(root);
}

NAOS_TEST(recording_and_reading_allocate_nothing)
{
    clargs_command_t *root = create_root();
    clargs_command_t *sub = add_sub(root, "sub");
    clargs_command_t *other = add_sub(root, "other");
    clargs_result_t *result;
    size_t before;

    add_four_options(root);
    add_four_options(sub);
    add_four_options(other);
    result = create_result(root, 0, NULL);
    NAOS_REQUIRE_TRUE(clargs_result_push_command(result, sub) == CLARGS_ERROR_NONE);
    before = clargs_test_allocator_requests();

    clargs_result_record_flag(result, option_of(sub, FLAG));
    clargs_result_record_state(result, option_of(sub, COLOR), false);
    clargs_result_record_count(result, option_of(sub, VERBOSE));
    NAOS_CHECK_TRUE(clargs_result_flag(result, option_of(sub, FLAG)));
    NAOS_CHECK_FALSE(clargs_result_flag(result, option_of(sub, COLOR)));
    NAOS_CHECK_UINT_EQ(clargs_result_count(result, option_of(sub, VERBOSE)), 1);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(sub, OUT)), CLARGS_SOURCE_NONE);
    NAOS_CHECK_FALSE(clargs_result_flag(result, option_of(other, FLAG)));
    NAOS_CHECK_UINT_EQ(clargs_result_count(result, option_of(other, VERBOSE)), 0);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(other, OUT)), CLARGS_SOURCE_NONE);

    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), before);

    clargs_result_destroy(result);
    destroy_root(root);
}

/* ---- The failure of an allocation --------------------------------------- */

/*
 * The operation of the test below: creates a result of commands[0] from
 * three arguments and pushes commands[1] to commands[count - 1], which
 * makes the array of the selected commands grow. Stops at the first step
 * that fails and returns its error; *out is what there is to destroy.
 * *pushed is the number of pushes that succeeded.
 */
static clargs_error_t create_and_push(clargs_command_t *const *commands, size_t count,
                                      clargs_result_t **out, size_t *pushed)
{
    char a0[] = "app";
    char a1[] = "sub";
    char a2[] = "--flag";
    char *argv[3];
    clargs_error_t error;
    size_t i;

    argv[0] = a0;
    argv[1] = a1;
    argv[2] = a2;
    *pushed = 0;

    error = clargs_result_create(commands[0], 3, argv, out);
    if (error != CLARGS_ERROR_NONE) {
        return error;
    }
    for (i = 1; i < count; ++i) {
        error = clargs_result_push_command(*out, commands[i]);
        if (error != CLARGS_ERROR_NONE) {
            return error;
        }
        ++*pushed;
    }
    return CLARGS_ERROR_NONE;
}

NAOS_TEST(fails_at_every_allocation_point)
{
    enum { COUNT = 7 };
    const size_t limit = 100;
    clargs_command_t *commands[COUNT];
    clargs_command_t *root;
    size_t live_of_schema;
    size_t failures = 0;
    size_t failures_of_create = 0;
    size_t failures_of_push = 0;
    int completed = 0;
    size_t i;
    size_t n;

    /* Every command has options, so every step allocates slots. The last
     * command has positionals too (a command with subcommands has none),
     * so the last push allocates both arrays of slots. */
    clargs_test_allocator_install();
    root = new_root();
    commands[0] = root;
    add_four_options(root);
    for (i = 1; i < COUNT; ++i) {
        commands[i] = add_sub(commands[i - 1], "sub");
        add_four_options(commands[i]);
    }
    add_two_positionals(commands[COUNT - 1]);
    live_of_schema = clargs_test_allocator_live();
    clargs_test_allocator_uninstall();

    for (n = 1; n <= limit; ++n) {
        clargs_result_t *result = NULL;
        clargs_error_t error;
        size_t pushed;

        /* The schema was allocated before the reset, so the count of live
         * blocks is that of the result alone from here on; the schema is
         * destroyed after the last reset below. */
        clargs_test_allocator_install();
        clargs_test_allocator_fail_at(n);

        error = create_and_push(commands, COUNT, &result, &pushed);

        if (error == CLARGS_ERROR_NONE && clargs_test_allocator_requests() < n) {
            /* The operation ended before the request to fail was reached:
             * every allocation point has been passed. */
            NAOS_CHECK_UINT_EQ(clargs_result_command_count(result), COUNT);
            NAOS_CHECK_TRUE(clargs_result_command(result) == commands[COUNT - 1]);
            clargs_result_destroy(result);
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
            completed = 1;
            break;
        }

        /* The n-th request failed, and the step reported it. */
        ++failures;
        NAOS_CHECK_INT_EQ(error, CLARGS_ERROR_OUT_OF_MEMORY);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), n);

        if (result == NULL) {
            /* The creation failed: there is no result, and nothing of it
             * is left. */
            ++failures_of_create;
            NAOS_CHECK_UINT_EQ(pushed, 0);
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
        } else {
            size_t live = clargs_test_allocator_live();

            /* A push failed: the result is as it was before that push. */
            ++failures_of_push;
            NAOS_CHECK_INT_EQ(clargs_result_error(result), CLARGS_ERROR_NONE);
            NAOS_CHECK_UINT_EQ(clargs_result_command_count(result), pushed + 1);
            for (i = 0; i <= pushed; ++i) {
                NAOS_CHECK_TRUE(clargs_result_command_at(result, i) == commands[i]);
            }
            NAOS_CHECK_TRUE(clargs_result_command(result) == commands[pushed]);
            NAOS_CHECK_STR_EQ(clargs_result_arg(result, 0), "app");
            NAOS_CHECK_STR_EQ(clargs_result_arg(result, 2), "--flag");

            /* A push that fails again leaves nothing behind either. */
            clargs_test_allocator_fail_at(n + 1);
            NAOS_CHECK_INT_EQ(clargs_result_push_command(result, commands[pushed + 1]),
                              CLARGS_ERROR_OUT_OF_MEMORY);
            NAOS_CHECK_UINT_EQ(clargs_result_command_count(result), pushed + 1);
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);

            /* The result is still usable: the same push now succeeds. */
            NAOS_CHECK_INT_EQ(clargs_result_push_command(result, commands[pushed + 1]),
                              CLARGS_ERROR_NONE);
            NAOS_CHECK_UINT_EQ(clargs_result_command_count(result), pushed + 2);
            NAOS_CHECK_TRUE(clargs_result_command(result) == commands[pushed + 1]);

            /* A result that is not complete can be destroyed. */
            clargs_result_destroy(result);
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
        }
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
        clargs_test_allocator_uninstall();
    }

    /* The loop ended because the operation succeeded, after failures of
     * the creation (the result, the copy of the arguments, the slots of
     * the options and the array of the selected commands) and of pushes
     * (the slots of the options of each command, the slots of the
     * positionals of the last, and the growth of the array). */
    NAOS_CHECK_TRUE(completed);
    NAOS_CHECK_UINT_EQ(failures_of_create, 4);
    NAOS_CHECK_UINT_EQ(failures_of_push, (COUNT - 1) + 1 + 1);
    NAOS_CHECK_UINT_EQ(failures, failures_of_create + failures_of_push);

    /* The schema is untouched. It was allocated by the test allocator, so
     * it is destroyed with the test allocator set; the reset of the loop
     * forgot its blocks, so the count of live blocks is not checked
     * after this. */
    NAOS_CHECK_TRUE(live_of_schema > 0);
    NAOS_CHECK_UINT_EQ(clargs_command_option_count(commands[COUNT - 1]), 4);
    clargs_command_destroy(root);
    clargs_test_allocator_uninstall();
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
}

/* ---- Values ------------------------------------------------------------- */

/* The value of the user-defined value type of these tests. It is larger
 * than a pointer, so the area of a value has to have the size that the
 * value type tells. */
typedef struct custom_value {
    const char *text; /* The input string itself. */
    size_t length;    /* Its length. */
    double filler[3]; /* 1.0, 2.0 and 3.0 */
} custom_value_t;

/* The user data of the user-defined value type: what was called. */
typedef struct custom_counts {
    size_t converts;           /* The calls of the conversion function. */
    size_t successes;          /* Those that succeeded. */
    size_t releases;           /* The calls of the cleanup function. */
    const char *last_text;     /* The input of the last conversion. */
    const char *last_released; /* The input of the value that was cleaned up last. */
} custom_counts_t;

/* Takes every input but those that start with '!' (not valid), '#' (out of
 * memory) and '?' (a code that a conversion function should not return). */
static clargs_error_t custom_convert(const char *text, void *value, void *context)
{
    custom_counts_t *counts = (custom_counts_t *)context;
    custom_value_t *custom = (custom_value_t *)value;

    counts->converts += 1;
    counts->last_text = text;
    switch (text[0]) {
    case '!':
        return CLARGS_ERROR_INVALID_VALUE;
    case '#':
        return CLARGS_ERROR_OUT_OF_MEMORY;
    case '?':
        return CLARGS_ERROR_NAME_CONFLICT;
    default:
        break;
    }

    custom->text = text;
    custom->length = strlen(text);
    custom->filler[0] = 1.0;
    custom->filler[1] = 2.0;
    custom->filler[2] = 3.0;
    counts->successes += 1;
    return CLARGS_ERROR_NONE;
}

static void custom_release(void *value, void *context)
{
    custom_counts_t *counts = (custom_counts_t *)context;
    const custom_value_t *custom = (const custom_value_t *)value;

    counts->releases += 1;
    counts->last_released = custom->text;
}

/* The value types that the tests of the values create, and the counts of
 * the user-defined one. */
typedef struct value_types {
    clargs_type_t *choice; /* "red", "green", "blue" */
    clargs_type_t *custom;
    custom_counts_t counts;
} value_types_t;

/* Creates the value types. The test allocator is installed. */
static void create_types(value_types_t *types)
{
    const char *const choices[3] = { "red", "green", "blue" };

    memset(&types->counts, 0, sizeof(types->counts));
    types->choice = NULL;
    types->custom = NULL;
    NAOS_REQUIRE_TRUE(clargs_type_create_choice(choices, 3, &types->choice) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_type_create("item", sizeof(custom_value_t), custom_convert, custom_release,
                                         &types->counts, &types->custom)
                      == CLARGS_ERROR_NONE);
}

/* Destroys the value types; the schema that used them is destroyed. */
static void destroy_types(value_types_t *types)
{
    clargs_type_destroy(types->choice);
    clargs_type_destroy(types->custom);
}

/* The options that add_value_options() adds, and the positionals that
 * add_value_positionals() adds, by their index in the command. */
enum { V_TEXT = 0, V_NUM = 1, V_RATIO = 2, V_MODE = 3, V_ITEM = 4 };

/* Adds one option with a value of each value type to command: --text (a
 * string), --num (an integer), --ratio (a float), --mode (a choice) and
 * --item (the user-defined value type). All of them must succeed. */
static void add_value_options(clargs_command_t *command, const value_types_t *types)
{
    clargs_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_value(command, 't', "text", clargs_type_string(), &option)
                      == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_command_add_value(command, 'n', "num", clargs_type_int(), &option)
                      == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_command_add_value(command, 'r', "ratio", clargs_type_float(), &option)
                      == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_command_add_value(command, 'm', "mode", types->choice, &option)
                      == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_command_add_value(command, 'i', "item", types->custom, &option)
                      == CLARGS_ERROR_NONE);
}

/* Adds one positional of each value type to command, in the order of
 * add_value_options(). The first four are required; the last, of the
 * user-defined value type, takes any number of arguments. */
static void add_value_positionals(clargs_command_t *command, const value_types_t *types)
{
    clargs_positional_t *positional = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_positional(command, "text", clargs_type_string(),
                                                    CLARGS_ARITY_REQUIRED, &positional)
                      == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_command_add_positional(command, "num", clargs_type_int(),
                                                    CLARGS_ARITY_REQUIRED, &positional)
                      == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_command_add_positional(command, "ratio", clargs_type_float(),
                                                    CLARGS_ARITY_REQUIRED, &positional)
                      == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_command_add_positional(command, "mode", types->choice,
                                                    CLARGS_ARITY_REQUIRED, &positional)
                      == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_command_add_positional(command, "items", types->custom,
                                                    CLARGS_ARITY_ZERO_OR_MORE, &positional)
                      == CLARGS_ERROR_NONE);
}

/* The positional of command at index. */
static const clargs_positional_t *positional_of(const clargs_command_t *command, size_t index)
{
    return clargs_command_positional_at(command, index);
}

/* Where a value of the tests is: on the command line, in the argument 1,
 * written as "--name". */
static clargs_result_where_t on_command_line(void)
{
    clargs_result_where_t where;

    where.source = CLARGS_SOURCE_COMMAND_LINE;
    where.index = 1;
    where.short_name = '\0';
    where.name_prefix = "--";
    where.name = "name";
    where.name_length = 4;
    return where;
}

/* Where a default value is. */
static clargs_result_where_t in_default(void)
{
    clargs_result_where_t where = on_command_line();

    where.source = CLARGS_SOURCE_DEFAULT;
    where.index = 0;
    return where;
}

/* Records text, from the command line, as the value of the option of
 * command at index, and returns what the record returns. */
static clargs_error_t record_option(clargs_result_t *result, const clargs_command_t *command,
                                    size_t index, const char *text)
{
    clargs_result_where_t where = on_command_line();

    return clargs_result_record_option_value(result, option_of(command, index), text, &where);
}

/* The same for a value of the positional of command at index. */
static clargs_error_t record_positional(clargs_result_t *result, const clargs_command_t *command,
                                        size_t index, const char *text)
{
    clargs_result_where_t where = on_command_line();

    return clargs_result_record_positional_value(result, positional_of(command, index), text, &where);
}

NAOS_TEST(option_values_of_every_value_type_are_read)
{
    char a0[] = "app";
    char a1[] = "  spaces, = and -- stay  ";
    char *argv[2];
    value_types_t types;
    clargs_command_t *root = create_root();
    clargs_result_t *result;
    const char *text = NULL;
    int64_t num = 0;
    double ratio = 0.0;
    size_t mode = 99;
    const custom_value_t *item;
    const void *area;

    argv[0] = a0;
    argv[1] = a1;
    create_types(&types);
    add_value_options(root, &types);
    result = create_result(root, 2, argv);

    /* The text is passed to the value type as it is: the value of a string
     * is the pointer that was recorded, into the copy of the arguments. */
    NAOS_CHECK_INT_EQ(record_option(result, root, V_TEXT, clargs_result_arg(result, 1)),
                      CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(record_option(result, root, V_NUM, "-42"), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(record_option(result, root, V_RATIO, "1.5"), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(record_option(result, root, V_MODE, "blue"), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(record_option(result, root, V_ITEM, " an item "), CLARGS_ERROR_NONE);

    NAOS_CHECK_TRUE(clargs_result_option_string(result, option_of(root, V_TEXT), &text));
    NAOS_CHECK_TRUE(text == clargs_result_arg(result, 1));
    NAOS_CHECK_STR_EQ(text, "  spaces, = and -- stay  ");
    NAOS_CHECK_TRUE(clargs_result_option_int(result, option_of(root, V_NUM), &num));
    NAOS_CHECK_TRUE(num == -42);
    NAOS_CHECK_TRUE(clargs_result_option_float(result, option_of(root, V_RATIO), &ratio));
    NAOS_CHECK_TRUE(ratio == 1.5);
    NAOS_CHECK_TRUE(clargs_result_option_choice(result, option_of(root, V_MODE), &mode));
    NAOS_CHECK_UINT_EQ(mode, 2);

    /* The area holds the same value, in the type of the value type. */
    area = clargs_result_option_value(result, option_of(root, V_TEXT));
    NAOS_REQUIRE_TRUE(area != NULL);
    NAOS_CHECK_TRUE(*(const char *const *)area == text);
    area = clargs_result_option_value(result, option_of(root, V_NUM));
    NAOS_REQUIRE_TRUE(area != NULL);
    NAOS_CHECK_TRUE(*(const int64_t *)area == num);
    area = clargs_result_option_value(result, option_of(root, V_RATIO));
    NAOS_REQUIRE_TRUE(area != NULL);
    NAOS_CHECK_TRUE(*(const double *)area == ratio);
    area = clargs_result_option_value(result, option_of(root, V_MODE));
    NAOS_REQUIRE_TRUE(area != NULL);
    NAOS_CHECK_UINT_EQ(*(const size_t *)area, mode);

    /* The user-defined value type is used like the built-in ones: its
     * value is read from the area. */
    item = (const custom_value_t *)clargs_result_option_value(result, option_of(root, V_ITEM));
    NAOS_REQUIRE_TRUE(item != NULL);
    NAOS_CHECK_STR_EQ(item->text, " an item ");
    NAOS_CHECK_UINT_EQ(item->length, 9);
    NAOS_CHECK_TRUE(item->filler[0] == 1.0);
    NAOS_CHECK_TRUE(item->filler[2] == 3.0);
    NAOS_CHECK_UINT_EQ(types.counts.converts, 1);
    NAOS_CHECK_UINT_EQ(types.counts.releases, 0);

    /* Reading again gives the same area. */
    NAOS_CHECK_TRUE(clargs_result_option_value(result, option_of(root, V_ITEM)) == item);

    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, V_TEXT)),
                      CLARGS_SOURCE_COMMAND_LINE);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, V_NUM)),
                      CLARGS_SOURCE_COMMAND_LINE);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, V_RATIO)),
                      CLARGS_SOURCE_COMMAND_LINE);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, V_MODE)),
                      CLARGS_SOURCE_COMMAND_LINE);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, V_ITEM)),
                      CLARGS_SOURCE_COMMAND_LINE);
    NAOS_CHECK_INT_EQ(clargs_result_error(result), CLARGS_ERROR_NONE);

    clargs_result_destroy(result);
    NAOS_CHECK_UINT_EQ(types.counts.releases, 1);
    clargs_command_destroy(root);
    destroy_types(&types);
    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(option_value_takes_one_allocation)
{
    value_types_t types;
    clargs_command_t *root = create_root();
    clargs_result_t *result;
    size_t requests;
    size_t live;

    create_types(&types);
    add_value_options(root, &types);
    result = create_result(root, 0, NULL);
    requests = clargs_test_allocator_requests();
    live = clargs_test_allocator_live();

    /* The area of the value, and nothing else. */
    NAOS_CHECK_INT_EQ(record_option(result, root, V_NUM, "7"), CLARGS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests + 1);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live + 1);

    /* A second value takes a new area, and the first is freed. */
    NAOS_CHECK_INT_EQ(record_option(result, root, V_NUM, "8"), CLARGS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests + 2);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live + 1);

    /* Reading allocates nothing. */
    requests = clargs_test_allocator_requests();
    NAOS_CHECK_TRUE(clargs_result_option_value(result, option_of(root, V_NUM)) != NULL);
    NAOS_CHECK_TRUE(clargs_result_option_value(result, option_of(root, V_TEXT)) == NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests);

    clargs_result_destroy(result);
    clargs_command_destroy(root);
    destroy_types(&types);
    clargs_test_allocator_uninstall();
    check_clean();
}

/* Checks that the options with a value of command read as "no value" in
 * result, with the outputs left as they were. */
static void check_options_have_no_value(const clargs_result_t *result, const clargs_command_t *command)
{
    const char *text = "untouched";
    int64_t num = 123;
    double ratio = 4.5;
    size_t mode = 77;

    NAOS_CHECK_FALSE(clargs_result_option_string(result, option_of(command, V_TEXT), &text));
    NAOS_CHECK_STR_EQ(text, "untouched");
    NAOS_CHECK_FALSE(clargs_result_option_int(result, option_of(command, V_NUM), &num));
    NAOS_CHECK_TRUE(num == 123);
    NAOS_CHECK_FALSE(clargs_result_option_float(result, option_of(command, V_RATIO), &ratio));
    NAOS_CHECK_TRUE(ratio == 4.5);
    NAOS_CHECK_FALSE(clargs_result_option_choice(result, option_of(command, V_MODE), &mode));
    NAOS_CHECK_UINT_EQ(mode, 77);

    NAOS_CHECK_TRUE(clargs_result_option_value(result, option_of(command, V_TEXT)) == NULL);
    NAOS_CHECK_TRUE(clargs_result_option_value(result, option_of(command, V_NUM)) == NULL);
    NAOS_CHECK_TRUE(clargs_result_option_value(result, option_of(command, V_RATIO)) == NULL);
    NAOS_CHECK_TRUE(clargs_result_option_value(result, option_of(command, V_MODE)) == NULL);
    NAOS_CHECK_TRUE(clargs_result_option_value(result, option_of(command, V_ITEM)) == NULL);

    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(command, V_TEXT)), CLARGS_SOURCE_NONE);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(command, V_ITEM)), CLARGS_SOURCE_NONE);
}

/* The same for the positionals of command. index is the index to ask for. */
static void check_positionals_have_no_value(const clargs_result_t *result,
                                            const clargs_command_t *command, size_t index)
{
    const char *text = "untouched";
    int64_t num = 123;
    double ratio = 4.5;
    size_t mode = 77;
    size_t i;

    NAOS_CHECK_FALSE(clargs_result_positional_string(result, positional_of(command, V_TEXT), index, &text));
    NAOS_CHECK_STR_EQ(text, "untouched");
    NAOS_CHECK_FALSE(clargs_result_positional_int(result, positional_of(command, V_NUM), index, &num));
    NAOS_CHECK_TRUE(num == 123);
    NAOS_CHECK_FALSE(clargs_result_positional_float(result, positional_of(command, V_RATIO), index, &ratio));
    NAOS_CHECK_TRUE(ratio == 4.5);
    NAOS_CHECK_FALSE(clargs_result_positional_choice(result, positional_of(command, V_MODE), index, &mode));
    NAOS_CHECK_UINT_EQ(mode, 77);

    for (i = V_TEXT; i <= V_ITEM; ++i) {
        NAOS_CHECK_TRUE(clargs_result_positional_value(result, positional_of(command, i), index) == NULL);
        NAOS_CHECK_UINT_EQ(clargs_result_positional_count(result, positional_of(command, i)), 0);
        NAOS_CHECK_INT_EQ(clargs_result_positional_source(result, positional_of(command, i)),
                          CLARGS_SOURCE_NONE);
    }
}

NAOS_TEST(values_that_were_not_recorded_read_as_no_value)
{
    value_types_t types;
    clargs_command_t *root = create_root();
    clargs_command_t *sub = add_sub(root, "sub");
    clargs_command_t *other = add_sub(root, "other");
    clargs_result_t *result;
    size_t requests;

    create_types(&types);
    add_value_options(root, &types);
    add_value_options(sub, &types);
    add_value_positionals(sub, &types);
    add_value_options(other, &types);
    add_value_positionals(other, &types);
    result = create_result(root, 0, NULL);
    NAOS_REQUIRE_TRUE(clargs_result_push_command(result, sub) == CLARGS_ERROR_NONE);
    requests = clargs_test_allocator_requests();

    /* Selected commands, nothing recorded. */
    check_options_have_no_value(result, root);
    check_options_have_no_value(result, sub);
    check_positionals_have_no_value(result, sub, 0);
    check_positionals_have_no_value(result, sub, 5);

    /* A command that is not selected. */
    check_options_have_no_value(result, other);
    check_positionals_have_no_value(result, other, 0);
    check_positionals_have_no_value(result, other, 5);

    /* What is recorded for the selected command does not show in the
     * command that is not selected, which has the same names. */
    NAOS_CHECK_INT_EQ(record_option(result, sub, V_NUM, "1"), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(record_positional(result, sub, V_NUM, "2"), CLARGS_ERROR_NONE);
    check_options_have_no_value(result, root);
    check_options_have_no_value(result, other);
    check_positionals_have_no_value(result, other, 0);

    /* Asking is not an error of the result, and it allocates nothing: the
     * requests are the two areas and the array of the values of the
     * positional. */
    NAOS_CHECK_INT_EQ(clargs_result_error(result), CLARGS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests + 3);
    NAOS_CHECK_UINT_EQ(types.counts.converts, 0);

    clargs_result_destroy(result);
    clargs_command_destroy(root);
    destroy_types(&types);
    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(second_value_of_an_option_replaces_the_first)
{
    value_types_t types;
    clargs_command_t *root = create_root();
    clargs_result_t *result;
    const custom_value_t *item;
    const char *text = NULL;
    int64_t num = 0;
    size_t live;

    create_types(&types);
    add_value_options(root, &types);
    result = create_result(root, 0, NULL);

    NAOS_CHECK_INT_EQ(record_option(result, root, V_ITEM, "first"), CLARGS_ERROR_NONE);
    live = clargs_test_allocator_live();
    NAOS_CHECK_UINT_EQ(types.counts.releases, 0);

    /* The first value is cleaned up, once, when the second is there. */
    NAOS_CHECK_INT_EQ(record_option(result, root, V_ITEM, "second"), CLARGS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(types.counts.successes, 2);
    NAOS_CHECK_UINT_EQ(types.counts.releases, 1);
    NAOS_CHECK_STR_EQ(types.counts.last_released, "first");
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);
    item = (const custom_value_t *)clargs_result_option_value(result, option_of(root, V_ITEM));
    NAOS_REQUIRE_TRUE(item != NULL);
    NAOS_CHECK_STR_EQ(item->text, "second");
    NAOS_CHECK_UINT_EQ(item->length, 6);

    NAOS_CHECK_INT_EQ(record_option(result, root, V_ITEM, "third"), CLARGS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(types.counts.releases, 2);
    NAOS_CHECK_STR_EQ(types.counts.last_released, "second");
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);
    item = (const custom_value_t *)clargs_result_option_value(result, option_of(root, V_ITEM));
    NAOS_REQUIRE_TRUE(item != NULL);
    NAOS_CHECK_STR_EQ(item->text, "third");

    /* The built-in value types are replaced the same way. */
    NAOS_CHECK_INT_EQ(record_option(result, root, V_TEXT, "a"), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(record_option(result, root, V_TEXT, "b"), CLARGS_ERROR_NONE);
    NAOS_CHECK_TRUE(clargs_result_option_string(result, option_of(root, V_TEXT), &text));
    NAOS_CHECK_STR_EQ(text, "b");
    NAOS_CHECK_INT_EQ(record_option(result, root, V_NUM, "1"), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(record_option(result, root, V_NUM, "2"), CLARGS_ERROR_NONE);
    NAOS_CHECK_TRUE(clargs_result_option_int(result, option_of(root, V_NUM), &num));
    NAOS_CHECK_TRUE(num == 2);

    /* A default value replaces nothing by itself, but the source is that
     * of the last record. */
    {
        clargs_result_where_t where = in_default();

        NAOS_CHECK_INT_EQ(clargs_result_record_option_value(result, option_of(root, V_RATIO), "0.25",
                                                            &where),
                          CLARGS_ERROR_NONE);
    }
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, V_RATIO)), CLARGS_SOURCE_DEFAULT);
    NAOS_CHECK_INT_EQ(record_option(result, root, V_RATIO, "0.5"), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, V_RATIO)),
                      CLARGS_SOURCE_COMMAND_LINE);

    /* The destruction cleans up the value that is left, once. */
    clargs_result_destroy(result);
    NAOS_CHECK_UINT_EQ(types.counts.releases, 3);
    NAOS_CHECK_UINT_EQ(types.counts.successes, 3);
    NAOS_CHECK_STR_EQ(types.counts.last_released, "third");

    clargs_command_destroy(root);
    destroy_types(&types);
    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(failed_conversion_keeps_the_value_of_an_option)
{
    /* What the conversion function returns, and what the record returns. */
    const char *const inputs[3] = { "!invalid", "#memory", "?other" };
    const clargs_error_t expected[3] = { CLARGS_ERROR_INVALID_COMMAND_LINE, CLARGS_ERROR_OUT_OF_MEMORY,
                                         CLARGS_ERROR_INVALID_COMMAND_LINE };
    value_types_t types;
    clargs_command_t *root = create_root();
    size_t i;

    create_types(&types);
    add_value_options(root, &types);

    for (i = 0; i < 3; ++i) {
        clargs_result_t *result = create_result(root, 0, NULL);
        const custom_value_t *item;
        size_t releases = types.counts.releases;
        size_t successes = types.counts.successes;
        size_t live;

        /* With no value yet: there is still none. */
        live = clargs_test_allocator_live();
        NAOS_CHECK_INT_EQ(record_option(result, root, V_ITEM, inputs[i]), expected[i]);
        NAOS_CHECK_TRUE(types.counts.last_text == inputs[i]);
        NAOS_CHECK_TRUE(clargs_result_option_value(result, option_of(root, V_ITEM)) == NULL);
        NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, V_ITEM)), CLARGS_SOURCE_NONE);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);
        clargs_result_destroy(result);
        NAOS_CHECK_UINT_EQ(types.counts.releases, releases);

        /* With a value: it stays, and it is not cleaned up before the
         * destruction. A value that failed is never cleaned up. */
        result = create_result(root, 0, NULL);
        {
            clargs_result_where_t where = in_default();

            NAOS_CHECK_INT_EQ(clargs_result_record_option_value(result, option_of(root, V_ITEM), "kept",
                                                                &where),
                              CLARGS_ERROR_NONE);
        }
        live = clargs_test_allocator_live();
        NAOS_CHECK_INT_EQ(record_option(result, root, V_ITEM, inputs[i]), expected[i]);
        NAOS_CHECK_UINT_EQ(types.counts.releases, releases);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);
        item = (const custom_value_t *)clargs_result_option_value(result, option_of(root, V_ITEM));
        NAOS_REQUIRE_TRUE(item != NULL);
        NAOS_CHECK_STR_EQ(item->text, "kept");
        NAOS_CHECK_INT_EQ(clargs_result_option_source(result, option_of(root, V_ITEM)),
                          CLARGS_SOURCE_DEFAULT);

        clargs_result_destroy(result);
        NAOS_CHECK_UINT_EQ(types.counts.successes, successes + 1);
        NAOS_CHECK_UINT_EQ(types.counts.releases, releases + 1);
        NAOS_CHECK_STR_EQ(types.counts.last_released, "kept");
    }

    /* A built-in value type that does not take the input. */
    {
        clargs_result_t *result = create_result(root, 0, NULL);
        int64_t num = 0;

        NAOS_CHECK_INT_EQ(record_option(result, root, V_NUM, "5"), CLARGS_ERROR_NONE);
        NAOS_CHECK_INT_EQ(record_option(result, root, V_NUM, "x"), CLARGS_ERROR_INVALID_COMMAND_LINE);
        NAOS_CHECK_TRUE(clargs_result_option_int(result, option_of(root, V_NUM), &num));
        NAOS_CHECK_TRUE(num == 5);
        clargs_result_destroy(result);
    }
    {
        clargs_result_t *result = create_result(root, 0, NULL);
        size_t mode = 77;

        NAOS_CHECK_INT_EQ(record_option(result, root, V_MODE, "Red"), CLARGS_ERROR_INVALID_COMMAND_LINE);
        NAOS_CHECK_FALSE(clargs_result_option_choice(result, option_of(root, V_MODE), &mode));
        NAOS_CHECK_UINT_EQ(mode, 77);
        clargs_result_destroy(result);
    }

    clargs_command_destroy(root);
    destroy_types(&types);
    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(positional_values_of_every_value_type_are_read)
{
    char a0[] = "app";
    char a1[] = " -not an option ";
    char *argv[2];
    value_types_t types;
    clargs_command_t *root = create_root();
    clargs_result_t *result;
    const char *text = NULL;
    int64_t num = 0;
    double ratio = 0.0;
    size_t mode = 99;
    const custom_value_t *item;
    const void *area;
    size_t i;

    argv[0] = a0;
    argv[1] = a1;
    create_types(&types);
    add_value_positionals(root, &types);
    result = create_result(root, 2, argv);

    NAOS_CHECK_INT_EQ(record_positional(result, root, V_TEXT, clargs_result_arg(result, 1)),
                      CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(record_positional(result, root, V_NUM, "+9223372036854775807"), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(record_positional(result, root, V_RATIO, "-.5"), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(record_positional(result, root, V_MODE, "green"), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(record_positional(result, root, V_ITEM, ""), CLARGS_ERROR_NONE);

    for (i = V_TEXT; i <= V_ITEM; ++i) {
        NAOS_CHECK_UINT_EQ(clargs_result_positional_count(result, positional_of(root, i)), 1);
        NAOS_CHECK_INT_EQ(clargs_result_positional_source(result, positional_of(root, i)),
                          CLARGS_SOURCE_COMMAND_LINE);
    }

    NAOS_CHECK_TRUE(clargs_result_positional_string(result, positional_of(root, V_TEXT), 0, &text));
    NAOS_CHECK_TRUE(text == clargs_result_arg(result, 1));
    NAOS_CHECK_STR_EQ(text, " -not an option ");
    NAOS_CHECK_TRUE(clargs_result_positional_int(result, positional_of(root, V_NUM), 0, &num));
    NAOS_CHECK_TRUE(num == INT64_MAX);
    NAOS_CHECK_TRUE(clargs_result_positional_float(result, positional_of(root, V_RATIO), 0, &ratio));
    NAOS_CHECK_TRUE(ratio == -0.5);
    NAOS_CHECK_TRUE(clargs_result_positional_choice(result, positional_of(root, V_MODE), 0, &mode));
    NAOS_CHECK_UINT_EQ(mode, 1);

    /* The area holds the same value, in the type of the value type. */
    area = clargs_result_positional_value(result, positional_of(root, V_TEXT), 0);
    NAOS_REQUIRE_TRUE(area != NULL);
    NAOS_CHECK_TRUE(*(const char *const *)area == text);
    area = clargs_result_positional_value(result, positional_of(root, V_NUM), 0);
    NAOS_REQUIRE_TRUE(area != NULL);
    NAOS_CHECK_TRUE(*(const int64_t *)area == num);
    area = clargs_result_positional_value(result, positional_of(root, V_RATIO), 0);
    NAOS_REQUIRE_TRUE(area != NULL);
    NAOS_CHECK_TRUE(*(const double *)area == ratio);
    area = clargs_result_positional_value(result, positional_of(root, V_MODE), 0);
    NAOS_REQUIRE_TRUE(area != NULL);
    NAOS_CHECK_UINT_EQ(*(const size_t *)area, mode);
    item = (const custom_value_t *)clargs_result_positional_value(result, positional_of(root, V_ITEM), 0);
    NAOS_REQUIRE_TRUE(item != NULL);
    NAOS_CHECK_STR_EQ(item->text, "");
    NAOS_CHECK_UINT_EQ(item->length, 0);
    NAOS_CHECK_TRUE(item->filler[1] == 2.0);

    /* An index that is the count or more reads as no value. */
    text = "untouched";
    num = 123;
    ratio = 4.5;
    mode = 77;
    NAOS_CHECK_FALSE(clargs_result_positional_string(result, positional_of(root, V_TEXT), 1, &text));
    NAOS_CHECK_STR_EQ(text, "untouched");
    NAOS_CHECK_FALSE(clargs_result_positional_int(result, positional_of(root, V_NUM), 1, &num));
    NAOS_CHECK_TRUE(num == 123);
    NAOS_CHECK_FALSE(clargs_result_positional_float(result, positional_of(root, V_RATIO), 2, &ratio));
    NAOS_CHECK_TRUE(ratio == 4.5);
    NAOS_CHECK_FALSE(clargs_result_positional_choice(result, positional_of(root, V_MODE), SIZE_MAX, &mode));
    NAOS_CHECK_UINT_EQ(mode, 77);
    NAOS_CHECK_TRUE(clargs_result_positional_value(result, positional_of(root, V_ITEM), 1) == NULL);
    NAOS_CHECK_TRUE(clargs_result_positional_value(result, positional_of(root, V_ITEM), SIZE_MAX) == NULL);

    NAOS_CHECK_INT_EQ(clargs_result_error(result), CLARGS_ERROR_NONE);

    clargs_result_destroy(result);
    NAOS_CHECK_UINT_EQ(types.counts.successes, 1);
    NAOS_CHECK_UINT_EQ(types.counts.releases, 1);
    clargs_command_destroy(root);
    destroy_types(&types);
    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(positional_values_are_kept_in_their_order)
{
    enum { COUNT = 11 };
    const char *const inputs[COUNT] = { "zero", "one", "two", "three", "four", "five",
                                        "six",  "seven", "eight", "nine", "ten" };
    value_types_t types;
    clargs_command_t *root = create_root();
    clargs_result_t *result;
    const custom_value_t *first;
    size_t i;
    size_t k;

    create_types(&types);
    add_value_positionals(root, &types);
    result = create_result(root, 0, NULL);

    /* A value is added after those that are there; none is replaced or
     * cleaned up, also when the array of the values grows. */
    for (i = 0; i < COUNT; ++i) {
        NAOS_CHECK_INT_EQ(record_positional(result, root, V_ITEM, inputs[i]), CLARGS_ERROR_NONE);
        NAOS_CHECK_UINT_EQ(clargs_result_positional_count(result, positional_of(root, V_ITEM)), i + 1);
        for (k = 0; k <= i; ++k) {
            const custom_value_t *item = (const custom_value_t *)clargs_result_positional_value(
                result, positional_of(root, V_ITEM), k);

            NAOS_REQUIRE_TRUE(item != NULL);
            NAOS_CHECK_TRUE(item->text == inputs[k]);
            NAOS_CHECK_UINT_EQ(item->length, strlen(inputs[k]));
        }
        NAOS_CHECK_TRUE(clargs_result_positional_value(result, positional_of(root, V_ITEM), i + 1) == NULL);
    }
    NAOS_CHECK_UINT_EQ(types.counts.successes, COUNT);
    NAOS_CHECK_UINT_EQ(types.counts.releases, 0);
    NAOS_CHECK_INT_EQ(clargs_result_positional_source(result, positional_of(root, V_ITEM)),
                      CLARGS_SOURCE_COMMAND_LINE);

    /* The area of a value does not move when values are added. */
    first = (const custom_value_t *)clargs_result_positional_value(result, positional_of(root, V_ITEM), 0);
    NAOS_CHECK_INT_EQ(record_positional(result, root, V_ITEM, "more"), CLARGS_ERROR_NONE);
    NAOS_CHECK_TRUE(clargs_result_positional_value(result, positional_of(root, V_ITEM), 0) == first);

    /* The other positionals have no value. */
    NAOS_CHECK_UINT_EQ(clargs_result_positional_count(result, positional_of(root, V_TEXT)), 0);
    NAOS_CHECK_INT_EQ(clargs_result_positional_source(result, positional_of(root, V_TEXT)),
                      CLARGS_SOURCE_NONE);

    /* A conversion that fails adds nothing, and what is there stays. */
    {
        size_t live = clargs_test_allocator_live();

        NAOS_CHECK_INT_EQ(record_positional(result, root, V_ITEM, "#memory"), CLARGS_ERROR_OUT_OF_MEMORY);
        NAOS_CHECK_INT_EQ(record_positional(result, root, V_ITEM, "!invalid"),
                          CLARGS_ERROR_INVALID_COMMAND_LINE);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);
    }
    NAOS_CHECK_UINT_EQ(clargs_result_positional_count(result, positional_of(root, V_ITEM)), COUNT + 1);
    NAOS_CHECK_UINT_EQ(types.counts.releases, 0);

    /* The destruction cleans up each value once. */
    clargs_result_destroy(result);
    NAOS_CHECK_UINT_EQ(types.counts.successes, COUNT + 1);
    NAOS_CHECK_UINT_EQ(types.counts.releases, COUNT + 1);

    clargs_command_destroy(root);
    destroy_types(&types);
    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(positional_default_value_has_the_source_default)
{
    value_types_t types;
    clargs_command_t *root = create_root();
    clargs_result_t *result;
    clargs_result_where_t where = in_default();
    int64_t num = 0;

    create_types(&types);
    add_value_positionals(root, &types);
    result = create_result(root, 0, NULL);

    NAOS_CHECK_INT_EQ(clargs_result_record_positional_value(result, positional_of(root, V_NUM), "10",
                                                            &where),
                      CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_result_positional_source(result, positional_of(root, V_NUM)),
                      CLARGS_SOURCE_DEFAULT);
    NAOS_CHECK_UINT_EQ(clargs_result_positional_count(result, positional_of(root, V_NUM)), 1);
    NAOS_CHECK_TRUE(clargs_result_positional_int(result, positional_of(root, V_NUM), 0, &num));
    NAOS_CHECK_TRUE(num == 10);

    /* A positional whose first value fails has none, and no source. */
    NAOS_CHECK_INT_EQ(clargs_result_record_positional_value(result, positional_of(root, V_RATIO), "",
                                                            &where),
                      CLARGS_ERROR_INVALID_COMMAND_LINE);
    NAOS_CHECK_UINT_EQ(clargs_result_positional_count(result, positional_of(root, V_RATIO)), 0);
    NAOS_CHECK_INT_EQ(clargs_result_positional_source(result, positional_of(root, V_RATIO)),
                      CLARGS_SOURCE_NONE);

    clargs_result_destroy(result);
    clargs_command_destroy(root);
    destroy_types(&types);
    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(destroy_releases_every_value_once)
{
    value_types_t types;
    clargs_command_t *root = create_root();
    clargs_command_t *sub = add_sub(root, "sub");
    clargs_command_t *leaf = add_sub(sub, "leaf");
    clargs_result_t *result;
    size_t live_of_schema;
    size_t i;

    create_types(&types);
    add_value_options(root, &types);
    add_value_options(sub, &types);
    add_value_options(leaf, &types);
    add_value_positionals(leaf, &types);
    live_of_schema = clargs_test_allocator_live();

    result = create_result(root, 0, NULL);
    NAOS_REQUIRE_TRUE(clargs_result_push_command(result, sub) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_result_push_command(result, leaf) == CLARGS_ERROR_NONE);

    /* Values of the user-defined value type in every selected command:
     * replaced ones, failed ones and several of one positional, with
     * values of the built-in value types between them. */
    NAOS_CHECK_INT_EQ(record_option(result, root, V_ITEM, "a"), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(record_option(result, root, V_ITEM, "b"), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(record_option(result, root, V_TEXT, "text"), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(record_option(result, sub, V_ITEM, "c"), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(record_option(result, sub, V_ITEM, "!d"), CLARGS_ERROR_INVALID_COMMAND_LINE);
    NAOS_CHECK_INT_EQ(record_option(result, sub, V_RATIO, "2.5"), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(record_option(result, leaf, V_ITEM, "#e"), CLARGS_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_INT_EQ(record_option(result, leaf, V_MODE, "red"), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(record_positional(result, leaf, V_NUM, "3"), CLARGS_ERROR_NONE);
    for (i = 0; i < 6; ++i) {
        NAOS_CHECK_INT_EQ(record_positional(result, leaf, V_ITEM, "f"), CLARGS_ERROR_NONE);
    }
    NAOS_CHECK_INT_EQ(record_positional(result, leaf, V_ITEM, "?g"), CLARGS_ERROR_INVALID_COMMAND_LINE);

    NAOS_CHECK_UINT_EQ(types.counts.converts, 12);
    NAOS_CHECK_UINT_EQ(types.counts.successes, 9);
    NAOS_CHECK_UINT_EQ(types.counts.releases, 1);

    clargs_result_destroy(result);

    /* One cleanup for each conversion that succeeded, and none for one
     * that failed; all the areas are freed, and nothing of the schema. */
    NAOS_CHECK_UINT_EQ(types.counts.releases, types.counts.successes);
    NAOS_CHECK_UINT_EQ(types.counts.converts, 12);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live_of_schema);

    clargs_command_destroy(root);
    destroy_types(&types);
    clargs_test_allocator_uninstall();
    check_clean();
}

/* ---- The failure of an allocation while recording values ----------------- */

/* One step of record_values(). */
typedef struct value_step {
    bool positional;  /* Whether the value is that of a positional. */
    size_t index;     /* The index of the option or the positional in the command. */
    const char *text; /* The input. */
} value_step_t;

/* The steps of record_values(): values of options, one of them replaced
 * twice, and values of positionals, six of them of one positional, so that
 * the array of its values grows twice. ITEM_STEPS of the steps are of the
 * user-defined value type. */
enum { VALUE_STEPS = 13, ITEM_STEPS = 9 };

static const value_step_t value_steps[VALUE_STEPS] = {
    { false, V_TEXT, "text" },
    { false, V_ITEM, "one" },
    { false, V_NUM, "1" },
    { false, V_ITEM, "two" },
    { true, V_TEXT, "input" },
    { true, V_ITEM, "p0" },
    { false, V_ITEM, "three" },
    { true, V_ITEM, "p1" },
    { true, V_ITEM, "p2" },
    { true, V_ITEM, "p3" },
    { true, V_ITEM, "p4" },
    { true, V_ITEM, "p5" },
    { false, V_RATIO, "0.5" }
};

/*
 * The operation of the test below: records the values of value_steps in
 * the selected command of result. Stops at the first step that fails and
 * returns its error. *done is the number of steps that succeeded, and
 * *live the number of live blocks before the step that failed.
 */
static clargs_error_t record_values(clargs_result_t *result, const clargs_command_t *command,
                                    size_t *done, size_t *live)
{
    size_t i;

    *done = 0;
    for (i = 0; i < VALUE_STEPS; ++i) {
        clargs_error_t error;

        *live = clargs_test_allocator_live();
        if (value_steps[i].positional) {
            error = record_positional(result, command, value_steps[i].index, value_steps[i].text);
        } else {
            error = record_option(result, command, value_steps[i].index, value_steps[i].text);
        }
        if (error != CLARGS_ERROR_NONE) {
            return error;
        }
        ++*done;
    }
    return CLARGS_ERROR_NONE;
}

/* Checks that result holds what the first done steps of value_steps
 * recorded in command, and nothing of the steps after them. */
static void check_recorded_values(const clargs_result_t *result, const clargs_command_t *command,
                                  size_t done)
{
    const char *option_item = NULL;
    const char *items[VALUE_STEPS];
    size_t item_count = 0;
    const custom_value_t *item;
    size_t i;

    for (i = 0; i < done; ++i) {
        if (value_steps[i].index != V_ITEM) {
            continue;
        }
        if (value_steps[i].positional) {
            items[item_count] = value_steps[i].text;
            ++item_count;
        } else {
            option_item = value_steps[i].text;
        }
    }

    item = (const custom_value_t *)clargs_result_option_value(result, option_of(command, V_ITEM));
    if (option_item == NULL) {
        NAOS_CHECK_TRUE(item == NULL);
    } else {
        NAOS_REQUIRE_TRUE(item != NULL);
        NAOS_CHECK_TRUE(item->text == option_item);
    }

    NAOS_CHECK_UINT_EQ(clargs_result_positional_count(result, positional_of(command, V_ITEM)), item_count);
    for (i = 0; i < item_count; ++i) {
        item = (const custom_value_t *)clargs_result_positional_value(result,
                                                                      positional_of(command, V_ITEM), i);
        NAOS_REQUIRE_TRUE(item != NULL);
        NAOS_CHECK_TRUE(item->text == items[i]);
    }
    NAOS_CHECK_INT_EQ(clargs_result_positional_source(result, positional_of(command, V_ITEM)),
                      item_count == 0 ? CLARGS_SOURCE_NONE : CLARGS_SOURCE_COMMAND_LINE);

    /* The first and the last step. */
    NAOS_CHECK_TRUE((clargs_result_option_value(result, option_of(command, V_TEXT)) != NULL)
                    == (done >= 1));
    NAOS_CHECK_TRUE((clargs_result_option_value(result, option_of(command, V_RATIO)) != NULL)
                    == (done >= VALUE_STEPS));
}

NAOS_TEST(recording_values_fails_at_every_allocation_point)
{
    const size_t limit = 100;
    value_types_t types;
    clargs_command_t *root;
    clargs_command_t *sub;
    size_t failures = 0;
    int completed = 0;
    size_t n;

    clargs_test_allocator_install();
    root = new_root();
    sub = add_sub(root, "sub");
    create_types(&types);
    add_value_options(root, &types);
    add_value_options(sub, &types);
    add_value_positionals(sub, &types);
    clargs_test_allocator_uninstall();

    for (n = 1; n <= limit; ++n) {
        clargs_result_t *result;
        clargs_error_t error;
        size_t done;
        size_t live;
        size_t first;

        /* The schema was allocated before the reset, so the count of live
         * blocks is that of the result alone from here on. The result is
         * created before the request to fail is set: the failures of its
         * creation have their own test. */
        clargs_test_allocator_install();
        memset(&types.counts, 0, sizeof(types.counts));
        result = create_result(root, 0, NULL);
        NAOS_REQUIRE_TRUE(clargs_result_push_command(result, sub) == CLARGS_ERROR_NONE);
        first = clargs_test_allocator_requests();
        clargs_test_allocator_fail_at(first + n);

        error = record_values(result, sub, &done, &live);

        if (error == CLARGS_ERROR_NONE && clargs_test_allocator_requests() < first + n) {
            /* The operation ended before the request to fail was reached:
             * every allocation point has been passed. */
            NAOS_CHECK_UINT_EQ(done, VALUE_STEPS);
            check_recorded_values(result, sub, VALUE_STEPS);
            clargs_result_destroy(result);
            NAOS_CHECK_UINT_EQ(types.counts.successes, ITEM_STEPS);
            NAOS_CHECK_UINT_EQ(types.counts.releases, ITEM_STEPS);
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
            completed = 1;
            break;
        }

        /* The n-th request failed, and the step reported it. */
        ++failures;
        NAOS_CHECK_INT_EQ(error, CLARGS_ERROR_OUT_OF_MEMORY);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), first + n);
        NAOS_REQUIRE_TRUE(done < VALUE_STEPS);

        /* The step that failed left nothing behind and changed nothing:
         * the values of the steps before it are there, and a value that
         * the step would have replaced is still the old one. A value that
         * was converted and then not kept is cleaned up at once. */
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);
        check_recorded_values(result, sub, done);
        NAOS_CHECK_INT_EQ(clargs_result_error(result), CLARGS_ERROR_NONE);

        /* The result is still usable: the same step now succeeds. */
        if (value_steps[done].positional) {
            NAOS_CHECK_INT_EQ(record_positional(result, sub, value_steps[done].index,
                                                value_steps[done].text),
                              CLARGS_ERROR_NONE);
        } else {
            NAOS_CHECK_INT_EQ(record_option(result, sub, value_steps[done].index, value_steps[done].text),
                              CLARGS_ERROR_NONE);
        }
        check_recorded_values(result, sub, done + 1);

        /* A result that is not complete can be destroyed, and every value
         * is cleaned up once. */
        clargs_result_destroy(result);
        NAOS_CHECK_UINT_EQ(types.counts.releases, types.counts.successes);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
        clargs_test_allocator_uninstall();
    }

    /* The loop ended because the operation succeeded, after one failure
     * for the area of each value, one for the array of the values of the
     * positional with one value and two for the growth of the array of the
     * positional with six. */
    NAOS_CHECK_TRUE(completed);
    NAOS_CHECK_UINT_EQ(failures, VALUE_STEPS + 3);

    /* The schema was allocated by the test allocator, so it is destroyed
     * with the test allocator set; the reset of the loop forgot its
     * blocks, so the count of live blocks is not checked after this. */
    clargs_command_destroy(root);
    destroy_types(&types);
    clargs_test_allocator_uninstall();
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
}

NAOS_SUITE(destruction,
    NAOS_ADD_TEST(destroy_of_null_does_nothing),
    NAOS_ADD_TEST(destroy_frees_everything_and_keeps_the_schema)
)

NAOS_SUITE(creation,
    NAOS_ADD_TEST(new_result_has_no_error_and_selects_the_root)
)

NAOS_SUITE(arguments,
    NAOS_ADD_TEST(no_argument_takes_no_copy),
    NAOS_ADD_TEST(one_argument_is_copied),
    NAOS_ADD_TEST(several_arguments_are_copied_in_one_allocation),
    NAOS_ADD_TEST(the_same_string_twice_is_copied_twice),
    NAOS_ADD_TEST(copy_does_not_change_when_the_arguments_change),
    NAOS_ADD_TEST(bytes_are_copied_as_they_are)
)

NAOS_SUITE(commands,
    NAOS_ADD_TEST(pushed_commands_are_read_in_their_order),
    NAOS_ADD_TEST(many_nested_commands_are_kept),
    NAOS_ADD_TEST(slots_are_allocated_only_for_what_a_command_has),
    NAOS_ADD_TEST(slots_of_the_root_are_allocated_at_the_creation)
)

NAOS_SUITE(options,
    NAOS_ADD_TEST(slots_start_as_not_given),
    NAOS_ADD_TEST(slots_of_a_pushed_command_start_as_not_given),
    NAOS_ADD_TEST(flag_recorded_twice_is_given),
    NAOS_ADD_TEST(count_is_the_number_of_records),
    NAOS_ADD_TEST(negatable_flag_has_the_last_recorded_state),
    NAOS_ADD_TEST(options_of_each_selected_command_are_kept_apart),
    NAOS_ADD_TEST(options_of_a_command_that_is_not_selected_read_as_not_given),
    NAOS_ADD_TEST(recording_and_reading_allocate_nothing)
)

NAOS_SUITE(values,
    NAOS_ADD_TEST(option_values_of_every_value_type_are_read),
    NAOS_ADD_TEST(option_value_takes_one_allocation),
    NAOS_ADD_TEST(values_that_were_not_recorded_read_as_no_value),
    NAOS_ADD_TEST(second_value_of_an_option_replaces_the_first),
    NAOS_ADD_TEST(failed_conversion_keeps_the_value_of_an_option),
    NAOS_ADD_TEST(positional_values_of_every_value_type_are_read),
    NAOS_ADD_TEST(positional_values_are_kept_in_their_order),
    NAOS_ADD_TEST(positional_default_value_has_the_source_default),
    NAOS_ADD_TEST(destroy_releases_every_value_once)
)

NAOS_SUITE(failure,
    NAOS_ADD_TEST(fails_at_every_allocation_point),
    NAOS_ADD_TEST(recording_values_fails_at_every_allocation_point)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(destruction),
    NAOS_ADD_SUITE(creation),
    NAOS_ADD_SUITE(arguments),
    NAOS_ADD_SUITE(commands),
    NAOS_ADD_SUITE(options),
    NAOS_ADD_SUITE(values),
    NAOS_ADD_SUITE(failure)
)
