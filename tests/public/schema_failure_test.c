/*
 * Tests of the failure of an allocation, across the schema: the functions
 * of <clargs/type.h> and <clargs/schema.h> that allocate.
 *
 * Each test takes one operation that is made of many calls and makes its
 * first, second, ... allocation request fail in turn, in the shape that
 * docs/conventions.md gives under the failure of every allocation in turn.
 * The operations are:
 *
 * 1. building a tree with subcommands, the four kinds of options, and
 *    positionals,
 * 2. setting every attribute, and setting it again,
 * 3. creating a choice value type and a user-defined value type,
 * 4. validating a tree that has default values, twice.
 *
 * The call that fails must return CLARGS_ERROR_OUT_OF_MEMORY, write NULL to
 * its outputs, keep no block of its own, and leave the value types and the
 * whole tree as they were before it: every count, every name, every
 * attribute, and whether the tree is validated. That is checked at the call
 * that failed, by comparing a picture of everything, taken just before the
 * call, with one taken after it. What exists is then destroyed, and nothing
 * may be left allocated.
 *
 * What a test does around a call (the pictures, the validation before a
 * call, the preparation of an operation) may allocate as well. So the
 * requests are counted, and the one to fail is set, call by call: only the
 * requests of the calls of the operation count.
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

/*
 * A number with more characters than the conversion of the built-in
 * floating-point type copies without an allocation of its own.
 */
#define LONG_FLOAT "0." "1234567890" "1234567890" "1234567890" "1234567890" \
                   "1234567890" "1234567890" "1234567890"

/* The most allocation requests that an operation is given to fail in. */
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

/* ---- The picture of the value types and the tree ------------------------- */

/*
 * Everything that can be read of the value types and of the tree, as one
 * string. Two pictures are equal exactly when nothing that can be read has
 * changed.
 */
typedef struct picture
{
    char text[16384];
    size_t length;
    bool overflow;
} picture_t;

static void put(picture_t *picture, const char *text)
{
    const size_t length = strlen(text);

    if (length >= (sizeof(picture->text) - picture->length))
    {
        picture->overflow = true;
        return;
    }
    memcpy(picture->text + picture->length, text, length + 1);
    picture->length += length;
}

static void put_count(picture_t *picture, const char *label, size_t count)
{
    char digits[32];
    size_t at = sizeof(digits) - 1;

    digits[at] = '\0';
    do
    {
        --at;
        digits[at] = (char)('0' + (int)(count % 10));
        count /= 10;
    } while (count != 0);

    put(picture, label);
    put(picture, digits + at);
}

/*
 * A string that may not be set: an attribute, or a name that a kind of
 * option does not have.
 */
static void put_text(picture_t *picture, const char *label, const char *text)
{
    put(picture, label);
    if (text == NULL)
    {
        put(picture, "(unset)");
    }
    else
    {
        put(picture, "'");
        put(picture, text);
        put(picture, "'");
    }
}

static void put_type(picture_t *picture, const char *label, const clargs_type_t *type)
{
    size_t index;

    put(picture, label);
    if (type == NULL)
    {
        put(picture, "(none)");
        return;
    }
    put_text(picture, "", clargs_type_name(type));
    put_count(picture, " kind=", (size_t)clargs_type_kind(type));
    put_count(picture, " size=", clargs_type_value_size(type));
    put_count(picture, " choices=", clargs_type_choice_count(type));
    for (index = 0; index < clargs_type_choice_count(type); ++index)
    {
        put_text(picture, " ", clargs_type_choice_at(type, index));
    }
}

static void put_option(picture_t *picture, const clargs_option_t *option)
{
    char short_name[2];

    short_name[0] = clargs_option_short_name(option);
    short_name[1] = '\0';

    put_count(picture, "  option kind=", (size_t)clargs_option_kind(option));
    put_text(picture, " short=", short_name);
    put_text(picture, " long=", clargs_option_long_name(option));
    put_text(picture, " negated=", clargs_option_negated_name(option));
    put_type(picture, " type=", clargs_option_type(option));
    put_text(picture, " description=", clargs_option_description(option));
    put_text(picture, " value_name=", clargs_option_value_name(option));
    put_count(picture, " required=", clargs_option_is_required(option) ? 1u : 0u);
    put_text(picture, " default=", clargs_option_default(option));
    put_count(picture, " initial=", clargs_option_initial(option) ? 1u : 0u);
    put(picture, "\n");
}

