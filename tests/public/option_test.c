/*
 * Tests of the public option API: the option part of <clargs/schema.h>.
 *
 * Options of the four kinds are added to commands, read and looked up the
 * way a consumer does it. These are also the tests of the enumeration and
 * of the lookups of a command that has options: clargs_command_option_at(),
 * clargs_command_find_short() and clargs_command_find_long().
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

/* Adds an option of the kind kind with the function of that kind. An option
 * with a value gets the value type of strings. */
static clargs_error_t add_kind(clargs_option_kind_t kind, clargs_command_t *command, char short_name,
                               const char *long_name, clargs_option_t **out)
{
    switch (kind) {
    case CLARGS_OPTION_FLAG:
        return clargs_command_add_flag(command, short_name, long_name, out);
    case CLARGS_OPTION_NEGATABLE:
        return clargs_command_add_negatable(command, short_name, long_name, out);
    case CLARGS_OPTION_COUNT:
        return clargs_command_add_count(command, short_name, long_name, out);
    case CLARGS_OPTION_VALUE:
        return clargs_command_add_value(command, short_name, long_name, clargs_type_string(), out);
    default:
        NAOS_FAIL("a test asked for an option of an unknown kind");
        return CLARGS_ERROR_NONE;
    }
}

/* Adds an option that must succeed, and returns it. */
static clargs_option_t *add(clargs_option_kind_t kind, clargs_command_t *command, char short_name,
                            const char *long_name)
{
    clargs_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(add_kind(kind, command, short_name, long_name, &option) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(option != NULL);
    return option;
}

/* Checks that an addition of every kind with these names fails with
 * expected, sets the output to NULL, adds nothing and allocates nothing. A
 * negatable flag without a long name is an invalid name whatever else is
 * wrong: that check comes before the conflicts. */
static void check_rejected(clargs_command_t *command, char short_name, const char *long_name,
                           clargs_error_t expected)
{
    static const clargs_option_kind_t kinds[] = { CLARGS_OPTION_FLAG, CLARGS_OPTION_NEGATABLE,
                                                  CLARGS_OPTION_COUNT, CLARGS_OPTION_VALUE };
    const size_t count = clargs_command_option_count(command);
    const size_t requests = clargs_test_allocator_requests();
    size_t index;

    for (index = 0; index < sizeof(kinds) / sizeof(kinds[0]); ++index) {
        clargs_error_t error = expected;
        clargs_option_t *sentinel = NULL;
        clargs_option_t *option;

        if (kinds[index] == CLARGS_OPTION_NEGATABLE && long_name == NULL) {
            error = CLARGS_ERROR_INVALID_NAME;
        }

        /* What a failure must not leave in the output. */
        if (count != 0) {
            sentinel = (clargs_option_t *)clargs_command_option_at(command, 0);
        }
        option = sentinel;

        NAOS_CHECK_INT_EQ(add_kind(kinds[index], command, short_name, long_name, &option), error);
        NAOS_CHECK_TRUE(option == NULL);

        /* The failure needs no output either. */
        NAOS_CHECK_INT_EQ(add_kind(kinds[index], command, short_name, long_name, NULL), error);
    }

    NAOS_CHECK_UINT_EQ(clargs_command_option_count(command), count);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests);
}

/* Checks what the readers return for an option. negated_name and type are
 * NULL when the option must have none. */
static void check_option(const clargs_option_t *option, const clargs_command_t *command,
                         clargs_option_kind_t kind, char short_name, const char *long_name,
                         const char *negated_name, const clargs_type_t *type)
{
    NAOS_REQUIRE_TRUE(option != NULL);
    NAOS_CHECK_TRUE(clargs_option_command(option) == command);
    NAOS_CHECK_INT_EQ(clargs_option_kind(option), kind);
    NAOS_CHECK_INT_EQ(clargs_option_short_name(option), short_name);

    if (long_name == NULL) {
        NAOS_CHECK_TRUE(clargs_option_long_name(option) == NULL);
    } else {
        NAOS_REQUIRE_TRUE(clargs_option_long_name(option) != NULL);
        NAOS_CHECK_STR_EQ(clargs_option_long_name(option), long_name);
    }

    if (negated_name == NULL) {
        NAOS_CHECK_TRUE(clargs_option_negated_name(option) == NULL);
    } else {
        NAOS_REQUIRE_TRUE(clargs_option_negated_name(option) != NULL);
        NAOS_CHECK_STR_EQ(clargs_option_negated_name(option), negated_name);
    }

    NAOS_CHECK_TRUE(clargs_option_type(option) == type);
}

/* Looks name up as a long name, the whole string. */
static const clargs_option_t *find_long(const clargs_command_t *command, const char *name, bool *negated)
{
    return clargs_command_find_long(command, name, strlen(name), negated);
}

/* ---- Addition and reading ---------------------------------------------- */

NAOS_TEST(the_four_kinds_are_added_and_read)
{
    clargs_command_t *root = create_root();
    clargs_option_t *flag = NULL;
    clargs_option_t *negatable = NULL;
    clargs_option_t *count = NULL;
    clargs_option_t *value = NULL;

    NAOS_CHECK_INT_EQ(clargs_command_add_flag(root, 'v', "verbose", &flag), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_command_add_negatable(root, 'c', "color", &negatable), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_command_add_count(root, 'd', "debug", &count), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_command_add_value(root, 'j', "jobs", clargs_type_int(), &value),
                      CLARGS_ERROR_NONE);

    NAOS_CHECK_UINT_EQ(clargs_command_option_count(root), 4);

    /* Only a negatable flag has a negated name, and only an option with a
     * value has a value type. */
    check_option(flag, root, CLARGS_OPTION_FLAG, 'v', "verbose", NULL, NULL);
    check_option(negatable, root, CLARGS_OPTION_NEGATABLE, 'c', "color", "no-color", NULL);
    check_option(count, root, CLARGS_OPTION_COUNT, 'd', "debug", NULL, NULL);
    check_option(value, root, CLARGS_OPTION_VALUE, 'j', "jobs", NULL, clargs_type_int());

    /* The rest of the command is as it was. */
    NAOS_CHECK_STR_EQ(clargs_command_name(root), "app");
    NAOS_CHECK_UINT_EQ(clargs_command_positional_count(root), 0);
    NAOS_CHECK_UINT_EQ(clargs_command_subcommand_count(root), 0);

    destroy_root(root);
}

