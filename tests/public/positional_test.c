/*
 * Tests of the public positional API: the positional part of
 * <clargs/schema.h>.
 *
 * Positionals of the four arities are added to commands, given attributes
 * and read the way a consumer does it. These are also the tests of the
 * enumeration of a command that has positionals,
 * clargs_command_positional_count() and clargs_command_positional_at(), of
 * the destruction of a tree with positionals, and of the rule that a command
 * has positionals or subcommands, not both, seen from both sides.
 *
 * Every test that creates a command uses the test allocator
 * ("../support/test_allocator.h"). Its state and the allocator that is set
 * are shared by all the tests of this executable. Such a test starts with
 * an install, destroys what it created, sets the default allocator back
 * before it ends, and ends with check_clean().
 */
#include <clargs/clargs.h>

#include "../support/test_allocator.h"

#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* The four arities, in the order of the enumeration. */
#define ARITY_COUNT 4
static const clargs_arity_t all_arities[ARITY_COUNT] = { CLARGS_ARITY_REQUIRED, CLARGS_ARITY_OPTIONAL,
                                                         CLARGS_ARITY_ZERO_OR_MORE,
                                                         CLARGS_ARITY_ONE_OR_MORE };

/* The end of every test that allocates: the test allocator has no live
 * block and saw no violation of the allocator contract. */
static void check_clean(void)
{
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
}

/* Installs the test allocator and creates a root with the name "app". */
static clargs_command_t *create_root(void)
{
    clargs_command_t *root = NULL;

    clargs_test_allocator_install();
    NAOS_REQUIRE_TRUE(clargs_command_create("app", &root) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(root != NULL);
    return root;
}

/* Destroys root and ends the test: nothing stays allocated. */
static void destroy_root(clargs_command_t *root)
{
    clargs_command_destroy(root);
    clargs_test_allocator_uninstall();
    check_clean();
}

/* Adds a subcommand that must succeed, and returns it. */
static clargs_command_t *add_sub(clargs_command_t *command, const char *name)
{
    clargs_command_t *sub = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_subcommand(command, name, &sub) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(sub != NULL);
    return sub;
}

/* Adds a positional with the value type of strings that must succeed, and
 * returns it. */
static clargs_positional_t *add(clargs_command_t *command, const char *name, clargs_arity_t arity)
{
    clargs_positional_t *positional = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_positional(command, name, clargs_type_string(), arity,
                                                    &positional) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(positional != NULL);
    return positional;
}

/* Checks that the addition of a positional with the name name fails with
 * expected for every arity, sets the output to NULL, adds nothing and
 * allocates nothing. */
static void check_rejected(clargs_command_t *command, const char *name, clargs_error_t expected)
{
    const size_t count = clargs_command_positional_count(command);
    const size_t subcommands = clargs_command_subcommand_count(command);
    clargs_command_t *other = NULL;
    clargs_positional_t *sentinel;
    size_t requests;
    size_t index;

    /* What a failure must not leave in the output: a handle of another
     * tree. */
    NAOS_REQUIRE_TRUE(clargs_command_create("other", &other) == CLARGS_ERROR_NONE);
    sentinel = add(other, "sentinel", CLARGS_ARITY_REQUIRED);
    requests = clargs_test_allocator_requests();

    for (index = 0; index < ARITY_COUNT; ++index) {
        clargs_positional_t *positional = sentinel;

        NAOS_CHECK_INT_EQ(clargs_command_add_positional(command, name, clargs_type_string(),
                                                        all_arities[index], &positional),
                          expected);
        NAOS_CHECK_TRUE(positional == NULL);

        /* The failure needs no output either. */
        NAOS_CHECK_INT_EQ(clargs_command_add_positional(command, name, clargs_type_string(),
                                                        all_arities[index], NULL),
                          expected);
    }

    NAOS_CHECK_UINT_EQ(clargs_command_positional_count(command), count);
    NAOS_CHECK_UINT_EQ(clargs_command_subcommand_count(command), subcommands);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests);

    clargs_command_destroy(other);
}

/* Checks that the addition of a subcommand with the name name to command
 * fails with expected, sets the output to NULL, adds nothing and allocates
 * nothing. */
static void check_subcommand_rejected(clargs_command_t *command, const char *name, clargs_error_t expected)
{
    const size_t count = clargs_command_positional_count(command);
    const size_t subcommands = clargs_command_subcommand_count(command);
    const size_t requests = clargs_test_allocator_requests();
    clargs_command_t *sub = command;

    NAOS_CHECK_INT_EQ(clargs_command_add_subcommand(command, name, &sub), expected);
    NAOS_CHECK_TRUE(sub == NULL);
    NAOS_CHECK_INT_EQ(clargs_command_add_subcommand(command, name, NULL), expected);

    NAOS_CHECK_UINT_EQ(clargs_command_positional_count(command), count);
    NAOS_CHECK_UINT_EQ(clargs_command_subcommand_count(command), subcommands);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests);
}

/* Checks that actual is the string expected, or NULL when expected is
 * NULL. */
static void check_text(const char *actual, const char *expected)
{
    if (expected == NULL) {
        NAOS_CHECK_TRUE(actual == NULL);
    } else {
        NAOS_REQUIRE_TRUE(actual != NULL);
        NAOS_CHECK_STR_EQ(actual, expected);
    }
}

/* Checks what the readers return for what a positional got when it was
 * added. */
static void check_positional(const clargs_positional_t *positional, const clargs_command_t *command,
                             const char *name, clargs_arity_t arity, const clargs_type_t *type)
{
    NAOS_REQUIRE_TRUE(positional != NULL);
    NAOS_CHECK_TRUE(clargs_positional_command(positional) == command);
    NAOS_REQUIRE_TRUE(clargs_positional_name(positional) != NULL);
    NAOS_CHECK_STR_EQ(clargs_positional_name(positional), name);
    NAOS_CHECK_INT_EQ(clargs_positional_arity(positional), arity);
    NAOS_CHECK_TRUE(clargs_positional_type(positional) == type);
}

/* Checks the attributes of a positional. A string is NULL when the
 * attribute must not be set. */
static void check_attributes(const clargs_positional_t *positional, const char *description,
                             const char *default_value)
{
    check_text(clargs_positional_description(positional), description);
    check_text(clargs_positional_default(positional), default_value);
}