static void put_positional(picture_t *picture, const clargs_positional_t *positional)
{
    put_text(picture, "  positional ", clargs_positional_name(positional));
    put_count(picture, " arity=", (size_t)clargs_positional_arity(positional));
    put_type(picture, " type=", clargs_positional_type(positional));
    put_text(picture, " description=", clargs_positional_description(positional));
    put_text(picture, " default=", clargs_positional_default(positional));
    put(picture, "\n");
}

/* The command, and then every command below it. */
static void put_command(picture_t *picture, const clargs_command_t *command)
{
    const clargs_command_t *parent = clargs_command_parent(command);
    size_t index;

    put_text(picture, "command ", clargs_command_name(command));
    put_text(picture, " parent=", (parent != NULL) ? clargs_command_name(parent) : NULL);
    put_text(picture, " description=", clargs_command_description(command));
    put_count(picture, " options=", clargs_command_option_count(command));
    put_count(picture, " positionals=", clargs_command_positional_count(command));
    put_count(picture, " subcommands=", clargs_command_subcommand_count(command));
    put(picture, "\n");

    for (index = 0; index < clargs_command_option_count(command); ++index)
    {
        const clargs_option_t *option = clargs_command_option_at(command, index);

        NAOS_CHECK_TRUE(clargs_option_command(option) == command);
        put_option(picture, option);
    }
    for (index = 0; index < clargs_command_positional_count(command); ++index)
    {
        const clargs_positional_t *positional = clargs_command_positional_at(command, index);

        NAOS_CHECK_TRUE(clargs_positional_command(positional) == command);
        put_positional(picture, positional);
    }
    for (index = 0; index < clargs_command_subcommand_count(command); ++index)
    {
        put_command(picture, clargs_command_subcommand_at(command, index));
    }
}

/* ---- One run of an operation --------------------------------------------- */

/*
 * The state of one run: which request of the operation fails, what exists,
 * and what the calls are given as the previous content of their outputs.
 */
typedef struct sweep
{
    /* The request of the operation that fails, from 1; 0 for none. */
    size_t nth;
    /* The requests that the calls of the operation have made so far. */
    size_t used;
    /* Whether a call of the operation has failed. The operation ends there. */
    bool failed;
    /*
     * Whether the tree is validated before every call, so that a call that
     * fails is also seen to leave a validated tree validated.
     */
    bool validates;

    /*
     * What the operation and its preparation created; NULL for what does
     * not exist.
     */
    clargs_type_t *choice;
    clargs_type_t *custom;
    clargs_command_t *root;

    /*
     * Handles of another tree and another value type. Every output holds
     * one of them before a call, so an output that a call does not write
     * does not pass for NULL.
     */
    clargs_command_t *other;
    clargs_option_t *other_option;
    clargs_positional_t *other_positional;
    clargs_type_t *other_type;

    /* Taken before the call that is running. */
    picture_t before;
    size_t requests_before;
    size_t live_before;
} sweep_t;

/* The handles of the tree that the operations build and work on. */
typedef struct tree
{
    clargs_command_t *build;
    clargs_command_t *run;
    clargs_option_t *verbose;
    clargs_option_t *color;
    clargs_option_t *quiet;
    clargs_option_t *output;
    clargs_option_t *ratio;
    clargs_option_t *mode;
    clargs_option_t *port;
    clargs_option_t *jobs;
    clargs_positional_t *target;
} tree_t;

static void draw(const sweep_t *sweep, picture_t *picture)
{
    picture->length = 0;
    picture->overflow = false;
    picture->text[0] = '\0';

    put_type(picture, "choice type: ", sweep->choice);
    put_type(picture, "\ncustom type: ", sweep->custom);
    put(picture, "\n");
    if (sweep->root == NULL)
    {
        put(picture, "no tree\n");
    }
    else
    {
        put_count(picture, "validated=", clargs_command_is_validated(sweep->root) ? 1u : 0u);
        put(picture, "\n");
        put_command(picture, sweep->root);
    }

    NAOS_REQUIRE_TRUE(!picture->overflow);
}

