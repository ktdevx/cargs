/*
 * Tests of the public validation API: clargs_command_validate() and
 * clargs_command_is_validated() of <clargs/schema.h>.
 *
 * Trees are built the way a consumer does it and validated. The tests cover
 * the rules of the validation, the default values that it converts with
 * their value types, where it reports a fault, the order in which it finds
 * faults, and the state of the tree: validated by a validation
 * that succeeds, back to not validated by every addition and every set that
 * succeeds, and unchanged by one that fails.
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
#include <string.h>

/* The arities, under short names for the tables of orders. */
#define REQ CLARGS_ARITY_REQUIRED
#define OPT CLARGS_ARITY_OPTIONAL
#define ANY CLARGS_ARITY_ZERO_OR_MORE
#define MANY CLARGS_ARITY_ONE_OR_MORE

/*
 * The end of every test: the test allocator has no live block and saw no
 * violation of the allocator contract.
 */
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

/* Additions that must succeed. Each returns what it added. */
static clargs_command_t *add_sub(clargs_command_t *command, const char *name)
{
    clargs_command_t *sub = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_subcommand(command, name, &sub) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(sub != NULL);
    return sub;
}

static clargs_option_t *add_flag(clargs_command_t *command, char short_name, const char *long_name)
{
    clargs_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_flag(command, short_name, long_name, &option) == CLARGS_ERROR_NONE);
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

static clargs_option_t *add_value(clargs_command_t *command, char short_name, const char *long_name)
{
    clargs_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_value(command, short_name, long_name, clargs_type_string(),
                                               &option) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(option != NULL);
    return option;
}

static clargs_positional_t *add_positional(clargs_command_t *command, const char *name,
                                           clargs_arity_t arity)
{
    clargs_positional_t *positional = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_positional(command, name, clargs_type_string(), arity,
                                                    &positional) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(positional != NULL);
    return positional;
}

/*
 * Makes option, an option that takes a value, faulty: required and with a
 * default value.
 */
static void make_faulty(clargs_option_t *option)
{
    NAOS_REQUIRE_TRUE(clargs_option_set_required(option, true) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_option_set_default(option, "value") == CLARGS_ERROR_NONE);
}

/*
 * Validates the tree of root with every combination of outputs that are
 * given and outputs that are left out, and checks each time that the result
 * is expected and that every output that is given holds what is expected.
 * The three expected handles are NULL for a validation that succeeds.
 *
 * Each output holds a handle of another tree before the call, so an output
 * that the validation does not write does not pass.
 */
static void check_validate(clargs_command_t *root, clargs_error_t expected,
                           const clargs_command_t *expected_command,
                           const clargs_option_t *expected_option,
                           const clargs_positional_t *expected_positional)
{
    clargs_command_t *other = NULL;
    clargs_option_t *other_option;
    clargs_positional_t *other_positional;
    unsigned mask;

    NAOS_REQUIRE_TRUE(clargs_command_create("other", &other) == CLARGS_ERROR_NONE);
    other_option = add_flag(other, 's', "sentinel");
    other_positional = add_positional(other, "sentinel", CLARGS_ARITY_REQUIRED);

    for (mask = 0; mask < 8; ++mask) {
        const clargs_command_t *command = other;
        const clargs_option_t *option = other_option;
        const clargs_positional_t *positional = other_positional;
        const bool with_command = (mask & 1u) != 0;
        const bool with_option = (mask & 2u) != 0;
        const bool with_positional = (mask & 4u) != 0;

        NAOS_CHECK_INT_EQ(clargs_command_validate(root, with_command ? &command : NULL,
                                                  with_option ? &option : NULL,
                                                  with_positional ? &positional : NULL),
                          expected);

        if (with_command) {
            NAOS_CHECK_TRUE(command == expected_command);
        }
        if (with_option) {
            NAOS_CHECK_TRUE(option == expected_option);
        }
        if (with_positional) {
            NAOS_CHECK_TRUE(positional == expected_positional);
        }
    }

    clargs_command_destroy(other);
}

/* ---- The state of a new tree, and validations that succeed --------------- */

NAOS_TEST(a_new_root_is_not_validated)
{
    clargs_command_t *root = create_root();

    NAOS_CHECK_FALSE(clargs_command_is_validated(root));

    destroy_root(root);
}

NAOS_TEST(a_new_subcommand_reads_a_tree_that_is_not_validated)
{
    clargs_command_t *root = create_root();
    clargs_command_t *sub = add_sub(root, "run");
    clargs_command_t *deep = add_sub(sub, "now");

    NAOS_CHECK_FALSE(clargs_command_is_validated(root));
    NAOS_CHECK_FALSE(clargs_command_is_validated(sub));
    NAOS_CHECK_FALSE(clargs_command_is_validated(deep));

    destroy_root(root);
}

NAOS_TEST(an_empty_root_is_valid)
{
    clargs_command_t *root = create_root();
    const size_t requests = clargs_test_allocator_requests();
    const clargs_command_t *command = root;

    NAOS_CHECK_INT_EQ(clargs_command_validate(root, &command, NULL, NULL), CLARGS_ERROR_NONE);
    NAOS_CHECK_TRUE(command == NULL);
    NAOS_CHECK_TRUE(clargs_command_is_validated(root));

    /* A tree without a default value is validated without an allocation. */
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests);

    /* The root is as empty as it was. */
    NAOS_CHECK_STR_EQ(clargs_command_name(root), "app");
    NAOS_CHECK_UINT_EQ(clargs_command_option_count(root), 0);
    NAOS_CHECK_UINT_EQ(clargs_command_positional_count(root), 0);
    NAOS_CHECK_UINT_EQ(clargs_command_subcommand_count(root), 0);

    destroy_root(root);
}

NAOS_TEST(an_empty_root_is_valid_with_every_combination_of_outputs)
{
    clargs_command_t *root = create_root();

    check_validate(root, CLARGS_ERROR_NONE, NULL, NULL, NULL);
    NAOS_CHECK_TRUE(clargs_command_is_validated(root));

    destroy_root(root);
}

/* A tree that uses everything a definition can hold and has no fault. */
typedef struct full_tree {
    clargs_command_t *root;
    clargs_command_t *build;
    clargs_command_t *remote;
    clargs_command_t *add;
    clargs_command_t *remove;
    clargs_option_t *verbose;
    clargs_option_t *color;
    clargs_option_t *config;
    clargs_option_t *jobs;
    clargs_option_t *token;
    clargs_positional_t *target;
    clargs_positional_t *profile;
    clargs_positional_t *names;
} full_tree_t;

static void build_full_tree(full_tree_t *tree)
{
    tree->root = new_root();
    NAOS_REQUIRE_TRUE(clargs_command_set_description(tree->root, "An application.") == CLARGS_ERROR_NONE);
    tree->verbose = add_flag(tree->root, 'v', "verbose");
    tree->color = add_negatable(tree->root, '\0', "color");
    NAOS_REQUIRE_TRUE(clargs_option_set_initial(tree->color, true) == CLARGS_ERROR_NONE);

    /* An option with a default value that is not required. */
    tree->config = add_value(tree->root, 'c', "config");
    NAOS_REQUIRE_TRUE(clargs_option_set_default(tree->config, "app.conf") == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_option_set_value_name(tree->config, "FILE") == CLARGS_ERROR_NONE);

    tree->build = add_sub(tree->root, "build");
    NAOS_REQUIRE_TRUE(clargs_command_add_count(tree->build, 'q', "quiet", NULL) == CLARGS_ERROR_NONE);
    tree->jobs = add_value(tree->build, 'j', "jobs");
    tree->target = add_positional(tree->build, "target", CLARGS_ARITY_REQUIRED);
    tree->profile = add_positional(tree->build, "profile", CLARGS_ARITY_OPTIONAL);
    NAOS_REQUIRE_TRUE(clargs_positional_set_default(tree->profile, "debug") == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_command_add_positional(tree->build, "rest", clargs_type_string(),
                                                    CLARGS_ARITY_ZERO_OR_MORE,
                                                    NULL) == CLARGS_ERROR_NONE);

    tree->remote = add_sub(tree->root, "remote");
    tree->add = add_sub(tree->remote, "add");

    /* An option that is required and has no default value. */
    tree->token = add_value(tree->add, 't', "token");
    NAOS_REQUIRE_TRUE(clargs_option_set_required(tree->token, true) == CLARGS_ERROR_NONE);
    tree->names = add_positional(tree->add, "names", CLARGS_ARITY_ONE_OR_MORE);
    tree->remove = add_sub(tree->remote, "remove");
}

/* Checks that the tree holds what build_full_tree() put in it. */
static void check_full_tree(const full_tree_t *tree)
{
    NAOS_CHECK_STR_EQ(clargs_command_name(tree->root), "app");
    NAOS_CHECK_STR_EQ(clargs_command_description(tree->root), "An application.");
    NAOS_REQUIRE_TRUE(clargs_command_option_count(tree->root) == 3);
    NAOS_CHECK_TRUE(clargs_command_option_at(tree->root, 0) == tree->verbose);
    NAOS_CHECK_TRUE(clargs_command_option_at(tree->root, 1) == tree->color);
    NAOS_CHECK_TRUE(clargs_command_option_at(tree->root, 2) == tree->config);
    NAOS_CHECK_UINT_EQ(clargs_command_positional_count(tree->root), 0);
    NAOS_REQUIRE_TRUE(clargs_command_subcommand_count(tree->root) == 2);
    NAOS_CHECK_TRUE(clargs_command_subcommand_at(tree->root, 0) == tree->build);
    NAOS_CHECK_TRUE(clargs_command_subcommand_at(tree->root, 1) == tree->remote);

    NAOS_CHECK_TRUE(clargs_option_initial(tree->color));
    NAOS_CHECK_STR_EQ(clargs_option_default(tree->config), "app.conf");
    NAOS_CHECK_STR_EQ(clargs_option_value_name(tree->config), "FILE");
    NAOS_CHECK_FALSE(clargs_option_is_required(tree->config));

    NAOS_CHECK_UINT_EQ(clargs_command_option_count(tree->build), 2);
    NAOS_REQUIRE_TRUE(clargs_command_positional_count(tree->build) == 3);
    NAOS_CHECK_TRUE(clargs_command_positional_at(tree->build, 0) == tree->target);
    NAOS_CHECK_TRUE(clargs_command_positional_at(tree->build, 1) == tree->profile);
    NAOS_CHECK_INT_EQ(clargs_positional_arity(tree->target), CLARGS_ARITY_REQUIRED);
    NAOS_CHECK_INT_EQ(clargs_positional_arity(tree->profile), CLARGS_ARITY_OPTIONAL);
    NAOS_CHECK_STR_EQ(clargs_positional_default(tree->profile), "debug");
    NAOS_CHECK_TRUE(clargs_option_default(tree->jobs) == NULL);

    NAOS_REQUIRE_TRUE(clargs_command_subcommand_count(tree->remote) == 2);
    NAOS_CHECK_TRUE(clargs_command_subcommand_at(tree->remote, 0) == tree->add);
    NAOS_CHECK_TRUE(clargs_command_subcommand_at(tree->remote, 1) == tree->remove);
    NAOS_CHECK_TRUE(clargs_option_is_required(tree->token));
    NAOS_CHECK_TRUE(clargs_option_default(tree->token) == NULL);
    NAOS_REQUIRE_TRUE(clargs_command_positional_count(tree->add) == 1);
    NAOS_CHECK_TRUE(clargs_command_positional_at(tree->add, 0) == tree->names);
    NAOS_CHECK_UINT_EQ(clargs_command_option_count(tree->remove), 0);
}