/* The attributes that are strings. */
typedef enum text_attribute { TEXT_DESCRIPTION, TEXT_DEFAULT } text_attribute_t;

#define TEXT_ATTRIBUTE_COUNT 2

/* Sets the attribute attribute with the function of that attribute. */
static clargs_error_t set_text(text_attribute_t attribute, clargs_positional_t *positional, const char *text)
{
    switch (attribute) {
    case TEXT_DESCRIPTION:
        return clargs_positional_set_description(positional, text);
    case TEXT_DEFAULT:
        return clargs_positional_set_default(positional, text);
    default:
        NAOS_FAIL("a test asked for an unknown attribute");
        /* NAOS_FAIL does not return; nothing may follow it (MSVC C4702). */
    }
}

/* Reads the attribute attribute with the function of that attribute. */
static const char *get_text(text_attribute_t attribute, const clargs_positional_t *positional)
{
    switch (attribute) {
    case TEXT_DESCRIPTION:
        return clargs_positional_description(positional);
    case TEXT_DEFAULT:
        return clargs_positional_default(positional);
    default:
        NAOS_FAIL("a test asked for an unknown attribute");
        /* NAOS_FAIL does not return; nothing may follow it (MSVC C4702). */
    }
}

/* ---- Addition ----------------------------------------------------------- */

/* A positional of each of the four arities is added, with its name and its
 * value type, and read back. */
NAOS_TEST(the_four_arities_are_added_and_read)
{
    static const char *const names[ARITY_COUNT] = { "input", "output", "extra", "file" };
    const clargs_type_t *types[ARITY_COUNT];
    size_t index;

    types[0] = clargs_type_string();
    types[1] = clargs_type_int();
    types[2] = clargs_type_float();
    types[3] = clargs_type_string();

    for (index = 0; index < ARITY_COUNT; ++index) {
        clargs_command_t *root = create_root();
        clargs_positional_t *positional = NULL;

        NAOS_CHECK_INT_EQ(clargs_command_add_positional(root, names[index], types[index],
                                                        all_arities[index], &positional),
                          CLARGS_ERROR_NONE);
        check_positional(positional, root, names[index], all_arities[index], types[index]);

        NAOS_CHECK_UINT_EQ(clargs_command_positional_count(root), 1);
        NAOS_CHECK_TRUE(clargs_command_positional_at(root, 0) == positional);

        /* Only a positional was added. */
        NAOS_CHECK_UINT_EQ(clargs_command_option_count(root), 0);
        NAOS_CHECK_UINT_EQ(clargs_command_subcommand_count(root), 0);

        destroy_root(root);
    }
}

/* The four arities are four different values. */
NAOS_TEST(the_four_arities_are_different)
{
    size_t first;
    size_t second;

    for (first = 0; first < ARITY_COUNT; ++first) {
        for (second = first + 1; second < ARITY_COUNT; ++second) {
            NAOS_CHECK_TRUE(all_arities[first] != all_arities[second]);
        }
    }
}

/* The name is for display and identification and never appears on a command
 * line: any string that is not empty is a name. */
NAOS_TEST(any_string_that_is_not_empty_is_a_name)
{
    static const char *const names[] = { "f",     "FILE",    "input-file", "<file>", "[file]...",
                                         "-x",    "--long",  "--",         "-",      " ",
                                         "a b",   "a=b",     "no-file",    "_",      "\t",
                                         "1",     "\xE5\x90\x8D\xE5\x89\x8D" };
    const size_t count = sizeof(names) / sizeof(names[0]);
    clargs_command_t *root = create_root();
    size_t index;

    for (index = 0; index < count; ++index) {
        clargs_positional_t *positional = NULL;

        NAOS_CHECK_INT_EQ(clargs_command_add_positional(root, names[index], clargs_type_string(),
                                                        CLARGS_ARITY_REQUIRED, &positional),
                          CLARGS_ERROR_NONE);
        check_positional(positional, root, names[index], CLARGS_ARITY_REQUIRED, clargs_type_string());
    }
    NAOS_CHECK_UINT_EQ(clargs_command_positional_count(root), count);

    destroy_root(root);
}