/* The conversion of the user-defined value type: a port number. */
static clargs_error_t port_convert(const char *text, void *value, void *context)
{
    int *out = value;
    int port = 0;
    size_t index;

    (void)context;

    if (text[0] == '\0')
    {
        return CLARGS_ERROR_INVALID_VALUE;
    }
    for (index = 0; text[index] != '\0'; ++index)
    {
        if ((text[index] < '0') || (text[index] > '9'))
        {
            return CLARGS_ERROR_INVALID_VALUE;
        }
        port = (port * 10) + (text[index] - '0');
        if (port > 65535)
        {
            return CLARGS_ERROR_INVALID_VALUE;
        }
    }
    *out = port;
    return CLARGS_ERROR_NONE;
}

/* Starts a run: nothing is allocated before, and nothing fails yet. */
static void open_sweep(sweep_t *sweep)
{
    /* Nothing is allocated here, so the counts can start again. */
    clargs_test_allocator_reset();

    sweep->nth = 0;
    sweep->used = 0;
    sweep->failed = false;
    sweep->validates = false;
    sweep->choice = NULL;
    sweep->custom = NULL;
    sweep->root = NULL;
    sweep->other = NULL;
    sweep->other_option = NULL;
    sweep->other_positional = NULL;
    sweep->other_type = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_create("other", &sweep->other) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_command_add_flag(sweep->other, 's', "sentinel", &sweep->other_option) ==
                      CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_command_add_positional(sweep->other, "sentinel", clargs_type_string(),
                                                    CLARGS_ARITY_REQUIRED,
                                                    &sweep->other_positional) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_type_create("other", sizeof(int), port_convert, NULL, NULL,
                                         &sweep->other_type) == CLARGS_ERROR_NONE);
}

/*
 * Ends a run: destroys what exists, the trees before the value types that
 * they borrow. Nothing is left and nothing was violated.
 */
static void close_sweep(sweep_t *sweep)
{
    clargs_command_destroy(sweep->root);
    clargs_command_destroy(sweep->other);
    clargs_type_destroy(sweep->choice);
    clargs_type_destroy(sweep->custom);
    clargs_type_destroy(sweep->other_type);
    check_clean();
}

/*
 * Makes the calls that follow the operation, with its nth request to fail;
 * 0 for none.
 */
static void arm_sweep(sweep_t *sweep, size_t nth, bool validates)
{
    sweep->nth = nth;
    sweep->used = 0;
    sweep->failed = false;
    sweep->validates = validates;
}

/*
 * Comes before every call of an operation: takes the picture, and sets the
 * request to fail if it is still ahead. With validates, the tree is
 * validated first; the operations keep it valid at every step.
 */
static void begin_call(sweep_t *sweep)
{
    if (sweep->validates && (sweep->root != NULL))
    {
        NAOS_REQUIRE_TRUE(clargs_command_validate(sweep->root, NULL, NULL, NULL) == CLARGS_ERROR_NONE);
        NAOS_REQUIRE_TRUE(clargs_command_is_validated(sweep->root));
    }

    draw(sweep, &sweep->before);
    sweep->live_before = clargs_test_allocator_live();
    sweep->requests_before = clargs_test_allocator_requests();
    if (sweep->nth > sweep->used)
    {
        clargs_test_allocator_fail_at(sweep->requests_before + (sweep->nth - sweep->used));
    }
}

/*
 * Comes after every call of an operation, with what the call returned and
 * whether all of its outputs hold NULL. Returns whether the call succeeded.
 * For a call that failed, checks everything that a failure must keep.
 */
static bool end_call(sweep_t *sweep, clargs_error_t error, bool outputs_are_null)
{
    static picture_t after;

    sweep->used += clargs_test_allocator_requests() - sweep->requests_before;
    clargs_test_allocator_fail_at(0);

    if (error == CLARGS_ERROR_NONE)
    {
        return true;
    }
    sweep->failed = true;

    /*
     * The request that was set to fail failed, it was the last one of the
     * call, and the call reported it.
     */
    NAOS_CHECK_INT_EQ(error, CLARGS_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_TRUE(sweep->nth != 0);
    NAOS_CHECK_UINT_EQ(sweep->used, sweep->nth);
    NAOS_CHECK_TRUE(outputs_are_null);

    /* The call kept no block and freed none that it did not allocate. */
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), sweep->live_before);

    /* The value types and the tree are as they were before the call. */
    draw(sweep, &after);
    NAOS_CHECK_STR_EQ(after.text, sweep->before.text);
    if (sweep->validates && (sweep->root != NULL))
    {
        NAOS_CHECK_TRUE(clargs_command_is_validated(sweep->root));
    }
    return false;
}

/* ---- The calls ----------------------------------------------------------- */