/* Checks that every command of the tree reads the state expected. */
static void check_full_tree_state(const full_tree_t *tree, bool expected)
{
    NAOS_CHECK_TRUE(clargs_command_is_validated(tree->root) == expected);
    NAOS_CHECK_TRUE(clargs_command_is_validated(tree->build) == expected);
    NAOS_CHECK_TRUE(clargs_command_is_validated(tree->remote) == expected);
    NAOS_CHECK_TRUE(clargs_command_is_validated(tree->add) == expected);
    NAOS_CHECK_TRUE(clargs_command_is_validated(tree->remove) == expected);
}

NAOS_TEST(a_tree_without_a_fault_is_valid_and_reads_validated_from_every_command)
{
    full_tree_t tree;

    clargs_test_allocator_install();
    build_full_tree(&tree);
    check_full_tree(&tree);
    check_full_tree_state(&tree, false);

    NAOS_CHECK_INT_EQ(clargs_command_validate(tree.root, NULL, NULL, NULL), CLARGS_ERROR_NONE);

    check_full_tree_state(&tree, true);

    /* The validation changes nothing of the definition. */
    check_full_tree(&tree);

    destroy_root(tree.root);
}

NAOS_TEST(a_validation_that_succeeds_writes_null_to_the_outputs)
{
    full_tree_t tree;

    clargs_test_allocator_install();
    build_full_tree(&tree);

    check_validate(tree.root, CLARGS_ERROR_NONE, NULL, NULL, NULL);
    check_full_tree_state(&tree, true);
    check_full_tree(&tree);

    destroy_root(tree.root);
}

NAOS_TEST(a_valid_tree_can_be_validated_again)
{
    full_tree_t tree;
    int round;

    clargs_test_allocator_install();
    build_full_tree(&tree);

    for (round = 0; round < 3; ++round) {
        const clargs_command_t *command = tree.root;
        const clargs_option_t *option = tree.verbose;
        const clargs_positional_t *positional = tree.target;

        NAOS_CHECK_INT_EQ(clargs_command_validate(tree.root, &command, &option, &positional),
                          CLARGS_ERROR_NONE);
        NAOS_CHECK_TRUE(command == NULL);
        NAOS_CHECK_TRUE(option == NULL);
        NAOS_CHECK_TRUE(positional == NULL);
        check_full_tree_state(&tree, true);
    }

    destroy_root(tree.root);
}

/* ---- The order of the positionals ---------------------------------------- */

#define ORDER_MAX 5

/*
 * The arities of the positionals of one command, in the order of the
 * definition, and the index of the positional that the validation reports;
 * count for an order without a fault.
 */
typedef struct order {
    size_t count;
    clargs_arity_t arities[ORDER_MAX];
    size_t faulty;
} order_t;

/*
 * Builds a tree in which one command has positionals of the arities of
 * order, validates it and checks the result. The command is the root, or,
 * with in_subcommand, a subcommand two levels below the root.
 */
static void check_order(const order_t *order, bool in_subcommand)
{
    static const char *const names[ORDER_MAX] = { "p0", "p1", "p2", "p3", "p4" };
    clargs_positional_t *positionals[ORDER_MAX];
    clargs_command_t *root = new_root();
    clargs_command_t *command = root;
    size_t index;

    NAOS_REQUIRE_TRUE(order->count <= ORDER_MAX);

    if (in_subcommand) {
        (void)add_sub(root, "first");
        command = add_sub(add_sub(root, "group"), "leaf");
    }

    /* An option without a fault does not change the result. */
    (void)add_value(command, 'o', "output");

    for (index = 0; index < order->count; ++index) {
        positionals[index] = add_positional(command, names[index], order->arities[index]);
    }

    if (order->faulty == order->count) {
        check_validate(root, CLARGS_ERROR_NONE, NULL, NULL, NULL);
        NAOS_CHECK_TRUE(clargs_command_is_validated(root));
        NAOS_CHECK_TRUE(clargs_command_is_validated(command));
    } else {
        NAOS_REQUIRE_TRUE(order->faulty < order->count);
        check_validate(root, CLARGS_ERROR_INVALID_DEFINITION, command, NULL, positionals[order->faulty]);
        NAOS_CHECK_FALSE(clargs_command_is_validated(root));
        NAOS_CHECK_FALSE(clargs_command_is_validated(command));
    }

    /* The positionals are as they were defined. */
    NAOS_REQUIRE_TRUE(clargs_command_positional_count(command) == order->count);
    for (index = 0; index < order->count; ++index) {
        NAOS_CHECK_TRUE(clargs_command_positional_at(command, index) == positionals[index]);
        NAOS_CHECK_INT_EQ(clargs_positional_arity(positionals[index]), order->arities[index]);
        NAOS_CHECK_STR_EQ(clargs_positional_name(positionals[index]), names[index]);
    }

    clargs_command_destroy(root);
}