/* The name is copied: the caller may change its buffer after the call. */
NAOS_TEST(the_name_of_a_positional_is_copied)
{
    char name[] = "input";
    clargs_command_t *root = create_root();
    clargs_positional_t *positional;

    positional = add(root, name, CLARGS_ARITY_REQUIRED);
    memset(name, 'x', sizeof(name) - 1);

    NAOS_CHECK_TRUE(clargs_positional_name(positional) != name);
    NAOS_CHECK_STR_EQ(clargs_positional_name(positional), "input");

    /* The changed buffer is another name now. */
    NAOS_CHECK_INT_EQ(clargs_command_add_positional(root, name, clargs_type_string(), CLARGS_ARITY_REQUIRED,
                                                    NULL),
                      CLARGS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(clargs_command_positional_count(root), 2);

    destroy_root(root);
}

/* The output is optional: the positional is added and is read from the
 * command. */
NAOS_TEST(a_positional_can_be_added_without_an_output)
{
    clargs_command_t *root = create_root();
    size_t index;

    for (index = 0; index < ARITY_COUNT; ++index) {
        static const char *const names[ARITY_COUNT] = { "a", "b", "c", "d" };

        NAOS_CHECK_INT_EQ(clargs_command_add_positional(root, names[index], clargs_type_int(),
                                                        all_arities[index], NULL),
                          CLARGS_ERROR_NONE);
        NAOS_REQUIRE_TRUE(clargs_command_positional_count(root) == index + 1);
        check_positional(clargs_command_positional_at(root, index), root, names[index], all_arities[index],
                         clargs_type_int());
    }

    destroy_root(root);
}

/* A positional is added after the ones the command has: they are read in
 * the order of the definition, and a handle stays the same while the array
 * grows. */
NAOS_TEST(positionals_are_read_in_the_order_of_the_definition)
{
    static const char *const names[] = { "p00", "p01", "p02", "p03", "p04", "p05", "p06", "p07", "p08",
                                         "p09", "p10", "p11", "p12", "p13", "p14", "p15", "p16", "p17",
                                         "p18", "p19", "p20", "p21", "p22", "p23", "p24", "p25", "p26" };
    enum { COUNT = sizeof(names) / sizeof(names[0]) };
    clargs_command_t *root = create_root();
    const clargs_positional_t *handles[COUNT];
    size_t index;
    size_t earlier;

    NAOS_CHECK_UINT_EQ(clargs_command_positional_count(root), 0);

    for (index = 0; index < COUNT; ++index) {
        handles[index] = add(root, names[index], all_arities[index % ARITY_COUNT]);
        NAOS_CHECK_UINT_EQ(clargs_command_positional_count(root), index + 1);

        /* The earlier ones are where they were and what they were. */
        for (earlier = 0; earlier <= index; ++earlier) {
            NAOS_CHECK_TRUE(clargs_command_positional_at(root, earlier) == handles[earlier]);
        }
    }

    for (index = 0; index < COUNT; ++index) {
        check_positional(clargs_command_positional_at(root, index), root, names[index],
                         all_arities[index % ARITY_COUNT], clargs_type_string());
    }

    destroy_root(root);
}

/* An order of the arities that no command line can follow is found by the
 * validation of the tree, not by the addition: every addition succeeds. */
NAOS_TEST(the_order_of_the_arities_is_not_checked_by_the_addition)
{
    clargs_command_t *root = create_root();

    (void)add(root, "optional", CLARGS_ARITY_OPTIONAL);
    (void)add(root, "required-after-optional", CLARGS_ARITY_REQUIRED);
    (void)add(root, "many", CLARGS_ARITY_ZERO_OR_MORE);
    (void)add(root, "after-many", CLARGS_ARITY_OPTIONAL);
    (void)add(root, "one-or-more", CLARGS_ARITY_ONE_OR_MORE);
    (void)add(root, "many-again", CLARGS_ARITY_ZERO_OR_MORE);
    (void)add(root, "one-or-more-again", CLARGS_ARITY_ONE_OR_MORE);
    (void)add(root, "required-last", CLARGS_ARITY_REQUIRED);

    NAOS_CHECK_UINT_EQ(clargs_command_positional_count(root), 8);
    NAOS_CHECK_INT_EQ(clargs_positional_arity(clargs_command_positional_at(root, 1)), CLARGS_ARITY_REQUIRED);
    NAOS_CHECK_INT_EQ(clargs_positional_arity(clargs_command_positional_at(root, 7)), CLARGS_ARITY_REQUIRED);

    destroy_root(root);
}

/* The value type is borrowed: positionals and options share one, and the
 * tree does not destroy it. */
NAOS_TEST(a_value_type_is_borrowed_and_can_be_shared)
{
    static const char *const choices[] = { "debug", "release" };
    clargs_command_t *root = create_root();
    clargs_command_t *build = add_sub(root, "build");
    clargs_command_t *test = add_sub(root, "test");
    clargs_type_t *choice = NULL;
    clargs_positional_t *first = NULL;
    clargs_positional_t *second = NULL;
    clargs_positional_t *third = NULL;
    clargs_option_t *option = NULL;
    size_t value = 99;

    NAOS_REQUIRE_TRUE(clargs_type_create_choice(choices, 2, &choice) == CLARGS_ERROR_NONE);

    NAOS_CHECK_INT_EQ(clargs_command_add_positional(build, "mode", choice, CLARGS_ARITY_REQUIRED, &first),
                      CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_command_add_positional(build, "fallback", choice, CLARGS_ARITY_OPTIONAL, &second),
                      CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_command_add_positional(test, "mode", choice, CLARGS_ARITY_ONE_OR_MORE, &third),
                      CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_command_add_value(root, '\0', "mode", choice, &option), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_positional_set_default(second, "debug"), CLARGS_ERROR_NONE);

    NAOS_CHECK_TRUE(clargs_positional_type(first) == choice);
    NAOS_CHECK_TRUE(clargs_positional_type(second) == choice);
    NAOS_CHECK_TRUE(clargs_positional_type(third) == choice);
    NAOS_CHECK_TRUE(clargs_option_type(option) == choice);

    /* The value type outlives the tree and still works. */
    clargs_command_destroy(root);
    NAOS_CHECK_INT_EQ(clargs_type_convert(choice, "release", &value), CLARGS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(value, 1);
    clargs_type_release(choice, &value);

    clargs_type_destroy(choice);
    clargs_test_allocator_uninstall();
    check_clean();
}

/* ---- The checks of an addition ----------------------------------------- */

/* The empty string is not a name, for any arity. */
NAOS_TEST(an_empty_name_is_an_invalid_name)
{
    clargs_command_t *root = create_root();

    check_rejected(root, "", CLARGS_ERROR_INVALID_NAME);

    /* The same when the command has positionals already. */
    (void)add(root, "input", CLARGS_ARITY_REQUIRED);
    check_rejected(root, "", CLARGS_ERROR_INVALID_NAME);
    NAOS_CHECK_UINT_EQ(clargs_command_positional_count(root), 1);

    destroy_root(root);
}

/* A name is the name of one positional of a command, whatever the arities
 * and the value types are. */
NAOS_TEST(a_name_is_used_once_in_a_command)
{
    clargs_command_t *root = create_root();
    clargs_positional_t *input = add(root, "input", CLARGS_ARITY_REQUIRED);
    clargs_positional_t *output = add(root, "output", CLARGS_ARITY_OPTIONAL);
    clargs_positional_t *again = input;

    check_rejected(root, "input", CLARGS_ERROR_NAME_CONFLICT);
    check_rejected(root, "output", CLARGS_ERROR_NAME_CONFLICT);

    /* Nor with another value type. */
    NAOS_CHECK_INT_EQ(clargs_command_add_positional(root, "output", clargs_type_int(),
                                                    CLARGS_ARITY_ZERO_OR_MORE, &again),
                      CLARGS_ERROR_NAME_CONFLICT);
    NAOS_CHECK_TRUE(again == NULL);

    /* The two that were added are as they were. */
    NAOS_CHECK_UINT_EQ(clargs_command_positional_count(root), 2);
    NAOS_CHECK_TRUE(clargs_command_positional_at(root, 0) == input);
    NAOS_CHECK_TRUE(clargs_command_positional_at(root, 1) == output);
    check_positional(input, root, "input", CLARGS_ARITY_REQUIRED, clargs_type_string());
    check_positional(output, root, "output", CLARGS_ARITY_OPTIONAL, clargs_type_string());

    /* The whole name is compared, byte by byte: upper and lower case are
     * different, and so are a part of a name and a longer name. */
    (void)add(root, "Input", CLARGS_ARITY_OPTIONAL);
    (void)add(root, "INPUT", CLARGS_ARITY_OPTIONAL);
    (void)add(root, "in", CLARGS_ARITY_OPTIONAL);
    (void)add(root, "inputs", CLARGS_ARITY_OPTIONAL);
    NAOS_CHECK_UINT_EQ(clargs_command_positional_count(root), 6);
    check_rejected(root, "Input", CLARGS_ERROR_NAME_CONFLICT);
    check_rejected(root, "inputs", CLARGS_ERROR_NAME_CONFLICT);

    destroy_root(root);
}

/* The names of the positionals of a command are apart from every other
 * name: the same name can be used in another command, and for an option and
 * for the command itself. */
NAOS_TEST(the_same_name_can_be_used_elsewhere)
{
    clargs_command_t *root = create_root();
    clargs_command_t *build = add_sub(root, "build");
    clargs_command_t *test = add_sub(root, "test");
    clargs_positional_t *of_build = add(build, "target", CLARGS_ARITY_REQUIRED);
    clargs_positional_t *of_test = add(test, "target", CLARGS_ARITY_ZERO_OR_MORE);
    clargs_option_t *option = NULL;

    NAOS_CHECK_TRUE(of_build != of_test);
    check_positional(of_build, build, "target", CLARGS_ARITY_REQUIRED, clargs_type_string());
    check_positional(of_test, test, "target", CLARGS_ARITY_ZERO_OR_MORE, clargs_type_string());
    NAOS_CHECK_UINT_EQ(clargs_command_positional_count(build), 1);
    NAOS_CHECK_UINT_EQ(clargs_command_positional_count(test), 1);
    NAOS_CHECK_UINT_EQ(clargs_command_positional_count(root), 0);

    /* The long name of an option, in either order, and the name of the
     * command. */
    NAOS_CHECK_INT_EQ(clargs_command_add_flag(build, '\0', "target", &option), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_command_add_flag(build, 'o', "output", NULL), CLARGS_ERROR_NONE);
    (void)add(build, "output", CLARGS_ARITY_OPTIONAL);
    (void)add(build, "o", CLARGS_ARITY_OPTIONAL);
    (void)add(build, "build", CLARGS_ARITY_OPTIONAL);
    NAOS_CHECK_UINT_EQ(clargs_command_positional_count(build), 4);
    NAOS_CHECK_UINT_EQ(clargs_command_option_count(build), 2);

    destroy_root(root);
}

/* A command has positionals or subcommands, not both: a command with a
 * subcommand takes no positional. */
NAOS_TEST(a_command_with_a_subcommand_takes_no_positional)
{
    clargs_command_t *root = create_root();
    clargs_command_t *build = add_sub(root, "build");
    clargs_command_t *inner;

    check_rejected(root, "input", CLARGS_ERROR_INVALID_DEFINITION);

    /* The name of the subcommand makes no difference. */
    check_rejected(root, "build", CLARGS_ERROR_INVALID_DEFINITION);
    NAOS_CHECK_UINT_EQ(clargs_command_subcommand_count(root), 1);
    NAOS_CHECK_TRUE(clargs_command_subcommand_at(root, 0) == build);

    /* The subcommand has no subcommand of its own, so it takes
     * positionals; it does not once it has one. */
    inner = add_sub(build, "inner");
    check_rejected(build, "input", CLARGS_ERROR_INVALID_DEFINITION);
    (void)add(inner, "input", CLARGS_ARITY_REQUIRED);
    NAOS_CHECK_UINT_EQ(clargs_command_positional_count(inner), 1);
    NAOS_CHECK_UINT_EQ(clargs_command_positional_count(build), 0);
    NAOS_CHECK_UINT_EQ(clargs_command_positional_count(root), 0);

    destroy_root(root);
}

/* The other side of the same rule: a command with a positional takes no
 * subcommand. */
NAOS_TEST(a_command_with_a_positional_takes_no_subcommand)
{
    clargs_command_t *root = create_root();
    clargs_command_t *build = add_sub(root, "build");
    clargs_positional_t *input = add(build, "input", CLARGS_ARITY_REQUIRED);

    check_subcommand_rejected(build, "inner", CLARGS_ERROR_INVALID_DEFINITION);

    /* The name of the positional makes no difference. */
    check_subcommand_rejected(build, "input", CLARGS_ERROR_INVALID_DEFINITION);

    NAOS_CHECK_UINT_EQ(clargs_command_subcommand_count(build), 0);
    NAOS_CHECK_TRUE(clargs_command_find_subcommand(build, "inner") == NULL);
    NAOS_CHECK_UINT_EQ(clargs_command_positional_count(build), 1);
    NAOS_CHECK_TRUE(clargs_command_positional_at(build, 0) == input);

    /* Its parent has no positional and takes more subcommands. */
    (void)add_sub(root, "test");
    NAOS_CHECK_UINT_EQ(clargs_command_subcommand_count(root), 2);

    /* The same for a root, and for every arity. */
    {
        size_t index;

        for (index = 0; index < ARITY_COUNT; ++index) {
            clargs_command_t *other = NULL;

            NAOS_REQUIRE_TRUE(clargs_command_create("other", &other) == CLARGS_ERROR_NONE);
            (void)add(other, "file", all_arities[index]);
            check_subcommand_rejected(other, "sub", CLARGS_ERROR_INVALID_DEFINITION);
            clargs_command_destroy(other);
        }
    }

    destroy_root(root);
}

/* The rule is about positionals only: a command with subcommands takes
 * options of every kind, before and after it got the subcommands. */
NAOS_TEST(a_command_with_a_subcommand_takes_options)
{
    clargs_command_t *root = create_root();
    clargs_option_t *before = NULL;
    clargs_option_t *flag = NULL;
    clargs_option_t *negatable = NULL;
    clargs_option_t *count = NULL;
    clargs_option_t *value = NULL;
    clargs_command_t *build;

    NAOS_CHECK_INT_EQ(clargs_command_add_flag(root, 'q', "quiet", &before), CLARGS_ERROR_NONE);
    build = add_sub(root, "build");
    (void)add_sub(root, "test");

    NAOS_CHECK_INT_EQ(clargs_command_add_flag(root, 'v', "verbose", &flag), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_command_add_negatable(root, '\0', "color", &negatable), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_command_add_count(root, 'd', NULL, &count), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_command_add_value(root, 'C', "directory", clargs_type_string(), &value),
                      CLARGS_ERROR_NONE);

    NAOS_CHECK_UINT_EQ(clargs_command_option_count(root), 5);
    NAOS_CHECK_TRUE(clargs_command_option_at(root, 0) == before);
    NAOS_CHECK_TRUE(clargs_command_option_at(root, 1) == flag);
    NAOS_CHECK_TRUE(clargs_command_option_at(root, 2) == negatable);
    NAOS_CHECK_TRUE(clargs_command_option_at(root, 3) == count);
    NAOS_CHECK_TRUE(clargs_command_option_at(root, 4) == value);
    NAOS_CHECK_TRUE(clargs_option_command(value) == root);
    NAOS_CHECK_UINT_EQ(clargs_command_subcommand_count(root), 2);

    /* A command with positionals takes options as well, and one with
     * options takes positionals. */
    NAOS_CHECK_INT_EQ(clargs_command_add_flag(build, 'f', "force", NULL), CLARGS_ERROR_NONE);
    (void)add(build, "target", CLARGS_ARITY_ZERO_OR_MORE);
    NAOS_CHECK_INT_EQ(clargs_command_add_value(build, 'j', "jobs", clargs_type_int(), NULL),
                      CLARGS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(clargs_command_option_count(build), 2);
    NAOS_CHECK_UINT_EQ(clargs_command_positional_count(build), 1);

    destroy_root(root);
}

/* The checks are made in the order of the contract: the name first, then
 * the rule of positionals and subcommands, then the conflict of names. */
NAOS_TEST(the_checks_are_made_in_order)
{
    clargs_command_t *root = create_root();
    clargs_command_t *build = add_sub(root, "build");
    clargs_command_t *test = add_sub(root, "test");

    /* An empty name on a command with a subcommand is an invalid name. */
    check_rejected(root, "", CLARGS_ERROR_INVALID_NAME);

    /* A name that is not the name of a subcommand, added to a command with
     * a positional, is an invalid name; a name that is one is refused by
     * the rule. */
    (void)add(build, "input", CLARGS_ARITY_REQUIRED);
    check_subcommand_rejected(build, "", CLARGS_ERROR_INVALID_NAME);
    check_subcommand_rejected(build, "-inner", CLARGS_ERROR_INVALID_NAME);
    check_subcommand_rejected(build, "inner", CLARGS_ERROR_INVALID_DEFINITION);

    /* A failure changes nothing: what was refused for another reason is
     * added afterwards. */
    (void)add(test, "input", CLARGS_ARITY_REQUIRED);
    check_rejected(test, "input", CLARGS_ERROR_NAME_CONFLICT);
    check_rejected(test, "", CLARGS_ERROR_INVALID_NAME);
    (void)add(test, "output", CLARGS_ARITY_OPTIONAL);
    NAOS_CHECK_UINT_EQ(clargs_command_positional_count(test), 2);

    destroy_root(root);
}

/* ---- Attributes --------------------------------------------------------- */

/* A positional that was just added has no description and no default
 * value, whatever its arity is. */
NAOS_TEST(a_new_positional_has_no_attribute)
{
    static const char *const names[ARITY_COUNT] = { "a", "b", "c", "d" };
    clargs_command_t *root = create_root();
    size_t index;

    for (index = 0; index < ARITY_COUNT; ++index) {
        check_attributes(add(root, names[index], all_arities[index]), NULL, NULL);
    }

    destroy_root(root);
}

/* Every arity takes a description. The empty string is a description; it is
 * not the same as none. */
NAOS_TEST(a_description_is_set_on_every_arity)
{
    static const char *const names[ARITY_COUNT] = { "a", "b", "c", "d" };
    static const char *const texts[ARITY_COUNT] = { "The file to read.", "", "More files.",
                                                    "At least one file." };
    clargs_command_t *root = create_root();
    clargs_positional_t *positionals[ARITY_COUNT];
    size_t index;

    for (index = 0; index < ARITY_COUNT; ++index) {
        positionals[index] = add(root, names[index], all_arities[index]);
    }

    for (index = 0; index < ARITY_COUNT; ++index) {
        NAOS_CHECK_INT_EQ(clargs_positional_set_description(positionals[index], texts[index]),
                          CLARGS_ERROR_NONE);
    }

    /* Each has its own, and nothing else changed. */
    for (index = 0; index < ARITY_COUNT; ++index) {
        check_attributes(positionals[index], texts[index], NULL);
        check_positional(positionals[index], root, names[index], all_arities[index], clargs_type_string());
        NAOS_CHECK_TRUE(clargs_command_positional_at(root, index) == positionals[index]);
    }
    NAOS_CHECK_UINT_EQ(clargs_command_positional_count(root), ARITY_COUNT);

    destroy_root(root);
}

/* An optional positional, none or one argument, takes a default value. The
 * string is kept as it is: whether the value type takes it is found by the
 * validation of the tree. */
NAOS_TEST(a_default_value_is_set_on_an_optional_positional)
{
    clargs_command_t *root = create_root();
    clargs_positional_t *output = add(root, "output", CLARGS_ARITY_OPTIONAL);
    clargs_positional_t *level = NULL;
    clargs_positional_t *empty = add(root, "empty", CLARGS_ARITY_OPTIONAL);

    NAOS_REQUIRE_TRUE(clargs_command_add_positional(root, "level", clargs_type_int(), CLARGS_ARITY_OPTIONAL,
                                                    &level) == CLARGS_ERROR_NONE);

    NAOS_CHECK_INT_EQ(clargs_positional_set_default(output, "a.out"), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_positional_set_default(level, "not a number"), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_positional_set_default(empty, ""), CLARGS_ERROR_NONE);

    check_attributes(output, NULL, "a.out");
    check_attributes(level, NULL, "not a number");
    check_attributes(empty, NULL, "");

    /* The description and the default value are set on their own, in any
     * order. */
    NAOS_CHECK_INT_EQ(clargs_positional_set_description(output, "Where the output goes."),
                      CLARGS_ERROR_NONE);
    check_attributes(output, "Where the output goes.", "a.out");
    check_positional(output, root, "output", CLARGS_ARITY_OPTIONAL, clargs_type_string());
    check_positional(level, root, "level", CLARGS_ARITY_OPTIONAL, clargs_type_int());

    destroy_root(root);
}

/* Only an optional positional takes a default value. For the three other
 * arities the set is an inconsistent definition, whatever the value is; it
 * sets nothing and allocates nothing. */
NAOS_TEST(a_default_value_on_another_arity_is_an_inconsistent_definition)
{
    static const clargs_arity_t others[] = { CLARGS_ARITY_REQUIRED, CLARGS_ARITY_ZERO_OR_MORE,
                                             CLARGS_ARITY_ONE_OR_MORE };
    static const char *const names[] = { "required", "zero-or-more", "one-or-more" };
    size_t index;

    for (index = 0; index < sizeof(others) / sizeof(others[0]); ++index) {
        clargs_command_t *root = create_root();
        clargs_positional_t *positional = add(root, names[index], others[index]);
        size_t requests = clargs_test_allocator_requests();
        size_t live = clargs_test_allocator_live();

        NAOS_CHECK_INT_EQ(clargs_positional_set_default(positional, "value"),
                          CLARGS_ERROR_INVALID_DEFINITION);
        NAOS_CHECK_INT_EQ(clargs_positional_set_default(positional, ""), CLARGS_ERROR_INVALID_DEFINITION);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);
        check_attributes(positional, NULL, NULL);

        /* It takes a description, and the failed set does not touch the
         * description either. */
        NAOS_CHECK_INT_EQ(clargs_positional_set_description(positional, "Described."), CLARGS_ERROR_NONE);
        NAOS_CHECK_INT_EQ(clargs_positional_set_default(positional, "value"),
                          CLARGS_ERROR_INVALID_DEFINITION);
        check_attributes(positional, "Described.", NULL);
        check_positional(positional, root, names[index], others[index], clargs_type_string());
        NAOS_CHECK_UINT_EQ(clargs_command_positional_count(root), 1);

        destroy_root(root);
    }
}

/* A set replaces what was set before, with a longer and with a shorter
 * string, and the old string is freed. */
NAOS_TEST(an_attribute_that_is_set_again_is_replaced)
{
    clargs_command_t *root = create_root();
    clargs_positional_t *output = add(root, "output", CLARGS_ARITY_OPTIONAL);
    size_t live;

    NAOS_CHECK_INT_EQ(clargs_positional_set_description(output, "First."), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_positional_set_default(output, "first"), CLARGS_ERROR_NONE);
    check_attributes(output, "First.", "first");
    live = clargs_test_allocator_live();

    NAOS_CHECK_INT_EQ(clargs_positional_set_description(output, "The second and longer description."),
                      CLARGS_ERROR_NONE);
    check_attributes(output, "The second and longer description.", "first");
    NAOS_CHECK_INT_EQ(clargs_positional_set_default(output, "the second and longer value"),
                      CLARGS_ERROR_NONE);
    check_attributes(output, "The second and longer description.", "the second and longer value");

    NAOS_CHECK_INT_EQ(clargs_positional_set_description(output, "3"), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_positional_set_default(output, "3"), CLARGS_ERROR_NONE);
    check_attributes(output, "3", "3");

    /* The empty string replaces too; the attribute stays set. */
    NAOS_CHECK_INT_EQ(clargs_positional_set_description(output, ""), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_positional_set_default(output, ""), CLARGS_ERROR_NONE);
    check_attributes(output, "", "");

    /* Each replaced string was freed: the positional holds two strings, as
     * before. */
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);
    check_positional(output, root, "output", CLARGS_ARITY_OPTIONAL, clargs_type_string());

    destroy_root(root);
}

/* The strings are copied: the caller may change its buffers after the
 * call. */
NAOS_TEST(the_strings_of_the_attributes_are_copied)
{
    char description[] = "Where the output goes.";
    char default_value[] = "a.out";
    clargs_command_t *root = create_root();
    clargs_positional_t *output = add(root, "output", CLARGS_ARITY_OPTIONAL);

    NAOS_CHECK_INT_EQ(clargs_positional_set_description(output, description), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_positional_set_default(output, default_value), CLARGS_ERROR_NONE);

    memset(description, 'x', sizeof(description) - 1);
    memset(default_value, 'z', sizeof(default_value) - 1);

    NAOS_CHECK_TRUE(clargs_positional_description(output) != description);
    NAOS_CHECK_TRUE(clargs_positional_default(output) != default_value);
    check_attributes(output, "Where the output goes.", "a.out");

    /* The same when a value is replaced. */
    memcpy(default_value, "b.out", sizeof(default_value));
    NAOS_CHECK_INT_EQ(clargs_positional_set_default(output, default_value), CLARGS_ERROR_NONE);
    memset(default_value, 'z', sizeof(default_value) - 1);
    check_attributes(output, "Where the output goes.", "b.out");

    destroy_root(root);
}

/* An attribute can be set with a string that was read from another
 * attribute or from a name of the tree: it gets a copy of its own. */
NAOS_TEST(an_attribute_can_be_set_with_a_string_that_was_read)
{
    clargs_command_t *root = create_root();
    clargs_positional_t *output = add(root, "output", CLARGS_ARITY_OPTIONAL);

    NAOS_CHECK_INT_EQ(clargs_positional_set_description(output, clargs_positional_name(output)),
                      CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_positional_set_default(output, clargs_positional_description(output)),
                      CLARGS_ERROR_NONE);
    check_attributes(output, "output", "output");
    NAOS_CHECK_TRUE(clargs_positional_description(output) != clargs_positional_name(output));
    NAOS_CHECK_TRUE(clargs_positional_default(output) != clargs_positional_description(output));

    destroy_root(root);
}

/* ---- Destruction ------------------------------------------------------- */

/* One destruction of the root frees the positionals of every command, their
 * names and the strings of their attributes. */
NAOS_TEST(destroy_frees_the_positionals_of_the_whole_tree)
{
    clargs_command_t *root = create_root();
    clargs_command_t *build = add_sub(root, "build");
    clargs_command_t *remote = add_sub(root, "remote");
    clargs_command_t *add_remote = add_sub(remote, "add");
    clargs_positional_t *output;
    clargs_positional_t *url;

    (void)add(build, "input", CLARGS_ARITY_REQUIRED);
    output = add(build, "output", CLARGS_ARITY_OPTIONAL);
    (void)add(build, "extra", CLARGS_ARITY_ZERO_OR_MORE);
    (void)add(add_remote, "name", CLARGS_ARITY_REQUIRED);
    url = add(add_remote, "url", CLARGS_ARITY_ONE_OR_MORE);
    NAOS_CHECK_INT_EQ(clargs_command_add_flag(build, 'f', "force", NULL), CLARGS_ERROR_NONE);

    NAOS_CHECK_INT_EQ(clargs_positional_set_description(output, "Where the output goes."),
                      CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_positional_set_default(output, "a.out"), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_positional_set_default(output, "b.out"), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_positional_set_description(url, "The addresses."), CLARGS_ERROR_NONE);

    NAOS_CHECK_TRUE(clargs_test_allocator_live() > 0);

    clargs_command_destroy(root);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);

    clargs_test_allocator_uninstall();
    check_clean();
}

