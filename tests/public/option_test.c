/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * Tests of the public option API: the option part of <argx/schema.h>.
 *
 * Options of the four kinds are added to commands, given attributes, read
 * and looked up the way a consumer does it. These are also the tests of the enumeration and
 * of the lookups of a command that has options: argx_command_option_at(),
 * argx_command_find_short() and argx_command_find_long().
 *
 * Every test that creates a command uses the test allocator
 * ("../support/test_allocator.h"). Its state and the allocator that is set
 * are shared by all the tests of this executable. Such a test starts with
 * an install, destroys what it created, sets the default allocator back
 * before it ends, and ends with check_clean().
 */
#include <argx/argx.h>

#include "../support/test_allocator.h"

#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/*
 * The end of every test that allocates: the test allocator has no live
 * block and saw no violation of the allocator contract.
 */
static void check_clean(void)
{
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_violations(), 0);
}

/* Installs the test allocator and creates a root with the name "app". */
static argx_command_t *create_root(void)
{
    argx_command_t *root = NULL;

    argx_test_allocator_install();
    NAOS_REQUIRE_TRUE(argx_command_create("app", &root) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(root != NULL);
    return root;
}

/* Destroys root and ends the test: nothing stays allocated. */
static void destroy_root(argx_command_t *root)
{
    argx_command_destroy(root);
    argx_test_allocator_uninstall();
    check_clean();
}

/* Adds a subcommand that must succeed, and returns it. */
static argx_command_t *add_sub(argx_command_t *command, const char *name)
{
    argx_command_t *sub = NULL;

    NAOS_REQUIRE_TRUE(argx_command_add_subcommand(command, name, &sub) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(sub != NULL);
    return sub;
}

/*
 * Adds an option of the kind kind with the function of that kind. An option
 * with a value gets the value type of strings.
 */
static argx_error_t add_kind(argx_option_kind_t kind, argx_command_t *command, char short_name,
                             const char *long_name, argx_option_t **out)
{
    switch (kind)
    {
    case ARGX_OPTION_FLAG:
        return argx_command_add_flag(command, short_name, long_name, out);
    case ARGX_OPTION_NEGATABLE:
        return argx_command_add_negatable(command, short_name, long_name, out);
    case ARGX_OPTION_COUNT:
        return argx_command_add_count(command, short_name, long_name, out);
    case ARGX_OPTION_VALUE:
        return argx_command_add_value(command, short_name, long_name, argx_type_string(), out);
    default:
        NAOS_FAIL("a test asked for an option of an unknown kind");
        /* NAOS_FAIL does not return; nothing may follow it (MSVC C4702). */
    }
}

/* Adds an option that must succeed, and returns it. */
static argx_option_t *add(argx_option_kind_t kind, argx_command_t *command, char short_name,
                          const char *long_name)
{
    argx_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(add_kind(kind, command, short_name, long_name, &option) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(option != NULL);
    return option;
}

/*
 * Checks that an addition of every kind with these names fails with
 * expected, sets the output to NULL, adds nothing and allocates nothing. A
 * negatable flag without a long name is an invalid name whatever else is
 * wrong: that check comes before the conflicts.
 */
static void check_rejected(argx_command_t *command, char short_name, const char *long_name,
                           argx_error_t expected)
{
    static const argx_option_kind_t kinds[] = { ARGX_OPTION_FLAG, ARGX_OPTION_NEGATABLE,
                                                  ARGX_OPTION_COUNT, ARGX_OPTION_VALUE };
    const size_t count = argx_command_option_count(command);
    const size_t requests = argx_test_allocator_requests();
    size_t index;

    for (index = 0; index < (sizeof(kinds) / sizeof(kinds[0])); ++index)
    {
        argx_error_t error = expected;
        argx_option_t *sentinel = NULL;
        argx_option_t *option;

        if ((kinds[index] == ARGX_OPTION_NEGATABLE) && (long_name == NULL))
        {
            error = ARGX_ERROR_INVALID_NAME;
        }

        /* What a failure must not leave in the output. */
        if (count != 0)
        {
            sentinel = (argx_option_t *)argx_command_option_at(command, 0);
        }
        option = sentinel;

        NAOS_CHECK_INT_EQ(add_kind(kinds[index], command, short_name, long_name, &option), error);
        NAOS_CHECK_TRUE(option == NULL);

        /* The failure needs no output either. */
        NAOS_CHECK_INT_EQ(add_kind(kinds[index], command, short_name, long_name, NULL), error);
    }

    NAOS_CHECK_UINT_EQ(argx_command_option_count(command), count);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), requests);
}

/*
 * Checks what the readers return for an option. negated_name and type are
 * NULL when the option must have none.
 */
static void check_option(const argx_option_t *option, const argx_command_t *command,
                         argx_option_kind_t kind, char short_name, const char *long_name,
                         const char *negated_name, const argx_type_t *type)
{
    NAOS_REQUIRE_TRUE(option != NULL);
    NAOS_CHECK_TRUE(argx_option_command(option) == command);
    NAOS_CHECK_INT_EQ(argx_option_kind(option), kind);
    NAOS_CHECK_INT_EQ(argx_option_short_name(option), short_name);

    if (long_name == NULL)
    {
        NAOS_CHECK_TRUE(argx_option_long_name(option) == NULL);
    }
    else
    {
        NAOS_REQUIRE_TRUE(argx_option_long_name(option) != NULL);
        NAOS_CHECK_STR_EQ(argx_option_long_name(option), long_name);
    }

    if (negated_name == NULL)
    {
        NAOS_CHECK_TRUE(argx_option_negated_name(option) == NULL);
    }
    else
    {
        NAOS_REQUIRE_TRUE(argx_option_negated_name(option) != NULL);
        NAOS_CHECK_STR_EQ(argx_option_negated_name(option), negated_name);
    }

    NAOS_CHECK_TRUE(argx_option_type(option) == type);
}

/*
 * The attributes that are strings, for the tests that do the same with each
 * of them.
 */
typedef enum text_attribute
{
    TEXT_DESCRIPTION,
    TEXT_VALUE_NAME,
    TEXT_DEFAULT
} text_attribute_t;

#define TEXT_ATTRIBUTE_COUNT 3

/* Sets the attribute of option with the function of that attribute. */
static argx_error_t set_text(text_attribute_t attribute, argx_option_t *option, const char *text)
{
    switch (attribute)
    {
    case TEXT_DESCRIPTION:
        return argx_option_set_description(option, text);
    case TEXT_VALUE_NAME:
        return argx_option_set_value_name(option, text);
    case TEXT_DEFAULT:
        return argx_option_set_default(option, text);
    default:
        NAOS_FAIL("a test asked for an unknown attribute");
        /* NAOS_FAIL does not return; nothing may follow it (MSVC C4702). */
    }
}

/* Reads the attribute of option with the function of that attribute. */
static const char *get_text(text_attribute_t attribute, const argx_option_t *option)
{
    switch (attribute)
    {
    case TEXT_DESCRIPTION:
        return argx_option_description(option);
    case TEXT_VALUE_NAME:
        return argx_option_value_name(option);
    case TEXT_DEFAULT:
        return argx_option_default(option);
    default:
        NAOS_FAIL("a test asked for an unknown attribute");
        /* NAOS_FAIL does not return; nothing may follow it (MSVC C4702). */
    }
}

/*
 * Checks a string that was read: NULL when expected is NULL, and else a
 * string with the same contents.
 */
static void check_text(const char *actual, const char *expected)
{
    if (expected == NULL)
    {
        NAOS_CHECK_TRUE(actual == NULL);
    }
    else
    {
        NAOS_REQUIRE_TRUE(actual != NULL);
        NAOS_CHECK_STR_EQ(actual, expected);
    }
}

/*
 * Checks what the readers of the attributes return for an option. A string
 * is NULL when the attribute must not be set.
 */
static void check_attributes(const argx_option_t *option, const char *description, const char *value_name,
                             const char *default_value, bool required, bool initial)
{
    NAOS_REQUIRE_TRUE(option != NULL);
    check_text(argx_option_description(option), description);
    check_text(argx_option_value_name(option), value_name);
    check_text(argx_option_default(option), default_value);
    NAOS_CHECK_TRUE(argx_option_is_required(option) == required);
    NAOS_CHECK_TRUE(argx_option_initial(option) == initial);
}

/* Looks name up as a long name, the whole string. */
static const argx_option_t *find_long(const argx_command_t *command, const char *name, bool *negated)
{
    return argx_command_find_long(command, name, strlen(name), negated);
}

/* ---- Addition and reading ---------------------------------------------- */

NAOS_TEST(the_four_kinds_are_added_and_read)
{
    argx_command_t *root = create_root();
    argx_option_t *flag = NULL;
    argx_option_t *negatable = NULL;
    argx_option_t *count = NULL;
    argx_option_t *value = NULL;

    NAOS_CHECK_INT_EQ(argx_command_add_flag(root, 'v', "verbose", &flag), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_command_add_negatable(root, 'c', "color", &negatable), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_command_add_count(root, 'd', "debug", &count), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_command_add_value(root, 'j', "jobs", argx_type_int(), &value),
                      ARGX_ERROR_NONE);

    NAOS_CHECK_UINT_EQ(argx_command_option_count(root), 4);

    /*
     * Only a negatable flag has a negated name, and only an option with a
     * value has a value type.
     */
    check_option(flag, root, ARGX_OPTION_FLAG, 'v', "verbose", NULL, NULL);
    check_option(negatable, root, ARGX_OPTION_NEGATABLE, 'c', "color", "no-color", NULL);
    check_option(count, root, ARGX_OPTION_COUNT, 'd', "debug", NULL, NULL);
    check_option(value, root, ARGX_OPTION_VALUE, 'j', "jobs", NULL, argx_type_int());

    /* The rest of the command is as it was. */
    NAOS_CHECK_STR_EQ(argx_command_name(root), "app");
    NAOS_CHECK_UINT_EQ(argx_command_positional_count(root), 0);
    NAOS_CHECK_UINT_EQ(argx_command_subcommand_count(root), 0);

    destroy_root(root);
}

/* No short name is read as '\0', no long name as NULL. */
NAOS_TEST(an_option_has_a_short_name_a_long_name_or_both)
{
    argx_command_t *root = create_root();

    check_option(add(ARGX_OPTION_FLAG, root, 'a', NULL), root, ARGX_OPTION_FLAG, 'a', NULL, NULL, NULL);
    check_option(add(ARGX_OPTION_FLAG, root, '\0', "all"), root, ARGX_OPTION_FLAG, '\0', "all", NULL,
                 NULL);
    check_option(add(ARGX_OPTION_FLAG, root, 'B', "both"), root, ARGX_OPTION_FLAG, 'B', "both", NULL,
                 NULL);

    check_option(add(ARGX_OPTION_COUNT, root, 'd', NULL), root, ARGX_OPTION_COUNT, 'd', NULL, NULL,
                 NULL);
    check_option(add(ARGX_OPTION_COUNT, root, '\0', "debug"), root, ARGX_OPTION_COUNT, '\0', "debug",
                 NULL, NULL);
    check_option(add(ARGX_OPTION_COUNT, root, 'q', "quiet"), root, ARGX_OPTION_COUNT, 'q', "quiet",
                 NULL, NULL);

    check_option(add(ARGX_OPTION_VALUE, root, 'o', NULL), root, ARGX_OPTION_VALUE, 'o', NULL, NULL,
                 argx_type_string());
    check_option(add(ARGX_OPTION_VALUE, root, '\0', "output"), root, ARGX_OPTION_VALUE, '\0', "output",
                 NULL, argx_type_string());
    check_option(add(ARGX_OPTION_VALUE, root, 'f', "file"), root, ARGX_OPTION_VALUE, 'f', "file", NULL,
                 argx_type_string());

    /* A negatable flag always has a long name. */
    check_option(add(ARGX_OPTION_NEGATABLE, root, '\0', "color"), root, ARGX_OPTION_NEGATABLE, '\0',
                 "color", "no-color", NULL);
    check_option(add(ARGX_OPTION_NEGATABLE, root, 'p', "pager"), root, ARGX_OPTION_NEGATABLE, 'p',
                 "pager", "no-pager", NULL);

    NAOS_CHECK_UINT_EQ(argx_command_option_count(root), 11);

    destroy_root(root);
}

NAOS_TEST(the_names_follow_the_rule_of_names)
{
    static const char shorts[] = { 'a', 'z', 'A', 'Z', '0', '9' };
    static const char *const longs[] = { "ab", "a-", "a_", "7z", "dry-run", "dry_run", "Aa", "no-x", "x2" };
    argx_command_t *root = create_root();
    size_t index;

    for (index = 0; index < (sizeof(shorts) / sizeof(shorts[0])); ++index)
    {
        check_option(add(ARGX_OPTION_FLAG, root, shorts[index], NULL), root, ARGX_OPTION_FLAG,
                     shorts[index], NULL, NULL, NULL);
    }
    for (index = 0; index < (sizeof(longs) / sizeof(longs[0])); ++index)
    {
        check_option(add(ARGX_OPTION_COUNT, root, '\0', longs[index]), root, ARGX_OPTION_COUNT, '\0',
                     longs[index], NULL, NULL);
    }

    destroy_root(root);
}

/* The negated name is "no-" and the long name, whatever the long name is. */
NAOS_TEST(the_negated_name_is_no_and_the_long_name)
{
    argx_command_t *root = create_root();

    NAOS_CHECK_STR_EQ(argx_option_negated_name(add(ARGX_OPTION_NEGATABLE, root, '\0', "color")),
                      "no-color");
    NAOS_CHECK_STR_EQ(argx_option_negated_name(add(ARGX_OPTION_NEGATABLE, root, '\0', "ab")), "no-ab");
    NAOS_CHECK_STR_EQ(argx_option_negated_name(add(ARGX_OPTION_NEGATABLE, root, 'n', "no-cache")),
                      "no-no-cache");
    NAOS_CHECK_STR_EQ(argx_option_negated_name(add(ARGX_OPTION_NEGATABLE, root, '\0', "Dry_Run-2")),
                      "no-Dry_Run-2");

    destroy_root(root);
}

NAOS_TEST(the_names_of_an_option_are_copied)
{
    char buffer[] = "color";
    argx_command_t *root = create_root();
    argx_option_t *option = add(ARGX_OPTION_NEGATABLE, root, 'c', buffer);

    memset(buffer, 'x', sizeof(buffer) - 1);
    NAOS_CHECK_TRUE(argx_option_long_name(option) != buffer);
    NAOS_CHECK_STR_EQ(argx_option_long_name(option), "color");
    NAOS_CHECK_STR_EQ(argx_option_negated_name(option), "no-color");
    NAOS_CHECK_TRUE(find_long(root, "color", NULL) == option);
    NAOS_CHECK_TRUE(find_long(root, "no-color", NULL) == option);
    NAOS_CHECK_TRUE(find_long(root, "xxxxx", NULL) == NULL);

    destroy_root(root);
}

/* The output is optional: the option is found by its name afterwards. */
NAOS_TEST(an_option_can_be_added_without_an_output)
{
    argx_command_t *root = create_root();

    NAOS_CHECK_INT_EQ(argx_command_add_flag(root, 'v', "verbose", NULL), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_command_add_negatable(root, 'c', "color", NULL), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_command_add_count(root, 'd', "debug", NULL), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_command_add_value(root, 'o', "output", argx_type_string(), NULL),
                      ARGX_ERROR_NONE);

    NAOS_CHECK_UINT_EQ(argx_command_option_count(root), 4);
    check_option(argx_command_find_short(root, 'v'), root, ARGX_OPTION_FLAG, 'v', "verbose", NULL,
                 NULL);
    check_option(argx_command_find_short(root, 'c'), root, ARGX_OPTION_NEGATABLE, 'c', "color",
                 "no-color", NULL);
    check_option(argx_command_find_short(root, 'd'), root, ARGX_OPTION_COUNT, 'd', "debug", NULL, NULL);
    check_option(argx_command_find_short(root, 'o'), root, ARGX_OPTION_VALUE, 'o', "output", NULL,
                 argx_type_string());

    destroy_root(root);
}

/*
 * The options are read in the order in which they were added, and a handle
 * stays the same when more options are added after it.
 */
NAOS_TEST(options_are_read_in_the_order_of_the_definition)
{
    static const char *const names[] = { "o0", "o1", "o2", "o3", "o4", "o5", "o6", "o7", "o8", "o9" };
    static const argx_option_kind_t kinds[] = { ARGX_OPTION_FLAG, ARGX_OPTION_NEGATABLE,
                                                  ARGX_OPTION_COUNT, ARGX_OPTION_VALUE };
    const size_t count = sizeof(names) / sizeof(names[0]);
    argx_command_t *root = create_root();
    argx_option_t *options[sizeof(names) / sizeof(names[0])];
    size_t index;

    for (index = 0; index < count; ++index)
    {
        options[index] = add(kinds[index % 4], root, (char)('0' + (int)index), names[index]);
        NAOS_CHECK_UINT_EQ(argx_command_option_count(root), index + 1);
    }

    for (index = 0; index < count; ++index)
    {
        NAOS_CHECK_TRUE(argx_command_option_at(root, index) == options[index]);
        NAOS_CHECK_INT_EQ(argx_option_kind(options[index]), kinds[index % 4]);
        NAOS_CHECK_INT_EQ(argx_option_short_name(options[index]), '0' + (int)index);
        NAOS_CHECK_STR_EQ(argx_option_long_name(options[index]), names[index]);
        NAOS_CHECK_TRUE(argx_option_command(options[index]) == root);
    }

    destroy_root(root);
}

/*
 * The value type is borrowed: several options share one, and the tree does
 * not destroy it.
 */
NAOS_TEST(a_value_type_is_borrowed_and_can_be_shared)
{
    static const char *const choices[] = { "never", "auto", "always" };
    argx_command_t *root = create_root();
    argx_command_t *build = add_sub(root, "build");
    argx_type_t *choice = NULL;
    argx_option_t *first = NULL;
    argx_option_t *second = NULL;
    argx_option_t *third = NULL;
    size_t value = 99;

    NAOS_REQUIRE_TRUE(argx_type_create_choice(choices, 3, &choice) == ARGX_ERROR_NONE);

    NAOS_CHECK_INT_EQ(argx_command_add_value(root, '\0', "color", choice, &first), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_command_add_value(root, '\0', "pager", choice, &second), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_command_add_value(build, '\0', "color", choice, &third), ARGX_ERROR_NONE);

    NAOS_CHECK_TRUE(argx_option_type(first) == choice);
    NAOS_CHECK_TRUE(argx_option_type(second) == choice);
    NAOS_CHECK_TRUE(argx_option_type(third) == choice);

    /* The value type outlives the tree and still works. */
    argx_command_destroy(root);
    NAOS_CHECK_INT_EQ(argx_type_convert(choice, "auto", &value), ARGX_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(value, 1);
    argx_type_release(choice, &value);

    argx_type_destroy(choice);
    argx_test_allocator_uninstall();
    check_clean();
}

/* ---- The checks of an addition ----------------------------------------- */

NAOS_TEST(an_option_without_a_name_is_an_invalid_name)
{
    argx_command_t *root = create_root();

    check_rejected(root, '\0', NULL, ARGX_ERROR_INVALID_NAME);

    /* The same with options in the command already. */
    (void)add(ARGX_OPTION_FLAG, root, 'v', "verbose");
    check_rejected(root, '\0', NULL, ARGX_ERROR_INVALID_NAME);

    destroy_root(root);
}

NAOS_TEST(a_negatable_flag_without_a_long_name_is_an_invalid_name)
{
    argx_command_t *root = create_root();
    argx_option_t *option = NULL;
    size_t requests = argx_test_allocator_requests();

    NAOS_CHECK_INT_EQ(argx_command_add_negatable(root, 'c', NULL, &option), ARGX_ERROR_INVALID_NAME);
    NAOS_CHECK_TRUE(option == NULL);
    NAOS_CHECK_INT_EQ(argx_command_add_negatable(root, 'c', NULL, NULL), ARGX_ERROR_INVALID_NAME);
    NAOS_CHECK_UINT_EQ(argx_command_option_count(root), 0);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), requests);
    NAOS_CHECK_TRUE(argx_command_find_short(root, 'c') == NULL);

    /*
     * The other kinds take a short name alone, and the same short name can
     * be used after the failure.
     */
    option = add(ARGX_OPTION_FLAG, root, 'c', NULL);
    NAOS_CHECK_TRUE(argx_command_find_short(root, 'c') == option);

    /*
     * It is the first check of a negatable flag after the missing names:
     * the conflict of the short name is not what is reported.
     */
    NAOS_CHECK_INT_EQ(argx_command_add_negatable(root, 'c', NULL, &option), ARGX_ERROR_INVALID_NAME);
    NAOS_CHECK_TRUE(option == NULL);

    /* With a long name it is added. */
    option = add(ARGX_OPTION_NEGATABLE, root, 'k', "color");
    NAOS_CHECK_UINT_EQ(argx_command_option_count(root), 2);

    destroy_root(root);
}