/* No short name is read as '\0', no long name as NULL. */
NAOS_TEST(an_option_has_a_short_name_a_long_name_or_both)
{
    clargs_command_t *root = create_root();

    check_option(add(CLARGS_OPTION_FLAG, root, 'a', NULL), root, CLARGS_OPTION_FLAG, 'a', NULL, NULL, NULL);
    check_option(add(CLARGS_OPTION_FLAG, root, '\0', "all"), root, CLARGS_OPTION_FLAG, '\0', "all", NULL,
                 NULL);
    check_option(add(CLARGS_OPTION_FLAG, root, 'B', "both"), root, CLARGS_OPTION_FLAG, 'B', "both", NULL,
                 NULL);

    check_option(add(CLARGS_OPTION_COUNT, root, 'd', NULL), root, CLARGS_OPTION_COUNT, 'd', NULL, NULL,
                 NULL);
    check_option(add(CLARGS_OPTION_COUNT, root, '\0', "debug"), root, CLARGS_OPTION_COUNT, '\0', "debug",
                 NULL, NULL);
    check_option(add(CLARGS_OPTION_COUNT, root, 'q', "quiet"), root, CLARGS_OPTION_COUNT, 'q', "quiet",
                 NULL, NULL);

    check_option(add(CLARGS_OPTION_VALUE, root, 'o', NULL), root, CLARGS_OPTION_VALUE, 'o', NULL, NULL,
                 clargs_type_string());
    check_option(add(CLARGS_OPTION_VALUE, root, '\0', "output"), root, CLARGS_OPTION_VALUE, '\0', "output",
                 NULL, clargs_type_string());
    check_option(add(CLARGS_OPTION_VALUE, root, 'f', "file"), root, CLARGS_OPTION_VALUE, 'f', "file", NULL,
                 clargs_type_string());

    /* A negatable flag always has a long name. */
    check_option(add(CLARGS_OPTION_NEGATABLE, root, '\0', "color"), root, CLARGS_OPTION_NEGATABLE, '\0',
                 "color", "no-color", NULL);
    check_option(add(CLARGS_OPTION_NEGATABLE, root, 'p', "pager"), root, CLARGS_OPTION_NEGATABLE, 'p',
                 "pager", "no-pager", NULL);

    NAOS_CHECK_UINT_EQ(clargs_command_option_count(root), 11);

    destroy_root(root);
}

NAOS_TEST(the_names_follow_the_rule_of_names)
{
    static const char shorts[] = { 'a', 'z', 'A', 'Z', '0', '9' };
    static const char *const longs[] = { "ab", "a-", "a_", "7z", "dry-run", "dry_run", "Aa", "no-x", "x2" };
    clargs_command_t *root = create_root();
    size_t index;

    for (index = 0; index < sizeof(shorts) / sizeof(shorts[0]); ++index) {
        check_option(add(CLARGS_OPTION_FLAG, root, shorts[index], NULL), root, CLARGS_OPTION_FLAG,
                     shorts[index], NULL, NULL, NULL);
    }
    for (index = 0; index < sizeof(longs) / sizeof(longs[0]); ++index) {
        check_option(add(CLARGS_OPTION_COUNT, root, '\0', longs[index]), root, CLARGS_OPTION_COUNT, '\0',
                     longs[index], NULL, NULL);
    }

    destroy_root(root);
}

/* The negated name is "no-" and the long name, whatever the long name is. */
NAOS_TEST(the_negated_name_is_no_and_the_long_name)
{
    clargs_command_t *root = create_root();

    NAOS_CHECK_STR_EQ(clargs_option_negated_name(add(CLARGS_OPTION_NEGATABLE, root, '\0', "color")),
                      "no-color");
    NAOS_CHECK_STR_EQ(clargs_option_negated_name(add(CLARGS_OPTION_NEGATABLE, root, '\0', "ab")), "no-ab");
    NAOS_CHECK_STR_EQ(clargs_option_negated_name(add(CLARGS_OPTION_NEGATABLE, root, 'n', "no-cache")),
                      "no-no-cache");
    NAOS_CHECK_STR_EQ(clargs_option_negated_name(add(CLARGS_OPTION_NEGATABLE, root, '\0', "Dry_Run-2")),
                      "no-Dry_Run-2");

    destroy_root(root);
}

NAOS_TEST(the_names_of_an_option_are_copied)
{
    char buffer[] = "color";
    clargs_command_t *root = create_root();
    clargs_option_t *option = add(CLARGS_OPTION_NEGATABLE, root, 'c', buffer);

    memset(buffer, 'x', sizeof(buffer) - 1);
    NAOS_CHECK_TRUE(clargs_option_long_name(option) != buffer);
    NAOS_CHECK_STR_EQ(clargs_option_long_name(option), "color");
    NAOS_CHECK_STR_EQ(clargs_option_negated_name(option), "no-color");
    NAOS_CHECK_TRUE(find_long(root, "color", NULL) == option);
    NAOS_CHECK_TRUE(find_long(root, "no-color", NULL) == option);
    NAOS_CHECK_TRUE(find_long(root, "xxxxx", NULL) == NULL);

    destroy_root(root);
}

/* The output is optional: the option is found by its name afterwards. */
NAOS_TEST(an_option_can_be_added_without_an_output)
{
    clargs_command_t *root = create_root();

    NAOS_CHECK_INT_EQ(clargs_command_add_flag(root, 'v', "verbose", NULL), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_command_add_negatable(root, 'c', "color", NULL), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_command_add_count(root, 'd', "debug", NULL), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_command_add_value(root, 'o', "output", clargs_type_string(), NULL),
                      CLARGS_ERROR_NONE);

    NAOS_CHECK_UINT_EQ(clargs_command_option_count(root), 4);
    check_option(clargs_command_find_short(root, 'v'), root, CLARGS_OPTION_FLAG, 'v', "verbose", NULL,
                 NULL);
    check_option(clargs_command_find_short(root, 'c'), root, CLARGS_OPTION_NEGATABLE, 'c', "color",
                 "no-color", NULL);
    check_option(clargs_command_find_short(root, 'd'), root, CLARGS_OPTION_COUNT, 'd', "debug", NULL, NULL);
    check_option(clargs_command_find_short(root, 'o'), root, CLARGS_OPTION_VALUE, 'o', "output", NULL,
                 clargs_type_string());

    destroy_root(root);
}