/*
 * Each makes one call of the public API between begin_call() and end_call()
 * and returns whether the call succeeded.
 */

static bool create_choice_type(sweep_t *sweep)
{
    static const char *const choices[] = { "fast", "slow", "exact" };
    clargs_type_t *type = sweep->other_type;
    clargs_error_t error;

    begin_call(sweep);
    error = clargs_type_create_choice(choices, sizeof(choices) / sizeof(choices[0]), &type);
    if (!end_call(sweep, error, type == NULL))
    {
        return false;
    }
    NAOS_REQUIRE_TRUE((type != NULL) && (type != sweep->other_type));
    sweep->choice = type;
    return true;
}

static bool create_custom_type(sweep_t *sweep)
{
    clargs_type_t *type = sweep->other_type;
    clargs_error_t error;

    begin_call(sweep);
    error = clargs_type_create("port", sizeof(int), port_convert, NULL, NULL, &type);
    if (!end_call(sweep, error, type == NULL))
    {
        return false;
    }
    NAOS_REQUIRE_TRUE((type != NULL) && (type != sweep->other_type));
    sweep->custom = type;
    return true;
}

static bool create_root(sweep_t *sweep)
{
    clargs_command_t *root = sweep->other;
    clargs_error_t error;

    begin_call(sweep);
    error = clargs_command_create("app", &root);
    if (!end_call(sweep, error, root == NULL))
    {
        return false;
    }
    NAOS_REQUIRE_TRUE((root != NULL) && (root != sweep->other));
    sweep->root = root;
    return true;
}

static bool add_subcommand(sweep_t *sweep, clargs_command_t *command, const char *name,
                           clargs_command_t **out)
{
    clargs_command_t *sub = sweep->other;
    clargs_error_t error;

    begin_call(sweep);
    error = clargs_command_add_subcommand(command, name, &sub);
    if (!end_call(sweep, error, sub == NULL))
    {
        return false;
    }
    NAOS_REQUIRE_TRUE((sub != NULL) && (sub != sweep->other));
    if (out != NULL)
    {
        *out = sub;
    }
    return true;
}

/*
 * type is the value type of an option that takes a value, and is not used
 * for another kind.
 */
static bool add_option(sweep_t *sweep, clargs_command_t *command, clargs_option_kind_t kind,
                       char short_name, const char *long_name, const clargs_type_t *type,
                       clargs_option_t **out)
{
    clargs_option_t *option = sweep->other_option;
    clargs_error_t error = CLARGS_ERROR_NONE;

    begin_call(sweep);
    switch (kind)
    {
    case CLARGS_OPTION_FLAG:
        error = clargs_command_add_flag(command, short_name, long_name, &option);
        break;
    case CLARGS_OPTION_NEGATABLE:
        error = clargs_command_add_negatable(command, short_name, long_name, &option);
        break;
    case CLARGS_OPTION_COUNT:
        error = clargs_command_add_count(command, short_name, long_name, &option);
        break;
    case CLARGS_OPTION_VALUE:
        error = clargs_command_add_value(command, short_name, long_name, type, &option);
        break;
    default:
        NAOS_FAIL("a test asked for an unknown kind of option");
        break;
    }
    if (!end_call(sweep, error, option == NULL))
    {
        return false;
    }
    NAOS_REQUIRE_TRUE((option != NULL) && (option != sweep->other_option));
    if (out != NULL)
    {
        *out = option;
    }
    return true;
}

static bool add_positional(sweep_t *sweep, clargs_command_t *command, const char *name,
                           const clargs_type_t *type, clargs_arity_t arity,
                           clargs_positional_t **out)
{
    clargs_positional_t *positional = sweep->other_positional;
    clargs_error_t error;

    begin_call(sweep);
    error = clargs_command_add_positional(command, name, type, arity, &positional);
    if (!end_call(sweep, error, positional == NULL))
    {
        return false;
    }
    NAOS_REQUIRE_TRUE((positional != NULL) && (positional != sweep->other_positional));
    if (out != NULL)
    {
        *out = positional;
    }
    return true;
}

/* The attributes that are strings. A set has no output. */
typedef enum text_attribute
{
    COMMAND_DESCRIPTION,
    OPTION_DESCRIPTION,
    OPTION_VALUE_NAME,
    OPTION_DEFAULT,
    POSITIONAL_DESCRIPTION,
    POSITIONAL_DEFAULT
} text_attribute_t;

