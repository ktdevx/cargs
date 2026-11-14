/*
 * Tests of the public command API: the command part of <argx/schema.h>.
 *
 * A tree of commands is created, read and destroyed the way a consumer does
 * it. The commands here have no options and no positionals: their counts
 * and lookups are tested on commands that have none. The tests with options
 * are in option_test.c, those with positionals in positional_test.c.
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
static argx_command_t *add(argx_command_t *command, const char *name)
{
    argx_command_t *sub = NULL;

    NAOS_REQUIRE_TRUE(argx_command_add_subcommand(command, name, &sub) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(sub != NULL);
    return sub;
}

/*
 * Checks that command has no option, no positional and no subcommand, and
 * that every lookup finds nothing.
 */
static void check_empty(const argx_command_t *command)
{
    bool negated = true;

    NAOS_CHECK_UINT_EQ(argx_command_option_count(command), 0);
    NAOS_CHECK_UINT_EQ(argx_command_positional_count(command), 0);
    NAOS_CHECK_UINT_EQ(argx_command_subcommand_count(command), 0);

    NAOS_CHECK_TRUE(argx_command_find_short(command, 'v') == NULL);
    NAOS_CHECK_TRUE(argx_command_find_short(command, '\0') == NULL);
    NAOS_CHECK_TRUE(argx_command_find_long(command, "verbose", 7, &negated) == NULL);
    NAOS_CHECK_TRUE(negated);
    NAOS_CHECK_TRUE(argx_command_find_long(command, "verbose", 7, NULL) == NULL);
    NAOS_CHECK_TRUE(argx_command_find_long(command, "verbose", 0, NULL) == NULL);
    NAOS_CHECK_TRUE(argx_command_find_subcommand(command, "build") == NULL);
    NAOS_CHECK_TRUE(argx_command_find_subcommand(command, "") == NULL);
}

/* ---- Creation ---------------------------------------------------------- */

NAOS_TEST(a_new_root_is_empty)
{
    argx_command_t *root = create_root();

    NAOS_CHECK_STR_EQ(argx_command_name(root), "app");
    NAOS_CHECK_TRUE(argx_command_description(root) == NULL);
    NAOS_CHECK_TRUE(argx_command_parent(root) == NULL);
    check_empty(root);

    destroy_root(root);
}

/*
 * The name of a root is any string that is not empty: the rule of the
 * names of subcommands does not apply.
 */
NAOS_TEST(a_root_takes_any_name_that_is_not_empty)
{
    static const char *const names[] = { "a", "my app", "-x", "_", "./bin/app.exe", "\xe3\x81\x82" };
    size_t index;

    argx_test_allocator_install();

    for (index = 0; index < (sizeof(names) / sizeof(names[0])); ++index)
    {
        argx_command_t *root = NULL;

        NAOS_CHECK_INT_EQ(argx_command_create(names[index], &root), ARGX_ERROR_NONE);
        NAOS_REQUIRE_TRUE(root != NULL);
        NAOS_CHECK_STR_EQ(argx_command_name(root), names[index]);
        argx_command_destroy(root);
    }

    argx_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(an_empty_root_name_is_an_invalid_name)
{
    argx_command_t *root = create_root();
    argx_command_t *other = root;

    NAOS_CHECK_INT_EQ(argx_command_create("", &other), ARGX_ERROR_INVALID_NAME);
    NAOS_CHECK_TRUE(other == NULL);

    /* Nothing was allocated for it: only the two blocks of root are live. */
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 2);

    destroy_root(root);
}

NAOS_TEST(the_name_of_a_root_is_copied)
{
    char buffer[] = "app";
    argx_command_t *root = NULL;

    argx_test_allocator_install();
    NAOS_REQUIRE_TRUE(argx_command_create(buffer, &root) == ARGX_ERROR_NONE);

    memset(buffer, 'x', sizeof(buffer) - 1);
    NAOS_CHECK_TRUE(argx_command_name(root) != buffer);
    NAOS_CHECK_STR_EQ(argx_command_name(root), "app");

    destroy_root(root);
}

/* ---- Subcommands ------------------------------------------------------- */