/* The options are read in the order in which they were added, and a handle
 * stays the same when more options are added after it. */
NAOS_TEST(options_are_read_in_the_order_of_the_definition)
{
    static const char *const names[] = { "o0", "o1", "o2", "o3", "o4", "o5", "o6", "o7", "o8", "o9" };
    static const clargs_option_kind_t kinds[] = { CLARGS_OPTION_FLAG, CLARGS_OPTION_NEGATABLE,
                                                  CLARGS_OPTION_COUNT, CLARGS_OPTION_VALUE };
    const size_t count = sizeof(names) / sizeof(names[0]);
    clargs_command_t *root = create_root();
    clargs_option_t *options[sizeof(names) / sizeof(names[0])];
    size_t index;

    for (index = 0; index < count; ++index) {
        options[index] = add(kinds[index % 4], root, (char)('0' + (int)index), names[index]);
        NAOS_CHECK_UINT_EQ(clargs_command_option_count(root), index + 1);
    }

    for (index = 0; index < count; ++index) {
        NAOS_CHECK_TRUE(clargs_command_option_at(root, index) == options[index]);
        NAOS_CHECK_INT_EQ(clargs_option_kind(options[index]), kinds[index % 4]);
        NAOS_CHECK_INT_EQ(clargs_option_short_name(options[index]), '0' + (int)index);
        NAOS_CHECK_STR_EQ(clargs_option_long_name(options[index]), names[index]);
        NAOS_CHECK_TRUE(clargs_option_command(options[index]) == root);
    }

    destroy_root(root);
}

/* The value type is borrowed: several options share one, and the tree does
 * not destroy it. */