/* ---- Failure of an allocation ------------------------------------------ */

/* Builds a root with existing positionals "p0", "p1", ... of the four
 * arities in turn, and returns it. The first gets a description and the
 * second a default value. The number is chosen by the tests so that the
 * array of the positionals is empty, has room, or is full. *last is the
 * positional that was added last, or NULL when existing is 0. */
static clargs_command_t *build_root_with(size_t existing, clargs_positional_t **last)
{
    static const char *const names[] = { "p0", "p1", "p2", "p3", "p4", "p5", "p6", "p7" };
    clargs_command_t *root = NULL;
    size_t index;

    *last = NULL;
    NAOS_REQUIRE_TRUE(existing <= sizeof(names) / sizeof(names[0]));
    NAOS_REQUIRE_TRUE(clargs_command_create("app", &root) == CLARGS_ERROR_NONE);
    for (index = 0; index < existing; ++index) {
        *last = add(root, names[index], all_arities[index % ARITY_COUNT]);
        if (index == 0) {
            NAOS_REQUIRE_TRUE(clargs_positional_set_description(*last, "The first.") == CLARGS_ERROR_NONE);
        } else if (index == 1) {
            NAOS_REQUIRE_TRUE(clargs_positional_set_default(*last, "second") == CLARGS_ERROR_NONE);
        }
    }
    return root;
}

