/*
 * Tests of the usage line: clargs_help_write_usage() of <clargs/help.h>.
 *
 * Each test builds a schema, validates it, has the usage line of one of its
 * commands written and compares the text with the expected string, by its
 * length and by its bytes. The write function of the tests appends what it
 * is given to an array of a fixed size.
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

/* The size of the text a sink holds: enough for every test. */
#define SINK_SIZE ((size_t)512)

/* What the write function of the tests has received. */
typedef struct sink {
    char text[SINK_SIZE]; /* the parts, joined in the order of the calls */
    size_t length;        /* the number of bytes of text */
    size_t calls;         /* the number of calls, the failed one included */
    size_t empty_calls;   /* the calls with the length 0 or without a text */
    size_t fail_at;       /* the call that reports a failure; 0: none */
} sink_t;

static void sink_init(sink_t *sink)
{
    memset(sink, 0, sizeof *sink);
}

/* Appends the part to the sink. The call number fail_at takes nothing and
 * reports a failure, and so would every later call. */
static bool sink_write(void *user_data, const char *text, size_t length)
{
    sink_t *sink = user_data;

    ++sink->calls;
    if (text == NULL || length == 0) {
        ++sink->empty_calls;
        return true;
    }
    if (sink->fail_at != 0 && sink->calls >= sink->fail_at) {
        return false;
    }
    if (length > SINK_SIZE - sink->length) {
        return false;
    }

    memcpy(sink->text + sink->length, text, length);
    sink->length += length;
    return true;
}

/* The end of every test: the test allocator has no live block and saw no
 * violation of the allocator contract. */
static void check_clean(void)
{
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
}