/* Checks every order of the table, in a root and in a subcommand. */
static void check_orders(const order_t *orders, size_t count)
{
    size_t index;

    clargs_test_allocator_install();

    for (index = 0; index < count; ++index) {
        check_order(&orders[index], false);
        check_order(&orders[index], true);
    }

    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(the_orders_that_a_command_line_can_follow_are_valid)
{
    static const order_t orders[] = {
        { 0, { REQ }, 0 },
        { 1, { REQ }, 1 },
        { 1, { OPT }, 1 },
        { 1, { ANY }, 1 },
        { 1, { MANY }, 1 },
        { 2, { REQ, REQ }, 2 },
        { 2, { REQ, OPT }, 2 },
        { 2, { OPT, OPT }, 2 },
        { 2, { REQ, ANY }, 2 },
        { 2, { OPT, ANY }, 2 },
        { 2, { REQ, MANY }, 2 },
        { 3, { REQ, REQ, MANY }, 3 },
        { 5, { REQ, REQ, OPT, OPT, ANY }, 5 },
    };

    check_orders(orders, sizeof(orders) / sizeof(orders[0]));
}

/*
 * A positional that takes none or one argument, or any number, is followed
 * by one that needs an argument.
 */
NAOS_TEST(a_required_positional_after_an_optional_one_is_an_inconsistent_definition)
{
    static const order_t orders[] = {
        { 2, { OPT, REQ }, 1 },
        { 2, { OPT, MANY }, 1 },
        { 2, { ANY, REQ }, 1 },
        { 2, { ANY, MANY }, 1 },
        { 3, { REQ, OPT, REQ }, 2 },
        { 4, { REQ, OPT, OPT, MANY }, 3 },
        { 3, { OPT, OPT, REQ }, 2 },
    };

    check_orders(orders, sizeof(orders) / sizeof(orders[0]));
}

/*
 * A positional that takes any number of arguments is followed by another
 * positional.
 */
NAOS_TEST(a_positional_after_a_variadic_one_is_an_inconsistent_definition)
{
    static const order_t orders[] = {
        { 2, { ANY, OPT }, 1 },
        { 2, { ANY, ANY }, 1 },
        { 2, { MANY, REQ }, 1 },
        { 2, { MANY, OPT }, 1 },
        { 2, { MANY, ANY }, 1 },
        { 2, { MANY, MANY }, 1 },
        { 4, { REQ, OPT, ANY, OPT }, 3 },
        { 3, { REQ, MANY, OPT }, 2 },
    };

    check_orders(orders, sizeof(orders) / sizeof(orders[0]));
}

NAOS_TEST(the_first_faulty_positional_of_a_command_is_reported)
{
    static const order_t orders[] = {
        { 4, { OPT, REQ, ANY, OPT }, 1 },
        { 3, { ANY, OPT, REQ }, 1 },
        { 4, { REQ, MANY, OPT, REQ }, 2 },
        { 5, { REQ, OPT, REQ, ANY, OPT }, 2 },
    };

    check_orders(orders, sizeof(orders) / sizeof(orders[0]));
}

/* ---- An option that is required and has a default value ------------------ */

NAOS_TEST(a_required_option_with_a_default_value_is_an_inconsistent_definition)
{
    clargs_command_t *root = create_root();
    clargs_option_t *output;

    (void)add_flag(root, 'v', "verbose");
    output = add_value(root, 'o', "output");
    NAOS_REQUIRE_TRUE(clargs_option_set_required(output, true) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_option_set_default(output, "out.txt") == CLARGS_ERROR_NONE);

    check_validate(root, CLARGS_ERROR_INVALID_DEFINITION, root, output, NULL);
    NAOS_CHECK_FALSE(clargs_command_is_validated(root));

    /* The option is as it was defined. */
    NAOS_CHECK_TRUE(clargs_option_is_required(output));
    NAOS_CHECK_STR_EQ(clargs_option_default(output), "out.txt");
    NAOS_CHECK_UINT_EQ(clargs_command_option_count(root), 2);

    destroy_root(root);
}

NAOS_TEST(the_fault_is_the_same_when_the_default_value_is_set_first)
{
    clargs_command_t *root = create_root();
    clargs_option_t *output = add_value(root, '\0', "output");

    NAOS_REQUIRE_TRUE(clargs_option_set_default(output, "") == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_option_set_required(output, true) == CLARGS_ERROR_NONE);

    check_validate(root, CLARGS_ERROR_INVALID_DEFINITION, root, output, NULL);
    NAOS_CHECK_FALSE(clargs_command_is_validated(root));

    destroy_root(root);
}

NAOS_TEST(required_alone_and_a_default_value_alone_are_valid)
{
    clargs_command_t *root = create_root();
    clargs_option_t *required = add_value(root, 'r', "required");
    clargs_option_t *defaulted = add_value(root, 'd', "defaulted");
    clargs_option_t *neither = add_value(root, 'n', "neither");

    NAOS_REQUIRE_TRUE(clargs_option_set_required(required, true) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_option_set_default(defaulted, "value") == CLARGS_ERROR_NONE);

    /* Required was set and taken back: the default value stays alone. */
    NAOS_REQUIRE_TRUE(clargs_option_set_required(neither, true) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_option_set_default(neither, "value") == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_option_set_required(neither, false) == CLARGS_ERROR_NONE);

    check_validate(root, CLARGS_ERROR_NONE, NULL, NULL, NULL);
    NAOS_CHECK_TRUE(clargs_command_is_validated(root));

    destroy_root(root);
}

NAOS_TEST(the_first_faulty_option_of_a_command_is_reported)
{
    clargs_command_t *root = create_root();
    clargs_option_t *first;
    clargs_option_t *second;
    clargs_option_t *third;

    (void)add_value(root, 'a', "fine");
    first = add_value(root, 'b', "first");
    second = add_value(root, 'c', "second");
    third = add_value(root, 'd', "third");

    /* The faults are made in another order than the options were added. */
    make_faulty(third);
    make_faulty(second);
    check_validate(root, CLARGS_ERROR_INVALID_DEFINITION, root, second, NULL);

    make_faulty(first);
    check_validate(root, CLARGS_ERROR_INVALID_DEFINITION, root, first, NULL);

    destroy_root(root);
}

/* ---- Faults anywhere in the tree, and which one is reported -------------- */

NAOS_TEST(a_faulty_option_in_a_subcommand_is_found)
{
    clargs_command_t *root = create_root();
    clargs_command_t *remote = add_sub(root, "remote");
    clargs_command_t *add = add_sub(remote, "add");
    clargs_command_t *now = add_sub(add, "now");
    clargs_option_t *token;

    (void)add_flag(root, 'v', "verbose");
    (void)add_value(remote, 'u', "url");
    token = add_value(now, 't', "token");
    make_faulty(token);

    check_validate(root, CLARGS_ERROR_INVALID_DEFINITION, now, token, NULL);
    NAOS_CHECK_FALSE(clargs_command_is_validated(root));
    NAOS_CHECK_FALSE(clargs_command_is_validated(now));

    /*
     * The handles that are reported are those of the tree: they read as
     * the definition was made.
     */
    NAOS_CHECK_TRUE(clargs_option_command(token) == now);
    NAOS_CHECK_STR_EQ(clargs_command_name(now), "now");
    NAOS_CHECK_STR_EQ(clargs_option_long_name(token), "token");

    destroy_root(root);
}

NAOS_TEST(a_faulty_positional_in_a_subcommand_is_found)
{
    clargs_command_t *root = create_root();
    clargs_command_t *build = add_sub(root, "build");
    clargs_command_t *remote = add_sub(root, "remote");
    clargs_command_t *add = add_sub(remote, "add");
    clargs_positional_t *name;

    (void)add_positional(build, "target", CLARGS_ARITY_REQUIRED);
    (void)add_positional(add, "urls", CLARGS_ARITY_ZERO_OR_MORE);
    name = add_positional(add, "name", CLARGS_ARITY_REQUIRED);

    check_validate(root, CLARGS_ERROR_INVALID_DEFINITION, add, NULL, name);
    NAOS_CHECK_FALSE(clargs_command_is_validated(root));
    NAOS_CHECK_FALSE(clargs_command_is_validated(add));
    NAOS_CHECK_TRUE(clargs_positional_command(name) == add);

    destroy_root(root);
}

/*
 * In one command, the positionals are checked before the options, whatever
 * was added first.
 */
NAOS_TEST(a_faulty_positional_is_reported_before_a_faulty_option_of_the_same_command)
{
    clargs_command_t *root = create_root();
    clargs_option_t *output = add_value(root, 'o', "output");
    clargs_positional_t *second;

    make_faulty(output);
    (void)add_positional(root, "first", CLARGS_ARITY_OPTIONAL);
    second = add_positional(root, "second", CLARGS_ARITY_REQUIRED);

    check_validate(root, CLARGS_ERROR_INVALID_DEFINITION, root, NULL, second);

    destroy_root(root);
}

/* A command is checked before its subcommands. */
NAOS_TEST(a_fault_of_a_command_is_reported_before_a_fault_of_its_subcommands)
{
    clargs_command_t *root = create_root();
    clargs_command_t *run = add_sub(root, "run");
    clargs_command_t *now = add_sub(run, "now");
    clargs_option_t *root_option;
    clargs_option_t *run_option;

    /* The faults are made from the deepest command up. */
    (void)add_positional(now, "first", CLARGS_ARITY_ZERO_OR_MORE);
    (void)add_positional(now, "second", CLARGS_ARITY_REQUIRED);
    run_option = add_value(run, 'o', "output");
    make_faulty(run_option);
    root_option = add_value(root, 'o', "output");
    make_faulty(root_option);

    check_validate(root, CLARGS_ERROR_INVALID_DEFINITION, root, root_option, NULL);

    /*
     * With the fault of the root taken away, the next one in the order is
     * reported.
     */
    NAOS_REQUIRE_TRUE(clargs_option_set_required(root_option, false) == CLARGS_ERROR_NONE);
    check_validate(root, CLARGS_ERROR_INVALID_DEFINITION, run, run_option, NULL);

    NAOS_REQUIRE_TRUE(clargs_option_set_required(run_option, false) == CLARGS_ERROR_NONE);
    check_validate(root, CLARGS_ERROR_INVALID_DEFINITION, now, NULL, clargs_command_positional_at(now, 1));

    destroy_root(root);
}

/*
 * The subcommands are checked in the order of their definition, and each
 * with everything below it before the next.
 */
NAOS_TEST(the_subcommands_are_checked_in_the_order_of_their_definition)
{
    clargs_command_t *root = create_root();
    clargs_command_t *first = add_sub(root, "first");
    clargs_command_t *second = add_sub(root, "second");
    clargs_command_t *third = add_sub(root, "third");
    clargs_command_t *below_first = add_sub(first, "below");
    clargs_option_t *in_second;
    clargs_option_t *in_third;
    clargs_option_t *in_below_first;

    /* The faults are made in the reverse of the order of the check. */
    in_third = add_value(third, 'o', "output");
    make_faulty(in_third);
    check_validate(root, CLARGS_ERROR_INVALID_DEFINITION, third, in_third, NULL);

    in_second = add_value(second, 'o', "output");
    make_faulty(in_second);
    check_validate(root, CLARGS_ERROR_INVALID_DEFINITION, second, in_second, NULL);

    /*
     * A command below the first subcommand comes before the second
     * subcommand.
     */
    in_below_first = add_value(below_first, 'o', "output");
    make_faulty(in_below_first);
    check_validate(root, CLARGS_ERROR_INVALID_DEFINITION, below_first, in_below_first, NULL);

    destroy_root(root);
}

NAOS_TEST(the_same_definition_gives_the_same_result_every_time)
{
    clargs_command_t *root = create_root();
    clargs_command_t *run = add_sub(root, "run");
    clargs_option_t *output = add_value(run, 'o', "output");
    clargs_positional_t *second;
    int round;

    make_faulty(output);
    (void)add_positional(run, "first", CLARGS_ARITY_ONE_OR_MORE);
    second = add_positional(run, "second", CLARGS_ARITY_OPTIONAL);

    for (round = 0; round < 3; ++round) {
        const clargs_command_t *command = NULL;
        const clargs_option_t *option = output;
        const clargs_positional_t *positional = NULL;

        NAOS_CHECK_INT_EQ(clargs_command_validate(root, &command, &option, &positional),
                          CLARGS_ERROR_INVALID_DEFINITION);
        NAOS_CHECK_TRUE(command == run);
        NAOS_CHECK_TRUE(option == NULL);
        NAOS_CHECK_TRUE(positional == second);
        NAOS_CHECK_FALSE(clargs_command_is_validated(root));
        NAOS_CHECK_FALSE(clargs_command_is_validated(run));
    }

    destroy_root(root);
}

/* ---- The state after a validation that fails ----------------------------- */

NAOS_TEST(a_validation_that_fails_leaves_the_tree_not_validated_and_unchanged)
{
    full_tree_t tree;
    clargs_option_t *faulty;

    clargs_test_allocator_install();
    build_full_tree(&tree);
    faulty = add_value(tree.remove, 'k', "key");
    make_faulty(faulty);

    NAOS_CHECK_INT_EQ(clargs_command_validate(tree.root, NULL, NULL, NULL),
                      CLARGS_ERROR_INVALID_DEFINITION);
    check_full_tree_state(&tree, false);

    /* The definition is as it was, the fault included. */
    NAOS_CHECK_TRUE(clargs_option_is_required(faulty));
    NAOS_CHECK_STR_EQ(clargs_option_default(faulty), "value");
    NAOS_REQUIRE_TRUE(clargs_command_option_count(tree.remove) == 1);
    NAOS_CHECK_TRUE(clargs_command_option_at(tree.remove, 0) == faulty);
    NAOS_REQUIRE_TRUE(clargs_option_set_required(faulty, false) == CLARGS_ERROR_NONE);

    /* Without the fault, the same tree is valid. */
    NAOS_CHECK_INT_EQ(clargs_command_validate(tree.root, NULL, NULL, NULL), CLARGS_ERROR_NONE);
    check_full_tree_state(&tree, true);

    destroy_root(tree.root);
}

/*
 * A fault that is added to a validated tree: the addition takes the state
 * back, and the validation that fails leaves it there.
 */
NAOS_TEST(a_fault_that_is_added_to_a_validated_tree_is_found)
{
    full_tree_t tree;
    clargs_positional_t *late;

    clargs_test_allocator_install();
    build_full_tree(&tree);
    NAOS_REQUIRE_TRUE(clargs_command_validate(tree.root, NULL, NULL, NULL) == CLARGS_ERROR_NONE);
    check_full_tree_state(&tree, true);

    /* "names" takes one argument or more; nothing can follow it. */
    late = add_positional(tree.add, "late", CLARGS_ARITY_OPTIONAL);
    check_full_tree_state(&tree, false);

    check_validate(tree.root, CLARGS_ERROR_INVALID_DEFINITION, tree.add, NULL, late);
    check_full_tree_state(&tree, false);

    destroy_root(tree.root);
}

/* ---- The state after a change -------------------------------------------- */

/* Where in the tree the command is that a change is made to. */
typedef enum location {
    AT_ROOT,
    AT_SUBCOMMAND
} location_t;

#define LOCATION_COUNT 2
static const location_t all_locations[LOCATION_COUNT] = { AT_ROOT, AT_SUBCOMMAND };

#define READER_MAX 8

/*
 * A validated tree with a command to change, target: the root, or a
 * subcommand two levels below it. The target has a flag (-f, --flag), an
 * option that takes a value (-o, --output), a negatable flag (--color) and
 * a count (-c, --count). A command has positionals or subcommands, not
 * both, so there are two shapes: the target has four positionals, of which
 * "input" (required) and "extra" (optional) are kept here, or it has four
 * subcommands, the first of them "child", and the two positionals are NULL.
 *
 * The target has four of each on purpose: an array of a command grows when
 * a fifth item is added, so every addition to the target allocates for the
 * array too, and that allocation can be made to fail.
 *
 * readers holds every command of the tree: the state is read from each.
 * (A root with positionals is the only command of its tree.)
 */
typedef struct fixture {
    clargs_command_t *root;
    clargs_command_t *target;
    const clargs_command_t *readers[READER_MAX];
    size_t reader_count;
    clargs_option_t *flag;
    clargs_option_t *value;
    clargs_option_t *negatable;
    clargs_positional_t *required;
    clargs_positional_t *optional;
} fixture_t;

static void add_reader(fixture_t *fixture, const clargs_command_t *command)
{
    NAOS_REQUIRE_TRUE(fixture->reader_count < READER_MAX);
    fixture->readers[fixture->reader_count] = command;
    ++fixture->reader_count;
}

/* Checks that every command of the tree reads the state expected. */
static void check_state(const fixture_t *fixture, bool expected)
{
    size_t index;

    for (index = 0; index < fixture->reader_count; ++index) {
        NAOS_CHECK_TRUE(clargs_command_is_validated(fixture->readers[index]) == expected);
    }
}

/* Builds the tree and validates it. The test allocator is installed. */
static void build_fixture(fixture_t *fixture, location_t location, bool with_positionals)
{
    fixture->reader_count = 0;
    fixture->required = NULL;
    fixture->optional = NULL;

    fixture->root = new_root();
    add_reader(fixture, fixture->root);

    if (location == AT_ROOT) {
        fixture->target = fixture->root;
    } else {
        clargs_command_t *middle = add_sub(fixture->root, "middle");

        fixture->target = add_sub(middle, "leaf");
        add_reader(fixture, middle);
        add_reader(fixture, fixture->target);
        add_reader(fixture, add_sub(fixture->root, "side"));
    }

    fixture->flag = add_flag(fixture->target, 'f', "flag");
    fixture->value = add_value(fixture->target, 'o', "output");
    fixture->negatable = add_negatable(fixture->target, '\0', "color");
    NAOS_REQUIRE_TRUE(clargs_command_add_count(fixture->target, 'c', "count", NULL) == CLARGS_ERROR_NONE);

    if (with_positionals) {
        fixture->required = add_positional(fixture->target, "input", CLARGS_ARITY_REQUIRED);
        (void)add_positional(fixture->target, "second", CLARGS_ARITY_REQUIRED);
        fixture->optional = add_positional(fixture->target, "extra", CLARGS_ARITY_OPTIONAL);
        (void)add_positional(fixture->target, "fourth", CLARGS_ARITY_OPTIONAL);
    } else {
        add_reader(fixture, add_sub(fixture->target, "child"));
        add_reader(fixture, add_sub(fixture->target, "second"));
        add_reader(fixture, add_sub(fixture->target, "third"));
        add_reader(fixture, add_sub(fixture->target, "fourth"));
    }

    check_state(fixture, false);
    NAOS_REQUIRE_TRUE(clargs_command_validate(fixture->root, NULL, NULL, NULL) == CLARGS_ERROR_NONE);
    check_state(fixture, true);
}

/* Every kind of change that a tree takes. */
typedef enum mutation {
    ADD_SUBCOMMAND,
    ADD_FLAG,
    ADD_NEGATABLE,
    ADD_COUNT,
    ADD_VALUE,
    ADD_POSITIONAL,
    SET_COMMAND_DESCRIPTION,
    SET_OPTION_DESCRIPTION,
    SET_OPTION_VALUE_NAME,
    SET_OPTION_REQUIRED,
    SET_OPTION_DEFAULT,
    SET_OPTION_INITIAL,
    SET_POSITIONAL_DESCRIPTION,
    SET_POSITIONAL_DEFAULT
} mutation_t;

#define MUTATION_COUNT 14
static const mutation_t all_mutations[MUTATION_COUNT] = {
    ADD_SUBCOMMAND,          ADD_FLAG,              ADD_NEGATABLE,       ADD_COUNT,
    ADD_VALUE,               ADD_POSITIONAL,        SET_COMMAND_DESCRIPTION,
    SET_OPTION_DESCRIPTION,  SET_OPTION_VALUE_NAME, SET_OPTION_REQUIRED, SET_OPTION_DEFAULT,
    SET_OPTION_INITIAL,      SET_POSITIONAL_DESCRIPTION,
    SET_POSITIONAL_DEFAULT
};

/* Whether the change is made to a target with positionals. */
static bool mutation_needs_positionals(mutation_t mutation)
{
    return mutation == ADD_POSITIONAL || mutation == SET_POSITIONAL_DESCRIPTION ||
           mutation == SET_POSITIONAL_DEFAULT;
}

/*
 * The least number of allocations of the change, each of which can fail:
 * the definition, its strings and the array it is put in for an addition,
 * the copy for a set of a string, none for a set of a truth value.
 */
static size_t mutation_allocations(mutation_t mutation)
{
    switch (mutation) {
    case ADD_SUBCOMMAND:
    case ADD_FLAG:
    case ADD_COUNT:
    case ADD_VALUE:
    case ADD_POSITIONAL:
        return 3;
    case ADD_NEGATABLE:
        return 4;
    case SET_OPTION_REQUIRED:
    case SET_OPTION_INITIAL:
        return 0;
    default:
        return 1;
    }
}

/*
 * Makes the change to the target of the fixture. Each change is one that
 * the tree takes and that leaves it without a fault.
 */
static clargs_error_t mutate(const fixture_t *fixture, mutation_t mutation)
{
    switch (mutation) {
    case ADD_SUBCOMMAND:
        return clargs_command_add_subcommand(fixture->target, "new", NULL);
    case ADD_FLAG:
        return clargs_command_add_flag(fixture->target, 'n', "new", NULL);
    case ADD_NEGATABLE:
        return clargs_command_add_negatable(fixture->target, 'n', "new", NULL);
    case ADD_COUNT:
        return clargs_command_add_count(fixture->target, 'n', "new", NULL);
    case ADD_VALUE:
        return clargs_command_add_value(fixture->target, 'n', "new", clargs_type_string(), NULL);
    case ADD_POSITIONAL:
        return clargs_command_add_positional(fixture->target, "new", clargs_type_string(),
                                             CLARGS_ARITY_OPTIONAL, NULL);
    case SET_COMMAND_DESCRIPTION:
        return clargs_command_set_description(fixture->target, "A description.");
    case SET_OPTION_DESCRIPTION:
        return clargs_option_set_description(fixture->flag, "A description.");
    case SET_OPTION_VALUE_NAME:
        return clargs_option_set_value_name(fixture->value, "FILE");
    case SET_OPTION_REQUIRED:
        return clargs_option_set_required(fixture->value, true);
    case SET_OPTION_DEFAULT:
        return clargs_option_set_default(fixture->value, "out.txt");
    case SET_OPTION_INITIAL:
        return clargs_option_set_initial(fixture->negatable, true);
    case SET_POSITIONAL_DESCRIPTION:
        return clargs_positional_set_description(fixture->required, "A description.");
    case SET_POSITIONAL_DEFAULT:
        return clargs_positional_set_default(fixture->optional, "more");
    default:
        NAOS_FAIL("a test asked for an unknown change");
        /* NAOS_FAIL does not return; nothing may follow it (MSVC C4702). */
    }
}

NAOS_TEST(every_change_that_succeeds_takes_a_validated_tree_back_to_not_validated)
{
    size_t where;
    size_t which;

    clargs_test_allocator_install();

    for (where = 0; where < LOCATION_COUNT; ++where) {
        for (which = 0; which < MUTATION_COUNT; ++which) {
            const mutation_t mutation = all_mutations[which];
            fixture_t fixture;

            build_fixture(&fixture, all_locations[where], mutation_needs_positionals(mutation));

            NAOS_CHECK_INT_EQ(mutate(&fixture, mutation), CLARGS_ERROR_NONE);
            check_state(&fixture, false);

            /*
             * The tree has no fault: it is validated again, and only by a
             * validation.
             */
            NAOS_CHECK_INT_EQ(clargs_command_validate(fixture.root, NULL, NULL, NULL), CLARGS_ERROR_NONE);
            check_state(&fixture, true);

            clargs_command_destroy(fixture.root);
        }
    }

    clargs_test_allocator_uninstall();
    check_clean();
}

/*
 * A set that succeeds takes the state back even when the value it sets is
 * the value that was there.
 */
NAOS_TEST(a_set_of_the_same_value_takes_a_validated_tree_back_to_not_validated)
{
    size_t where;

    clargs_test_allocator_install();

    for (where = 0; where < LOCATION_COUNT; ++where) {
        fixture_t fixture;

        build_fixture(&fixture, all_locations[where], false);

        NAOS_CHECK_INT_EQ(clargs_option_set_required(fixture.value, false), CLARGS_ERROR_NONE);
        check_state(&fixture, false);
        NAOS_REQUIRE_TRUE(clargs_command_validate(fixture.root, NULL, NULL, NULL) == CLARGS_ERROR_NONE);

        NAOS_CHECK_INT_EQ(clargs_option_set_initial(fixture.negatable, false), CLARGS_ERROR_NONE);
        check_state(&fixture, false);
        NAOS_REQUIRE_TRUE(clargs_command_validate(fixture.root, NULL, NULL, NULL) == CLARGS_ERROR_NONE);

        NAOS_REQUIRE_TRUE(clargs_option_set_description(fixture.flag, "same") == CLARGS_ERROR_NONE);
        NAOS_REQUIRE_TRUE(clargs_command_validate(fixture.root, NULL, NULL, NULL) == CLARGS_ERROR_NONE);
        NAOS_CHECK_INT_EQ(clargs_option_set_description(fixture.flag, "same"), CLARGS_ERROR_NONE);
        check_state(&fixture, false);

        clargs_command_destroy(fixture.root);
    }

    clargs_test_allocator_uninstall();
    check_clean();
}

/* A change in one tree does not touch the state of another tree. */
NAOS_TEST(a_change_takes_back_the_state_of_its_own_tree_only)
{
    fixture_t changed;
    fixture_t untouched;

    clargs_test_allocator_install();
    build_fixture(&changed, AT_SUBCOMMAND, false);
    build_fixture(&untouched, AT_SUBCOMMAND, false);

    NAOS_CHECK_INT_EQ(mutate(&changed, ADD_FLAG), CLARGS_ERROR_NONE);
    check_state(&changed, false);
    check_state(&untouched, true);

    clargs_command_destroy(changed.root);
    clargs_command_destroy(untouched.root);
    clargs_test_allocator_uninstall();
    check_clean();
}

/* Every kind of change that a tree refuses because of the definition. */
typedef enum refusal {
    /* A name that is not one. */
    SUBCOMMAND_WITH_A_WRONG_NAME,
    OPTION_WITHOUT_A_NAME,
    FLAG_WITH_A_WRONG_SHORT_NAME,
    NEGATABLE_WITHOUT_A_LONG_NAME,
    COUNT_WITH_A_WRONG_LONG_NAME,
    VALUE_WITH_A_WRONG_LONG_NAME,
    POSITIONAL_WITH_AN_EMPTY_NAME,
    /* A name that is in use. */
    SUBCOMMAND_WITH_A_NAME_IN_USE,
    FLAG_WITH_A_SHORT_NAME_IN_USE,
    COUNT_WITH_A_LONG_NAME_IN_USE,
    VALUE_WITH_A_LONG_NAME_IN_USE,
    NEGATABLE_WITH_A_LONG_NAME_IN_USE,
    POSITIONAL_WITH_A_NAME_IN_USE,
    /* A definition that does not fit. */
    SUBCOMMAND_NEXT_TO_POSITIONALS,
    POSITIONAL_NEXT_TO_SUBCOMMANDS,
    VALUE_NAME_OF_A_FLAG,
    REQUIRED_OF_A_FLAG,
    DEFAULT_OF_A_FLAG,
    INITIAL_OF_A_VALUE,
    DEFAULT_OF_A_REQUIRED_POSITIONAL
} refusal_t;

#define REFUSAL_COUNT 20
static const refusal_t all_refusals[REFUSAL_COUNT] = {
    SUBCOMMAND_WITH_A_WRONG_NAME,   OPTION_WITHOUT_A_NAME,
    FLAG_WITH_A_WRONG_SHORT_NAME,   NEGATABLE_WITHOUT_A_LONG_NAME,
    COUNT_WITH_A_WRONG_LONG_NAME,   VALUE_WITH_A_WRONG_LONG_NAME,
    POSITIONAL_WITH_AN_EMPTY_NAME,  SUBCOMMAND_WITH_A_NAME_IN_USE,
    FLAG_WITH_A_SHORT_NAME_IN_USE,  COUNT_WITH_A_LONG_NAME_IN_USE,
    VALUE_WITH_A_LONG_NAME_IN_USE,  NEGATABLE_WITH_A_LONG_NAME_IN_USE,
    POSITIONAL_WITH_A_NAME_IN_USE,  SUBCOMMAND_NEXT_TO_POSITIONALS,
    POSITIONAL_NEXT_TO_SUBCOMMANDS, VALUE_NAME_OF_A_FLAG,
    REQUIRED_OF_A_FLAG,             DEFAULT_OF_A_FLAG,
    INITIAL_OF_A_VALUE,             DEFAULT_OF_A_REQUIRED_POSITIONAL
};

/* Whether the change is tried on a target with positionals. */
static bool refusal_needs_positionals(refusal_t refusal)
{
    return refusal == POSITIONAL_WITH_AN_EMPTY_NAME || refusal == POSITIONAL_WITH_A_NAME_IN_USE ||
           refusal == SUBCOMMAND_NEXT_TO_POSITIONALS || refusal == DEFAULT_OF_A_REQUIRED_POSITIONAL;
}

/* The error code that the change is refused with. */
static clargs_error_t refusal_error(refusal_t refusal)
{
    switch (refusal) {
    case SUBCOMMAND_WITH_A_WRONG_NAME:
    case OPTION_WITHOUT_A_NAME:
    case FLAG_WITH_A_WRONG_SHORT_NAME:
    case NEGATABLE_WITHOUT_A_LONG_NAME:
    case COUNT_WITH_A_WRONG_LONG_NAME:
    case VALUE_WITH_A_WRONG_LONG_NAME:
    case POSITIONAL_WITH_AN_EMPTY_NAME:
        return CLARGS_ERROR_INVALID_NAME;
    case SUBCOMMAND_WITH_A_NAME_IN_USE:
    case FLAG_WITH_A_SHORT_NAME_IN_USE:
    case COUNT_WITH_A_LONG_NAME_IN_USE:
    case VALUE_WITH_A_LONG_NAME_IN_USE:
    case NEGATABLE_WITH_A_LONG_NAME_IN_USE:
    case POSITIONAL_WITH_A_NAME_IN_USE:
        return CLARGS_ERROR_NAME_CONFLICT;
    case SUBCOMMAND_NEXT_TO_POSITIONALS:
    case POSITIONAL_NEXT_TO_SUBCOMMANDS:
    case VALUE_NAME_OF_A_FLAG:
    case REQUIRED_OF_A_FLAG:
    case DEFAULT_OF_A_FLAG:
    case INITIAL_OF_A_VALUE:
    case DEFAULT_OF_A_REQUIRED_POSITIONAL:
        return CLARGS_ERROR_INVALID_DEFINITION;
    default:
        NAOS_FAIL("a test asked for an unknown refusal");
        /* NAOS_FAIL does not return; nothing may follow it (MSVC C4702). */
    }
}

/* Tries the change on the target of the fixture. */
static clargs_error_t attempt(const fixture_t *fixture, refusal_t refusal)
{
    switch (refusal) {
    case SUBCOMMAND_WITH_A_WRONG_NAME:
        return clargs_command_add_subcommand(fixture->target, "-new", NULL);
    case OPTION_WITHOUT_A_NAME:
        return clargs_command_add_flag(fixture->target, '\0', NULL, NULL);
    case FLAG_WITH_A_WRONG_SHORT_NAME:
        return clargs_command_add_flag(fixture->target, '?', "new", NULL);
    case NEGATABLE_WITHOUT_A_LONG_NAME:
        return clargs_command_add_negatable(fixture->target, 'n', NULL, NULL);
    case COUNT_WITH_A_WRONG_LONG_NAME:
        return clargs_command_add_count(fixture->target, 'n', "n", NULL);
    case VALUE_WITH_A_WRONG_LONG_NAME:
        return clargs_command_add_value(fixture->target, 'n', "new=name", clargs_type_string(), NULL);
    case POSITIONAL_WITH_AN_EMPTY_NAME:
        return clargs_command_add_positional(fixture->target, "", clargs_type_string(),
                                             CLARGS_ARITY_OPTIONAL, NULL);
    case SUBCOMMAND_WITH_A_NAME_IN_USE:
        return clargs_command_add_subcommand(fixture->target, "child", NULL);
    case FLAG_WITH_A_SHORT_NAME_IN_USE:
        return clargs_command_add_flag(fixture->target, 'f', "new", NULL);
    case COUNT_WITH_A_LONG_NAME_IN_USE:
        return clargs_command_add_count(fixture->target, 'n', "output", NULL);
    case VALUE_WITH_A_LONG_NAME_IN_USE:
        return clargs_command_add_value(fixture->target, 'n', "no-color", clargs_type_string(), NULL);
    case NEGATABLE_WITH_A_LONG_NAME_IN_USE:
        return clargs_command_add_negatable(fixture->target, 'n', "color", NULL);
    case POSITIONAL_WITH_A_NAME_IN_USE:
        return clargs_command_add_positional(fixture->target, "input", clargs_type_string(),
                                             CLARGS_ARITY_OPTIONAL, NULL);
    case SUBCOMMAND_NEXT_TO_POSITIONALS:
        return clargs_command_add_subcommand(fixture->target, "new", NULL);
    case POSITIONAL_NEXT_TO_SUBCOMMANDS:
        return clargs_command_add_positional(fixture->target, "new", clargs_type_string(),
                                             CLARGS_ARITY_OPTIONAL, NULL);
    case VALUE_NAME_OF_A_FLAG:
        return clargs_option_set_value_name(fixture->flag, "FILE");
    case REQUIRED_OF_A_FLAG:
        return clargs_option_set_required(fixture->flag, true);
    case DEFAULT_OF_A_FLAG:
        return clargs_option_set_default(fixture->negatable, "on");
    case INITIAL_OF_A_VALUE:
        return clargs_option_set_initial(fixture->value, true);
    case DEFAULT_OF_A_REQUIRED_POSITIONAL:
        return clargs_positional_set_default(fixture->required, "more");
    default:
        NAOS_FAIL("a test asked for an unknown refusal");
        /* NAOS_FAIL does not return; nothing may follow it (MSVC C4702). */
    }
}

NAOS_TEST(a_change_that_is_refused_leaves_a_validated_tree_validated)
{
    size_t where;
    size_t which;

    clargs_test_allocator_install();

    for (where = 0; where < LOCATION_COUNT; ++where) {
        for (which = 0; which < REFUSAL_COUNT; ++which) {
            const refusal_t refusal = all_refusals[which];
            fixture_t fixture;
            size_t requests;

            build_fixture(&fixture, all_locations[where], refusal_needs_positionals(refusal));
            requests = clargs_test_allocator_requests();

            NAOS_CHECK_INT_EQ(attempt(&fixture, refusal), refusal_error(refusal));
            check_state(&fixture, true);
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests);

            clargs_command_destroy(fixture.root);
        }
    }

    clargs_test_allocator_uninstall();
    check_clean();
}