NAOS_TEST(a_name_that_breaks_the_rule_is_an_invalid_name)
{
    static const char shorts[] = { '-', '_', '?', ' ', '=', '.', '\xe9', '\x7f', '\x01' };
    static const char *const longs[] = { "",    "a",   "-x",  "_x",          "--",  "a b", " ab",
                                         "ab ", "a=b", "a.b", "caf\xc3\xa9", "a/b", "-",   "--all" };
    argx_command_t *root = create_root();
    argx_option_t *option;
    size_t index;

    for (index = 0; index < (sizeof(shorts) / sizeof(shorts[0])); ++index)
    {
        /* Alone, and with a long name that follows the rule. */
        check_rejected(root, shorts[index], NULL, ARGX_ERROR_INVALID_NAME);
        check_rejected(root, shorts[index], "valid", ARGX_ERROR_INVALID_NAME);
    }
    for (index = 0; index < (sizeof(longs) / sizeof(longs[0])); ++index)
    {
        /* Alone, and with a short name that follows the rule. */
        check_rejected(root, '\0', longs[index], ARGX_ERROR_INVALID_NAME);
        check_rejected(root, 'x', longs[index], ARGX_ERROR_INVALID_NAME);
    }

    NAOS_CHECK_UINT_EQ(argx_command_option_count(root), 0);
    NAOS_CHECK_TRUE(argx_command_find_short(root, 'x') == NULL);
    NAOS_CHECK_TRUE(find_long(root, "valid", NULL) == NULL);

    /* The names of the failed additions can be used afterwards. */
    option = add(ARGX_OPTION_FLAG, root, 'x', "valid");
    NAOS_CHECK_UINT_EQ(argx_command_option_count(root), 1);
    NAOS_CHECK_TRUE(argx_command_option_at(root, 0) == option);

    destroy_root(root);
}