/*
 * Three levels below the root:
 *
 *     app
 *     ├── remote
 *     │   ├── add
 *     │   │   └── mirror
 *     │   └── remove
 *     ├── build
 *     └── test
 */
NAOS_TEST(subcommands_nest_and_are_read_in_the_order_of_the_definition)
{
    argx_command_t *root = create_root();
    argx_command_t *remote = add(root, "remote");
    argx_command_t *build = add(root, "build");
    argx_command_t *test = add(root, "test");
    argx_command_t *remote_add = add(remote, "add");
    argx_command_t *remote_remove = add(remote, "remove");
    argx_command_t *mirror = add(remote_add, "mirror");

    NAOS_CHECK_UINT_EQ(argx_command_subcommand_count(root), 3);
    NAOS_CHECK_TRUE(argx_command_subcommand_at(root, 0) == remote);
    NAOS_CHECK_TRUE(argx_command_subcommand_at(root, 1) == build);
    NAOS_CHECK_TRUE(argx_command_subcommand_at(root, 2) == test);

    NAOS_CHECK_UINT_EQ(argx_command_subcommand_count(remote), 2);
    NAOS_CHECK_TRUE(argx_command_subcommand_at(remote, 0) == remote_add);
    NAOS_CHECK_TRUE(argx_command_subcommand_at(remote, 1) == remote_remove);

    NAOS_CHECK_UINT_EQ(argx_command_subcommand_count(remote_add), 1);
    NAOS_CHECK_TRUE(argx_command_subcommand_at(remote_add, 0) == mirror);

    NAOS_CHECK_STR_EQ(argx_command_name(remote), "remote");
    NAOS_CHECK_STR_EQ(argx_command_name(build), "build");
    NAOS_CHECK_STR_EQ(argx_command_name(test), "test");
    NAOS_CHECK_STR_EQ(argx_command_name(remote_add), "add");
    NAOS_CHECK_STR_EQ(argx_command_name(remote_remove), "remove");
    NAOS_CHECK_STR_EQ(argx_command_name(mirror), "mirror");

    NAOS_CHECK_TRUE(argx_command_parent(root) == NULL);
    NAOS_CHECK_TRUE(argx_command_parent(remote) == root);
    NAOS_CHECK_TRUE(argx_command_parent(build) == root);
    NAOS_CHECK_TRUE(argx_command_parent(test) == root);
    NAOS_CHECK_TRUE(argx_command_parent(remote_add) == remote);
    NAOS_CHECK_TRUE(argx_command_parent(remote_remove) == remote);
    NAOS_CHECK_TRUE(argx_command_parent(mirror) == remote_add);

    /* A new subcommand is empty, like a new root. */
    NAOS_CHECK_TRUE(argx_command_description(mirror) == NULL);
    check_empty(mirror);
    check_empty(build);

    destroy_root(root);
}

/* A handle stays the same when more subcommands are added after it. */
NAOS_TEST(handles_stay_valid_while_the_tree_grows)
{
    static const char *const names[] = { "a", "b", "c", "d", "e", "f", "g", "h", "i", "j" };
    const size_t count = sizeof(names) / sizeof(names[0]);
    argx_command_t *root = create_root();
    argx_command_t *subs[sizeof(names) / sizeof(names[0])];
    size_t index;

    for (index = 0; index < count; ++index)
    {
        subs[index] = add(root, names[index]);
    }

    NAOS_CHECK_UINT_EQ(argx_command_subcommand_count(root), count);
    for (index = 0; index < count; ++index)
    {
        NAOS_CHECK_TRUE(argx_command_subcommand_at(root, index) == subs[index]);
        NAOS_CHECK_STR_EQ(argx_command_name(subs[index]), names[index]);
        NAOS_CHECK_TRUE(argx_command_parent(subs[index]) == root);
    }

    destroy_root(root);
}