/* The attributes of an option that are not strings. */
typedef enum bool_attribute
{
    OPTION_REQUIRED,
    OPTION_INITIAL
} bool_attribute_t;

static bool set_command_text(sweep_t *sweep, clargs_command_t *command, const char *text)
{
    clargs_error_t error;

    begin_call(sweep);
    error = clargs_command_set_description(command, text);
    if (!end_call(sweep, error, true))
    {
        return false;
    }
    NAOS_CHECK_STR_EQ(clargs_command_description(command), text);
    return true;
}

static bool set_option_text(sweep_t *sweep, text_attribute_t attribute, clargs_option_t *option,
                            const char *text)
{
    clargs_error_t error = CLARGS_ERROR_NONE;
    const char *read = NULL;

    begin_call(sweep);
    switch (attribute)
    {
    case OPTION_DESCRIPTION:
        error = clargs_option_set_description(option, text);
        read = clargs_option_description(option);
        break;
    case OPTION_VALUE_NAME:
        error = clargs_option_set_value_name(option, text);
        read = clargs_option_value_name(option);
        break;
    case OPTION_DEFAULT:
        error = clargs_option_set_default(option, text);
        read = clargs_option_default(option);
        break;
    case COMMAND_DESCRIPTION:
    case POSITIONAL_DESCRIPTION:
    case POSITIONAL_DEFAULT:
    default:
        NAOS_FAIL("a test asked for an attribute that an option does not have");
        break;
    }
    if (!end_call(sweep, error, true))
    {
        return false;
    }
    NAOS_REQUIRE_TRUE(read != NULL);
    NAOS_CHECK_STR_EQ(read, text);
    return true;
}

static bool set_positional_text(sweep_t *sweep, text_attribute_t attribute,
                                clargs_positional_t *positional, const char *text)
{
    clargs_error_t error = CLARGS_ERROR_NONE;
    const char *read = NULL;

    begin_call(sweep);
    switch (attribute)
    {
    case POSITIONAL_DESCRIPTION:
        error = clargs_positional_set_description(positional, text);
        read = clargs_positional_description(positional);
        break;
    case POSITIONAL_DEFAULT:
        error = clargs_positional_set_default(positional, text);
        read = clargs_positional_default(positional);
        break;
    case COMMAND_DESCRIPTION:
    case OPTION_DESCRIPTION:
    case OPTION_VALUE_NAME:
    case OPTION_DEFAULT:
    default:
        NAOS_FAIL("a test asked for an attribute that a positional does not have");
        break;
    }
    if (!end_call(sweep, error, true))
    {
        return false;
    }
    NAOS_REQUIRE_TRUE(read != NULL);
    NAOS_CHECK_STR_EQ(read, text);
    return true;
}

static bool set_option_bool(sweep_t *sweep, bool_attribute_t attribute, clargs_option_t *option,
                            bool value)
{
    clargs_error_t error = CLARGS_ERROR_NONE;
    bool read = !value;

    begin_call(sweep);
    switch (attribute)
    {
    case OPTION_REQUIRED:
        error = clargs_option_set_required(option, value);
        read = clargs_option_is_required(option);
        break;
    case OPTION_INITIAL:
        error = clargs_option_set_initial(option, value);
        read = clargs_option_initial(option);
        break;
    default:
        NAOS_FAIL("a test asked for an unknown attribute");
        break;
    }
    if (!end_call(sweep, error, true))
    {
        return false;
    }
    NAOS_CHECK_TRUE(read == value);
    return true;
}

/* A validation of the tree, which has no fault. */
static bool validate_tree(sweep_t *sweep)
{
    const clargs_command_t *command = sweep->other;
    const clargs_option_t *option = sweep->other_option;
    const clargs_positional_t *positional = sweep->other_positional;
    clargs_error_t error;

    begin_call(sweep);
    error = clargs_command_validate(sweep->root, &command, &option, &positional);
    if (!end_call(sweep, error, (command == NULL) && (option == NULL) && (positional == NULL)))
    {
        return false;
    }
    NAOS_CHECK_TRUE((command == NULL) && (option == NULL) && (positional == NULL));
    NAOS_CHECK_TRUE(clargs_command_is_validated(sweep->root));
    return true;
}

/* ---- The operations ------------------------------------------------------ */

/*
 * Each makes its calls in order, stops at the first one that fails, and
 * returns whether all of them succeeded.
 */