/* Checks that root is what build_root_with(existing, ...) built. */
static void check_built_root(const clargs_command_t *root, size_t existing)
{
    static const char *const names[] = { "p0", "p1", "p2", "p3", "p4", "p5", "p6", "p7" };
    size_t index;

    NAOS_CHECK_STR_EQ(clargs_command_name(root), "app");
    NAOS_REQUIRE_TRUE(clargs_command_positional_count(root) == existing);
    NAOS_CHECK_UINT_EQ(clargs_command_option_count(root), 0);
    NAOS_CHECK_UINT_EQ(clargs_command_subcommand_count(root), 0);
    for (index = 0; index < existing; ++index) {
        const clargs_positional_t *old = clargs_command_positional_at(root, index);

        check_positional(old, root, names[index], all_arities[index % ARITY_COUNT], clargs_type_string());
        check_attributes(old, index == 0 ? "The first." : NULL, index == 1 ? "second" : NULL);
    }
}

/* Fails each allocation of the addition of a positional of the arity arity,
 * with the name "new", to a root that has existing positionals already.
 * Returns the number of the failures. */
static size_t fail_each_allocation_of_an_addition(clargs_arity_t arity, size_t existing)
{
    const size_t limit = 20;
    size_t failures = 0;
    bool completed = false;
    size_t n;

    for (n = 1; n <= limit; ++n) {
        clargs_command_t *root;
        clargs_positional_t *positional;
        clargs_error_t error;
        size_t requests;
        size_t live;

        clargs_test_allocator_reset();

        /* What a failure must not leave in the output: the positional
         * that was added last, when there is one. */
        root = build_root_with(existing, &positional);
        requests = clargs_test_allocator_requests();
        live = clargs_test_allocator_live();
        clargs_test_allocator_fail_at(requests + n);

        error = clargs_command_add_positional(root, "new", clargs_type_int(), arity, &positional);

        if (error == CLARGS_ERROR_NONE && clargs_test_allocator_requests() < requests + n) {
            /* Every allocation point has been passed. */
            NAOS_CHECK_TRUE(positional != NULL);
            NAOS_CHECK_UINT_EQ(clargs_command_positional_count(root), existing + 1);
            clargs_command_destroy(root);
            completed = true;
            break;
        }

        ++failures;
        NAOS_CHECK_INT_EQ(error, CLARGS_ERROR_OUT_OF_MEMORY);
        NAOS_CHECK_TRUE(positional == NULL);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests + n);

        /* The tree is as it was before the call, and nothing of the call
         * stays allocated. */
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);
        check_built_root(root, existing);

        /* The same addition works afterwards. */
        clargs_test_allocator_fail_at(0);
        NAOS_CHECK_INT_EQ(clargs_command_add_positional(root, "new", clargs_type_int(), arity, &positional),
                          CLARGS_ERROR_NONE);
        NAOS_REQUIRE_TRUE(clargs_command_positional_count(root) == existing + 1);
        NAOS_CHECK_TRUE(clargs_command_positional_at(root, existing) == positional);
        check_positional(positional, root, "new", arity, clargs_type_int());
        check_attributes(positional, NULL, NULL);

        clargs_command_destroy(root);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
    }

    NAOS_CHECK_TRUE(completed);
    return failures;
}