/* The rule of the names is checked before the conflicts. */
NAOS_TEST(an_invalid_name_is_reported_before_a_conflict)
{
    argx_command_t *root = create_root();

    (void)add(ARGX_OPTION_FLAG, root, 'v', "verbose");

    /* The short name is in use, and the long name breaks the rule. */
    check_rejected(root, 'v', "x", ARGX_ERROR_INVALID_NAME);
    /* The long name is in use, and the short name breaks the rule. */
    check_rejected(root, '-', "verbose", ARGX_ERROR_INVALID_NAME);

    destroy_root(root);
}

NAOS_TEST(a_short_name_is_used_once_in_a_command)
{
    argx_command_t *root = create_root();
    argx_option_t *verbose = add(ARGX_OPTION_FLAG, root, 'v', "verbose");
    argx_option_t *output = add(ARGX_OPTION_VALUE, root, 'o', NULL);
    argx_option_t *option;

    /*
     * Whatever the kind of the new and of the existing option is, with and
     * without a long name.
     */
    check_rejected(root, 'v', NULL, ARGX_ERROR_NAME_CONFLICT);
    check_rejected(root, 'v', "version", ARGX_ERROR_NAME_CONFLICT);
    check_rejected(root, 'o', NULL, ARGX_ERROR_NAME_CONFLICT);
    check_rejected(root, 'o', "output", ARGX_ERROR_NAME_CONFLICT);

    /* The tree is as it was. */
    NAOS_CHECK_UINT_EQ(argx_command_option_count(root), 2);
    NAOS_CHECK_TRUE(argx_command_option_at(root, 0) == verbose);
    NAOS_CHECK_TRUE(argx_command_option_at(root, 1) == output);
    NAOS_CHECK_TRUE(argx_command_find_short(root, 'v') == verbose);
    NAOS_CHECK_TRUE(argx_command_find_short(root, 'o') == output);
    NAOS_CHECK_TRUE(find_long(root, "version", NULL) == NULL);
    NAOS_CHECK_TRUE(find_long(root, "output", NULL) == NULL);

    /*
     * The long names of the failed additions were not taken: they can be
     * added with another short name, or with none.
     */
    option = add(ARGX_OPTION_FLAG, root, 'V', "version");
    NAOS_CHECK_TRUE(find_long(root, "version", NULL) == option);
    option = add(ARGX_OPTION_NEGATABLE, root, '\0', "output");
    NAOS_CHECK_TRUE(find_long(root, "output", NULL) == option);
    NAOS_CHECK_UINT_EQ(argx_command_option_count(root), 4);

    destroy_root(root);
}

/*
 * A long name is in conflict with a long name and with a negated name of
 * the command.
 */