/*
 * How many calls each operation makes that cannot succeed without a
 * request: the least number of requests that it must be seen to make.
 */
#define CREATE_TYPES_CALLS 2
#define BUILD_TREE_CALLS 21
#define SET_ATTRIBUTES_CALLS 28
#define VALIDATE_TWICE_REQUESTS 14

/* Operation 3: a choice value type and a user-defined value type. */
static bool create_types(sweep_t *sweep)
{
    return create_choice_type(sweep) && create_custom_type(sweep);
}

/*
 * Operation 1: a root with seven options, of the four kinds and with the
 * five kinds of value types, and five subcommands; a subcommand with an
 * option and five positionals; a subcommand with an option and a
 * positional. Seven and five are more than the first capacity of an array
 * of the library, so the growth of the arrays is made to fail as well.
 *
 * The tree has no fault at any step. It uses the value types of
 * create_types().
 */
static bool build_tree(sweep_t *sweep, tree_t *tree)
{
    clargs_command_t *root;

    if (!create_root(sweep))
    {
        return false;
    }
    root = sweep->root;

    return add_option(sweep, root, CLARGS_OPTION_FLAG, 'v', "verbose", NULL, &tree->verbose) &&
           add_option(sweep, root, CLARGS_OPTION_NEGATABLE, '\0', "color", NULL, &tree->color) &&
           add_subcommand(sweep, root, "build", &tree->build) &&
           add_option(sweep, root, CLARGS_OPTION_COUNT, 'q', "quiet", NULL, &tree->quiet) &&
           add_option(sweep, root, CLARGS_OPTION_VALUE, 'o', "output", clargs_type_string(),
                      &tree->output) &&
           add_subcommand(sweep, root, "run", &tree->run) &&
           add_option(sweep, root, CLARGS_OPTION_VALUE, '\0', "ratio", clargs_type_float(),
                      &tree->ratio) &&
           add_option(sweep, root, CLARGS_OPTION_VALUE, 'm', "mode", sweep->choice, &tree->mode) &&
           add_option(sweep, root, CLARGS_OPTION_VALUE, 'p', NULL, sweep->custom, &tree->port) &&
           add_subcommand(sweep, root, "test", NULL) &&
           add_subcommand(sweep, root, "bench", NULL) &&
           add_subcommand(sweep, root, "clean", NULL) &&
           add_option(sweep, tree->build, CLARGS_OPTION_VALUE, 'j', "jobs", clargs_type_int(),
                      &tree->jobs) &&
           add_positional(sweep, tree->build, "source", clargs_type_string(),
                          CLARGS_ARITY_REQUIRED, NULL) &&
           add_positional(sweep, tree->build, "level", clargs_type_int(), CLARGS_ARITY_REQUIRED,
                          NULL) &&
           add_positional(sweep, tree->build, "first", sweep->choice, CLARGS_ARITY_REQUIRED, NULL) &&
           add_positional(sweep, tree->build, "second", sweep->custom, CLARGS_ARITY_REQUIRED, NULL) &&
           add_positional(sweep, tree->build, "target", clargs_type_string(),
                          CLARGS_ARITY_OPTIONAL, &tree->target) &&
           add_option(sweep, tree->run, CLARGS_OPTION_FLAG, 'v', "verbose", NULL, NULL) &&
           add_positional(sweep, tree->run, "args", clargs_type_string(),
                          CLARGS_ARITY_ZERO_OR_MORE, NULL);
}

/*
 * Operation 2: every attribute of a command, of each kind of option and of
 * a positional, set and then set again to another value. The tree, built by
 * build_tree(), has no fault at any step, and has a default value of every
 * kind of value type afterwards.
 */