/* When an allocation of the addition of a positional fails, whichever it
 * is, the output is NULL, the tree is as it was, and nothing leaks. The
 * array of the positionals grows in the first and in the third case, and
 * has room in the second. The arity makes no difference to the
 * allocations. */
NAOS_TEST(a_failed_addition_leaves_the_tree_as_it_was)
{
    size_t with_room_of_the_first = 0;
    size_t index;

    clargs_test_allocator_install();

    for (index = 0; index < ARITY_COUNT; ++index) {
        size_t growing = fail_each_allocation_of_an_addition(all_arities[index], 0);
        size_t with_room = fail_each_allocation_of_an_addition(all_arities[index], 1);

        NAOS_CHECK_TRUE(with_room > 0);
        NAOS_CHECK_UINT_EQ(growing, with_room + 1);
        NAOS_CHECK_UINT_EQ(fail_each_allocation_of_an_addition(all_arities[index], 4), growing);

        if (index == 0) {
            with_room_of_the_first = with_room;
        } else {
            NAOS_CHECK_UINT_EQ(with_room, with_room_of_the_first);
        }
    }

    clargs_test_allocator_uninstall();
    check_clean();
}

/* When the copy of a string of an attribute cannot be allocated, the set is
 * out of memory, the attribute has the value it had, set or not, and nothing
 * leaks. A set makes one allocation, the copy. */