NAOS_TEST(a_long_name_is_used_once_in_a_command)
{
    argx_command_t *root = create_root();
    argx_option_t *verbose = add(ARGX_OPTION_FLAG, root, 'v', "verbose");
    argx_option_t *color = add(ARGX_OPTION_NEGATABLE, root, '\0', "color");
    argx_option_t *option;
    bool negated = false;

    /* The long name of another option, with and without a short name. */
    check_rejected(root, '\0', "verbose", ARGX_ERROR_NAME_CONFLICT);
    check_rejected(root, 'x', "verbose", ARGX_ERROR_NAME_CONFLICT);
    check_rejected(root, '\0', "color", ARGX_ERROR_NAME_CONFLICT);

    /* The negated name of a negatable flag. */
    check_rejected(root, '\0', "no-color", ARGX_ERROR_NAME_CONFLICT);
    check_rejected(root, 'x', "no-color", ARGX_ERROR_NAME_CONFLICT);

    /* The tree is as it was. */
    NAOS_CHECK_UINT_EQ(argx_command_option_count(root), 2);
    NAOS_CHECK_TRUE(argx_command_option_at(root, 0) == verbose);
    NAOS_CHECK_TRUE(argx_command_option_at(root, 1) == color);
    NAOS_CHECK_TRUE(argx_command_find_short(root, 'x') == NULL);
    NAOS_CHECK_TRUE(find_long(root, "no-color", &negated) == color);
    NAOS_CHECK_TRUE(negated);

    /*
     * Only a negatable flag has a negated name: "no-verbose" is free. And
     * the short name of the failed additions was not taken.
     */
    option = add(ARGX_OPTION_FLAG, root, 'x', "no-verbose");
    NAOS_CHECK_TRUE(find_long(root, "no-verbose", &negated) == option);
    NAOS_CHECK_FALSE(negated);
    NAOS_CHECK_UINT_EQ(argx_command_option_count(root), 3);

    destroy_root(root);
}

/*
 * The negated name of a new negatable flag is in conflict with a long name
 * and with a negated name of the command.
 */
NAOS_TEST(a_negated_name_is_used_once_in_a_command)
{
    argx_command_t *root = create_root();
    argx_option_t *no_color = add(ARGX_OPTION_FLAG, root, '\0', "no-color");
    argx_option_t *no_pager = add(ARGX_OPTION_VALUE, root, 'p', "no-pager");
    argx_option_t *cache = add(ARGX_OPTION_NEGATABLE, root, '\0', "cache");
    argx_option_t *option = no_color;
    size_t requests = argx_test_allocator_requests();
    bool negated = true;

    /* "no-color" and "no-pager" are long names already. */
    NAOS_CHECK_INT_EQ(argx_command_add_negatable(root, '\0', "color", &option),
                      ARGX_ERROR_NAME_CONFLICT);
    NAOS_CHECK_TRUE(option == NULL);
    option = no_color;
    NAOS_CHECK_INT_EQ(argx_command_add_negatable(root, 'c', "pager", &option), ARGX_ERROR_NAME_CONFLICT);
    NAOS_CHECK_TRUE(option == NULL);
    NAOS_CHECK_INT_EQ(argx_command_add_negatable(root, 'c', "pager", NULL), ARGX_ERROR_NAME_CONFLICT);

    /* "no-cache" is a negated name already. */
    option = no_color;
    NAOS_CHECK_INT_EQ(argx_command_add_negatable(root, 'c', "cache", &option), ARGX_ERROR_NAME_CONFLICT);
    NAOS_CHECK_TRUE(option == NULL);

    /* The tree is as it was, and nothing was allocated. */
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), requests);
    NAOS_CHECK_UINT_EQ(argx_command_option_count(root), 3);
    NAOS_CHECK_TRUE(argx_command_option_at(root, 0) == no_color);
    NAOS_CHECK_TRUE(argx_command_option_at(root, 1) == no_pager);
    NAOS_CHECK_TRUE(argx_command_option_at(root, 2) == cache);
    NAOS_CHECK_TRUE(find_long(root, "color", NULL) == NULL);
    NAOS_CHECK_TRUE(find_long(root, "pager", NULL) == NULL);
    NAOS_CHECK_TRUE(argx_command_find_short(root, 'c') == NULL);
    NAOS_CHECK_TRUE(find_long(root, "no-color", &negated) == no_color);
    NAOS_CHECK_FALSE(negated);

    /*
     * The other kinds have no negated name, so the same names are free for
     * them, also after the failures.
     */
    option = add(ARGX_OPTION_FLAG, root, 'c', "color");
    NAOS_CHECK_TRUE(find_long(root, "color", NULL) == option);
    option = add(ARGX_OPTION_COUNT, root, '\0', "pager");
    NAOS_CHECK_TRUE(find_long(root, "pager", NULL) == option);

    /* A long name that only starts like a negated name is no conflict. */
    option = add(ARGX_OPTION_NEGATABLE, root, '\0', "col");
    NAOS_CHECK_STR_EQ(argx_option_negated_name(option), "no-col");
    option = add(ARGX_OPTION_NEGATABLE, root, '\0', "colors");
    NAOS_CHECK_STR_EQ(argx_option_negated_name(option), "no-colors");
    NAOS_CHECK_UINT_EQ(argx_command_option_count(root), 7);

    destroy_root(root);
}

/*
 * The same names can be used in another command: in the parent, in a
 * child and in a sibling. Each command finds its own option.
 */
NAOS_TEST(the_same_names_can_be_used_in_another_command)
{
    argx_command_t *root = create_root();
    argx_command_t *build = add_sub(root, "build");
    argx_command_t *test = add_sub(root, "test");
    argx_command_t *inner = add_sub(build, "inner");
    argx_option_t *in_root = add(ARGX_OPTION_FLAG, root, 'v', "verbose");
    argx_option_t *in_build = add(ARGX_OPTION_COUNT, build, 'v', "verbose");
    argx_option_t *in_test = add(ARGX_OPTION_VALUE, test, 'v', "verbose");
    argx_option_t *in_inner = add(ARGX_OPTION_NEGATABLE, inner, 'v', "verbose");
    argx_option_t *color_root = add(ARGX_OPTION_NEGATABLE, root, '\0', "color");
    argx_option_t *color_build = add(ARGX_OPTION_FLAG, build, '\0', "no-color");
    bool negated = false;

    NAOS_CHECK_TRUE(in_root != in_build);
    NAOS_CHECK_TRUE(in_build != in_test);
    NAOS_CHECK_TRUE(in_test != in_inner);

    NAOS_CHECK_UINT_EQ(argx_command_option_count(root), 2);
    NAOS_CHECK_UINT_EQ(argx_command_option_count(build), 2);
    NAOS_CHECK_UINT_EQ(argx_command_option_count(test), 1);
    NAOS_CHECK_UINT_EQ(argx_command_option_count(inner), 1);

    NAOS_CHECK_TRUE(argx_option_command(in_root) == root);
    NAOS_CHECK_TRUE(argx_option_command(in_build) == build);
    NAOS_CHECK_TRUE(argx_option_command(in_test) == test);
    NAOS_CHECK_TRUE(argx_option_command(in_inner) == inner);

    NAOS_CHECK_TRUE(argx_command_find_short(root, 'v') == in_root);
    NAOS_CHECK_TRUE(argx_command_find_short(build, 'v') == in_build);
    NAOS_CHECK_TRUE(argx_command_find_short(test, 'v') == in_test);
    NAOS_CHECK_TRUE(argx_command_find_short(inner, 'v') == in_inner);
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
    check_rejected(build, 'v', NULL, ARGX_ERROR_NAME_CONFLICT);
    check_rejected(inner, '\0', "no-verbose", ARGX_ERROR_NAME_CONFLICT);

    destroy_root(root);
}

/* Upper and lower case are different names. */
NAOS_TEST(upper_and_lower_case_are_different_names)
{
    argx_command_t *root = create_root();
    argx_option_t *lower = add(ARGX_OPTION_FLAG, root, 'v', "all");
    argx_option_t *upper = add(ARGX_OPTION_FLAG, root, 'V', "All");
    argx_option_t *color = add(ARGX_OPTION_NEGATABLE, root, '\0', "Color");
    argx_option_t *no_color = add(ARGX_OPTION_FLAG, root, '\0', "No-Color");
    argx_option_t *lower_color = add(ARGX_OPTION_NEGATABLE, root, '\0', "color");
    bool negated = false;

    NAOS_CHECK_UINT_EQ(argx_command_option_count(root), 5);

    NAOS_CHECK_TRUE(argx_command_find_short(root, 'v') == lower);
    NAOS_CHECK_TRUE(argx_command_find_short(root, 'V') == upper);
    NAOS_CHECK_TRUE(find_long(root, "all", NULL) == lower);
    NAOS_CHECK_TRUE(find_long(root, "All", NULL) == upper);
    NAOS_CHECK_TRUE(find_long(root, "ALL", NULL) == NULL);

    NAOS_CHECK_STR_EQ(argx_option_negated_name(color), "no-Color");
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
    argx_command_t *root = create_root();
    argx_option_t *verbose = add(ARGX_OPTION_FLAG, root, 'v', "verbose");
    argx_option_t *long_only = add(ARGX_OPTION_FLAG, root, '\0', "all");
    argx_option_t *color = add(ARGX_OPTION_NEGATABLE, root, 'c', "color");
    argx_option_t *debug = add(ARGX_OPTION_COUNT, root, 'd', NULL);
    argx_option_t *jobs = add(ARGX_OPTION_VALUE, root, '4', "jobs");

    NAOS_CHECK_TRUE(argx_command_find_short(root, 'v') == verbose);
    NAOS_CHECK_TRUE(argx_command_find_short(root, 'c') == color);
    NAOS_CHECK_TRUE(argx_command_find_short(root, 'd') == debug);
    NAOS_CHECK_TRUE(argx_command_find_short(root, '4') == jobs);

    /*
     * A name that no option has, the first letter of a long name, and the
     * other case.
     */
    NAOS_CHECK_TRUE(argx_command_find_short(root, 'x') == NULL);
    NAOS_CHECK_TRUE(argx_command_find_short(root, 'a') == NULL);
    NAOS_CHECK_TRUE(argx_command_find_short(root, 'j') == NULL);
    NAOS_CHECK_TRUE(argx_command_find_short(root, 'D') == NULL);

    /*
     * '\0' is not a name: an option without a short name is not found by
     * it.
     */
    NAOS_CHECK_INT_EQ(argx_option_short_name(long_only), '\0');
    NAOS_CHECK_TRUE(argx_command_find_short(root, '\0') == NULL);

    destroy_root(root);
}