NAOS_TEST(a_value_type_is_borrowed_and_can_be_shared)
{
    static const char *const choices[] = { "never", "auto", "always" };
    clargs_command_t *root = create_root();
    clargs_command_t *build = add_sub(root, "build");
    clargs_type_t *choice = NULL;
    clargs_option_t *first = NULL;
    clargs_option_t *second = NULL;
    clargs_option_t *third = NULL;
    size_t value = 99;

    NAOS_REQUIRE_TRUE(clargs_type_create_choice(choices, 3, &choice) == CLARGS_ERROR_NONE);

    NAOS_CHECK_INT_EQ(clargs_command_add_value(root, '\0', "color", choice, &first), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_command_add_value(root, '\0', "pager", choice, &second), CLARGS_ERROR_NONE);
    NAOS_CHECK_INT_EQ(clargs_command_add_value(build, '\0', "color", choice, &third), CLARGS_ERROR_NONE);

    NAOS_CHECK_TRUE(clargs_option_type(first) == choice);
    NAOS_CHECK_TRUE(clargs_option_type(second) == choice);
    NAOS_CHECK_TRUE(clargs_option_type(third) == choice);

    /* The value type outlives the tree and still works. */
    clargs_command_destroy(root);
    NAOS_CHECK_INT_EQ(clargs_type_convert(choice, "auto", &value), CLARGS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(value, 1);
    clargs_type_release(choice, &value);

    clargs_type_destroy(choice);
    clargs_test_allocator_uninstall();
    check_clean();
}

/* ---- The checks of an addition ----------------------------------------- */

NAOS_TEST(an_option_without_a_name_is_an_invalid_name)
{
    clargs_command_t *root = create_root();

    check_rejected(root, '\0', NULL, CLARGS_ERROR_INVALID_NAME);

    /* The same with options in the command already. */
    (void)add(CLARGS_OPTION_FLAG, root, 'v', "verbose");
    check_rejected(root, '\0', NULL, CLARGS_ERROR_INVALID_NAME);

    destroy_root(root);
}

NAOS_TEST(a_negatable_flag_without_a_long_name_is_an_invalid_name)
{
    clargs_command_t *root = create_root();
    clargs_option_t *option = NULL;
    size_t requests = clargs_test_allocator_requests();

    NAOS_CHECK_INT_EQ(clargs_command_add_negatable(root, 'c', NULL, &option), CLARGS_ERROR_INVALID_NAME);
    NAOS_CHECK_TRUE(option == NULL);
    NAOS_CHECK_INT_EQ(clargs_command_add_negatable(root, 'c', NULL, NULL), CLARGS_ERROR_INVALID_NAME);
    NAOS_CHECK_UINT_EQ(clargs_command_option_count(root), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests);
    NAOS_CHECK_TRUE(clargs_command_find_short(root, 'c') == NULL);

    /* The other kinds take a short name alone, and the same short name can
     * be used after the failure. */
    option = add(CLARGS_OPTION_FLAG, root, 'c', NULL);
    NAOS_CHECK_TRUE(clargs_command_find_short(root, 'c') == option);

    /* It is the first check of a negatable flag after the missing names:
     * the conflict of the short name is not what is reported. */
    NAOS_CHECK_INT_EQ(clargs_command_add_negatable(root, 'c', NULL, &option), CLARGS_ERROR_INVALID_NAME);
    NAOS_CHECK_TRUE(option == NULL);

    /* With a long name it is added. */
    option = add(CLARGS_OPTION_NEGATABLE, root, 'k', "color");
    NAOS_CHECK_UINT_EQ(clargs_command_option_count(root), 2);

    destroy_root(root);
}

NAOS_TEST(a_name_that_breaks_the_rule_is_an_invalid_name)
{
    static const char shorts[] = { '-', '_', '?', ' ', '=', '.', '\xe9', '\x7f', '\x01' };
    static const char *const longs[] = { "",    "a",   "-x",  "_x",          "--",  "a b", " ab",
                                         "ab ", "a=b", "a.b", "caf\xc3\xa9", "a/b", "-",   "--all" };
    clargs_command_t *root = create_root();
    clargs_option_t *option;
    size_t index;

    for (index = 0; index < sizeof(shorts) / sizeof(shorts[0]); ++index) {
        /* Alone, and with a long name that follows the rule. */
        check_rejected(root, shorts[index], NULL, CLARGS_ERROR_INVALID_NAME);
        check_rejected(root, shorts[index], "valid", CLARGS_ERROR_INVALID_NAME);
    }
    for (index = 0; index < sizeof(longs) / sizeof(longs[0]); ++index) {
        /* Alone, and with a short name that follows the rule. */
        check_rejected(root, '\0', longs[index], CLARGS_ERROR_INVALID_NAME);
        check_rejected(root, 'x', longs[index], CLARGS_ERROR_INVALID_NAME);
    }

    NAOS_CHECK_UINT_EQ(clargs_command_option_count(root), 0);
    NAOS_CHECK_TRUE(clargs_command_find_short(root, 'x') == NULL);
    NAOS_CHECK_TRUE(find_long(root, "valid", NULL) == NULL);

    /* The names of the failed additions can be used afterwards. */
    option = add(CLARGS_OPTION_FLAG, root, 'x', "valid");
    NAOS_CHECK_UINT_EQ(clargs_command_option_count(root), 1);
    NAOS_CHECK_TRUE(clargs_command_option_at(root, 0) == option);

    destroy_root(root);
}

/* The rule of the names is checked before the conflicts. */
NAOS_TEST(an_invalid_name_is_reported_before_a_conflict)
{
    clargs_command_t *root = create_root();

    (void)add(CLARGS_OPTION_FLAG, root, 'v', "verbose");

    /* The short name is in use, and the long name breaks the rule. */
    check_rejected(root, 'v', "x", CLARGS_ERROR_INVALID_NAME);
    /* The long name is in use, and the short name breaks the rule. */
    check_rejected(root, '-', "verbose", CLARGS_ERROR_INVALID_NAME);

    destroy_root(root);
}

NAOS_TEST(a_short_name_is_used_once_in_a_command)
{
    clargs_command_t *root = create_root();
    clargs_option_t *verbose = add(CLARGS_OPTION_FLAG, root, 'v', "verbose");
    clargs_option_t *output = add(CLARGS_OPTION_VALUE, root, 'o', NULL);
    clargs_option_t *option;

    /* Whatever the kind of the new and of the existing option is, with and
     * without a long name. */
    check_rejected(root, 'v', NULL, CLARGS_ERROR_NAME_CONFLICT);
    check_rejected(root, 'v', "version", CLARGS_ERROR_NAME_CONFLICT);
    check_rejected(root, 'o', NULL, CLARGS_ERROR_NAME_CONFLICT);
    check_rejected(root, 'o', "output", CLARGS_ERROR_NAME_CONFLICT);

    /* The tree is as it was. */
    NAOS_CHECK_UINT_EQ(clargs_command_option_count(root), 2);
    NAOS_CHECK_TRUE(clargs_command_option_at(root, 0) == verbose);
    NAOS_CHECK_TRUE(clargs_command_option_at(root, 1) == output);
    NAOS_CHECK_TRUE(clargs_command_find_short(root, 'v') == verbose);
    NAOS_CHECK_TRUE(clargs_command_find_short(root, 'o') == output);
    NAOS_CHECK_TRUE(find_long(root, "version", NULL) == NULL);
    NAOS_CHECK_TRUE(find_long(root, "output", NULL) == NULL);

    /* The long names of the failed additions were not taken: they can be
     * added with another short name, or with none. */
    option = add(CLARGS_OPTION_FLAG, root, 'V', "version");
    NAOS_CHECK_TRUE(find_long(root, "version", NULL) == option);
    option = add(CLARGS_OPTION_NEGATABLE, root, '\0', "output");
    NAOS_CHECK_TRUE(find_long(root, "output", NULL) == option);
    NAOS_CHECK_UINT_EQ(clargs_command_option_count(root), 4);

    destroy_root(root);
}

/* A long name is in conflict with a long name and with a negated name of
 * the command. */
NAOS_TEST(a_long_name_is_used_once_in_a_command)
{
    clargs_command_t *root = create_root();
    clargs_option_t *verbose = add(CLARGS_OPTION_FLAG, root, 'v', "verbose");
    clargs_option_t *color = add(CLARGS_OPTION_NEGATABLE, root, '\0', "color");
    clargs_option_t *option;
    bool negated = false;

    /* The long name of another option, with and without a short name. */
    check_rejected(root, '\0', "verbose", CLARGS_ERROR_NAME_CONFLICT);
    check_rejected(root, 'x', "verbose", CLARGS_ERROR_NAME_CONFLICT);
    check_rejected(root, '\0', "color", CLARGS_ERROR_NAME_CONFLICT);

    /* The negated name of a negatable flag. */
    check_rejected(root, '\0', "no-color", CLARGS_ERROR_NAME_CONFLICT);
    check_rejected(root, 'x', "no-color", CLARGS_ERROR_NAME_CONFLICT);

    /* The tree is as it was. */
    NAOS_CHECK_UINT_EQ(clargs_command_option_count(root), 2);
    NAOS_CHECK_TRUE(clargs_command_option_at(root, 0) == verbose);
    NAOS_CHECK_TRUE(clargs_command_option_at(root, 1) == color);
    NAOS_CHECK_TRUE(clargs_command_find_short(root, 'x') == NULL);
    NAOS_CHECK_TRUE(find_long(root, "no-color", &negated) == color);
    NAOS_CHECK_TRUE(negated);

    /* Only a negatable flag has a negated name: "no-verbose" is free. And
     * the short name of the failed additions was not taken. */
    option = add(CLARGS_OPTION_FLAG, root, 'x', "no-verbose");
    NAOS_CHECK_TRUE(find_long(root, "no-verbose", &negated) == option);
    NAOS_CHECK_FALSE(negated);
    NAOS_CHECK_UINT_EQ(clargs_command_option_count(root), 3);

    destroy_root(root);
}

/* The negated name of a new negatable flag is in conflict with a long name
 * and with a negated name of the command. */
NAOS_TEST(a_negated_name_is_used_once_in_a_command)
{
    clargs_command_t *root = create_root();
    clargs_option_t *no_color = add(CLARGS_OPTION_FLAG, root, '\0', "no-color");
    clargs_option_t *no_pager = add(CLARGS_OPTION_VALUE, root, 'p', "no-pager");
    clargs_option_t *cache = add(CLARGS_OPTION_NEGATABLE, root, '\0', "cache");
    clargs_option_t *option = no_color;
    size_t requests = clargs_test_allocator_requests();
    bool negated = true;

    /* "no-color" and "no-pager" are long names already. */
    NAOS_CHECK_INT_EQ(clargs_command_add_negatable(root, '\0', "color", &option),
                      CLARGS_ERROR_NAME_CONFLICT);
    NAOS_CHECK_TRUE(option == NULL);
    option = no_color;
    NAOS_CHECK_INT_EQ(clargs_command_add_negatable(root, 'c', "pager", &option), CLARGS_ERROR_NAME_CONFLICT);
    NAOS_CHECK_TRUE(option == NULL);
    NAOS_CHECK_INT_EQ(clargs_command_add_negatable(root, 'c', "pager", NULL), CLARGS_ERROR_NAME_CONFLICT);

    /* "no-cache" is a negated name already. */
    option = no_color;
    NAOS_CHECK_INT_EQ(clargs_command_add_negatable(root, 'c', "cache", &option), CLARGS_ERROR_NAME_CONFLICT);
    NAOS_CHECK_TRUE(option == NULL);

    /* The tree is as it was, and nothing was allocated. */
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests);
    NAOS_CHECK_UINT_EQ(clargs_command_option_count(root), 3);
    NAOS_CHECK_TRUE(clargs_command_option_at(root, 0) == no_color);
    NAOS_CHECK_TRUE(clargs_command_option_at(root, 1) == no_pager);
    NAOS_CHECK_TRUE(clargs_command_option_at(root, 2) == cache);
    NAOS_CHECK_TRUE(find_long(root, "color", NULL) == NULL);
    NAOS_CHECK_TRUE(find_long(root, "pager", NULL) == NULL);
    NAOS_CHECK_TRUE(clargs_command_find_short(root, 'c') == NULL);
    NAOS_CHECK_TRUE(find_long(root, "no-color", &negated) == no_color);
    NAOS_CHECK_FALSE(negated);

    /* The other kinds have no negated name, so the same names are free for
     * them, also after the failures. */
    option = add(CLARGS_OPTION_FLAG, root, 'c', "color");
    NAOS_CHECK_TRUE(find_long(root, "color", NULL) == option);
    option = add(CLARGS_OPTION_COUNT, root, '\0', "pager");
    NAOS_CHECK_TRUE(find_long(root, "pager", NULL) == option);

    /* A long name that only starts like a negated name is no conflict. */
    option = add(CLARGS_OPTION_NEGATABLE, root, '\0', "col");
    NAOS_CHECK_STR_EQ(clargs_option_negated_name(option), "no-col");
    option = add(CLARGS_OPTION_NEGATABLE, root, '\0', "colors");
    NAOS_CHECK_STR_EQ(clargs_option_negated_name(option), "no-colors");
    NAOS_CHECK_UINT_EQ(clargs_command_option_count(root), 7);

    destroy_root(root);
}