static bool set_attributes(sweep_t *sweep, const tree_t *tree)
{
    return set_command_text(sweep, sweep->root, "An application.") &&
           set_command_text(sweep, sweep->root, "The application, described again.") &&
           set_command_text(sweep, tree->build, "Builds.") &&
           set_command_text(sweep, tree->build, "") &&
           set_option_text(sweep, OPTION_DESCRIPTION, tree->verbose, "Says more.") &&
           set_option_text(sweep, OPTION_DESCRIPTION, tree->verbose, "Says a lot more than before.") &&
           set_option_text(sweep, OPTION_DESCRIPTION, tree->color, "Colors the output.") &&
           set_option_text(sweep, OPTION_DESCRIPTION, tree->color, "Colors.") &&
           set_option_bool(sweep, OPTION_INITIAL, tree->color, true) &&
           set_option_bool(sweep, OPTION_INITIAL, tree->color, false) &&
           set_option_text(sweep, OPTION_DESCRIPTION, tree->quiet, "Says less.") &&
           set_option_text(sweep, OPTION_DESCRIPTION, tree->quiet, "Says less, each time it is given.") &&
           set_option_text(sweep, OPTION_DESCRIPTION, tree->output, "Where the output goes.") &&
           set_option_text(sweep, OPTION_DESCRIPTION, tree->output, "The file of the output.") &&
           set_option_text(sweep, OPTION_VALUE_NAME, tree->output, "FILE") &&
           set_option_text(sweep, OPTION_VALUE_NAME, tree->output, "PATH") &&
           set_option_bool(sweep, OPTION_REQUIRED, tree->output, true) &&
           set_option_bool(sweep, OPTION_REQUIRED, tree->output, false) &&
           set_option_text(sweep, OPTION_DEFAULT, tree->output, "out.txt") &&
           set_option_text(sweep, OPTION_DEFAULT, tree->output, "a.out") &&
           set_option_text(sweep, OPTION_DEFAULT, tree->ratio, "0.5") &&
           set_option_text(sweep, OPTION_DEFAULT, tree->ratio, LONG_FLOAT) &&
           set_option_text(sweep, OPTION_DEFAULT, tree->mode, "fast") &&
           set_option_text(sweep, OPTION_DEFAULT, tree->mode, "exact") &&
           set_option_text(sweep, OPTION_DEFAULT, tree->port, "80") &&
           set_option_text(sweep, OPTION_DEFAULT, tree->port, "8080") &&
           set_option_text(sweep, OPTION_DEFAULT, tree->jobs, "1") &&
           set_option_text(sweep, OPTION_DEFAULT, tree->jobs, "-4") &&
           set_positional_text(sweep, POSITIONAL_DESCRIPTION, tree->target, "What to build.") &&
           set_positional_text(sweep, POSITIONAL_DESCRIPTION, tree->target, "The target to build.") &&
           set_positional_text(sweep, POSITIONAL_DEFAULT, tree->target, "all") &&
           set_positional_text(sweep, POSITIONAL_DEFAULT, tree->target, "everything");
}

/*
 * Operation 4: the validation of the tree that set_attributes() leaves,
 * with its six default values, one of them LONG_FLOAT: first of a tree that
 * is not validated, then of one that is. A validation that fails must leave
 * the tree in the state that it had.
 */
static bool validate_twice(sweep_t *sweep)
{
    return validate_tree(sweep) && validate_tree(sweep);
}

/* ---- The tests ----------------------------------------------------------- */

typedef enum operation
{
    OPERATION_BUILD_TREE,
    OPERATION_SET_ATTRIBUTES,
    OPERATION_CREATE_TYPES,
    OPERATION_VALIDATE
} operation_t;

/*
 * Prepares the operation without a failure, then makes the nth request of
 * the operation fail and runs it. Returns whether the operation completed.
 * *least gets the least number of requests that the operation makes.
 */