/* The most allocations that one change is given to fail in. */
#define ALLOCATION_LIMIT 16

/*
 * Every allocation of every change is made to fail in turn, each time on a
 * tree that was just validated.
 */
NAOS_TEST(a_change_that_fails_for_a_lack_of_memory_leaves_a_validated_tree_validated)
{
    size_t where;
    size_t which;

    clargs_test_allocator_install();

    for (where = 0; where < LOCATION_COUNT; ++where) {
        for (which = 0; which < MUTATION_COUNT; ++which) {
            const mutation_t mutation = all_mutations[which];
            bool completed = false;
            size_t failures = 0;
            size_t n;

            for (n = 1; n <= ALLOCATION_LIMIT && !completed; ++n) {
                fixture_t fixture;
                clargs_error_t error;
                size_t requests;
                size_t live;

                build_fixture(&fixture, all_locations[where], mutation_needs_positionals(mutation));
                requests = clargs_test_allocator_requests();
                live = clargs_test_allocator_live();
                clargs_test_allocator_fail_at(requests + n);

                error = mutate(&fixture, mutation);

                clargs_test_allocator_fail_at(0);

                if (error == CLARGS_ERROR_NONE) {
                    /* Every allocation point has been passed. */
                    NAOS_CHECK_TRUE(clargs_test_allocator_requests() < requests + n);
                    check_state(&fixture, false);
                    completed = true;
                } else {
                    NAOS_CHECK_INT_EQ(error, CLARGS_ERROR_OUT_OF_MEMORY);
                    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests + n);
                    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);
                    check_state(&fixture, true);
                    ++failures;

                    /*
                     * The same change succeeds afterwards, and only then
                     * takes the state back.
                     */
                    NAOS_CHECK_INT_EQ(mutate(&fixture, mutation), CLARGS_ERROR_NONE);
                    check_state(&fixture, false);
                }

                clargs_command_destroy(fixture.root);
                NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
            }

            NAOS_CHECK_TRUE(completed);
            NAOS_CHECK_TRUE(failures >= mutation_allocations(mutation));
        }
    }

    clargs_test_allocator_uninstall();
    check_clean();
}