NAOS_TEST(find_subcommand_looks_in_the_command_itself_only)
{
    argx_command_t *root = create_root();
    argx_command_t *remote = add(root, "remote");
    argx_command_t *build = add(root, "build");
    argx_command_t *remote_add = add(remote, "add");
    argx_command_t *mirror = add(remote_add, "mirror");

    NAOS_CHECK_TRUE(argx_command_find_subcommand(root, "remote") == remote);
    NAOS_CHECK_TRUE(argx_command_find_subcommand(root, "build") == build);
    NAOS_CHECK_TRUE(argx_command_find_subcommand(remote, "add") == remote_add);
    NAOS_CHECK_TRUE(argx_command_find_subcommand(remote_add, "mirror") == mirror);

    /*
     * Not the subcommands of a subcommand, not those of the parent, not the
     * command itself.
     */
    NAOS_CHECK_TRUE(argx_command_find_subcommand(root, "add") == NULL);
    NAOS_CHECK_TRUE(argx_command_find_subcommand(root, "mirror") == NULL);
    NAOS_CHECK_TRUE(argx_command_find_subcommand(remote, "build") == NULL);
    NAOS_CHECK_TRUE(argx_command_find_subcommand(remote, "remote") == NULL);
    NAOS_CHECK_TRUE(argx_command_find_subcommand(root, "app") == NULL);

    /* The whole name, byte by byte. */
    NAOS_CHECK_TRUE(argx_command_find_subcommand(root, "buil") == NULL);
    NAOS_CHECK_TRUE(argx_command_find_subcommand(root, "builds") == NULL);
    NAOS_CHECK_TRUE(argx_command_find_subcommand(root, "Build") == NULL);
    NAOS_CHECK_TRUE(argx_command_find_subcommand(root, "") == NULL);

    destroy_root(root);
}

NAOS_TEST(the_name_of_a_subcommand_is_copied)
{
    char buffer[] = "build";
    argx_command_t *root = create_root();
    argx_command_t *build = add(root, buffer);

    memset(buffer, 'x', sizeof(buffer) - 1);
    NAOS_CHECK_TRUE(argx_command_name(build) != buffer);
    NAOS_CHECK_STR_EQ(argx_command_name(build), "build");
    NAOS_CHECK_TRUE(argx_command_find_subcommand(root, "build") == build);

    destroy_root(root);
}

/* The output is optional: the subcommand is found by its name afterwards. */
NAOS_TEST(a_subcommand_can_be_added_without_an_output)
{
    argx_command_t *root = create_root();
    const argx_command_t *build;

    NAOS_CHECK_INT_EQ(argx_command_add_subcommand(root, "build", NULL), ARGX_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(argx_command_subcommand_count(root), 1);
    build = argx_command_find_subcommand(root, "build");
    NAOS_REQUIRE_TRUE(build != NULL);
    NAOS_CHECK_STR_EQ(argx_command_name(build), "build");

    /* The failures need no output either. */
    NAOS_CHECK_INT_EQ(argx_command_add_subcommand(root, "build", NULL), ARGX_ERROR_NAME_CONFLICT);
    NAOS_CHECK_INT_EQ(argx_command_add_subcommand(root, "", NULL), ARGX_ERROR_INVALID_NAME);
    NAOS_CHECK_UINT_EQ(argx_command_subcommand_count(root), 1);

    destroy_root(root);
}

NAOS_TEST(a_subcommand_name_follows_the_rule_of_names)
{
    static const char *const valid[] = { "a", "7", "build", "dry-run", "dry_run", "v2", "B-_" };
    static const char *const invalid[] = { "", "-x", "_x", "a b", " a", "a=b", "a.b", "caf\xc3\xa9" };
    argx_command_t *root = create_root();
    size_t requests;
    size_t index;

    for (index = 0; index < (sizeof(valid) / sizeof(valid[0])); ++index)
    {
        argx_command_t *sub = NULL;

        NAOS_CHECK_INT_EQ(argx_command_add_subcommand(root, valid[index], &sub), ARGX_ERROR_NONE);
        NAOS_CHECK_TRUE(sub != NULL);
    }
    NAOS_CHECK_UINT_EQ(argx_command_subcommand_count(root), sizeof(valid) / sizeof(valid[0]));

    requests = argx_test_allocator_requests();
    for (index = 0; index < (sizeof(invalid) / sizeof(invalid[0])); ++index)
    {
        argx_command_t *sub = root;

        NAOS_CHECK_INT_EQ(argx_command_add_subcommand(root, invalid[index], &sub),
                          ARGX_ERROR_INVALID_NAME);
        NAOS_CHECK_TRUE(sub == NULL);
    }

    /* The failures added nothing and allocated nothing. */
    NAOS_CHECK_UINT_EQ(argx_command_subcommand_count(root), sizeof(valid) / sizeof(valid[0]));
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), requests);

    destroy_root(root);
}