static bool run_operation(sweep_t *sweep, operation_t operation, size_t nth, size_t *least)
{
    tree_t tree;

    memset(&tree, 0, sizeof(tree));
    *least = 1;

    switch (operation)
    {
    case OPERATION_BUILD_TREE:
        NAOS_REQUIRE_TRUE(create_types(sweep));
        arm_sweep(sweep, nth, true);
        *least = BUILD_TREE_CALLS;
        if (!build_tree(sweep, &tree))
        {
            return false;
        }
        NAOS_CHECK_UINT_EQ(clargs_command_option_count(sweep->root), 7);
        NAOS_CHECK_UINT_EQ(clargs_command_subcommand_count(sweep->root), 5);
        NAOS_CHECK_UINT_EQ(clargs_command_option_count(tree.build), 1);
        NAOS_CHECK_UINT_EQ(clargs_command_positional_count(tree.build), 5);
        NAOS_CHECK_UINT_EQ(clargs_command_positional_count(tree.run), 1);
        NAOS_CHECK_FALSE(clargs_command_is_validated(sweep->root));
        return true;

    case OPERATION_SET_ATTRIBUTES:
        NAOS_REQUIRE_TRUE(create_types(sweep));
        NAOS_REQUIRE_TRUE(build_tree(sweep, &tree));
        arm_sweep(sweep, nth, true);
        *least = SET_ATTRIBUTES_CALLS;
        if (!set_attributes(sweep, &tree))
        {
            return false;
        }
        NAOS_CHECK_FALSE(clargs_command_is_validated(sweep->root));
        return true;

    case OPERATION_CREATE_TYPES:
        arm_sweep(sweep, nth, false);
        *least = CREATE_TYPES_CALLS;
        if (!create_types(sweep))
        {
            return false;
        }
        NAOS_CHECK_TRUE(clargs_type_kind(sweep->choice) == CLARGS_TYPE_CHOICE);
        NAOS_CHECK_UINT_EQ(clargs_type_choice_count(sweep->choice), 3);
        NAOS_CHECK_STR_EQ(clargs_type_choice_at(sweep->choice, 2), "exact");
        NAOS_CHECK_TRUE(clargs_type_kind(sweep->custom) == CLARGS_TYPE_CUSTOM);
        NAOS_CHECK_STR_EQ(clargs_type_name(sweep->custom), "port");
        return true;

    case OPERATION_VALIDATE:
        NAOS_REQUIRE_TRUE(create_types(sweep));
        NAOS_REQUIRE_TRUE(build_tree(sweep, &tree));
        NAOS_REQUIRE_TRUE(set_attributes(sweep, &tree));
        arm_sweep(sweep, nth, false);
        *least = VALIDATE_TWICE_REQUESTS;
        if (!validate_twice(sweep))
        {
            return false;
        }
        NAOS_CHECK_TRUE(clargs_command_is_validated(sweep->root));
        return true;

    default:
        NAOS_FAIL("a test asked for an unknown operation");
        /* NAOS_FAIL does not return; nothing may follow it (MSVC C4702). */
    }
}

/*
 * Makes the first, second, ... request of the operation fail, until the
 * operation completes without reaching the request to fail.
 */
static void fail_every_allocation_of(operation_t operation)
{
    static sweep_t sweep;
    bool completed = false;
    size_t n;

    clargs_test_allocator_install();

    for (n = 1; n <= REQUEST_LIMIT; ++n)
    {
        size_t least = 0;
        bool succeeded;

        open_sweep(&sweep);

        /* The operation. */
        succeeded = run_operation(&sweep, operation, n, &least);

        if (succeeded && (sweep.used < n))
        {
            /*
             * The operation ended before the request to fail was reached:
             * every allocation point has failed once. The operation is not
             * one without requests.
             */
            NAOS_CHECK_FALSE(sweep.failed);
            NAOS_CHECK_UINT_EQ(sweep.used, n - 1);
            NAOS_CHECK_TRUE(sweep.used >= least);
            close_sweep(&sweep);
            completed = true;
            break;
        }

        /*
         * The nth request failed, and the call that made it reported it.
         * end_call() has checked the error code, the outputs, and that the
         * value types and the tree are as they were before that call.
         */
        NAOS_CHECK_FALSE(succeeded);
        NAOS_CHECK_TRUE(sweep.failed);
        NAOS_CHECK_UINT_EQ(sweep.used, n);

        /* Destroy what exists; nothing is left and nothing was violated. */
        close_sweep(&sweep);
    }

    /* The loop ended because the operation completed, not at the limit. */
    NAOS_CHECK_TRUE(completed);

    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(building_a_tree_fails_cleanly_at_every_allocation_point)
{
    fail_every_allocation_of(OPERATION_BUILD_TREE);
}

NAOS_TEST(setting_every_attribute_twice_fails_cleanly_at_every_allocation_point)
{
    fail_every_allocation_of(OPERATION_SET_ATTRIBUTES);
}

NAOS_TEST(creating_value_types_fails_cleanly_at_every_allocation_point)
{
    fail_every_allocation_of(OPERATION_CREATE_TYPES);
}

NAOS_TEST(validating_default_values_fails_cleanly_at_every_allocation_point)
{
    fail_every_allocation_of(OPERATION_VALIDATE);
}

NAOS_SUITE(schema_failure,
    NAOS_ADD_TEST(building_a_tree_fails_cleanly_at_every_allocation_point),
    NAOS_ADD_TEST(setting_every_attribute_twice_fails_cleanly_at_every_allocation_point),
    NAOS_ADD_TEST(creating_value_types_fails_cleanly_at_every_allocation_point),
    NAOS_ADD_TEST(validating_default_values_fails_cleanly_at_every_allocation_point)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(schema_failure)
)