NAOS_TEST(a_failed_set_of_a_string_keeps_the_value_it_had)
{
    static const text_attribute_t attributes[TEXT_ATTRIBUTE_COUNT] = { TEXT_DESCRIPTION, TEXT_DEFAULT };
    size_t index;
    size_t was_set;

    clargs_test_allocator_install();

    for (index = 0; index < TEXT_ATTRIBUTE_COUNT; ++index) {
        for (was_set = 0; was_set < 2; ++was_set) {
            const text_attribute_t attribute = attributes[index];
            const char *const old = was_set != 0 ? "old value" : NULL;
            clargs_command_t *root = NULL;
            clargs_positional_t *positional;
            size_t requests;
            size_t live;
            size_t other;

            clargs_test_allocator_reset();
            NAOS_REQUIRE_TRUE(clargs_command_create("app", &root) == CLARGS_ERROR_NONE);
            positional = add(root, "output", CLARGS_ARITY_OPTIONAL);

            /* The other attribute is set, so that it shows if the failure
             * touches it. */
            for (other = 0; other < TEXT_ATTRIBUTE_COUNT; ++other) {
                if (other != index) {
                    NAOS_REQUIRE_TRUE(set_text(attributes[other], positional, "other") == CLARGS_ERROR_NONE);
                }
            }
            if (old != NULL) {
                NAOS_REQUIRE_TRUE(set_text(attribute, positional, old) == CLARGS_ERROR_NONE);
            }

            requests = clargs_test_allocator_requests();
            live = clargs_test_allocator_live();
            clargs_test_allocator_fail_at(requests + 1);

            NAOS_CHECK_INT_EQ(set_text(attribute, positional, "a new and longer value"),
                              CLARGS_ERROR_OUT_OF_MEMORY);
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests + 1);

            /* The positional is as it was before the call, and nothing of
             * the call stays allocated. */
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);
            check_text(get_text(attribute, positional), old);
            for (other = 0; other < TEXT_ATTRIBUTE_COUNT; ++other) {
                if (other != index) {
                    check_text(get_text(attributes[other], positional), "other");
                }
            }
            check_positional(positional, root, "output", CLARGS_ARITY_OPTIONAL, clargs_type_string());
            NAOS_CHECK_UINT_EQ(clargs_command_positional_count(root), 1);

            /* The same set works afterwards, with one allocation. */
            clargs_test_allocator_fail_at(0);
            NAOS_CHECK_INT_EQ(set_text(attribute, positional, "a new and longer value"), CLARGS_ERROR_NONE);
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests + 2);
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), old != NULL ? live : live + 1);
            check_text(get_text(attribute, positional), "a new and longer value");

            clargs_command_destroy(root);
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
        }
    }

    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_SUITE(add,
    NAOS_ADD_TEST(the_four_arities_are_added_and_read),
    NAOS_ADD_TEST(the_four_arities_are_different),
    NAOS_ADD_TEST(any_string_that_is_not_empty_is_a_name),
    NAOS_ADD_TEST(the_name_of_a_positional_is_copied),
    NAOS_ADD_TEST(a_positional_can_be_added_without_an_output),
    NAOS_ADD_TEST(positionals_are_read_in_the_order_of_the_definition),
    NAOS_ADD_TEST(the_order_of_the_arities_is_not_checked_by_the_addition),
    NAOS_ADD_TEST(a_value_type_is_borrowed_and_can_be_shared)
)