NAOS_TEST(find_long_finds_a_long_name_or_a_negated_name)
{
    argx_command_t *root = create_root();
    argx_option_t *verbose = add(ARGX_OPTION_FLAG, root, 'v', "verbose");
    argx_option_t *short_only = add(ARGX_OPTION_FLAG, root, 'x', NULL);
    argx_option_t *color = add(ARGX_OPTION_NEGATABLE, root, '\0', "color");
    argx_option_t *debug = add(ARGX_OPTION_COUNT, root, '\0', "debug");
    argx_option_t *jobs = add(ARGX_OPTION_VALUE, root, 'j', "jobs");
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

    /*
     * Only a negatable flag has a negated name. When nothing is found,
     * negated is not written.
     */
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

    /*
     * A short name is not a long name, and an option without a long name
     * is found by no string.
     */
    NAOS_CHECK_TRUE(argx_option_long_name(short_only) == NULL);
    NAOS_CHECK_TRUE(find_long(root, "x", NULL) == NULL);
    NAOS_CHECK_TRUE(find_long(root, "v", NULL) == NULL);
    NAOS_CHECK_TRUE(find_long(root, "", NULL) == NULL);

    destroy_root(root);
}

/*
 * The name is the first length bytes of the string, and the whole name must
 * match: neither a part of a name nor a longer string does.
 */
NAOS_TEST(find_long_compares_exactly_length_bytes)
{
    argx_command_t *root = create_root();
    argx_option_t *verbose = add(ARGX_OPTION_FLAG, root, 'v', "verbose");
    argx_option_t *color = add(ARGX_OPTION_NEGATABLE, root, '\0', "color");
    bool negated = true;

    /* In place, in a longer string that does not end after the name. */
    NAOS_CHECK_TRUE(argx_command_find_long(root, "verbose=3", 7, &negated) == verbose);
    NAOS_CHECK_FALSE(negated);
    NAOS_CHECK_TRUE(argx_command_find_long(root, "no-color=never", 8, &negated) == color);
    NAOS_CHECK_TRUE(negated);
    NAOS_CHECK_TRUE(argx_command_find_long(root, "color=never", 5, &negated) == color);
    NAOS_CHECK_FALSE(negated);
    NAOS_CHECK_TRUE(argx_command_find_long(root, "verbosely", 7, NULL) == verbose);

    /* A length that is shorter than the name. */
    negated = true;
    NAOS_CHECK_TRUE(argx_command_find_long(root, "verbose", 6, &negated) == NULL);
    NAOS_CHECK_TRUE(argx_command_find_long(root, "verbose", 1, &negated) == NULL);
    NAOS_CHECK_TRUE(argx_command_find_long(root, "verbose", 0, &negated) == NULL);
    NAOS_CHECK_TRUE(argx_command_find_long(root, "no-color", 7, &negated) == NULL);
    NAOS_CHECK_TRUE(argx_command_find_long(root, "no-color", 2, &negated) == NULL);
    NAOS_CHECK_TRUE(argx_command_find_long(root, "color", 4, &negated) == NULL);

    /* A length that is longer than the name. */
    NAOS_CHECK_TRUE(argx_command_find_long(root, "verbose=3", 8, &negated) == NULL);
    NAOS_CHECK_TRUE(argx_command_find_long(root, "verbose=3", 9, &negated) == NULL);
    NAOS_CHECK_TRUE(argx_command_find_long(root, "verbosely", 9, &negated) == NULL);
    NAOS_CHECK_TRUE(argx_command_find_long(root, "colors", 6, &negated) == NULL);
    NAOS_CHECK_TRUE(argx_command_find_long(root, "no-colors", 9, &negated) == NULL);
    /* The byte after the name counts, even when it ends the string. */
    NAOS_CHECK_TRUE(argx_command_find_long(root, "verbose", 8, &negated) == NULL);
    NAOS_CHECK_TRUE(argx_command_find_long(root, "no-color", 9, &negated) == NULL);
    NAOS_CHECK_TRUE(negated);

    destroy_root(root);
}

/* ---- Attributes -------------------------------------------------------- */

/*
 * Until an attribute is set, a string is read as NULL, an option is not
 * required, and the initial state is off.
 */
NAOS_TEST(a_new_option_has_no_attribute)
{
    argx_command_t *root = create_root();

    check_attributes(add(ARGX_OPTION_FLAG, root, 'v', "verbose"), NULL, NULL, NULL, false, false);
    check_attributes(add(ARGX_OPTION_NEGATABLE, root, 'c', "color"), NULL, NULL, NULL, false, false);
    check_attributes(add(ARGX_OPTION_COUNT, root, 'd', "debug"), NULL, NULL, NULL, false, false);
    check_attributes(add(ARGX_OPTION_VALUE, root, 'o', "output"), NULL, NULL, NULL, false, false);

    destroy_root(root);
}

/* Every kind of option has a description. */
NAOS_TEST(a_description_is_set_on_every_kind)
{
    static const argx_option_kind_t kinds[] = { ARGX_OPTION_FLAG, ARGX_OPTION_NEGATABLE,
                                                  ARGX_OPTION_COUNT, ARGX_OPTION_VALUE };
    static const char *const names[] = { "flag", "negatable", "count", "value" };
    argx_command_t *root = create_root();
    size_t index;

    for (index = 0; index < (sizeof(kinds) / sizeof(kinds[0])); ++index)
    {
        argx_option_t *option = add(kinds[index], root, '\0', names[index]);

        NAOS_CHECK_INT_EQ(argx_option_set_description(option, "Says what it does."), ARGX_ERROR_NONE);
        check_attributes(option, "Says what it does.", NULL, NULL, false, false);

        /* Set again, it replaces what was set. */
        NAOS_CHECK_INT_EQ(argx_option_set_description(option, "Another, longer description."),
                          ARGX_ERROR_NONE);
        check_attributes(option, "Another, longer description.", NULL, NULL, false, false);

        /* The empty string is a description; it is not the same as none. */
        NAOS_CHECK_INT_EQ(argx_option_set_description(option, ""), ARGX_ERROR_NONE);
        check_attributes(option, "", NULL, NULL, false, false);

        NAOS_CHECK_INT_EQ(argx_option_set_description(option, "x"), ARGX_ERROR_NONE);
        check_attributes(option, "x", NULL, NULL, false, false);
    }

    /* The description of one option is not the description of another. */
    NAOS_CHECK_INT_EQ(
        argx_option_set_description(add(ARGX_OPTION_FLAG, root, 'a', NULL), "Only this one."),
        ARGX_ERROR_NONE);
    check_attributes(argx_command_option_at(root, 0), "x", NULL, NULL, false, false);
    check_attributes(argx_command_find_short(root, 'a'), "Only this one.", NULL, NULL, false, false);

    destroy_root(root);
}

/*
 * An option with a value has a display name of the value, can be required
 * and has a default value. Each attribute is set alone and leaves the
 * others, and the rest of the option, as they were.
 */