NAOS_TEST(a_subcommand_name_is_used_once_in_a_command)
{
    argx_command_t *root = create_root();
    argx_command_t *build = add(root, "build");
    argx_command_t *test = add(root, "test");
    argx_command_t *sub = root;
    size_t requests = argx_test_allocator_requests();

    NAOS_CHECK_INT_EQ(argx_command_add_subcommand(root, "build", &sub), ARGX_ERROR_NAME_CONFLICT);
    NAOS_CHECK_TRUE(sub == NULL);
    sub = root;
    NAOS_CHECK_INT_EQ(argx_command_add_subcommand(root, "test", &sub), ARGX_ERROR_NAME_CONFLICT);
    NAOS_CHECK_TRUE(sub == NULL);

    /* The tree is as it was, and nothing was allocated. */
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), requests);
    NAOS_CHECK_UINT_EQ(argx_command_subcommand_count(root), 2);
    NAOS_CHECK_TRUE(argx_command_subcommand_at(root, 0) == build);
    NAOS_CHECK_TRUE(argx_command_subcommand_at(root, 1) == test);
    NAOS_CHECK_TRUE(argx_command_find_subcommand(root, "build") == build);

    /* The tree can still be used. */
    sub = add(root, "run");
    NAOS_CHECK_UINT_EQ(argx_command_subcommand_count(root), 3);
    NAOS_CHECK_TRUE(argx_command_subcommand_at(root, 2) == sub);

    destroy_root(root);
}

/*
 * Upper and lower case are different names, and the same name can be used
 * in another command: in the parent, in a child, in a sibling, and as the
 * name of the root.
 */
NAOS_TEST(names_conflict_only_bytewise_and_only_in_one_command)
{
    argx_command_t *root = create_root();
    argx_command_t *build = add(root, "build");
    argx_command_t *upper = add(root, "Build");
    argx_command_t *app = add(root, "app");
    argx_command_t *inner = add(build, "build");
    argx_command_t *sibling = add(upper, "build");

    NAOS_CHECK_UINT_EQ(argx_command_subcommand_count(root), 3);
    NAOS_CHECK_TRUE(argx_command_find_subcommand(root, "build") == build);
    NAOS_CHECK_TRUE(argx_command_find_subcommand(root, "Build") == upper);
    NAOS_CHECK_TRUE(argx_command_find_subcommand(root, "BUILD") == NULL);
    NAOS_CHECK_TRUE(argx_command_find_subcommand(root, "app") == app);
    NAOS_CHECK_TRUE(argx_command_find_subcommand(build, "build") == inner);
    NAOS_CHECK_TRUE(argx_command_find_subcommand(upper, "build") == sibling);
    NAOS_CHECK_TRUE(inner != sibling);

    destroy_root(root);
}

/* The depth of the nesting has no limit. */
NAOS_TEST(subcommands_nest_deeply)
{
    const size_t depth = 200;
    argx_command_t *root = create_root();
    argx_command_t *command = root;
    const argx_command_t *up;
    size_t level;

    for (level = 0; level < depth; ++level)
    {
        command = add(command, "sub");
    }

    up = command;
    for (level = 0; level < depth; ++level)
    {
        NAOS_REQUIRE_TRUE(up != NULL);
        NAOS_CHECK_UINT_EQ(argx_command_subcommand_count(up), (level == 0) ? 0u : 1u);
        up = argx_command_parent(up);
    }
    NAOS_CHECK_TRUE(up == root);

    destroy_root(root);
}

/* ---- Description ------------------------------------------------------- */