/* ---- Default values, checked with their value types ---------------------- */

/* Additions with a value type that must succeed. */
static clargs_option_t *add_typed_value(clargs_command_t *command, char short_name,
                                        const char *long_name, const clargs_type_t *type)
{
    clargs_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_value(command, short_name, long_name, type, &option) ==
                      CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(option != NULL);
    return option;
}

/* Adds an optional positional: the only kind that takes a default value. */
static clargs_positional_t *add_typed_positional(clargs_command_t *command, const char *name,
                                                 const clargs_type_t *type)
{
    clargs_positional_t *positional = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_positional(command, name, type, CLARGS_ARITY_OPTIONAL,
                                                    &positional) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(positional != NULL);
    return positional;
}

static void set_option_default(clargs_option_t *option, const char *value)
{
    NAOS_REQUIRE_TRUE(clargs_option_set_default(option, value) == CLARGS_ERROR_NONE);
}

static void set_positional_default(clargs_positional_t *positional, const char *value)
{
    NAOS_REQUIRE_TRUE(clargs_positional_set_default(positional, value) == CLARGS_ERROR_NONE);
}

/* Creates the value type of the choices "debug", "release" and "". */
static clargs_type_t *new_choice_type(void)
{
    static const char *const choices[] = { "debug", "release", "" };
    clargs_type_t *type = NULL;

    NAOS_REQUIRE_TRUE(clargs_type_create_choice(choices, 3, &type) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(type != NULL);
    return type;
}

/*
 * What the functions of a user-defined value type count. It is the user
 * data of the value type.
 */
typedef struct probe {
    size_t conversions; /* calls of the conversion function */
    size_t successes;   /* conversions that succeeded */
    size_t releases;    /* calls of the cleanup function */
    size_t mismatches;  /* cleanups of something that is not a value */
} probe_t;

/*
 * The value of the user-defined value type. It is larger than every value
 * of a built-in value type, and the conversion writes all of it.
 */
typedef struct probe_value {
    double number;
    const char *text;
    int marker;
    unsigned char fill[48];
} probe_value_t;

#define PROBE_FILL 0xA5
#define PROBE_MARKER 1234

/*
 * The inputs that the conversion of the user-defined value type does not
 * take, and what it returns for each. Every other input is taken.
 */
#define PROBE_REJECTED "rejected"       /* CLARGS_ERROR_INVALID_VALUE */
#define PROBE_OTHER_CODE "other-code"   /* CLARGS_ERROR_INVALID_NAME */
#define PROBE_NO_MEMORY "no-memory"     /* CLARGS_ERROR_OUT_OF_MEMORY */

static clargs_error_t probe_convert(const char *text, void *value, void *context)
{
    probe_t *probe = context;
    probe_value_t *out = value;

    ++probe->conversions;

    /*
     * The whole area is written, also by a conversion that fails: what it
     * leaves behind is not a value and must not be cleaned up.
     */
    memset(out, PROBE_FILL, sizeof(*out));

    if (strcmp(text, PROBE_REJECTED) == 0) {
        return CLARGS_ERROR_INVALID_VALUE;
    }
    if (strcmp(text, PROBE_OTHER_CODE) == 0) {
        return CLARGS_ERROR_INVALID_NAME;
    }
    if (strcmp(text, PROBE_NO_MEMORY) == 0) {
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }

    out->number = 1.5;
    out->text = text;
    out->marker = PROBE_MARKER;
    ++probe->successes;
    return CLARGS_ERROR_NONE;
}

static void probe_release(void *value, void *context)
{
    probe_t *probe = context;
    const probe_value_t *in = value;

    ++probe->releases;

    /*
     * The value is the one that the last conversion wrote, and that
     * conversion succeeded.
     */
    if (probe->releases > probe->successes || in->marker != PROBE_MARKER || in->text == NULL ||
        in->fill[0] != PROBE_FILL || in->fill[sizeof(in->fill) - 1] != PROBE_FILL) {
        ++probe->mismatches;
    }
}

/* Creates the user-defined value type that counts in probe. */
static clargs_type_t *new_probe_type(probe_t *probe)
{
    clargs_type_t *type = NULL;

    probe->conversions = 0;
    probe->successes = 0;
    probe->releases = 0;
    probe->mismatches = 0;

    NAOS_REQUIRE_TRUE(clargs_type_create("probe", sizeof(probe_value_t), probe_convert, probe_release,
                                         probe, &type) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(type != NULL);
    return type;
}

/* Checks the counts of a user-defined value type. */
static void check_probe(const probe_t *probe, size_t conversions, size_t successes)
{
    NAOS_CHECK_UINT_EQ(probe->conversions, conversions);
    NAOS_CHECK_UINT_EQ(probe->successes, successes);

    /*
     * The cleanup function is called once for each conversion that
     * succeeded, and for nothing else.
     */
    NAOS_CHECK_UINT_EQ(probe->releases, successes);
    NAOS_CHECK_UINT_EQ(probe->mismatches, 0);
}

/* A conversion that needs no cleanup function. Takes the input "yes". */
static clargs_error_t plain_convert(const char *text, void *value, void *context)
{
    int *out = value;

    (void)context;

    if (strcmp(text, "yes") != 0) {
        return CLARGS_ERROR_INVALID_VALUE;
    }
    *out = 1;
    return CLARGS_ERROR_NONE;
}

/* Ends a test that destroyed its roots and its value types itself. */
static void end_test(void)
{
    clargs_test_allocator_uninstall();
    check_clean();
}

/* The number of validations that one check_validate() makes. */
#define CHECK_VALIDATE_ROUNDS 8

/*
 * Validates the tree of root with every combination of outputs, each time
 * with the nth request of the validation made to fail, and checks that the
 * validation fails for a lack of memory: the error code, NULL in every
 * output that is given, no block more or less than before, and the state as
 * it was.
 */
static void check_validate_without_memory(clargs_command_t *root, size_t nth)
{
    const bool validated = clargs_command_is_validated(root);
    clargs_command_t *other = NULL;
    clargs_option_t *other_option;
    clargs_positional_t *other_positional;
    unsigned mask;

    NAOS_REQUIRE_TRUE(clargs_command_create("other", &other) == CLARGS_ERROR_NONE);
    other_option = add_flag(other, 's', "sentinel");
    other_positional = add_positional(other, "sentinel", CLARGS_ARITY_REQUIRED);

    for (mask = 0; mask < 8; ++mask) {
        const clargs_command_t *command = other;
        const clargs_option_t *option = other_option;
        const clargs_positional_t *positional = other_positional;
        const bool with_command = (mask & 1u) != 0;
        const bool with_option = (mask & 2u) != 0;
        const bool with_positional = (mask & 4u) != 0;
        const size_t requests = clargs_test_allocator_requests();
        const size_t live = clargs_test_allocator_live();
        clargs_error_t error;

        clargs_test_allocator_fail_at(requests + nth);
        error = clargs_command_validate(root, with_command ? &command : NULL,
                                        with_option ? &option : NULL,
                                        with_positional ? &positional : NULL);
        clargs_test_allocator_fail_at(0);

        NAOS_CHECK_INT_EQ(error, CLARGS_ERROR_OUT_OF_MEMORY);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests + nth);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);
        NAOS_CHECK_TRUE(clargs_command_is_validated(root) == validated);

        if (with_command) {
            NAOS_CHECK_TRUE(command == NULL);
        }
        if (with_option) {
            NAOS_CHECK_TRUE(option == NULL);
        }
        if (with_positional) {
            NAOS_CHECK_TRUE(positional == NULL);
        }
    }

    clargs_command_destroy(other);
}