/* Installs the test allocator and creates a root with the name "tool". */
static clargs_command_t *create_root(void)
{
    clargs_command_t *root = NULL;

    clargs_test_allocator_install();
    NAOS_REQUIRE_TRUE(clargs_command_create("tool", &root) == CLARGS_ERROR_NONE);
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

/* Adds an option with a value that must succeed. value_name is its display
 * name, or NULL to set none. */
static void add_value(clargs_command_t *command, char short_name, const char *long_name,
                      const clargs_type_t *type, const char *value_name, bool required)
{
    clargs_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_value(command, short_name, long_name, type, &option) ==
                      CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(option != NULL);
    if (value_name != NULL) {
        NAOS_REQUIRE_TRUE(clargs_option_set_value_name(option, value_name) == CLARGS_ERROR_NONE);
    }
    if (required) {
        NAOS_REQUIRE_TRUE(clargs_option_set_required(option, true) == CLARGS_ERROR_NONE);
    }
}

/* Adds a positional of strings that must succeed. */
static void add_positional(clargs_command_t *command, const char *name, clargs_arity_t arity)
{
    NAOS_REQUIRE_TRUE(clargs_command_add_positional(command, name, clargs_type_string(), arity,
                                                    NULL) == CLARGS_ERROR_NONE);
}

/* Validates the tree of root; it must have no fault. */
static void validate(clargs_command_t *root)
{
    NAOS_REQUIRE_TRUE(clargs_command_validate(root, NULL, NULL, NULL) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_command_is_validated(root));
}

/*
 * Has the usage line of command written with options and checks that the
 * call succeeds, that the text is exactly the string expected, that no call
 * of the write function was empty, that nothing was allocated and that the
 * schema is still validated.
 */
static void check_usage_with(const clargs_command_t *command, const clargs_help_options_t *options,
                             const char *expected)
{
    sink_t sink;
    size_t requests = clargs_test_allocator_requests();

    sink_init(&sink);

    NAOS_CHECK_TRUE(clargs_help_write_usage(command, options, sink_write, &sink) ==
                    CLARGS_ERROR_NONE);

    NAOS_CHECK_UINT_EQ(sink.length, strlen(expected));
    NAOS_CHECK_MEM_EQ(sink.text, sink.length, expected, strlen(expected));
    NAOS_CHECK_UINT_EQ(sink.empty_calls, 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests);
    NAOS_CHECK_TRUE(clargs_command_is_validated(command));
}

/* The same without help options: every setting has its default value. */
static void check_usage(const clargs_command_t *command, const char *expected)
{
    check_usage_with(command, NULL, expected);
}

/* ---- The command alone --------------------------------------------------- */

/* A command with no option, no positional and no subcommand: "Usage: " and
 * the name, nothing else. */
NAOS_TEST(command_with_nothing_is_only_its_name)
{
    clargs_command_t *root = create_root();

    validate(root);
    check_usage(root, "Usage: tool\n");

    destroy_root(root);
}

/* ---- [OPTIONS] ----------------------------------------------------------- */

/* An option that is not required, of any of the four kinds, gives
 * [OPTIONS], and is not written itself. */
NAOS_TEST(option_that_is_not_required_gives_options_marker)
{
    clargs_command_t *root;

    root = create_root();
    NAOS_REQUIRE_TRUE(clargs_command_add_flag(root, 'h', "help", NULL) == CLARGS_ERROR_NONE);
    validate(root);
    check_usage(root, "Usage: tool [OPTIONS]\n");
    destroy_root(root);

    root = create_root();
    NAOS_REQUIRE_TRUE(clargs_command_add_negatable(root, '\0', "color", NULL) == CLARGS_ERROR_NONE);
    validate(root);
    check_usage(root, "Usage: tool [OPTIONS]\n");
    destroy_root(root);

    root = create_root();
    NAOS_REQUIRE_TRUE(clargs_command_add_count(root, 'v', NULL, NULL) == CLARGS_ERROR_NONE);
    validate(root);
    check_usage(root, "Usage: tool [OPTIONS]\n");
    destroy_root(root);

    root = create_root();
    add_value(root, 'f', "format", clargs_type_string(), "FMT", false);
    validate(root);
    check_usage(root, "Usage: tool [OPTIONS]\n");
    destroy_root(root);
}

/* Several options that are not required give one [OPTIONS]. */
NAOS_TEST(several_options_give_one_options_marker)
{
    clargs_command_t *root = create_root();

    NAOS_REQUIRE_TRUE(clargs_command_add_flag(root, 'h', "help", NULL) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_command_add_count(root, 'v', "verbose", NULL) == CLARGS_ERROR_NONE);
    add_value(root, 'f', "format", clargs_type_string(), NULL, false);
    validate(root);
    check_usage(root, "Usage: tool [OPTIONS]\n");

    destroy_root(root);
}

/* A command whose options are all required has no [OPTIONS]. */
NAOS_TEST(only_required_options_give_no_options_marker)
{
    clargs_command_t *root = create_root();

    add_value(root, 'o', "output", clargs_type_string(), "FILE", true);
    validate(root);
    check_usage(root, "Usage: tool --output <FILE>\n");

    destroy_root(root);
}

/* ---- Required options ---------------------------------------------------- */

/* A required option with a long name is written with the long name, also
 * when it has a short name. */
NAOS_TEST(required_option_uses_long_name)
{
    clargs_command_t *root;

    root = create_root();
    add_value(root, '\0', "output", clargs_type_string(), "FILE", true);
    validate(root);
    check_usage(root, "Usage: tool --output <FILE>\n");
    destroy_root(root);

    root = create_root();
    add_value(root, 'o', "output", clargs_type_string(), "FILE", true);
    validate(root);
    check_usage(root, "Usage: tool --output <FILE>\n");
    destroy_root(root);
}

/* A required option without a long name is written with the short name. */
NAOS_TEST(required_option_without_long_name_uses_short_name)
{
    clargs_command_t *root = create_root();

    add_value(root, 'o', NULL, clargs_type_string(), "FILE", true);
    validate(root);
    check_usage(root, "Usage: tool -o <FILE>\n");

    destroy_root(root);
}

/* The conversion function of the user-defined value type of the tests. No
 * test converts a value with it. */
static clargs_error_t convert_nothing(const char *text, void *value, void *context)
{
    (void)text;
    (void)context;
    *(int *)value = 0;
    return CLARGS_ERROR_NONE;
}

/* Without a display name of the value, the name of the value type stands
 * in its place: of a built-in type, of a choice and of a user-defined
 * type. */
NAOS_TEST(required_option_without_value_name_uses_type_name)
{
    static const char *const choices[] = { "json", "yaml" };
    clargs_type_t *choice = NULL;
    clargs_type_t *port = NULL;
    clargs_command_t *root = create_root();

    NAOS_REQUIRE_TRUE(clargs_type_create_choice(choices, 2, &choice) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_type_create("Port number", sizeof(int), convert_nothing, NULL, NULL,
                                         &port) == CLARGS_ERROR_NONE);

    add_value(root, '\0', "name", clargs_type_string(), NULL, true);
    add_value(root, 'n', NULL, clargs_type_int(), NULL, true);
    add_value(root, '\0', "ratio", clargs_type_float(), NULL, true);
    add_value(root, '\0', "format", choice, NULL, true);
    add_value(root, 'p', "port", port, NULL, true);
    validate(root);
    check_usage(root, "Usage: tool --name <string> -n <integer> --ratio <float> --format <choice> "
                      "--port <Port number>\n");

    clargs_command_destroy(root);
    clargs_type_destroy(choice);
    clargs_type_destroy(port);
    clargs_test_allocator_uninstall();
    check_clean();
}

/* The required options come after [OPTIONS], in the order in which they
 * were added; the options between them that are not required are not
 * written. */
NAOS_TEST(required_options_follow_options_marker_in_definition_order)
{
    clargs_command_t *root = create_root();

    add_value(root, 'z', "zone", clargs_type_string(), "ZONE", true);
    NAOS_REQUIRE_TRUE(clargs_command_add_flag(root, 'h', "help", NULL) == CLARGS_ERROR_NONE);
    add_value(root, 'a', NULL, clargs_type_int(), "N", true);
    add_value(root, 'f', "format", clargs_type_string(), "FMT", false);
    add_value(root, '\0', "key", clargs_type_string(), "KEY", true);
    validate(root);
    check_usage(root, "Usage: tool [OPTIONS] --zone <ZONE> -a <N> --key <KEY>\n");

    destroy_root(root);
}

/* ---- Positionals --------------------------------------------------------- */

/* The four arities: <name>, [name], [name]... and <name>... */
NAOS_TEST(positionals_show_their_arity)
{
    clargs_command_t *root;

    root = create_root();
    add_positional(root, "input", CLARGS_ARITY_REQUIRED);
    validate(root);
    check_usage(root, "Usage: tool <input>\n");
    destroy_root(root);

    root = create_root();
    add_positional(root, "input", CLARGS_ARITY_OPTIONAL);
    validate(root);
    check_usage(root, "Usage: tool [input]\n");
    destroy_root(root);

    root = create_root();
    add_positional(root, "input", CLARGS_ARITY_ZERO_OR_MORE);
    validate(root);
    check_usage(root, "Usage: tool [input]...\n");
    destroy_root(root);

    root = create_root();
    add_positional(root, "input", CLARGS_ARITY_ONE_OR_MORE);
    validate(root);
    check_usage(root, "Usage: tool <input>...\n");
    destroy_root(root);
}

/* Several positionals come in the order in which they were added. */
NAOS_TEST(positionals_come_in_definition_order)
{
    clargs_command_t *root;

    root = create_root();
    add_positional(root, "source", CLARGS_ARITY_REQUIRED);
    add_positional(root, "target", CLARGS_ARITY_OPTIONAL);
    add_positional(root, "extra", CLARGS_ARITY_ZERO_OR_MORE);
    validate(root);
    check_usage(root, "Usage: tool <source> [target] [extra]...\n");
    destroy_root(root);

    root = create_root();
    add_positional(root, "zeta", CLARGS_ARITY_REQUIRED);
    add_positional(root, "alpha", CLARGS_ARITY_ONE_OR_MORE);
    validate(root);
    check_usage(root, "Usage: tool <zeta> <alpha>...\n");
    destroy_root(root);
}

/* The parts in their order: [OPTIONS], the required options, the
 * positionals. */
NAOS_TEST(options_come_before_positionals)
{
    clargs_command_t *root = create_root();

    add_positional(root, "input", CLARGS_ARITY_REQUIRED);
    add_positional(root, "extra", CLARGS_ARITY_ZERO_OR_MORE);
    add_value(root, 'o', "output", clargs_type_string(), "FILE", true);
    add_value(root, 'f', "format", clargs_type_string(), "FMT", false);
    NAOS_REQUIRE_TRUE(clargs_command_add_count(root, 'v', "verbose", NULL) == CLARGS_ERROR_NONE);
    validate(root);
    check_usage(root, "Usage: tool [OPTIONS] --output <FILE> <input> [extra]...\n");

    destroy_root(root);
}

/* ---- Subcommands --------------------------------------------------------- */

/* A command with subcommands ends with [COMMAND], once, whatever their
 * number. */
NAOS_TEST(subcommands_give_command_marker)
{
    clargs_command_t *root;

    root = create_root();
    add_sub(root, "build");
    validate(root);
    check_usage(root, "Usage: tool [COMMAND]\n");
    destroy_root(root);

    root = create_root();
    add_sub(root, "build");
    add_sub(root, "clean");
    NAOS_REQUIRE_TRUE(clargs_command_add_flag(root, 'h', "help", NULL) == CLARGS_ERROR_NONE);
    add_value(root, '\0', "key", clargs_type_string(), "KEY", true);
    validate(root);
    check_usage(root, "Usage: tool [OPTIONS] --key <KEY> [COMMAND]\n");
    destroy_root(root);
}

/* The path of a subcommand is written from the root down, and the line
 * shows what the subcommand itself has: not the options of its parent, and
 * not what its own subcommands have. */
NAOS_TEST(subcommand_usage_has_path_from_root)
{
    clargs_command_t *root = create_root();
    clargs_command_t *remote;
    clargs_command_t *add;
    clargs_command_t *list;

    NAOS_REQUIRE_TRUE(clargs_command_add_flag(root, 'h', "help", NULL) == CLARGS_ERROR_NONE);
    add_value(root, '\0', "key", clargs_type_string(), "KEY", true);
    remote = add_sub(root, "remote");
    add = add_sub(remote, "add");
    list = add_sub(remote, "list");
    add_positional(add, "name", CLARGS_ARITY_REQUIRED);
    add_positional(add, "url", CLARGS_ARITY_OPTIONAL);
    add_value(add, 't', "track", clargs_type_string(), "BRANCH", true);
    NAOS_REQUIRE_TRUE(clargs_command_add_flag(list, 'a', "all", NULL) == CLARGS_ERROR_NONE);
    validate(root);

    check_usage(root, "Usage: tool [OPTIONS] --key <KEY> [COMMAND]\n");
    check_usage(remote, "Usage: tool remote [COMMAND]\n");
    check_usage(add, "Usage: tool remote add --track <BRANCH> <name> [url]\n");
    check_usage(list, "Usage: tool remote list [OPTIONS]\n");

    destroy_root(root);
}

/* ---- The strings of the schema ------------------------------------------- */

/* Names are written as they are: upper and lower case, bytes that are not
 * ASCII, blanks and a newline. */
NAOS_TEST(schema_strings_are_written_unchanged)
{
    clargs_command_t *root = NULL;

    clargs_test_allocator_install();
    NAOS_REQUIRE_TRUE(clargs_command_create("My Tool", &root) == CLARGS_ERROR_NONE);

    add_value(root, 'O', "OutPut", clargs_type_string(), "File Name", true);
    add_value(root, 'x', NULL, clargs_type_string(), "\xE5\x80\xA4", true);
    add_positional(root, "In Put", CLARGS_ARITY_REQUIRED);
    add_positional(root, "\xC3\xA9t\xC3\xA9", CLARGS_ARITY_OPTIONAL);
    add_positional(root, "two\nlines", CLARGS_ARITY_ZERO_OR_MORE);
    validate(root);
    check_usage(root, "Usage: My Tool --OutPut <File Name> -x <\xE5\x80\xA4> <In Put> "
                      "[\xC3\xA9t\xC3\xA9] [two\nlines]...\n");

    destroy_root(root);
}

/* ---- The help options ---------------------------------------------------- */

/* Without help options (NULL), the text is the one that help options with
 * their default values give, and help options can be used for more than one
 * call. */
NAOS_TEST(null_options_are_the_default_options)
{
    static const char expected[] = "Usage: tool [OPTIONS] --output <FILE> <input> [extra]...\n";
    clargs_help_options_t *options = NULL;
    clargs_command_t *root = create_root();

    NAOS_REQUIRE_TRUE(clargs_help_options_create(&options) == CLARGS_ERROR_NONE);

    NAOS_REQUIRE_TRUE(clargs_command_add_flag(root, 'h', "help", NULL) == CLARGS_ERROR_NONE);
    add_value(root, 'o', "output", clargs_type_string(), "FILE", true);
    add_positional(root, "input", CLARGS_ARITY_REQUIRED);
    add_positional(root, "extra", CLARGS_ARITY_ZERO_OR_MORE);
    validate(root);

    check_usage_with(root, NULL, expected);
    check_usage_with(root, options, expected);
    check_usage_with(root, options, expected);
    check_usage_with(root, NULL, expected);

    clargs_help_options_destroy(options);
    destroy_root(root);
}

/* ---- The write function -------------------------------------------------- */

/* When the write function reports a failure, the call returns
 * CLARGS_ERROR_WRITE_FAILED, the write function is not called again, and
 * what it took before stays: the start of the text. */
NAOS_TEST(write_failure_is_reported_and_stops_the_calls)
{
    static const char expected[] = "Usage: tool [OPTIONS] --output <FILE> <input>\n";
    clargs_command_t *root = create_root();
    sink_t whole;
    size_t n;

    NAOS_REQUIRE_TRUE(clargs_command_add_flag(root, 'h', "help", NULL) == CLARGS_ERROR_NONE);
    add_value(root, 'o', "output", clargs_type_string(), "FILE", true);
    add_positional(root, "input", CLARGS_ARITY_REQUIRED);
    validate(root);

    sink_init(&whole);
    NAOS_REQUIRE_TRUE(clargs_help_write_usage(root, NULL, sink_write, &whole) == CLARGS_ERROR_NONE);
    NAOS_CHECK_MEM_EQ(whole.text, whole.length, expected, strlen(expected));
    NAOS_REQUIRE_TRUE(whole.calls >= 2);

    /* Each call in turn is the one that fails. */
    for (n = 1; n <= whole.calls; ++n) {
        sink_t sink;

        sink_init(&sink);
        sink.fail_at = n;

        NAOS_CHECK_TRUE(clargs_help_write_usage(root, NULL, sink_write, &sink) ==
                        CLARGS_ERROR_WRITE_FAILED);
        NAOS_CHECK_UINT_EQ(sink.calls, n);
        NAOS_CHECK_UINT_EQ(sink.empty_calls, 0);
        NAOS_CHECK_TRUE(sink.length < strlen(expected));
        NAOS_CHECK_MEM_EQ(sink.text, sink.length, expected, sink.length);
    }

    NAOS_CHECK_TRUE(clargs_command_is_validated(root));

    destroy_root(root);
}

NAOS_SUITE(help_usage,
    NAOS_ADD_TEST(command_with_nothing_is_only_its_name),
    NAOS_ADD_TEST(option_that_is_not_required_gives_options_marker),
    NAOS_ADD_TEST(several_options_give_one_options_marker),
    NAOS_ADD_TEST(only_required_options_give_no_options_marker),
    NAOS_ADD_TEST(required_option_uses_long_name),
    NAOS_ADD_TEST(required_option_without_long_name_uses_short_name),
    NAOS_ADD_TEST(required_option_without_value_name_uses_type_name),
    NAOS_ADD_TEST(required_options_follow_options_marker_in_definition_order),
    NAOS_ADD_TEST(positionals_show_their_arity),
    NAOS_ADD_TEST(positionals_come_in_definition_order),
    NAOS_ADD_TEST(options_come_before_positionals),
    NAOS_ADD_TEST(subcommands_give_command_marker),
    NAOS_ADD_TEST(subcommand_usage_has_path_from_root),
    NAOS_ADD_TEST(schema_strings_are_written_unchanged),
    NAOS_ADD_TEST(null_options_are_the_default_options),
    NAOS_ADD_TEST(write_failure_is_reported_and_stops_the_calls)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(help_usage)
)