NAOS_TEST(a_description_is_set_and_replaced)
{
    argx_command_t *root = create_root();
    argx_command_t *build = add(root, "build");

    NAOS_CHECK_TRUE(argx_command_description(root) == NULL);

    NAOS_CHECK_INT_EQ(argx_command_set_description(root, "The first text."), ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_description(root) != NULL);
    NAOS_CHECK_STR_EQ(argx_command_description(root), "The first text.");

    NAOS_CHECK_INT_EQ(argx_command_set_description(root, "A second, longer text."), ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_description(root) != NULL);
    NAOS_CHECK_STR_EQ(argx_command_description(root), "A second, longer text.");

    /* The empty string is a description, and is not "no description". */
    NAOS_CHECK_INT_EQ(argx_command_set_description(root, ""), ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_description(root) != NULL);
    NAOS_CHECK_STR_EQ(argx_command_description(root), "");

    /* Each command has its own. */
    NAOS_CHECK_TRUE(argx_command_description(build) == NULL);
    NAOS_CHECK_INT_EQ(argx_command_set_description(build, "Builds."), ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_description(build) != NULL);
    NAOS_CHECK_STR_EQ(argx_command_description(build), "Builds.");
    NAOS_CHECK_STR_EQ(argx_command_description(root), "");

    /* The name and the rest of the command are as they were. */
    NAOS_CHECK_STR_EQ(argx_command_name(root), "app");
    NAOS_CHECK_UINT_EQ(argx_command_subcommand_count(root), 1);

    destroy_root(root);
}

NAOS_TEST(a_description_is_copied)
{
    char buffer[] = "Does things.";
    argx_command_t *root = create_root();

    NAOS_CHECK_INT_EQ(argx_command_set_description(root, buffer), ARGX_ERROR_NONE);

    memset(buffer, 'x', sizeof(buffer) - 1);
    NAOS_REQUIRE_TRUE(argx_command_description(root) != NULL);
    NAOS_CHECK_TRUE(argx_command_description(root) != buffer);
    NAOS_CHECK_STR_EQ(argx_command_description(root), "Does things.");

    destroy_root(root);
}

/* ---- Destruction ------------------------------------------------------- */

NAOS_TEST(destroy_does_nothing_for_null)
{
    argx_test_allocator_install();

    argx_command_destroy(NULL);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), 0);

    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * One destruction of the root frees the whole tree: commands, names,
 * descriptions and the arrays.
 */
NAOS_TEST(destroy_frees_the_whole_tree)
{
    argx_command_t *root = create_root();
    argx_command_t *remote = add(root, "remote");
    argx_command_t *remote_add = add(remote, "add");
    argx_command_t *mirror = add(remote_add, "mirror");

    (void)add(root, "build");
    (void)add(remote, "remove");
    NAOS_CHECK_INT_EQ(argx_command_set_description(root, "An application."), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_command_set_description(remote, "Remotes."), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_command_set_description(mirror, "Mirrors."), ARGX_ERROR_NONE);
    NAOS_CHECK_INT_EQ(argx_command_set_description(mirror, "Mirrors a remote."), ARGX_ERROR_NONE);

    NAOS_CHECK_TRUE(argx_test_allocator_live() > 0);

    argx_command_destroy(root);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);

    argx_test_allocator_uninstall();
    check_clean();
}

/* ---- Failure of an allocation ------------------------------------------ */

/*
 * When an allocation of the creation fails, whichever it is, nothing stays
 * allocated, the output is NULL, and the error is the allocation failure.
 */