NAOS_TEST(a_default_value_that_is_not_a_number_is_rejected_by_the_integer_type)
{
    static const char *const rejected[] = { "abc", "", "12x", " 12", "1.5", "9223372036854775808" };
    clargs_command_t *root = create_root();
    clargs_option_t *jobs;
    size_t index;

    (void)add_flag(root, 'v', "verbose");
    jobs = add_typed_value(root, 'j', "jobs", clargs_type_int());

    for (index = 0; index < sizeof(rejected) / sizeof(rejected[0]); ++index) {
        set_option_default(jobs, rejected[index]);

        check_validate(root, CLARGS_ERROR_INVALID_DEFAULT, root, jobs, NULL);
        NAOS_CHECK_FALSE(clargs_command_is_validated(root));

        /* The option is as it was defined, the default value included. */
        NAOS_CHECK_STR_EQ(clargs_option_default(jobs), rejected[index]);
        NAOS_CHECK_TRUE(clargs_option_type(jobs) == clargs_type_int());
        NAOS_CHECK_FALSE(clargs_option_is_required(jobs));
    }

    /* With a number, the same tree is valid. */
    set_option_default(jobs, "-12");
    check_validate(root, CLARGS_ERROR_NONE, NULL, NULL, NULL);
    NAOS_CHECK_TRUE(clargs_command_is_validated(root));
    NAOS_CHECK_STR_EQ(clargs_option_default(jobs), "-12");

    destroy_root(root);
}

NAOS_TEST(a_default_value_that_is_not_a_number_is_rejected_by_the_float_type)
{
    clargs_command_t *root = create_root();
    clargs_option_t *ratio = add_typed_value(root, 'r', "ratio", clargs_type_float());

    set_option_default(ratio, "half");
    check_validate(root, CLARGS_ERROR_INVALID_DEFAULT, root, ratio, NULL);
    NAOS_CHECK_FALSE(clargs_command_is_validated(root));

    set_option_default(ratio, "0.5");
    check_validate(root, CLARGS_ERROR_NONE, NULL, NULL, NULL);
    NAOS_CHECK_TRUE(clargs_command_is_validated(root));

    destroy_root(root);
}

NAOS_TEST(a_default_value_that_is_not_a_choice_is_rejected_by_the_choice_type)
{
    clargs_type_t *choice;
    clargs_command_t *root;
    clargs_option_t *mode;

    clargs_test_allocator_install();
    choice = new_choice_type();
    root = new_root();
    mode = add_typed_value(root, 'm', "mode", choice);

    set_option_default(mode, "fast");
    check_validate(root, CLARGS_ERROR_INVALID_DEFAULT, root, mode, NULL);
    NAOS_CHECK_FALSE(clargs_command_is_validated(root));
    NAOS_CHECK_STR_EQ(clargs_option_default(mode), "fast");

    /* Upper and lower case are different. */
    set_option_default(mode, "Debug");
    check_validate(root, CLARGS_ERROR_INVALID_DEFAULT, root, mode, NULL);

    set_option_default(mode, "release");
    check_validate(root, CLARGS_ERROR_NONE, NULL, NULL, NULL);
    NAOS_CHECK_TRUE(clargs_command_is_validated(root));

    /* The empty string is one of the choices. */
    set_option_default(mode, "");
    check_validate(root, CLARGS_ERROR_NONE, NULL, NULL, NULL);

    clargs_command_destroy(root);
    clargs_type_destroy(choice);
    end_test();
}

NAOS_TEST(a_default_value_that_a_user_defined_type_rejects_is_an_invalid_default_value)
{
    probe_t probe;
    clargs_type_t *type;
    clargs_command_t *root;
    clargs_option_t *port;

    clargs_test_allocator_install();
    type = new_probe_type(&probe);
    root = new_root();
    port = add_typed_value(root, 'p', "port", type);

    set_option_default(port, PROBE_REJECTED);
    check_validate(root, CLARGS_ERROR_INVALID_DEFAULT, root, port, NULL);
    NAOS_CHECK_FALSE(clargs_command_is_validated(root));
    NAOS_CHECK_STR_EQ(clargs_option_default(port), PROBE_REJECTED);

    /*
     * Every validation asked for the conversion, and nothing was cleaned
     * up: no conversion succeeded.
     */
    check_probe(&probe, CHECK_VALIDATE_ROUNDS, 0);

    clargs_command_destroy(root);
    clargs_type_destroy(type);
    end_test();
}

/*
 * A conversion function may return any code for a failure. Whatever it is,
 * other than the lack of memory, the validation reports an invalid default
 * value.
 */
NAOS_TEST(any_other_code_of_a_user_defined_conversion_is_an_invalid_default_value)
{
    probe_t probe;
    clargs_type_t *type;
    clargs_command_t *root;
    clargs_command_t *run;
    clargs_positional_t *target;

    clargs_test_allocator_install();
    type = new_probe_type(&probe);
    root = new_root();
    run = add_sub(root, "run");
    target = add_typed_positional(run, "target", type);

    set_positional_default(target, PROBE_OTHER_CODE);
    check_validate(root, CLARGS_ERROR_INVALID_DEFAULT, run, NULL, target);
    NAOS_CHECK_FALSE(clargs_command_is_validated(root));
    check_probe(&probe, CHECK_VALIDATE_ROUNDS, 0);

    clargs_command_destroy(root);
    clargs_type_destroy(type);
    end_test();
}

NAOS_TEST(a_default_value_of_a_positional_that_its_type_rejects_is_reported_with_the_positional)
{
    clargs_type_t *choice;
    clargs_command_t *root;
    clargs_command_t *build;
    clargs_positional_t *profile;
    clargs_positional_t *level;

    clargs_test_allocator_install();
    choice = new_choice_type();
    root = new_root();
    build = add_sub(add_sub(root, "project"), "build");
    (void)add_positional(build, "target", CLARGS_ARITY_REQUIRED);
    profile = add_typed_positional(build, "profile", choice);
    level = add_typed_positional(build, "level", clargs_type_int());

    set_positional_default(profile, "debug");
    set_positional_default(level, "high");
    check_validate(root, CLARGS_ERROR_INVALID_DEFAULT, build, NULL, level);
    NAOS_CHECK_FALSE(clargs_command_is_validated(build));
    NAOS_CHECK_STR_EQ(clargs_positional_default(level), "high");
    NAOS_CHECK_TRUE(clargs_positional_command(level) == build);

    /* The first positional with such a default value is the one reported. */
    set_positional_default(profile, "fast");
    check_validate(root, CLARGS_ERROR_INVALID_DEFAULT, build, NULL, profile);

    set_positional_default(profile, "release");
    set_positional_default(level, "3");
    check_validate(root, CLARGS_ERROR_NONE, NULL, NULL, NULL);
    NAOS_CHECK_TRUE(clargs_command_is_validated(build));

    clargs_command_destroy(root);
    clargs_type_destroy(choice);
    end_test();
}

NAOS_TEST(default_values_that_their_types_take_are_valid)
{
    probe_t probe;
    clargs_type_t *custom;
    clargs_type_t *choice;
    clargs_type_t *plain = NULL;
    clargs_command_t *root;
    clargs_command_t *run;
    size_t requests;

    clargs_test_allocator_install();
    custom = new_probe_type(&probe);
    choice = new_choice_type();
    NAOS_REQUIRE_TRUE(clargs_type_create("plain", sizeof(int), plain_convert, NULL, NULL, &plain) ==
                      CLARGS_ERROR_NONE);
    root = new_root();
    run = add_sub(root, "run");

    set_option_default(add_typed_value(root, 'n', "name", clargs_type_string()), "");
    set_option_default(add_typed_value(root, 'j', "jobs", clargs_type_int()), "+8");
    set_option_default(add_typed_value(root, 'r', "ratio", clargs_type_float()), "-1.5e-3");
    set_option_default(add_typed_value(run, 'm', "mode", choice), "debug");
    set_option_default(add_typed_value(run, 'p', "port", custom), "8080");

    /* A value type without a cleanup function. */
    set_option_default(add_typed_value(run, 'y', "confirm", plain), "yes");
    set_positional_default(add_typed_positional(run, "level", clargs_type_int()), "0");

    /* Definitions without a default value are not converted. */
    (void)add_typed_value(run, 'q', "quota", clargs_type_int());
    (void)add_typed_value(run, 'o', "other-port", custom);
    (void)add_typed_positional(run, "extra", custom);

    requests = clargs_test_allocator_requests();
    NAOS_CHECK_INT_EQ(clargs_command_validate(root, NULL, NULL, NULL), CLARGS_ERROR_NONE);
    NAOS_CHECK_TRUE(clargs_command_is_validated(root));
    NAOS_CHECK_TRUE(clargs_command_is_validated(run));
    check_probe(&probe, 1, 1);

    /*
     * One allocation for each of the seven default values: the area that
     * the value is written to.
     */
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests + 7);

    check_validate(root, CLARGS_ERROR_NONE, NULL, NULL, NULL);
    check_probe(&probe, 1 + CHECK_VALIDATE_ROUNDS, 1 + CHECK_VALIDATE_ROUNDS);

    clargs_command_destroy(root);
    clargs_type_destroy(plain);
    clargs_type_destroy(choice);
    clargs_type_destroy(custom);
    end_test();
}