NAOS_SUITE(check,
    NAOS_ADD_TEST(an_empty_name_is_an_invalid_name),
    NAOS_ADD_TEST(a_name_is_used_once_in_a_command),
    NAOS_ADD_TEST(the_same_name_can_be_used_elsewhere),
    NAOS_ADD_TEST(a_command_with_a_subcommand_takes_no_positional),
    NAOS_ADD_TEST(a_command_with_a_positional_takes_no_subcommand),
    NAOS_ADD_TEST(a_command_with_a_subcommand_takes_options),
    NAOS_ADD_TEST(the_checks_are_made_in_order)
)

NAOS_SUITE(attribute,
    NAOS_ADD_TEST(a_new_positional_has_no_attribute),
    NAOS_ADD_TEST(a_description_is_set_on_every_arity),
    NAOS_ADD_TEST(a_default_value_is_set_on_an_optional_positional),
    NAOS_ADD_TEST(a_default_value_on_another_arity_is_an_inconsistent_definition),
    NAOS_ADD_TEST(an_attribute_that_is_set_again_is_replaced),
    NAOS_ADD_TEST(the_strings_of_the_attributes_are_copied),
    NAOS_ADD_TEST(an_attribute_can_be_set_with_a_string_that_was_read)
)

NAOS_SUITE(destroy,
    NAOS_ADD_TEST(destroy_frees_the_positionals_of_the_whole_tree)
)

NAOS_SUITE(failure,
    NAOS_ADD_TEST(a_failed_addition_leaves_the_tree_as_it_was),
    NAOS_ADD_TEST(a_failed_set_of_a_string_keeps_the_value_it_had)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(add),
    NAOS_ADD_SUITE(check),
    NAOS_ADD_SUITE(attribute),
    NAOS_ADD_SUITE(destroy),
    NAOS_ADD_SUITE(failure)
)