/* The same names can be used in another command: in the parent, in a
 * child and in a sibling. Each command finds its own option. */
NAOS_TEST(the_same_names_can_be_used_in_another_command)
{
    clargs_command_t *root = create_root();
    clargs_command_t *build = add_sub(root, "build");
    clargs_command_t *test = add_sub(root, "test");
    clargs_command_t *inner = add_sub(build, "inner");
    clargs_option_t *in_root = add(CLARGS_OPTION_FLAG, root, 'v', "verbose");
    clargs_option_t *in_build = add(CLARGS_OPTION_COUNT, build, 'v', "verbose");
    clargs_option_t *in_test = add(CLARGS_OPTION_VALUE, test, 'v', "verbose");
    clargs_option_t *in_inner = add(CLARGS_OPTION_NEGATABLE, inner, 'v', "verbose");
    clargs_option_t *color_root = add(CLARGS_OPTION_NEGATABLE, root, '\0', "color");
    clargs_option_t *color_build = add(CLARGS_OPTION_FLAG, build, '\0', "no-color");
    bool negated = false;

    NAOS_CHECK_TRUE(in_root != in_build);
    NAOS_CHECK_TRUE(in_build != in_test);
    NAOS_CHECK_TRUE(in_test != in_inner);

    NAOS_CHECK_UINT_EQ(clargs_command_option_count(root), 2);
    NAOS_CHECK_UINT_EQ(clargs_command_option_count(build), 2);
    NAOS_CHECK_UINT_EQ(clargs_command_option_count(test), 1);
    NAOS_CHECK_UINT_EQ(clargs_command_option_count(inner), 1);

    NAOS_CHECK_TRUE(clargs_option_command(in_root) == root);
    NAOS_CHECK_TRUE(clargs_option_command(in_build) == build);
    NAOS_CHECK_TRUE(clargs_option_command(in_test) == test);
    NAOS_CHECK_TRUE(clargs_option_command(in_inner) == inner);

    NAOS_CHECK_TRUE(clargs_command_find_short(root, 'v') == in_root);
    NAOS_CHECK_TRUE(clargs_command_find_short(build, 'v') == in_build);
    NAOS_CHECK_TRUE(clargs_command_find_short(test, 'v') == in_test);
    NAOS_CHECK_TRUE(clargs_command_find_short(inner, 'v') == in_inner);
    NAOS_CHECK_TRUE(find_long(root, "verbose", NULL) == in_root);
    NAOS_CHECK_TRUE(find_long(build, "verbose", NULL) == in_build);
    NAOS_CHECK_TRUE(find_long(test, "verbose", NULL) == in_test);
    NAOS_CHECK_TRUE(find_long(inner, "verbose", NULL) == in_inner);

    /* "no-color" is a negated name in the root and a long name in build. */
    NAOS_CHECK_TRUE(find_long(root, "no-color", &negated) == color_root);
    NAOS_CHECK_TRUE(negated);
    NAOS_CHECK_TRUE(find_long(build, "no-color", &negated) == color_build);
    NAOS_CHECK_FALSE(negated);

    /* A lookup does not see the options of the parent or of a child. */
    NAOS_CHECK_TRUE(find_long(test, "color", NULL) == NULL);
    NAOS_CHECK_TRUE(find_long(build, "color", NULL) == NULL);
    NAOS_CHECK_TRUE(find_long(inner, "no-color", NULL) == NULL);
    NAOS_CHECK_TRUE(find_long(root, "no-verbose", NULL) == NULL);
    NAOS_CHECK_TRUE(find_long(inner, "no-verbose", NULL) == in_inner);

    /* In one command the names are still in conflict. */
    check_rejected(build, 'v', NULL, CLARGS_ERROR_NAME_CONFLICT);
    check_rejected(inner, '\0', "no-verbose", CLARGS_ERROR_NAME_CONFLICT);

    destroy_root(root);
}