/*
 * The cleanup function of a user-defined value type is called as often as
 * its conversions succeeded: no more, no less, and before the validation
 * returns.
 */
NAOS_TEST(the_cleanup_function_is_called_once_for_each_conversion_that_succeeded)
{
    probe_t probe;
    clargs_type_t *type;
    clargs_command_t *root;
    clargs_command_t *run;
    clargs_option_t *second;
    clargs_positional_t *last;
    const clargs_command_t *command = NULL;
    const clargs_option_t *option = NULL;
    const clargs_positional_t *positional = NULL;

    clargs_test_allocator_install();
    type = new_probe_type(&probe);
    root = new_root();
    run = add_sub(root, "run");

    set_option_default(add_typed_value(root, 'a', "first", type), "one");
    second = add_typed_value(root, 'b', "second", type);
    set_option_default(second, "two");
    set_option_default(add_typed_value(run, 'c', "third", type), "three");
    last = add_typed_positional(run, "last", type);
    set_positional_default(last, "four");

    /* The creation of the tree asked for no conversion. */
    check_probe(&probe, 0, 0);

    NAOS_CHECK_INT_EQ(clargs_command_validate(root, NULL, NULL, NULL), CLARGS_ERROR_NONE);
    check_probe(&probe, 4, 4);

    /* Every validation converts again. */
    NAOS_CHECK_INT_EQ(clargs_command_validate(root, NULL, NULL, NULL), CLARGS_ERROR_NONE);
    check_probe(&probe, 8, 8);

    /*
     * The validation stops at the conversion that fails: the one before it
     * is cleaned up, the one that failed is not, the ones after it are not
     * asked for.
     */
    set_option_default(second, PROBE_REJECTED);
    NAOS_CHECK_INT_EQ(clargs_command_validate(root, &command, &option, &positional),
                      CLARGS_ERROR_INVALID_DEFAULT);
    NAOS_CHECK_TRUE(command == root);
    NAOS_CHECK_TRUE(option == second);
    NAOS_CHECK_TRUE(positional == NULL);
    check_probe(&probe, 10, 9);

    /* The last default value of the tree fails: three succeed first. */
    set_option_default(second, "two");
    set_positional_default(last, PROBE_OTHER_CODE);
    NAOS_CHECK_INT_EQ(clargs_command_validate(root, &command, &option, &positional),
                      CLARGS_ERROR_INVALID_DEFAULT);
    NAOS_CHECK_TRUE(command == run);
    NAOS_CHECK_TRUE(option == NULL);
    NAOS_CHECK_TRUE(positional == last);
    check_probe(&probe, 14, 12);

    /* The destruction of the tree and of the value type calls neither. */
    clargs_command_destroy(root);
    clargs_type_destroy(type);
    check_probe(&probe, 14, 12);
    end_test();
}

NAOS_TEST(one_value_type_can_be_given_to_several_definitions)
{
    probe_t probe;
    clargs_type_t *custom;
    clargs_type_t *choice;
    clargs_command_t *root;
    clargs_command_t *build;
    clargs_command_t *test;
    clargs_option_t *root_mode;
    clargs_option_t *build_mode;
    clargs_positional_t *test_mode;
    clargs_positional_t *test_port;

    clargs_test_allocator_install();
    custom = new_probe_type(&probe);
    choice = new_choice_type();
    root = new_root();
    build = add_sub(root, "build");
    test = add_sub(root, "test");

    /*
     * The same value types in three commands, for options and for
     * positionals, with a default value and without.
     */
    root_mode = add_typed_value(root, 'm', "mode", choice);
    (void)add_typed_value(root, 'p', "port", custom);
    (void)add_typed_value(root, 'j', "jobs", clargs_type_int());
    build_mode = add_typed_value(build, 'm', "mode", choice);
    set_option_default(add_typed_value(build, 'p', "port", custom), "80");
    set_option_default(add_typed_value(build, 'q', "proxy-port", custom), "8080");
    set_option_default(add_typed_value(build, 'j', "jobs", clargs_type_int()), "4");
    test_mode = add_typed_positional(test, "mode", choice);
    test_port = add_typed_positional(test, "port", custom);
    set_positional_default(add_typed_positional(test, "jobs", clargs_type_int()), "1");

    set_option_default(root_mode, "debug");
    set_option_default(build_mode, "release");
    set_positional_default(test_mode, "debug");
    set_positional_default(test_port, "443");

    NAOS_CHECK_TRUE(clargs_option_type(root_mode) == clargs_option_type(build_mode));
    NAOS_CHECK_TRUE(clargs_option_type(root_mode) == clargs_positional_type(test_mode));

    NAOS_CHECK_INT_EQ(clargs_command_validate(root, NULL, NULL, NULL), CLARGS_ERROR_NONE);
    NAOS_CHECK_TRUE(clargs_command_is_validated(test));
    check_probe(&probe, 3, 3);

    /*
     * Each definition is checked for itself: one of those that share the
     * value type has a default value that the type does not take.
     */
    set_option_default(build_mode, "fast");
    check_validate(root, CLARGS_ERROR_INVALID_DEFAULT, build, build_mode, NULL);
    set_option_default(build_mode, "debug");

    set_positional_default(test_port, PROBE_REJECTED);
    check_validate(root, CLARGS_ERROR_INVALID_DEFAULT, test, NULL, test_port);
    set_positional_default(test_port, "443");

    check_validate(root, CLARGS_ERROR_NONE, NULL, NULL, NULL);
    NAOS_CHECK_TRUE(clargs_command_is_validated(root));

    clargs_command_destroy(root);
    clargs_type_destroy(choice);
    clargs_type_destroy(custom);
    check_probe(&probe, probe.conversions, probe.successes);
    end_test();
}

/*
 * For one option, required with a default value is checked first: the
 * default value of such an option is not converted.
 */
NAOS_TEST(a_required_option_with_a_default_value_is_reported_before_its_default_value_is_checked)
{
    probe_t probe;
    clargs_type_t *type;
    clargs_command_t *root;
    clargs_option_t *port;

    clargs_test_allocator_install();
    type = new_probe_type(&probe);
    root = new_root();
    port = add_typed_value(root, 'p', "port", type);
    set_option_default(port, PROBE_REJECTED);
    NAOS_REQUIRE_TRUE(clargs_option_set_required(port, true) == CLARGS_ERROR_NONE);

    check_validate(root, CLARGS_ERROR_INVALID_DEFINITION, root, port, NULL);
    check_probe(&probe, 0, 0);

    NAOS_REQUIRE_TRUE(clargs_option_set_required(port, false) == CLARGS_ERROR_NONE);
    check_validate(root, CLARGS_ERROR_INVALID_DEFAULT, root, port, NULL);

    clargs_command_destroy(root);
    clargs_type_destroy(type);
    end_test();
}

/*
 * The options are checked one after the other, each with both of its
 * checks: an option whose default value is not taken comes before a later
 * option that is required and has a default value.
 */
NAOS_TEST(the_options_are_checked_one_by_one_in_the_order_of_their_definition)
{
    clargs_command_t *root = create_root();
    clargs_option_t *first = add_typed_value(root, 'a', "first", clargs_type_int());
    clargs_option_t *second = add_typed_value(root, 'b', "second", clargs_type_int());
    clargs_option_t *third = add_typed_value(root, 'c', "third", clargs_type_int());

    /* The faults are made from the last option to the first. */
    set_option_default(third, "three");
    check_validate(root, CLARGS_ERROR_INVALID_DEFAULT, root, third, NULL);

    make_faulty(second);
    check_validate(root, CLARGS_ERROR_INVALID_DEFINITION, root, second, NULL);

    set_option_default(first, "one");
    check_validate(root, CLARGS_ERROR_INVALID_DEFAULT, root, first, NULL);

    destroy_root(root);
}

/*
 * In one command: the order of the positionals, then the options, then the
 * default values of the positionals.
 */
NAOS_TEST(the_default_values_of_the_positionals_are_checked_after_the_options)
{
    clargs_command_t *root = create_root();
    clargs_positional_t *level = add_typed_positional(root, "level", clargs_type_int());
    clargs_positional_t *late;
    clargs_option_t *jobs;
    clargs_option_t *output;

    /* The positional is defined before the options. */
    set_positional_default(level, "high");
    check_validate(root, CLARGS_ERROR_INVALID_DEFAULT, root, NULL, level);

    /* An option that is required and has a default value comes first. */
    (void)add_flag(root, 'v', "verbose");
    output = add_value(root, 'o', "output");
    make_faulty(output);
    check_validate(root, CLARGS_ERROR_INVALID_DEFINITION, root, output, NULL);
    NAOS_REQUIRE_TRUE(clargs_option_set_required(output, false) == CLARGS_ERROR_NONE);
    check_validate(root, CLARGS_ERROR_INVALID_DEFAULT, root, NULL, level);

    /* So does an option whose default value is not taken. */
    jobs = add_typed_value(root, 'j', "jobs", clargs_type_int());
    set_option_default(jobs, "many");
    check_validate(root, CLARGS_ERROR_INVALID_DEFAULT, root, jobs, NULL);

    /* The order of the positionals comes before all of it. */
    late = add_positional(root, "late", CLARGS_ARITY_REQUIRED);
    check_validate(root, CLARGS_ERROR_INVALID_DEFINITION, root, NULL, late);

    destroy_root(root);
}

NAOS_TEST(a_default_value_of_a_command_is_checked_before_its_subcommands)
{
    clargs_command_t *root = create_root();
    clargs_command_t *first = add_sub(root, "first");
    clargs_command_t *second = add_sub(root, "second");
    clargs_command_t *below = add_sub(first, "below");
    clargs_option_t *in_root = add_typed_value(root, 'j', "jobs", clargs_type_int());
    clargs_option_t *in_first = add_value(first, 'o', "output");
    clargs_option_t *in_second = add_typed_value(second, 'j', "jobs", clargs_type_int());
    clargs_positional_t *in_below = add_typed_positional(below, "level", clargs_type_int());

    /* The faults are made in the reverse of the order of the check. */
    set_option_default(in_second, "many");
    check_validate(root, CLARGS_ERROR_INVALID_DEFAULT, second, in_second, NULL);

    /*
     * A command below the first subcommand comes before the second
     * subcommand.
     */
    set_positional_default(in_below, "high");
    check_validate(root, CLARGS_ERROR_INVALID_DEFAULT, below, NULL, in_below);

    /*
     * A fault of another kind in a command that is checked earlier comes
     * first.
     */
    make_faulty(in_first);
    check_validate(root, CLARGS_ERROR_INVALID_DEFINITION, first, in_first, NULL);

    set_option_default(in_root, "many");
    check_validate(root, CLARGS_ERROR_INVALID_DEFAULT, root, in_root, NULL);

    destroy_root(root);
}