NAOS_TEST(creation_frees_everything_when_an_allocation_fails)
{
    const size_t limit = 20;
    argx_command_t *other = NULL;
    size_t failures = 0;
    bool completed = false;
    size_t n;

    argx_test_allocator_install();

    /* What a failure must not set the output to. */
    NAOS_REQUIRE_TRUE(argx_command_create("other", &other) == ARGX_ERROR_NONE);
    argx_command_destroy(other);

    for (n = 1; n <= limit; ++n)
    {
        argx_command_t *root = other;
        argx_error_t error;

        argx_test_allocator_reset();
        argx_test_allocator_fail_at(n);

        error = argx_command_create("app", &root);

        if ((error == ARGX_ERROR_NONE) && (argx_test_allocator_requests() < n))
        {
            /* Every allocation point has been passed. */
            NAOS_CHECK_TRUE(root != NULL);
            argx_command_destroy(root);
            completed = true;
            break;
        }

        ++failures;
        NAOS_CHECK_INT_EQ(error, ARGX_ERROR_OUT_OF_MEMORY);
        NAOS_CHECK_TRUE(root == NULL);
        NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), n);
        NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);
        NAOS_CHECK_UINT_EQ(argx_test_allocator_violations(), 0);
    }

    NAOS_CHECK_TRUE(completed);
    NAOS_CHECK_TRUE(failures > 0);

    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * Builds a root with existing subcommands "s0", "s1", ... and returns it.
 * The number is chosen by the tests so that the array of the subcommands
 * is empty, has room, or is full.
 */
static argx_command_t *build_root_with(size_t existing)
{
    static const char *const names[] = { "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7" };
    argx_command_t *root = NULL;
    size_t index;

    NAOS_REQUIRE_TRUE(existing <= (sizeof(names) / sizeof(names[0])));
    NAOS_REQUIRE_TRUE(argx_command_create("app", &root) == ARGX_ERROR_NONE);
    for (index = 0; index < existing; ++index)
    {
        (void)add(root, names[index]);
    }
    NAOS_REQUIRE_TRUE(argx_command_set_description(root, "Text.") == ARGX_ERROR_NONE);
    return root;
}

/*
 * Fails each allocation of the addition of a subcommand to a root that has
 * existing subcommands already. Returns the number of the failures.
 */
static size_t fail_each_allocation_of_an_addition(size_t existing)
{
    const size_t limit = 20;
    size_t failures = 0;
    bool completed = false;
    size_t n;

    for (n = 1; n <= limit; ++n)
    {
        argx_command_t *root;
        argx_command_t *sub;
        argx_error_t error;
        size_t requests;
        size_t live;
        size_t index;

        argx_test_allocator_reset();
        root = build_root_with(existing);
        requests = argx_test_allocator_requests();
        live = argx_test_allocator_live();
        argx_test_allocator_fail_at(requests + n);

        sub = root;
        error = argx_command_add_subcommand(root, "new", &sub);

        if ((error == ARGX_ERROR_NONE) && (argx_test_allocator_requests() < (requests + n)))
        {
            /* Every allocation point has been passed. */
            NAOS_CHECK_TRUE(sub != NULL);
            NAOS_CHECK_UINT_EQ(argx_command_subcommand_count(root), existing + 1);
            argx_command_destroy(root);
            completed = true;
            break;
        }

        ++failures;
        NAOS_CHECK_INT_EQ(error, ARGX_ERROR_OUT_OF_MEMORY);
        NAOS_CHECK_TRUE(sub == NULL);
        NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), requests + n);

        /*
         * The tree is as it was before the call, and nothing of the call
         * stays allocated.
         */
        NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), live);
        NAOS_CHECK_UINT_EQ(argx_command_subcommand_count(root), existing);
        NAOS_CHECK_TRUE(argx_command_find_subcommand(root, "new") == NULL);
        NAOS_CHECK_STR_EQ(argx_command_name(root), "app");
        NAOS_CHECK_STR_EQ(argx_command_description(root), "Text.");
        for (index = 0; index < existing; ++index)
        {
            const argx_command_t *old = argx_command_subcommand_at(root, index);

            NAOS_CHECK_TRUE(argx_command_find_subcommand(root, argx_command_name(old)) == old);
            NAOS_CHECK_TRUE(argx_command_parent(old) == root);
        }

        /* The same addition works afterwards. */
        argx_test_allocator_fail_at(0);
        NAOS_CHECK_INT_EQ(argx_command_add_subcommand(root, "new", &sub), ARGX_ERROR_NONE);
        NAOS_CHECK_UINT_EQ(argx_command_subcommand_count(root), existing + 1);

        argx_command_destroy(root);
        NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);
        NAOS_CHECK_UINT_EQ(argx_test_allocator_violations(), 0);
    }

    NAOS_CHECK_TRUE(completed);
    return failures;
}