NAOS_TEST(the_attributes_of_an_option_with_a_value_are_set_and_read)
{
    argx_command_t *root = create_root();
    argx_option_t *jobs = NULL;
    argx_option_t *output = add(ARGX_OPTION_VALUE, root, 'o', "output");

    NAOS_REQUIRE_TRUE(argx_command_add_value(root, 'j', "jobs", argx_type_int(), &jobs) ==
                      ARGX_ERROR_NONE);

    NAOS_CHECK_INT_EQ(argx_option_set_value_name(jobs, "N"), ARGX_ERROR_NONE);
    check_attributes(jobs, NULL, "N", NULL, false, false);

    NAOS_CHECK_INT_EQ(argx_option_set_default(jobs, "4"), ARGX_ERROR_NONE);
    check_attributes(jobs, NULL, "N", "4", false, false);

    NAOS_CHECK_INT_EQ(argx_option_set_description(jobs, "Number of jobs."), ARGX_ERROR_NONE);
    check_attributes(jobs, "Number of jobs.", "N", "4", false, false);

    NAOS_CHECK_INT_EQ(argx_option_set_required(output, true), ARGX_ERROR_NONE);
    check_attributes(output, NULL, NULL, NULL, true, false);

    /*
     * The other option, the names, the kind and the value type are as they
     * were.
     */
    check_attributes(jobs, "Number of jobs.", "N", "4", false, false);
    check_option(jobs, root, ARGX_OPTION_VALUE, 'j', "jobs", NULL, argx_type_int());
    check_option(output, root, ARGX_OPTION_VALUE, 'o', "output", NULL, argx_type_string());
    NAOS_CHECK_UINT_EQ(argx_command_option_count(root), 2);
    NAOS_CHECK_TRUE(argx_command_find_short(root, 'j') == jobs);
    NAOS_CHECK_TRUE(find_long(root, "output", NULL) == output);

    /*
     * A default value is a string as it is written on a command line. It
     * is kept as it is: whether the value type takes it is found by the
     * validation, not here. The empty string is a value.
     */
    NAOS_CHECK_INT_EQ(argx_option_set_default(jobs, "not a number"), ARGX_ERROR_NONE);
    check_attributes(jobs, "Number of jobs.", "N", "not a number", false, false);
    NAOS_CHECK_INT_EQ(argx_option_set_default(output, ""), ARGX_ERROR_NONE);
    check_attributes(output, NULL, NULL, "", true, false);

    destroy_root(root);
}

/* An attribute that is set again has the later value. */
NAOS_TEST(an_attribute_that_is_set_again_is_replaced)
{
    argx_command_t *root = create_root();
    argx_option_t *value = add(ARGX_OPTION_VALUE, root, 'o', "output");
    argx_option_t *negatable = add(ARGX_OPTION_NEGATABLE, root, 'c', "color");
    size_t live;

    NAOS_CHECK_INT_EQ(argx_option_set_value_name(value, "FILE"), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_option_set_default(value, "a.out"), ARGX_ERROR_NONE);
    check_attributes(value, NULL, "FILE", "a.out", false, false);
    live = argx_test_allocator_live();

    /* A longer and a shorter string. */
    NAOS_CHECK_INT_EQ(argx_option_set_value_name(value, "PATH-OF-THE-OUTPUT"), ARGX_ERROR_NONE);
    check_attributes(value, NULL, "PATH-OF-THE-OUTPUT", "a.out", false, false);
    NAOS_CHECK_INT_EQ(argx_option_set_value_name(value, "P"), ARGX_ERROR_NONE);
    check_attributes(value, NULL, "P", "a.out", false, false);

    NAOS_CHECK_INT_EQ(argx_option_set_default(value, "build/output/program.bin"), ARGX_ERROR_NONE);
    check_attributes(value, NULL, "P", "build/output/program.bin", false, false);
    NAOS_CHECK_INT_EQ(argx_option_set_default(value, "-"), ARGX_ERROR_NONE);
    check_attributes(value, NULL, "P", "-", false, false);

    /* The same value again. */
    NAOS_CHECK_INT_EQ(argx_option_set_default(value, "-"), ARGX_ERROR_NONE);
    check_attributes(value, NULL, "P", "-", false, false);

    /* The string that was replaced is freed: no block more than before. */
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), live);

    /* Required, and not required again. */
    NAOS_CHECK_INT_EQ(argx_option_set_required(value, true), ARGX_ERROR_NONE);
    NAOS_CHECK_TRUE(argx_option_is_required(value));
    NAOS_CHECK_INT_EQ(argx_option_set_required(value, true), ARGX_ERROR_NONE);
    NAOS_CHECK_TRUE(argx_option_is_required(value));
    NAOS_CHECK_INT_EQ(argx_option_set_required(value, false), ARGX_ERROR_NONE);
    NAOS_CHECK_FALSE(argx_option_is_required(value));
    check_attributes(value, NULL, "P", "-", false, false);

    /* On, and off again. */
    NAOS_CHECK_INT_EQ(argx_option_set_initial(negatable, true), ARGX_ERROR_NONE);
    NAOS_CHECK_TRUE(argx_option_initial(negatable));
    NAOS_CHECK_INT_EQ(argx_option_set_initial(negatable, true), ARGX_ERROR_NONE);
    NAOS_CHECK_TRUE(argx_option_initial(negatable));
    NAOS_CHECK_INT_EQ(argx_option_set_initial(negatable, false), ARGX_ERROR_NONE);
    NAOS_CHECK_FALSE(argx_option_initial(negatable));
    check_attributes(negatable, NULL, NULL, NULL, false, false);

    destroy_root(root);
}

/* A negatable flag has an initial state: the state when it is not given. */
NAOS_TEST(the_initial_state_of_a_negatable_flag_is_set_and_read)
{
    argx_command_t *root = create_root();
    argx_option_t *color = add(ARGX_OPTION_NEGATABLE, root, 'c', "color");
    argx_option_t *pager = add(ARGX_OPTION_NEGATABLE, root, '\0', "pager");
    size_t requests = argx_test_allocator_requests();

    NAOS_CHECK_INT_EQ(argx_option_set_initial(color, true), ARGX_ERROR_NONE);
    check_attributes(color, NULL, NULL, NULL, false, true);
    check_attributes(pager, NULL, NULL, NULL, false, false);

    /* Off can be set where nothing was set. */
    NAOS_CHECK_INT_EQ(argx_option_set_initial(pager, false), ARGX_ERROR_NONE);
    check_attributes(pager, NULL, NULL, NULL, false, false);

    /* Nothing is allocated for a state. */
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), requests);

    /*
     * The names are as they were, and it has a description like every
     * kind.
     */
    check_option(color, root, ARGX_OPTION_NEGATABLE, 'c', "color", "no-color", NULL);
    NAOS_CHECK_INT_EQ(argx_option_set_description(color, "Use colors."), ARGX_ERROR_NONE);
    check_attributes(color, "Use colors.", NULL, NULL, false, true);

    destroy_root(root);
}

/*
 * Checks that the attributes of an option with a value cannot be set on
 * option, whatever the value is, and that nothing is allocated for it.
 */
static void check_takes_no_attribute_of_a_value(argx_option_t *option)
{
    const size_t requests = argx_test_allocator_requests();

    NAOS_CHECK_INT_EQ(argx_option_set_value_name(option, "NAME"), ARGX_ERROR_INVALID_DEFINITION);
    NAOS_CHECK_INT_EQ(argx_option_set_value_name(option, ""), ARGX_ERROR_INVALID_DEFINITION);
    NAOS_CHECK_INT_EQ(argx_option_set_required(option, true), ARGX_ERROR_INVALID_DEFINITION);
    NAOS_CHECK_INT_EQ(argx_option_set_required(option, false), ARGX_ERROR_INVALID_DEFINITION);
    NAOS_CHECK_INT_EQ(argx_option_set_default(option, "value"), ARGX_ERROR_INVALID_DEFINITION);
    NAOS_CHECK_INT_EQ(argx_option_set_default(option, ""), ARGX_ERROR_INVALID_DEFINITION);

    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), requests);
}

/* Checks that the initial state cannot be set on option, on or off. */
static void check_takes_no_initial_state(argx_option_t *option)
{
    const size_t requests = argx_test_allocator_requests();

    NAOS_CHECK_INT_EQ(argx_option_set_initial(option, true), ARGX_ERROR_INVALID_DEFINITION);
    NAOS_CHECK_INT_EQ(argx_option_set_initial(option, false), ARGX_ERROR_INVALID_DEFINITION);

    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), requests);
}

/*
 * The display name of the value, required and the default value are for an
 * option with a value only, and the initial state is for a negatable flag
 * only. On another kind the set is an inconsistent definition and changes
 * nothing.
 */