/* Upper and lower case are different names. */
NAOS_TEST(upper_and_lower_case_are_different_names)
{
    clargs_command_t *root = create_root();
    clargs_option_t *lower = add(CLARGS_OPTION_FLAG, root, 'v', "all");
    clargs_option_t *upper = add(CLARGS_OPTION_FLAG, root, 'V', "All");
    clargs_option_t *color = add(CLARGS_OPTION_NEGATABLE, root, '\0', "Color");
    clargs_option_t *no_color = add(CLARGS_OPTION_FLAG, root, '\0', "No-Color");
    clargs_option_t *lower_color = add(CLARGS_OPTION_NEGATABLE, root, '\0', "color");
    bool negated = false;

    NAOS_CHECK_UINT_EQ(clargs_command_option_count(root), 5);

    NAOS_CHECK_TRUE(clargs_command_find_short(root, 'v') == lower);
    NAOS_CHECK_TRUE(clargs_command_find_short(root, 'V') == upper);
    NAOS_CHECK_TRUE(find_long(root, "all", NULL) == lower);
    NAOS_CHECK_TRUE(find_long(root, "All", NULL) == upper);
    NAOS_CHECK_TRUE(find_long(root, "ALL", NULL) == NULL);

    NAOS_CHECK_STR_EQ(clargs_option_negated_name(color), "no-Color");
    NAOS_CHECK_TRUE(find_long(root, "no-Color", &negated) == color);
    NAOS_CHECK_TRUE(negated);
    NAOS_CHECK_TRUE(find_long(root, "No-Color", &negated) == no_color);
    NAOS_CHECK_FALSE(negated);
    NAOS_CHECK_TRUE(find_long(root, "no-color", &negated) == lower_color);
    NAOS_CHECK_TRUE(negated);
    NAOS_CHECK_TRUE(find_long(root, "NO-COLOR", NULL) == NULL);

    destroy_root(root);
}

/* ---- Lookup ------------------------------------------------------------ */

NAOS_TEST(find_short_finds_the_option_with_that_short_name)
{
    clargs_command_t *root = create_root();
    clargs_option_t *verbose = add(CLARGS_OPTION_FLAG, root, 'v', "verbose");
    clargs_option_t *long_only = add(CLARGS_OPTION_FLAG, root, '\0', "all");
    clargs_option_t *color = add(CLARGS_OPTION_NEGATABLE, root, 'c', "color");
    clargs_option_t *debug = add(CLARGS_OPTION_COUNT, root, 'd', NULL);
    clargs_option_t *jobs = add(CLARGS_OPTION_VALUE, root, '4', "jobs");

    NAOS_CHECK_TRUE(clargs_command_find_short(root, 'v') == verbose);
    NAOS_CHECK_TRUE(clargs_command_find_short(root, 'c') == color);
    NAOS_CHECK_TRUE(clargs_command_find_short(root, 'd') == debug);
    NAOS_CHECK_TRUE(clargs_command_find_short(root, '4') == jobs);

    /* A name that no option has, the first letter of a long name, and the
     * other case. */
    NAOS_CHECK_TRUE(clargs_command_find_short(root, 'x') == NULL);
    NAOS_CHECK_TRUE(clargs_command_find_short(root, 'a') == NULL);
    NAOS_CHECK_TRUE(clargs_command_find_short(root, 'j') == NULL);
    NAOS_CHECK_TRUE(clargs_command_find_short(root, 'D') == NULL);

    /* '\0' is not a name: an option without a short name is not found by
     * it. */
    NAOS_CHECK_INT_EQ(clargs_option_short_name(long_only), '\0');
    NAOS_CHECK_TRUE(clargs_command_find_short(root, '\0') == NULL);

    destroy_root(root);
}

NAOS_TEST(find_long_finds_a_long_name_or_a_negated_name)
{
    clargs_command_t *root = create_root();
    clargs_option_t *verbose = add(CLARGS_OPTION_FLAG, root, 'v', "verbose");
    clargs_option_t *short_only = add(CLARGS_OPTION_FLAG, root, 'x', NULL);
    clargs_option_t *color = add(CLARGS_OPTION_NEGATABLE, root, '\0', "color");
    clargs_option_t *debug = add(CLARGS_OPTION_COUNT, root, '\0', "debug");
    clargs_option_t *jobs = add(CLARGS_OPTION_VALUE, root, 'j', "jobs");
    bool negated;

    /* A long name: negated is false. */
    negated = true;
    NAOS_CHECK_TRUE(find_long(root, "verbose", &negated) == verbose);
    NAOS_CHECK_FALSE(negated);
    negated = true;
    NAOS_CHECK_TRUE(find_long(root, "color", &negated) == color);
    NAOS_CHECK_FALSE(negated);
    negated = true;
    NAOS_CHECK_TRUE(find_long(root, "debug", &negated) == debug);
    NAOS_CHECK_FALSE(negated);
    negated = true;
    NAOS_CHECK_TRUE(find_long(root, "jobs", &negated) == jobs);
    NAOS_CHECK_FALSE(negated);

    /* A negated name: negated is true. */
    negated = false;
    NAOS_CHECK_TRUE(find_long(root, "no-color", &negated) == color);
    NAOS_CHECK_TRUE(negated);

    /* The answer is not needed. */
    NAOS_CHECK_TRUE(find_long(root, "verbose", NULL) == verbose);
    NAOS_CHECK_TRUE(find_long(root, "no-color", NULL) == color);

    /* Only a negatable flag has a negated name. When nothing is found,
     * negated is not written. */
    negated = true;
    NAOS_CHECK_TRUE(find_long(root, "no-verbose", &negated) == NULL);
    NAOS_CHECK_TRUE(find_long(root, "no-debug", &negated) == NULL);
    NAOS_CHECK_TRUE(find_long(root, "no-jobs", &negated) == NULL);
    NAOS_CHECK_TRUE(find_long(root, "no-no-color", &negated) == NULL);
    NAOS_CHECK_TRUE(find_long(root, "other", &negated) == NULL);
    NAOS_CHECK_TRUE(negated);
    negated = false;
    NAOS_CHECK_TRUE(find_long(root, "other", &negated) == NULL);
    NAOS_CHECK_FALSE(negated);

    /* A short name is not a long name, and an option without a long name
     * is found by no string. */
    NAOS_CHECK_TRUE(clargs_option_long_name(short_only) == NULL);
    NAOS_CHECK_TRUE(find_long(root, "x", NULL) == NULL);
    NAOS_CHECK_TRUE(find_long(root, "v", NULL) == NULL);
    NAOS_CHECK_TRUE(find_long(root, "", NULL) == NULL);

    destroy_root(root);
}