NAOS_TEST(an_invalid_default_value_leaves_the_tree_not_validated_and_unchanged)
{
    full_tree_t tree;
    clargs_option_t *limit;
    int round;

    clargs_test_allocator_install();
    build_full_tree(&tree);
    NAOS_REQUIRE_TRUE(clargs_command_validate(tree.root, NULL, NULL, NULL) == CLARGS_ERROR_NONE);
    check_full_tree_state(&tree, true);

    /*
     * A new option with a default value that its value type does not
     * take. The changes take the state back.
     */
    limit = add_typed_value(tree.add, 'l', "limit", clargs_type_int());
    set_option_default(limit, "none");
    check_full_tree_state(&tree, false);

    for (round = 0; round < 3; ++round) {
        const clargs_command_t *command = NULL;
        const clargs_option_t *option = NULL;
        const clargs_positional_t *positional = tree.target;

        NAOS_CHECK_INT_EQ(clargs_command_validate(tree.root, &command, &option, &positional),
                          CLARGS_ERROR_INVALID_DEFAULT);
        NAOS_CHECK_TRUE(command == tree.add);
        NAOS_CHECK_TRUE(option == limit);
        NAOS_CHECK_TRUE(positional == NULL);
        check_full_tree_state(&tree, false);
    }

    /* The definition is as it was, the fault included. */
    check_full_tree(&tree);
    NAOS_REQUIRE_TRUE(clargs_command_option_count(tree.add) == 2);
    NAOS_CHECK_TRUE(clargs_command_option_at(tree.add, 1) == limit);
    NAOS_CHECK_STR_EQ(clargs_option_default(limit), "none");

    /* Without the fault, the same tree is valid. */
    set_option_default(limit, "10");
    NAOS_CHECK_INT_EQ(clargs_command_validate(tree.root, NULL, NULL, NULL), CLARGS_ERROR_NONE);
    check_full_tree_state(&tree, true);

    destroy_root(tree.root);
}

/* The area that a value is written to cannot be allocated. */
NAOS_TEST(a_validation_that_cannot_allocate_the_area_of_a_value_fails_for_a_lack_of_memory)
{
    probe_t probe;
    clargs_type_t *type;
    clargs_command_t *root;
    clargs_command_t *run;
    size_t nth;

    clargs_test_allocator_install();
    type = new_probe_type(&probe);
    root = new_root();
    run = add_sub(root, "run");
    set_option_default(add_typed_value(root, 'p', "port", type), "80");
    set_option_default(add_typed_value(root, 'j', "jobs", clargs_type_int()), "4");
    set_positional_default(add_typed_positional(run, "level", type), "3");

    /*
     * The tree has three default values, so a validation asks for three
     * areas. Each of them fails in turn, on a tree that is not validated.
     */
    for (nth = 1; nth <= 3; ++nth) {
        const size_t successes = probe.successes;

        check_validate_without_memory(root, nth);
        NAOS_CHECK_FALSE(clargs_command_is_validated(root));
        NAOS_CHECK_FALSE(clargs_command_is_validated(run));

        /*
         * The value that was converted before the failure is cleaned up.
         * That is the first default value when the second or the third
         * area fails; the second default value is an integer and is not
         * counted.
         */
        check_probe(&probe, probe.conversions, probe.successes);
        NAOS_CHECK_UINT_EQ(probe.successes, successes + (nth == 1 ? 0 : CHECK_VALIDATE_ROUNDS));
    }

    /* The same tree is valid when memory is there. */
    check_validate(root, CLARGS_ERROR_NONE, NULL, NULL, NULL);
    NAOS_CHECK_TRUE(clargs_command_is_validated(root));

    /*
     * A validation that fails for a lack of memory does not change the
     * state: the tree stays validated.
     */
    check_validate_without_memory(root, 1);
    NAOS_CHECK_TRUE(clargs_command_is_validated(root));
    NAOS_CHECK_TRUE(clargs_command_is_validated(run));

    clargs_command_destroy(root);
    clargs_type_destroy(type);
    end_test();
}

/*
 * A conversion function that reports a lack of memory: the validation
 * returns that, and not an invalid default value.
 */
NAOS_TEST(a_lack_of_memory_of_a_user_defined_conversion_is_returned_as_it_is)
{
    probe_t probe;
    clargs_type_t *type;
    clargs_command_t *root;
    clargs_option_t *port;
    clargs_command_t *other = NULL;
    const clargs_command_t *command;
    const clargs_option_t *option;
    const clargs_positional_t *positional;
    size_t live;

    clargs_test_allocator_install();
    type = new_probe_type(&probe);
    root = new_root();
    set_option_default(add_typed_value(root, 'a', "first", type), "one");
    port = add_typed_value(root, 'p', "port", type);
    set_option_default(port, PROBE_NO_MEMORY);

    NAOS_REQUIRE_TRUE(clargs_command_create("other", &other) == CLARGS_ERROR_NONE);
    command = other;
    option = port;
    positional = add_positional(other, "sentinel", CLARGS_ARITY_REQUIRED);
    live = clargs_test_allocator_live();

    NAOS_CHECK_INT_EQ(clargs_command_validate(root, &command, &option, &positional),
                      CLARGS_ERROR_OUT_OF_MEMORY);

    /*
     * It is a lack of memory, not a fault of a definition: the outputs
     * hold NULL.
     */
    NAOS_CHECK_TRUE(command == NULL);
    NAOS_CHECK_TRUE(option == NULL);
    NAOS_CHECK_TRUE(positional == NULL);
    NAOS_CHECK_FALSE(clargs_command_is_validated(root));
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);

    /*
     * The value before it was cleaned up, the conversion that failed was
     * not.
     */
    check_probe(&probe, 2, 1);

    NAOS_CHECK_INT_EQ(clargs_command_validate(root, NULL, NULL, NULL), CLARGS_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_FALSE(clargs_command_is_validated(root));
    check_probe(&probe, 4, 2);

    clargs_command_destroy(other);
    clargs_command_destroy(root);
    clargs_type_destroy(type);
    end_test();
}

/*
 * The float conversion copies an input that is too long for its fixed
 * array. When that allocation fails, the validation fails for a lack of
 * memory, and the area of the value is freed.
 */
NAOS_TEST(a_lack_of_memory_of_a_built_in_conversion_is_returned_as_it_is)
{
    static const char long_number[] =
        "1.00000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000000000000000000000";
    clargs_command_t *root = create_root();
    clargs_command_t *run = add_sub(root, "run");
    clargs_positional_t *ratio = add_typed_positional(run, "ratio", clargs_type_float());
    size_t requests;

    set_positional_default(ratio, long_number);

    /* The second request of the validation is the copy of the input. */
    check_validate_without_memory(root, 2);
    NAOS_CHECK_FALSE(clargs_command_is_validated(root));
    NAOS_CHECK_STR_EQ(clargs_positional_default(ratio), long_number);

    /*
     * With memory, the default value is taken, and the validation asked
     * for both allocations.
     */
    requests = clargs_test_allocator_requests();
    NAOS_CHECK_INT_EQ(clargs_command_validate(root, NULL, NULL, NULL), CLARGS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests + 2);
    NAOS_CHECK_TRUE(clargs_command_is_validated(root));

    destroy_root(root);
}

NAOS_SUITE(valid,
    NAOS_ADD_TEST(a_new_root_is_not_validated),
    NAOS_ADD_TEST(a_new_subcommand_reads_a_tree_that_is_not_validated),
    NAOS_ADD_TEST(an_empty_root_is_valid),
    NAOS_ADD_TEST(an_empty_root_is_valid_with_every_combination_of_outputs),
    NAOS_ADD_TEST(a_tree_without_a_fault_is_valid_and_reads_validated_from_every_command),
    NAOS_ADD_TEST(a_validation_that_succeeds_writes_null_to_the_outputs),
    NAOS_ADD_TEST(a_valid_tree_can_be_validated_again)
)

NAOS_SUITE(positional_order,
    NAOS_ADD_TEST(the_orders_that_a_command_line_can_follow_are_valid),
    NAOS_ADD_TEST(a_required_positional_after_an_optional_one_is_an_inconsistent_definition),
    NAOS_ADD_TEST(a_positional_after_a_variadic_one_is_an_inconsistent_definition),
    NAOS_ADD_TEST(the_first_faulty_positional_of_a_command_is_reported)
)

NAOS_SUITE(required_default,
    NAOS_ADD_TEST(a_required_option_with_a_default_value_is_an_inconsistent_definition),
    NAOS_ADD_TEST(the_fault_is_the_same_when_the_default_value_is_set_first),
    NAOS_ADD_TEST(required_alone_and_a_default_value_alone_are_valid),
    NAOS_ADD_TEST(the_first_faulty_option_of_a_command_is_reported)
)

NAOS_SUITE(traversal,
    NAOS_ADD_TEST(a_faulty_option_in_a_subcommand_is_found),
    NAOS_ADD_TEST(a_faulty_positional_in_a_subcommand_is_found),
    NAOS_ADD_TEST(a_faulty_positional_is_reported_before_a_faulty_option_of_the_same_command),
    NAOS_ADD_TEST(a_fault_of_a_command_is_reported_before_a_fault_of_its_subcommands),
    NAOS_ADD_TEST(the_subcommands_are_checked_in_the_order_of_their_definition),
    NAOS_ADD_TEST(the_same_definition_gives_the_same_result_every_time)
)

NAOS_SUITE(state,
    NAOS_ADD_TEST(a_validation_that_fails_leaves_the_tree_not_validated_and_unchanged),
    NAOS_ADD_TEST(a_fault_that_is_added_to_a_validated_tree_is_found),
    NAOS_ADD_TEST(every_change_that_succeeds_takes_a_validated_tree_back_to_not_validated),
    NAOS_ADD_TEST(a_set_of_the_same_value_takes_a_validated_tree_back_to_not_validated),
    NAOS_ADD_TEST(a_change_takes_back_the_state_of_its_own_tree_only),
    NAOS_ADD_TEST(a_change_that_is_refused_leaves_a_validated_tree_validated),
    NAOS_ADD_TEST(a_change_that_fails_for_a_lack_of_memory_leaves_a_validated_tree_validated)
)

NAOS_SUITE(defaults,
    NAOS_ADD_TEST(a_default_value_that_is_not_a_number_is_rejected_by_the_integer_type),
    NAOS_ADD_TEST(a_default_value_that_is_not_a_number_is_rejected_by_the_float_type),
    NAOS_ADD_TEST(a_default_value_that_is_not_a_choice_is_rejected_by_the_choice_type),
    NAOS_ADD_TEST(a_default_value_that_a_user_defined_type_rejects_is_an_invalid_default_value),
    NAOS_ADD_TEST(any_other_code_of_a_user_defined_conversion_is_an_invalid_default_value),
    NAOS_ADD_TEST(a_default_value_of_a_positional_that_its_type_rejects_is_reported_with_the_positional),
    NAOS_ADD_TEST(default_values_that_their_types_take_are_valid),
    NAOS_ADD_TEST(the_cleanup_function_is_called_once_for_each_conversion_that_succeeded),
    NAOS_ADD_TEST(one_value_type_can_be_given_to_several_definitions),
    NAOS_ADD_TEST(a_required_option_with_a_default_value_is_reported_before_its_default_value_is_checked),
    NAOS_ADD_TEST(the_options_are_checked_one_by_one_in_the_order_of_their_definition),
    NAOS_ADD_TEST(the_default_values_of_the_positionals_are_checked_after_the_options),
    NAOS_ADD_TEST(a_default_value_of_a_command_is_checked_before_its_subcommands),
    NAOS_ADD_TEST(an_invalid_default_value_leaves_the_tree_not_validated_and_unchanged),
    NAOS_ADD_TEST(a_validation_that_cannot_allocate_the_area_of_a_value_fails_for_a_lack_of_memory),
    NAOS_ADD_TEST(a_lack_of_memory_of_a_user_defined_conversion_is_returned_as_it_is),
    NAOS_ADD_TEST(a_lack_of_memory_of_a_built_in_conversion_is_returned_as_it_is)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(valid),
    NAOS_ADD_SUITE(positional_order),
    NAOS_ADD_SUITE(required_default),
    NAOS_ADD_SUITE(traversal),
    NAOS_ADD_SUITE(state),
    NAOS_ADD_SUITE(defaults)
)