NAOS_TEST(an_attribute_of_another_kind_is_an_inconsistent_definition)
{
    argx_command_t *root = create_root();
    argx_option_t *flag = add(ARGX_OPTION_FLAG, root, 'v', "verbose");
    argx_option_t *negatable = add(ARGX_OPTION_NEGATABLE, root, 'c', "color");
    argx_option_t *count = add(ARGX_OPTION_COUNT, root, 'd', "debug");
    argx_option_t *value = add(ARGX_OPTION_VALUE, root, 'o', "output");
    size_t live;

    /* Options without an attribute. */
    check_takes_no_attribute_of_a_value(flag);
    check_takes_no_initial_state(flag);
    check_takes_no_attribute_of_a_value(negatable);
    check_takes_no_attribute_of_a_value(count);
    check_takes_no_initial_state(count);
    check_takes_no_initial_state(value);

    check_attributes(flag, NULL, NULL, NULL, false, false);
    check_attributes(negatable, NULL, NULL, NULL, false, false);
    check_attributes(count, NULL, NULL, NULL, false, false);
    check_attributes(value, NULL, NULL, NULL, false, false);

    /* Options with every attribute that they can have. */
    NAOS_CHECK_INT_EQ(argx_option_set_description(flag, "Flag."), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_option_set_description(negatable, "Negatable."), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_option_set_initial(negatable, true), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_option_set_description(count, "Count."), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_option_set_description(value, "Value."), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_option_set_value_name(value, "FILE"), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_option_set_default(value, "a.out"), ARGX_ERROR_NONE);
    live = argx_test_allocator_live();

    check_takes_no_attribute_of_a_value(flag);
    check_takes_no_initial_state(flag);
    check_takes_no_attribute_of_a_value(negatable);
    check_takes_no_attribute_of_a_value(count);
    check_takes_no_initial_state(count);
    check_takes_no_initial_state(value);

    /* Every value is as it was, and so is the rest of the tree. */
    check_attributes(flag, "Flag.", NULL, NULL, false, false);
    check_attributes(negatable, "Negatable.", NULL, NULL, false, true);
    check_attributes(count, "Count.", NULL, NULL, false, false);
    check_attributes(value, "Value.", "FILE", "a.out", false, false);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), live);
    NAOS_CHECK_UINT_EQ(argx_command_option_count(root), 4);
    check_option(flag, root, ARGX_OPTION_FLAG, 'v', "verbose", NULL, NULL);
    check_option(negatable, root, ARGX_OPTION_NEGATABLE, 'c', "color", "no-color", NULL);
    check_option(count, root, ARGX_OPTION_COUNT, 'd', "debug", NULL, NULL);
    check_option(value, root, ARGX_OPTION_VALUE, 'o', "output", NULL, argx_type_string());

    /* The same with a required option. */
    NAOS_CHECK_INT_EQ(argx_option_set_required(value, true), ARGX_ERROR_NONE);
    check_takes_no_initial_state(value);
    check_attributes(value, "Value.", "FILE", "a.out", true, false);

    /*
     * The schema can be used after the failures: the attributes that the
     * kinds do have are set, and an option is added.
     */
    NAOS_CHECK_INT_EQ(argx_option_set_description(flag, "Still a flag."), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_option_set_initial(negatable, false), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_option_set_value_name(value, "PATH"), ARGX_ERROR_NONE);
    check_attributes(flag, "Still a flag.", NULL, NULL, false, false);
    check_attributes(negatable, "Negatable.", NULL, NULL, false, false);
    check_attributes(value, "Value.", "PATH", "a.out", true, false);
    (void)add(ARGX_OPTION_FLAG, root, 'x', NULL);
    NAOS_CHECK_UINT_EQ(argx_command_option_count(root), 5);

    destroy_root(root);
}

/*
 * The strings are copied: the caller may change its buffers after the
 * call.
 */
NAOS_TEST(the_strings_of_the_attributes_are_copied)
{
    char description[] = "Where the output goes.";
    char value_name[] = "FILE";
    char default_value[] = "a.out";
    argx_command_t *root = create_root();
    argx_option_t *option = add(ARGX_OPTION_VALUE, root, 'o', "output");

    NAOS_CHECK_INT_EQ(argx_option_set_description(option, description), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_option_set_value_name(option, value_name), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_option_set_default(option, default_value), ARGX_ERROR_NONE);

    memset(description, 'x', sizeof(description) - 1);
    memset(value_name, 'y', sizeof(value_name) - 1);
    memset(default_value, 'z', sizeof(default_value) - 1);

    NAOS_CHECK_TRUE(argx_option_description(option) != description);
    NAOS_CHECK_TRUE(argx_option_value_name(option) != value_name);
    NAOS_CHECK_TRUE(argx_option_default(option) != default_value);
    check_attributes(option, "Where the output goes.", "FILE", "a.out", false, false);

    /* The same when a value is replaced. */
    memcpy(value_name, "PATH", sizeof(value_name));
    NAOS_CHECK_INT_EQ(argx_option_set_value_name(option, value_name), ARGX_ERROR_NONE);
    memset(value_name, 'y', sizeof(value_name) - 1);
    check_attributes(option, "Where the output goes.", "PATH", "a.out", false, false);

    destroy_root(root);
}

/*
 * An attribute can be set with a string that was read from another
 * attribute or from a name of the tree: it gets a copy of its own.
 */
NAOS_TEST(an_attribute_can_be_set_with_a_string_that_was_read)
{
    argx_command_t *root = create_root();
    argx_option_t *option = add(ARGX_OPTION_VALUE, root, 'o', "output");

    NAOS_CHECK_INT_EQ(argx_option_set_value_name(option, "FILE"), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_option_set_default(option, argx_option_value_name(option)),
                      ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_option_set_description(option, argx_option_long_name(option)),
                      ARGX_ERROR_NONE);
    check_attributes(option, "output", "FILE", "FILE", false, false);
    NAOS_CHECK_TRUE(argx_option_default(option) != argx_option_value_name(option));

    destroy_root(root);
}

/*
 * Required and a default value do not go together, but that is found by the
 * validation of the tree: the order in which the two are set does not
 * matter, and each set succeeds.
 */
NAOS_TEST(required_and_a_default_value_are_both_kept)
{
    argx_command_t *root = create_root();
    argx_option_t *first = add(ARGX_OPTION_VALUE, root, 'a', NULL);
    argx_option_t *second = add(ARGX_OPTION_VALUE, root, 'b', NULL);

    NAOS_CHECK_INT_EQ(argx_option_set_required(first, true), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_option_set_default(first, "1"), ARGX_ERROR_NONE);
    check_attributes(first, NULL, NULL, "1", true, false);

    NAOS_CHECK_INT_EQ(argx_option_set_default(second, "2"), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_option_set_required(second, true), ARGX_ERROR_NONE);
    check_attributes(second, NULL, NULL, "2", true, false);

    destroy_root(root);
}

/* ---- Destruction ------------------------------------------------------- */

/*
 * One destruction of the root frees the options of every command, their
 * names and the strings of their attributes.
 */
NAOS_TEST(destroy_frees_the_options_of_the_whole_tree)
{
    argx_command_t *root = create_root();
    argx_command_t *build = add_sub(root, "build");
    argx_command_t *inner = add_sub(build, "inner");
    argx_option_t *output;
    argx_option_t *pager;

    (void)add(ARGX_OPTION_FLAG, root, 'v', "verbose");
    (void)add(ARGX_OPTION_FLAG, root, 'x', NULL);
    (void)add(ARGX_OPTION_NEGATABLE, root, 'c', "color");
    (void)add(ARGX_OPTION_COUNT, build, 'd', "debug");
    output = add(ARGX_OPTION_VALUE, build, 'o', "output");
    pager = add(ARGX_OPTION_NEGATABLE, inner, '\0', "pager");
    (void)add(ARGX_OPTION_VALUE, inner, '\0', "jobs");

    NAOS_CHECK_INT_EQ(argx_option_set_description(output, "Where the output goes."), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_option_set_value_name(output, "FILE"), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_option_set_default(output, "a.out"), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_option_set_description(pager, "Use a pager."), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_option_set_initial(pager, true), ARGX_ERROR_NONE);

    NAOS_CHECK_TRUE(argx_test_allocator_live() > 0);

    argx_command_destroy(root);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);

    argx_test_allocator_uninstall();
    check_clean();
}

/* ---- Failure of an allocation ------------------------------------------ */

/*
 * Builds a root with existing options "o0", "o1", ... of the four kinds in
 * turn, and returns it. The number is chosen by the tests so that the array
 * of the options is empty, has room, or is full.
 */
static argx_command_t *build_root_with(size_t existing)
{
    static const char *const names[] = { "o0", "o1", "o2", "o3", "o4", "o5", "o6", "o7" };
    static const argx_option_kind_t kinds[] = { ARGX_OPTION_NEGATABLE, ARGX_OPTION_FLAG,
                                                  ARGX_OPTION_COUNT, ARGX_OPTION_VALUE };
    argx_command_t *root = NULL;
    size_t index;

    NAOS_REQUIRE_TRUE(existing <= (sizeof(names) / sizeof(names[0])));
    NAOS_REQUIRE_TRUE(argx_command_create("app", &root) == ARGX_ERROR_NONE);
    for (index = 0; index < existing; ++index)
    {
        (void)add(kinds[index % 4], root, (char)('0' + (int)index), names[index]);
    }
    return root;
}

/*
 * Fails each allocation of the addition of an option of the kind kind, with
 * the names -n and --new, to a root that has existing options already.
 * Returns the number of the failures.
 */