/* The name is the first length bytes of the string, and the whole name must
 * match: neither a part of a name nor a longer string does. */
NAOS_TEST(find_long_compares_exactly_length_bytes)
{
    clargs_command_t *root = create_root();
    clargs_option_t *verbose = add(CLARGS_OPTION_FLAG, root, 'v', "verbose");
    clargs_option_t *color = add(CLARGS_OPTION_NEGATABLE, root, '\0', "color");
    bool negated = true;

    /* In place, in a longer string that does not end after the name. */
    NAOS_CHECK_TRUE(clargs_command_find_long(root, "verbose=3", 7, &negated) == verbose);
    NAOS_CHECK_FALSE(negated);
    NAOS_CHECK_TRUE(clargs_command_find_long(root, "no-color=never", 8, &negated) == color);
    NAOS_CHECK_TRUE(negated);
    NAOS_CHECK_TRUE(clargs_command_find_long(root, "color=never", 5, &negated) == color);
    NAOS_CHECK_FALSE(negated);
    NAOS_CHECK_TRUE(clargs_command_find_long(root, "verbosely", 7, NULL) == verbose);

    /* A length that is shorter than the name. */
    negated = true;
    NAOS_CHECK_TRUE(clargs_command_find_long(root, "verbose", 6, &negated) == NULL);
    NAOS_CHECK_TRUE(clargs_command_find_long(root, "verbose", 1, &negated) == NULL);
    NAOS_CHECK_TRUE(clargs_command_find_long(root, "verbose", 0, &negated) == NULL);
    NAOS_CHECK_TRUE(clargs_command_find_long(root, "no-color", 7, &negated) == NULL);
    NAOS_CHECK_TRUE(clargs_command_find_long(root, "no-color", 2, &negated) == NULL);
    NAOS_CHECK_TRUE(clargs_command_find_long(root, "color", 4, &negated) == NULL);

    /* A length that is longer than the name. */
    NAOS_CHECK_TRUE(clargs_command_find_long(root, "verbose=3", 8, &negated) == NULL);
    NAOS_CHECK_TRUE(clargs_command_find_long(root, "verbose=3", 9, &negated) == NULL);
    NAOS_CHECK_TRUE(clargs_command_find_long(root, "verbosely", 9, &negated) == NULL);
    NAOS_CHECK_TRUE(clargs_command_find_long(root, "colors", 6, &negated) == NULL);
    NAOS_CHECK_TRUE(clargs_command_find_long(root, "no-colors", 9, &negated) == NULL);
    /* The byte after the name counts, even when it ends the string. */
    NAOS_CHECK_TRUE(clargs_command_find_long(root, "verbose", 8, &negated) == NULL);
    NAOS_CHECK_TRUE(clargs_command_find_long(root, "no-color", 9, &negated) == NULL);
    NAOS_CHECK_TRUE(negated);

    destroy_root(root);
}

/* ---- Destruction ------------------------------------------------------- */

/* One destruction of the root frees the options of every command and their
 * names. */
NAOS_TEST(destroy_frees_the_options_of_the_whole_tree)
{
    clargs_command_t *root = create_root();
    clargs_command_t *build = add_sub(root, "build");
    clargs_command_t *inner = add_sub(build, "inner");

    (void)add(CLARGS_OPTION_FLAG, root, 'v', "verbose");
    (void)add(CLARGS_OPTION_FLAG, root, 'x', NULL);
    (void)add(CLARGS_OPTION_NEGATABLE, root, 'c', "color");
    (void)add(CLARGS_OPTION_COUNT, build, 'd', "debug");
    (void)add(CLARGS_OPTION_VALUE, build, 'o', "output");
    (void)add(CLARGS_OPTION_NEGATABLE, inner, '\0', "pager");
    (void)add(CLARGS_OPTION_VALUE, inner, '\0', "jobs");

    NAOS_CHECK_TRUE(clargs_test_allocator_live() > 0);

    clargs_command_destroy(root);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);

    clargs_test_allocator_uninstall();
    check_clean();
}

/* ---- Failure of an allocation ------------------------------------------ */

/* Builds a root with existing options "o0", "o1", ... of the four kinds in
 * turn, and returns it. The number is chosen by the tests so that the array
 * of the options is empty, has room, or is full. */
static clargs_command_t *build_root_with(size_t existing)
{
    static const char *const names[] = { "o0", "o1", "o2", "o3", "o4", "o5", "o6", "o7" };
    static const clargs_option_kind_t kinds[] = { CLARGS_OPTION_NEGATABLE, CLARGS_OPTION_FLAG,
                                                  CLARGS_OPTION_COUNT, CLARGS_OPTION_VALUE };
    clargs_command_t *root = NULL;
    size_t index;

    NAOS_REQUIRE_TRUE(existing <= sizeof(names) / sizeof(names[0]));
    NAOS_REQUIRE_TRUE(clargs_command_create("app", &root) == CLARGS_ERROR_NONE);
    for (index = 0; index < existing; ++index) {
        (void)add(kinds[index % 4], root, (char)('0' + (int)index), names[index]);
    }
    return root;
}

/* Fails each allocation of the addition of an option of the kind kind, with
 * the names -n and --new, to a root that has existing options already.
 * Returns the number of the failures. */