/*
 * When an allocation of the addition of a subcommand fails, whichever it
 * is, the output is NULL, the tree is as it was, and nothing leaks. The
 * array of the subcommands grows in the first and in the third case, and
 * has room in the second.
 */
NAOS_TEST(a_failed_addition_leaves_the_tree_as_it_was)
{
    size_t growing;
    size_t with_room;

    argx_test_allocator_install();

    growing = fail_each_allocation_of_an_addition(0);
    with_room = fail_each_allocation_of_an_addition(1);
    NAOS_CHECK_TRUE(with_room > 0);
    NAOS_CHECK_UINT_EQ(growing, with_room + 1);
    NAOS_CHECK_UINT_EQ(fail_each_allocation_of_an_addition(4), growing);

    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * When the copy of a new description cannot be allocated, the command
 * keeps the description it had.
 */
NAOS_TEST(a_failed_description_keeps_the_old_one)
{
    argx_command_t *root = create_root();

    argx_test_allocator_fail_at(argx_test_allocator_requests() + 1);
    NAOS_CHECK_INT_EQ(argx_command_set_description(root, "First."), ARGX_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_TRUE(argx_command_description(root) == NULL);

    NAOS_CHECK_INT_EQ(argx_command_set_description(root, "First."), ARGX_ERROR_NONE);

    argx_test_allocator_fail_at(argx_test_allocator_requests() + 1);
    NAOS_CHECK_INT_EQ(argx_command_set_description(root, "Second."), ARGX_ERROR_OUT_OF_MEMORY);
    NAOS_REQUIRE_TRUE(argx_command_description(root) != NULL);
    NAOS_CHECK_STR_EQ(argx_command_description(root), "First.");

    NAOS_CHECK_INT_EQ(argx_command_set_description(root, "Second."), ARGX_ERROR_NONE);
    NAOS_CHECK_STR_EQ(argx_command_description(root), "Second.");

    destroy_root(root);
}

NAOS_SUITE(create,
    NAOS_ADD_TEST(a_new_root_is_empty),
    NAOS_ADD_TEST(a_root_takes_any_name_that_is_not_empty),
    NAOS_ADD_TEST(an_empty_root_name_is_an_invalid_name),
    NAOS_ADD_TEST(the_name_of_a_root_is_copied)
)

NAOS_SUITE(subcommand,
    NAOS_ADD_TEST(subcommands_nest_and_are_read_in_the_order_of_the_definition),
    NAOS_ADD_TEST(handles_stay_valid_while_the_tree_grows),
    NAOS_ADD_TEST(find_subcommand_looks_in_the_command_itself_only),
    NAOS_ADD_TEST(the_name_of_a_subcommand_is_copied),
    NAOS_ADD_TEST(a_subcommand_can_be_added_without_an_output),
    NAOS_ADD_TEST(a_subcommand_name_follows_the_rule_of_names),
    NAOS_ADD_TEST(a_subcommand_name_is_used_once_in_a_command),
    NAOS_ADD_TEST(names_conflict_only_bytewise_and_only_in_one_command),
    NAOS_ADD_TEST(subcommands_nest_deeply)
)

NAOS_SUITE(description,
    NAOS_ADD_TEST(a_description_is_set_and_replaced),
    NAOS_ADD_TEST(a_description_is_copied)
)

NAOS_SUITE(destroy,
    NAOS_ADD_TEST(destroy_does_nothing_for_null),
    NAOS_ADD_TEST(destroy_frees_the_whole_tree)
)

NAOS_SUITE(failure,
    NAOS_ADD_TEST(creation_frees_everything_when_an_allocation_fails),
    NAOS_ADD_TEST(a_failed_addition_leaves_the_tree_as_it_was),
    NAOS_ADD_TEST(a_failed_description_keeps_the_old_one)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(create),
    NAOS_ADD_SUITE(subcommand),
    NAOS_ADD_SUITE(description),
    NAOS_ADD_SUITE(destroy),
    NAOS_ADD_SUITE(failure)
)