static size_t fail_each_allocation_of_an_addition(argx_option_kind_t kind, size_t existing)
{
    const size_t limit = 20;
    size_t failures = 0;
    bool completed = false;
    size_t n;

    for (n = 1; n <= limit; ++n)
    {
        argx_command_t *root;
        argx_option_t *option;
        argx_error_t error;
        size_t requests;
        size_t live;
        size_t index;

        argx_test_allocator_reset();
        root = build_root_with(existing);
        requests = argx_test_allocator_requests();
        live = argx_test_allocator_live();
        argx_test_allocator_fail_at(requests + n);

        /* What a failure must not leave in the output. */
        option = (existing == 0) ? NULL : (argx_option_t *)argx_command_option_at(root, 0);
        error = add_kind(kind, root, 'n', "new", &option);

        if ((error == ARGX_ERROR_NONE) && (argx_test_allocator_requests() < (requests + n)))
        {
            /* Every allocation point has been passed. */
            NAOS_CHECK_TRUE(option != NULL);
            NAOS_CHECK_UINT_EQ(argx_command_option_count(root), existing + 1);
            argx_command_destroy(root);
            completed = true;
            break;
        }

        ++failures;
        NAOS_CHECK_INT_EQ(error, ARGX_ERROR_OUT_OF_MEMORY);
        NAOS_CHECK_TRUE(option == NULL);
        NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), requests + n);

        /*
         * The tree is as it was before the call, and nothing of the call
         * stays allocated.
         */
        NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), live);
        NAOS_CHECK_UINT_EQ(argx_command_option_count(root), existing);
        NAOS_CHECK_TRUE(argx_command_find_short(root, 'n') == NULL);
        NAOS_CHECK_TRUE(find_long(root, "new", NULL) == NULL);
        NAOS_CHECK_TRUE(find_long(root, "no-new", NULL) == NULL);
        NAOS_CHECK_STR_EQ(argx_command_name(root), "app");
        for (index = 0; index < existing; ++index)
        {
            const argx_option_t *old = argx_command_option_at(root, index);

            NAOS_CHECK_TRUE(argx_option_command(old) == root);
            NAOS_CHECK_TRUE(argx_command_find_short(root, argx_option_short_name(old)) == old);
            NAOS_CHECK_TRUE(find_long(root, argx_option_long_name(old), NULL) == old);
        }

        /* The same addition works afterwards. */
        argx_test_allocator_fail_at(0);
        NAOS_CHECK_INT_EQ(add_kind(kind, root, 'n', "new", &option), ARGX_ERROR_NONE);
        NAOS_CHECK_TRUE(option != NULL);
        NAOS_CHECK_UINT_EQ(argx_command_option_count(root), existing + 1);
        NAOS_CHECK_TRUE(argx_command_option_at(root, existing) == option);
        NAOS_CHECK_TRUE(argx_command_find_short(root, 'n') == option);

        argx_command_destroy(root);
        NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);
        NAOS_CHECK_UINT_EQ(argx_test_allocator_violations(), 0);
    }

    NAOS_CHECK_TRUE(completed);
    return failures;
}

/*
 * When an allocation of the addition of an option fails, whichever it is,
 * the output is NULL, the tree is as it was, and nothing leaks. The array of
 * the options grows in the first and in the third case, and has room in the
 * second.
 */
NAOS_TEST(a_failed_addition_leaves_the_tree_as_it_was)
{
    static const argx_option_kind_t kinds[] = { ARGX_OPTION_FLAG, ARGX_OPTION_NEGATABLE,
                                                  ARGX_OPTION_COUNT, ARGX_OPTION_VALUE };
    size_t with_room_of_a_flag = 0;
    size_t index;

    argx_test_allocator_install();

    for (index = 0; index < (sizeof(kinds) / sizeof(kinds[0])); ++index)
    {
        size_t growing = fail_each_allocation_of_an_addition(kinds[index], 0);
        size_t with_room = fail_each_allocation_of_an_addition(kinds[index], 1);

        NAOS_CHECK_TRUE(with_room > 0);
        NAOS_CHECK_UINT_EQ(growing, with_room + 1);
        NAOS_CHECK_UINT_EQ(fail_each_allocation_of_an_addition(kinds[index], 4), growing);

        /* A negatable flag allocates its negated name as well. */
        if (kinds[index] == ARGX_OPTION_FLAG)
        {
            with_room_of_a_flag = with_room;
        }
        else if (kinds[index] == ARGX_OPTION_NEGATABLE)
        {
            NAOS_CHECK_UINT_EQ(with_room, with_room_of_a_flag + 1);
        }
        else
        {
            NAOS_CHECK_UINT_EQ(with_room, with_room_of_a_flag);
        }
    }

    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * When the copy of a string of an attribute cannot be allocated, the set is
 * out of memory, the attribute has the value it had, set or not, and nothing
 * leaks. A set makes one allocation, the copy.
 */
NAOS_TEST(a_failed_set_of_a_string_keeps_the_value_it_had)
{
    static const text_attribute_t attributes[TEXT_ATTRIBUTE_COUNT] = { TEXT_DESCRIPTION, TEXT_VALUE_NAME,
                                                                       TEXT_DEFAULT };
    size_t index;
    size_t was_set;

    argx_test_allocator_install();

    for (index = 0; index < TEXT_ATTRIBUTE_COUNT; ++index)
    {
        for (was_set = 0; was_set < 2; ++was_set)
        {
            const text_attribute_t attribute = attributes[index];
            const char *const old = (was_set != 0) ? "old value" : NULL;
            argx_command_t *root = NULL;
            argx_option_t *option;
            size_t requests;
            size_t live;
            size_t other;

            argx_test_allocator_reset();
            NAOS_REQUIRE_TRUE(argx_command_create("app", &root) == ARGX_ERROR_NONE);
            option = add(ARGX_OPTION_VALUE, root, 'o', "output");

            /*
             * The other attributes are set, so that it shows if the failure
             * touches one of them.
             */
            for (other = 0; other < TEXT_ATTRIBUTE_COUNT; ++other)
            {
                if (other != index)
                {
                    NAOS_REQUIRE_TRUE(set_text(attributes[other], option, "other") == ARGX_ERROR_NONE);
                }
            }
            NAOS_REQUIRE_TRUE(argx_option_set_required(option, true) == ARGX_ERROR_NONE);
            if (old != NULL)
            {
                NAOS_REQUIRE_TRUE(set_text(attribute, option, old) == ARGX_ERROR_NONE);
            }

            requests = argx_test_allocator_requests();
            live = argx_test_allocator_live();
            argx_test_allocator_fail_at(requests + 1);

            NAOS_CHECK_INT_EQ(set_text(attribute, option, "a new and longer value"),
                              ARGX_ERROR_OUT_OF_MEMORY);
            NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), requests + 1);

            /*
             * The option is as it was before the call, and nothing of the
             * call stays allocated.
             */
            NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), live);
            check_text(get_text(attribute, option), old);
            for (other = 0; other < TEXT_ATTRIBUTE_COUNT; ++other)
            {
                if (other != index)
                {
                    check_text(get_text(attributes[other], option), "other");
                }
            }
            NAOS_CHECK_TRUE(argx_option_is_required(option));
            check_option(option, root, ARGX_OPTION_VALUE, 'o', "output", NULL, argx_type_string());
            NAOS_CHECK_UINT_EQ(argx_command_option_count(root), 1);

            /* The same set works afterwards, with one allocation. */
            argx_test_allocator_fail_at(0);
            NAOS_CHECK_INT_EQ(set_text(attribute, option, "a new and longer value"), ARGX_ERROR_NONE);
            NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), requests + 2);
            NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), (old != NULL) ? live : (live + 1));
            check_text(get_text(attribute, option), "a new and longer value");

            argx_command_destroy(root);
            NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);
            NAOS_CHECK_UINT_EQ(argx_test_allocator_violations(), 0);
        }
    }

    argx_test_allocator_uninstall();
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

NAOS_SUITE(attribute,
    NAOS_ADD_TEST(a_new_option_has_no_attribute),
    NAOS_ADD_TEST(a_description_is_set_on_every_kind),
    NAOS_ADD_TEST(the_attributes_of_an_option_with_a_value_are_set_and_read),
    NAOS_ADD_TEST(an_attribute_that_is_set_again_is_replaced),
    NAOS_ADD_TEST(the_initial_state_of_a_negatable_flag_is_set_and_read),
    NAOS_ADD_TEST(an_attribute_of_another_kind_is_an_inconsistent_definition),
    NAOS_ADD_TEST(the_strings_of_the_attributes_are_copied),
    NAOS_ADD_TEST(an_attribute_can_be_set_with_a_string_that_was_read),
    NAOS_ADD_TEST(required_and_a_default_value_are_both_kept)
)

NAOS_SUITE(destroy,
    NAOS_ADD_TEST(destroy_frees_the_options_of_the_whole_tree)
)

NAOS_SUITE(failure,
    NAOS_ADD_TEST(a_failed_addition_leaves_the_tree_as_it_was),
    NAOS_ADD_TEST(a_failed_set_of_a_string_keeps_the_value_it_had)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(add),
    NAOS_ADD_SUITE(check),
    NAOS_ADD_SUITE(lookup),
    NAOS_ADD_SUITE(attribute),
    NAOS_ADD_SUITE(destroy),
    NAOS_ADD_SUITE(failure)
)