static size_t fail_each_allocation_of_an_addition(clargs_option_kind_t kind, size_t existing)
{
    const size_t limit = 20;
    size_t failures = 0;
    bool completed = false;
    size_t n;

    for (n = 1; n <= limit; ++n) {
        clargs_command_t *root;
        clargs_option_t *option;
        clargs_error_t error;
        size_t requests;
        size_t live;
        size_t index;

        clargs_test_allocator_reset();
        root = build_root_with(existing);
        requests = clargs_test_allocator_requests();
        live = clargs_test_allocator_live();
        clargs_test_allocator_fail_at(requests + n);

        /* What a failure must not leave in the output. */
        option = existing == 0 ? NULL : (clargs_option_t *)clargs_command_option_at(root, 0);
        error = add_kind(kind, root, 'n', "new", &option);

        if (error == CLARGS_ERROR_NONE && clargs_test_allocator_requests() < requests + n) {
            /* Every allocation point has been passed. */
            NAOS_CHECK_TRUE(option != NULL);
            NAOS_CHECK_UINT_EQ(clargs_command_option_count(root), existing + 1);
            clargs_command_destroy(root);
            completed = true;
            break;
        }

        ++failures;
        NAOS_CHECK_INT_EQ(error, CLARGS_ERROR_OUT_OF_MEMORY);
        NAOS_CHECK_TRUE(option == NULL);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests + n);

        /* The tree is as it was before the call, and nothing of the call
         * stays allocated. */
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);
        NAOS_CHECK_UINT_EQ(clargs_command_option_count(root), existing);
        NAOS_CHECK_TRUE(clargs_command_find_short(root, 'n') == NULL);
        NAOS_CHECK_TRUE(find_long(root, "new", NULL) == NULL);
        NAOS_CHECK_TRUE(find_long(root, "no-new", NULL) == NULL);
        NAOS_CHECK_STR_EQ(clargs_command_name(root), "app");
        for (index = 0; index < existing; ++index) {
            const clargs_option_t *old = clargs_command_option_at(root, index);

            NAOS_CHECK_TRUE(clargs_option_command(old) == root);
            NAOS_CHECK_TRUE(clargs_command_find_short(root, clargs_option_short_name(old)) == old);
            NAOS_CHECK_TRUE(find_long(root, clargs_option_long_name(old), NULL) == old);
        }

        /* The same addition works afterwards. */
        clargs_test_allocator_fail_at(0);
        NAOS_CHECK_INT_EQ(add_kind(kind, root, 'n', "new", &option), CLARGS_ERROR_NONE);
        NAOS_CHECK_TRUE(option != NULL);
        NAOS_CHECK_UINT_EQ(clargs_command_option_count(root), existing + 1);
        NAOS_CHECK_TRUE(clargs_command_option_at(root, existing) == option);
        NAOS_CHECK_TRUE(clargs_command_find_short(root, 'n') == option);

        clargs_command_destroy(root);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
    }

    NAOS_CHECK_TRUE(completed);
    return failures;
}

/* When an allocation of the addition of an option fails, whichever it is,
 * the output is NULL, the tree is as it was, and nothing leaks. The array of
 * the options grows in the first and in the third case, and has room in the
 * second. */
NAOS_TEST(a_failed_addition_leaves_the_tree_as_it_was)
{
    static const clargs_option_kind_t kinds[] = { CLARGS_OPTION_FLAG, CLARGS_OPTION_NEGATABLE,
                                                  CLARGS_OPTION_COUNT, CLARGS_OPTION_VALUE };
    size_t with_room_of_a_flag = 0;
    size_t index;

    clargs_test_allocator_install();

    for (index = 0; index < sizeof(kinds) / sizeof(kinds[0]); ++index) {
        size_t growing = fail_each_allocation_of_an_addition(kinds[index], 0);
        size_t with_room = fail_each_allocation_of_an_addition(kinds[index], 1);

        NAOS_CHECK_TRUE(with_room > 0);
        NAOS_CHECK_UINT_EQ(growing, with_room + 1);
        NAOS_CHECK_UINT_EQ(fail_each_allocation_of_an_addition(kinds[index], 4), growing);

        /* A negatable flag allocates its negated name as well. */
        if (kinds[index] == CLARGS_OPTION_FLAG) {
            with_room_of_a_flag = with_room;
        } else if (kinds[index] == CLARGS_OPTION_NEGATABLE) {
            NAOS_CHECK_UINT_EQ(with_room, with_room_of_a_flag + 1);
        } else {
            NAOS_CHECK_UINT_EQ(with_room, with_room_of_a_flag);
        }
    }

    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_SUITE(add,
    NAOS_ADD_TEST(the_four_kinds_are_added_and_read),
    NAOS_ADD_TEST(an_option_has_a_short_name_a_long_name_or_both),
    NAOS_ADD_TEST(the_names_follow_the_rule_of_names),
    NAOS_ADD_TEST(the_negated_name_is_no_and_the_long_name),
    NAOS_ADD_TEST(the_names_of_an_option_are_copied),
    NAOS_ADD_TEST(an_option_can_be_added_without_an_output),
    NAOS_ADD_TEST(options_are_read_in_the_order_of_the_definition),
    NAOS_ADD_TEST(a_value_type_is_borrowed_and_can_be_shared)
)

NAOS_SUITE(check,
    NAOS_ADD_TEST(an_option_without_a_name_is_an_invalid_name),
    NAOS_ADD_TEST(a_negatable_flag_without_a_long_name_is_an_invalid_name),
    NAOS_ADD_TEST(a_name_that_breaks_the_rule_is_an_invalid_name),
    NAOS_ADD_TEST(an_invalid_name_is_reported_before_a_conflict),
    NAOS_ADD_TEST(a_short_name_is_used_once_in_a_command),
    NAOS_ADD_TEST(a_long_name_is_used_once_in_a_command),
    NAOS_ADD_TEST(a_negated_name_is_used_once_in_a_command),
    NAOS_ADD_TEST(the_same_names_can_be_used_in_another_command),
    NAOS_ADD_TEST(upper_and_lower_case_are_different_names)
)

NAOS_SUITE(lookup,
    NAOS_ADD_TEST(find_short_finds_the_option_with_that_short_name),
    NAOS_ADD_TEST(find_long_finds_a_long_name_or_a_negated_name),
    NAOS_ADD_TEST(find_long_compares_exactly_length_bytes)
)

NAOS_SUITE(destroy,
    NAOS_ADD_TEST(destroy_frees_the_options_of_the_whole_tree)
)

NAOS_SUITE(failure,
    NAOS_ADD_TEST(a_failed_addition_leaves_the_tree_as_it_was)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(add),
    NAOS_ADD_SUITE(check),
    NAOS_ADD_SUITE(lookup),
    NAOS_ADD_SUITE(destroy),
    NAOS_ADD_SUITE(failure)
)
