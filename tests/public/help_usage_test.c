/*
 * Tests of the usage line: argx_help_write_usage() of <argx/help.h>.
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
#include <argx/argx.h>

#include "../support/test_allocator.h"

#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* The size of the text a sink holds: enough for every test. */
#define SINK_SIZE ((size_t)512)

/* What the write function of the tests has received. */
typedef struct sink
{
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

/*
 * Appends the part to the sink. The call number fail_at takes nothing and
 * reports a failure, and so would every later call.
 */
static bool sink_write(void *user_data, const char *text, size_t length)
{
    sink_t *sink = user_data;

    ++sink->calls;
    if ((text == NULL) || (length == 0))
    {
        ++sink->empty_calls;
        return true;
    }
    if ((sink->fail_at != 0) && (sink->calls >= sink->fail_at))
    {
        return false;
    }
    if (length > (SINK_SIZE - sink->length))
    {
        return false;
    }

    memcpy(sink->text + sink->length, text, length);
    sink->length += length;
    return true;
}

/*
 * The end of every test: the test allocator has no live block and saw no
 * violation of the allocator contract.
 */
static void check_clean(void)
{
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_violations(), 0);
}

/* Installs the test allocator and creates a root with the name "tool". */
static argx_command_t *create_root(void)
{
    argx_command_t *root = NULL;

    argx_test_allocator_install();
    NAOS_REQUIRE_TRUE(argx_command_create("tool", &root) == ARGX_ERROR_NONE);
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
 * Adds an option with a value that must succeed. value_name is its display
 * name, or NULL to set none.
 */
static void add_value(argx_command_t *command, char short_name, const char *long_name,
                      const argx_type_t *type, const char *value_name, bool required)
{
    argx_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(argx_command_add_value(command, short_name, long_name, type, &option) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(option != NULL);
    if (value_name != NULL)
    {
        NAOS_REQUIRE_TRUE(argx_option_set_value_name(option, value_name) == ARGX_ERROR_NONE);
    }
    if (required)
    {
        NAOS_REQUIRE_TRUE(argx_option_set_required(option, true) == ARGX_ERROR_NONE);
    }
}

/* Adds a positional of strings that must succeed. */
static void add_positional(argx_command_t *command, const char *name, argx_arity_t arity)
{
    NAOS_REQUIRE_TRUE(argx_command_add_positional(command, name, argx_type_string(), arity,
                                                    NULL) == ARGX_ERROR_NONE);
}

/* Validates the tree of root; it must have no fault. */
static void validate(argx_command_t *root)
{
    NAOS_REQUIRE_TRUE(argx_command_validate(root, NULL, NULL, NULL) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_is_validated(root));
}

/*
 * Creates help options with the width given; the other settings keep their
 * default values.
 */
static argx_help_options_t *create_options(size_t width)
{
    argx_help_options_t *options = NULL;

    NAOS_REQUIRE_TRUE(argx_help_options_create(&options) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(options != NULL);
    argx_help_options_set_width(options, width);
    return options;
}

/*
 * Returns whether a line of the length bytes of text ends with a blank: a
 * blank before a newline, or at the end of the text.
 */
static bool has_blank_at_line_end(const char *text, size_t length)
{
    size_t index;

    for (index = 0; index < length; ++index)
    {
        if ((text[index] == ' ') && (((index + 1) == length) || (text[index + 1] == '\n')))
        {
            return true;
        }
    }
    return false;
}

/*
 * Has the usage line of command written with options and checks that the
 * call succeeds, that the text is exactly the string expected, that no line
 * of it ends with a blank, that no call of the write function was empty,
 * that nothing was allocated and that the schema is still validated.
 */
static void check_usage_with(const argx_command_t *command, const argx_help_options_t *options,
                             const char *expected)
{
    sink_t sink;
    size_t requests = argx_test_allocator_requests();

    sink_init(&sink);

    NAOS_CHECK_TRUE(argx_help_write_usage(command, options, sink_write, &sink) ==
                    ARGX_ERROR_NONE);

    NAOS_CHECK_UINT_EQ(sink.length, strlen(expected));
    NAOS_CHECK_MEM_EQ(sink.text, sink.length, expected, strlen(expected));
    NAOS_CHECK_TRUE(!has_blank_at_line_end(sink.text, sink.length));
    NAOS_CHECK_TRUE(!has_blank_at_line_end(expected, strlen(expected)));
    NAOS_CHECK_UINT_EQ(sink.empty_calls, 0);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), requests);
    NAOS_CHECK_TRUE(argx_command_is_validated(command));
}

/* The same without help options: every setting has its default value. */
static void check_usage(const argx_command_t *command, const char *expected)
{
    check_usage_with(command, NULL, expected);
}

/* ---- The command alone --------------------------------------------------- */

/*
 * A command with no option, no positional and no subcommand: "Usage: " and
 * the name, nothing else.
 */
NAOS_TEST(command_with_nothing_is_only_its_name)
{
    argx_command_t *root = create_root();

    validate(root);
    check_usage(root, "Usage: tool\n");

    destroy_root(root);
}

/* ---- [OPTIONS] ----------------------------------------------------------- */

/*
 * An option that is not required, of any of the four kinds, gives
 * [OPTIONS], and is not written itself.
 */
NAOS_TEST(option_that_is_not_required_gives_options_marker)
{
    argx_command_t *root;

    root = create_root();
    NAOS_REQUIRE_TRUE(argx_command_add_flag(root, 'h', "help", NULL) == ARGX_ERROR_NONE);
    validate(root);
    check_usage(root, "Usage: tool [OPTIONS]\n");
    destroy_root(root);

    root = create_root();
    NAOS_REQUIRE_TRUE(argx_command_add_negatable(root, '\0', "color", NULL) == ARGX_ERROR_NONE);
    validate(root);
    check_usage(root, "Usage: tool [OPTIONS]\n");
    destroy_root(root);

    root = create_root();
    NAOS_REQUIRE_TRUE(argx_command_add_count(root, 'v', NULL, NULL) == ARGX_ERROR_NONE);
    validate(root);
    check_usage(root, "Usage: tool [OPTIONS]\n");
    destroy_root(root);

    root = create_root();
    add_value(root, 'f', "format", argx_type_string(), "FMT", false);
    validate(root);
    check_usage(root, "Usage: tool [OPTIONS]\n");
    destroy_root(root);
}

/* Several options that are not required give one [OPTIONS]. */
NAOS_TEST(several_options_give_one_options_marker)
{
    argx_command_t *root = create_root();

    NAOS_REQUIRE_TRUE(argx_command_add_flag(root, 'h', "help", NULL) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_add_count(root, 'v', "verbose", NULL) == ARGX_ERROR_NONE);
    add_value(root, 'f', "format", argx_type_string(), NULL, false);
    validate(root);
    check_usage(root, "Usage: tool [OPTIONS]\n");

    destroy_root(root);
}

/* A command whose options are all required has no [OPTIONS]. */
NAOS_TEST(only_required_options_give_no_options_marker)
{
    argx_command_t *root = create_root();

    add_value(root, 'o', "output", argx_type_string(), "FILE", true);
    validate(root);
    check_usage(root, "Usage: tool --output <FILE>\n");

    destroy_root(root);
}

/* ---- Required options ---------------------------------------------------- */

/*
 * A required option with a long name is written with the long name, also
 * when it has a short name.
 */
NAOS_TEST(required_option_uses_long_name)
{
    argx_command_t *root;

    root = create_root();
    add_value(root, '\0', "output", argx_type_string(), "FILE", true);
    validate(root);
    check_usage(root, "Usage: tool --output <FILE>\n");
    destroy_root(root);

    root = create_root();
    add_value(root, 'o', "output", argx_type_string(), "FILE", true);
    validate(root);
    check_usage(root, "Usage: tool --output <FILE>\n");
    destroy_root(root);
}

/* A required option without a long name is written with the short name. */
NAOS_TEST(required_option_without_long_name_uses_short_name)
{
    argx_command_t *root = create_root();

    add_value(root, 'o', NULL, argx_type_string(), "FILE", true);
    validate(root);
    check_usage(root, "Usage: tool -o <FILE>\n");

    destroy_root(root);
}

/*
 * The conversion function of the user-defined value type of the tests. No
 * test converts a value with it.
 */
static argx_error_t convert_nothing(const char *text, void *value, void *context)
{
    (void)text;
    (void)context;
    *(int *)value = 0;
    return ARGX_ERROR_NONE;
}

/*
 * Without a display name of the value, the name of the value type stands
 * in its place: of a built-in type, of a choice and of a user-defined
 * type.
 */
NAOS_TEST(required_option_without_value_name_uses_type_name)
{
    static const char *const choices[] = { "json", "yaml" };
    argx_type_t *choice = NULL;
    argx_type_t *port = NULL;
    argx_command_t *root = create_root();

    NAOS_REQUIRE_TRUE(argx_type_create_choice(choices, 2, &choice) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_type_create("Port number", sizeof(int), convert_nothing, NULL, NULL,
                                         &port) == ARGX_ERROR_NONE);

    add_value(root, '\0', "name", argx_type_string(), NULL, true);
    add_value(root, 'n', NULL, argx_type_int(), NULL, true);
    add_value(root, '\0', "ratio", argx_type_float(), NULL, true);
    add_value(root, '\0', "format", choice, NULL, true);
    add_value(root, 'p', "port", port, NULL, true);
    validate(root);
    /* The last option would end at the column 95: it starts a new line. */
    check_usage(root, "Usage: tool --name <string> -n <integer> --ratio <float> --format <choice>\n"
                      "       --port <Port number>\n");

    argx_command_destroy(root);
    argx_type_destroy(choice);
    argx_type_destroy(port);
    argx_test_allocator_uninstall();
    check_clean();
}

/*
 * The required options come after [OPTIONS], in the order in which they
 * were added; the options between them that are not required are not
 * written.
 */
NAOS_TEST(required_options_follow_options_marker_in_definition_order)
{
    argx_command_t *root = create_root();

    add_value(root, 'z', "zone", argx_type_string(), "ZONE", true);
    NAOS_REQUIRE_TRUE(argx_command_add_flag(root, 'h', "help", NULL) == ARGX_ERROR_NONE);
    add_value(root, 'a', NULL, argx_type_int(), "N", true);
    add_value(root, 'f', "format", argx_type_string(), "FMT", false);
    add_value(root, '\0', "key", argx_type_string(), "KEY", true);
    validate(root);
    check_usage(root, "Usage: tool [OPTIONS] --zone <ZONE> -a <N> --key <KEY>\n");

    destroy_root(root);
}

/* ---- Positionals --------------------------------------------------------- */

/* The four arities: <name>, [name], [name]... and <name>... */
NAOS_TEST(positionals_show_their_arity)
{
    argx_command_t *root;

    root = create_root();
    add_positional(root, "input", ARGX_ARITY_REQUIRED);
    validate(root);
    check_usage(root, "Usage: tool <input>\n");
    destroy_root(root);

    root = create_root();
    add_positional(root, "input", ARGX_ARITY_OPTIONAL);
    validate(root);
    check_usage(root, "Usage: tool [input]\n");
    destroy_root(root);

    root = create_root();
    add_positional(root, "input", ARGX_ARITY_ZERO_OR_MORE);
    validate(root);
    check_usage(root, "Usage: tool [input]...\n");
    destroy_root(root);

    root = create_root();
    add_positional(root, "input", ARGX_ARITY_ONE_OR_MORE);
    validate(root);
    check_usage(root, "Usage: tool <input>...\n");
    destroy_root(root);
}

/* Several positionals come in the order in which they were added. */
NAOS_TEST(positionals_come_in_definition_order)
{
    argx_command_t *root;

    root = create_root();
    add_positional(root, "source", ARGX_ARITY_REQUIRED);
    add_positional(root, "target", ARGX_ARITY_OPTIONAL);
    add_positional(root, "extra", ARGX_ARITY_ZERO_OR_MORE);
    validate(root);
    check_usage(root, "Usage: tool <source> [target] [extra]...\n");
    destroy_root(root);

    root = create_root();
    add_positional(root, "zeta", ARGX_ARITY_REQUIRED);
    add_positional(root, "alpha", ARGX_ARITY_ONE_OR_MORE);
    validate(root);
    check_usage(root, "Usage: tool <zeta> <alpha>...\n");
    destroy_root(root);
}

/*
 * The parts in their order: [OPTIONS], the required options, the
 * positionals.
 */
NAOS_TEST(options_come_before_positionals)
{
    argx_command_t *root = create_root();

    add_positional(root, "input", ARGX_ARITY_REQUIRED);
    add_positional(root, "extra", ARGX_ARITY_ZERO_OR_MORE);
    add_value(root, 'o', "output", argx_type_string(), "FILE", true);
    add_value(root, 'f', "format", argx_type_string(), "FMT", false);
    NAOS_REQUIRE_TRUE(argx_command_add_count(root, 'v', "verbose", NULL) == ARGX_ERROR_NONE);
    validate(root);
    check_usage(root, "Usage: tool [OPTIONS] --output <FILE> <input> [extra]...\n");

    destroy_root(root);
}

/* ---- Subcommands --------------------------------------------------------- */

/*
 * A command with subcommands ends with [COMMAND], once, whatever their
 * number.
 */
NAOS_TEST(subcommands_give_command_marker)
{
    argx_command_t *root;

    root = create_root();
    add_sub(root, "build");
    validate(root);
    check_usage(root, "Usage: tool [COMMAND]\n");
    destroy_root(root);

    root = create_root();
    add_sub(root, "build");
    add_sub(root, "clean");
    NAOS_REQUIRE_TRUE(argx_command_add_flag(root, 'h', "help", NULL) == ARGX_ERROR_NONE);
    add_value(root, '\0', "key", argx_type_string(), "KEY", true);
    validate(root);
    check_usage(root, "Usage: tool [OPTIONS] --key <KEY> [COMMAND]\n");
    destroy_root(root);
}

/*
 * The path of a subcommand is written from the root down, and the line
 * shows what the subcommand itself has: not the options of its parent, and
 * not what its own subcommands have.
 */
NAOS_TEST(subcommand_usage_has_path_from_root)
{
    argx_command_t *root = create_root();
    argx_command_t *remote;
    argx_command_t *add;
    argx_command_t *list;

    NAOS_REQUIRE_TRUE(argx_command_add_flag(root, 'h', "help", NULL) == ARGX_ERROR_NONE);
    add_value(root, '\0', "key", argx_type_string(), "KEY", true);
    remote = add_sub(root, "remote");
    add = add_sub(remote, "add");
    list = add_sub(remote, "list");
    add_positional(add, "name", ARGX_ARITY_REQUIRED);
    add_positional(add, "url", ARGX_ARITY_OPTIONAL);
    add_value(add, 't', "track", argx_type_string(), "BRANCH", true);
    NAOS_REQUIRE_TRUE(argx_command_add_flag(list, 'a', "all", NULL) == ARGX_ERROR_NONE);
    validate(root);

    check_usage(root, "Usage: tool [OPTIONS] --key <KEY> [COMMAND]\n");
    check_usage(remote, "Usage: tool remote [COMMAND]\n");
    check_usage(add, "Usage: tool remote add --track <BRANCH> <name> [url]\n");
    check_usage(list, "Usage: tool remote list [OPTIONS]\n");

    destroy_root(root);
}

/* ---- The strings of the schema ------------------------------------------- */

/*
 * Names are written as they are: upper and lower case, bytes that are not
 * ASCII, blanks and a newline.
 */
NAOS_TEST(schema_strings_are_written_unchanged)
{
    argx_command_t *root = NULL;

    argx_test_allocator_install();
    NAOS_REQUIRE_TRUE(argx_command_create("My Tool", &root) == ARGX_ERROR_NONE);

    add_value(root, 'O', "OutPut", argx_type_string(), "File Name", true);
    add_value(root, 'x', NULL, argx_type_string(), "\xE5\x80\xA4", true);
    add_positional(root, "In Put", ARGX_ARITY_REQUIRED);
    add_positional(root, "\xC3\xA9t\xC3\xA9", ARGX_ARITY_OPTIONAL);
    add_positional(root, "two\nlines", ARGX_ARITY_ZERO_OR_MORE);
    validate(root);
    check_usage(root, "Usage: My Tool --OutPut <File Name> -x <\xE5\x80\xA4> <In Put> "
                      "[\xC3\xA9t\xC3\xA9] [two\nlines]...\n");

    destroy_root(root);
}

/* ---- The help options ---------------------------------------------------- */

/*
 * Without help options (NULL), the text is the one that help options with
 * their default values give, and help options can be used for more than one
 * call.
 */
NAOS_TEST(null_options_are_the_default_options)
{
    static const char expected[] = "Usage: tool [OPTIONS] --output <FILE> <input> [extra]...\n";
    argx_help_options_t *options = NULL;
    argx_command_t *root = create_root();

    NAOS_REQUIRE_TRUE(argx_help_options_create(&options) == ARGX_ERROR_NONE);

    NAOS_REQUIRE_TRUE(argx_command_add_flag(root, 'h', "help", NULL) == ARGX_ERROR_NONE);
    add_value(root, 'o', "output", argx_type_string(), "FILE", true);
    add_positional(root, "input", ARGX_ARITY_REQUIRED);
    add_positional(root, "extra", ARGX_ARITY_ZERO_OR_MORE);
    validate(root);

    check_usage_with(root, NULL, expected);
    check_usage_with(root, options, expected);
    check_usage_with(root, options, expected);
    check_usage_with(root, NULL, expected);

    argx_help_options_destroy(options);
    destroy_root(root);
}

/* ---- The width ------------------------------------------------------------ */

/*
 * Adds what the tests of the width share: with every item on one line,
 *
 *     Usage: tool [OPTIONS] --output <FILE> <input> [extra]...
 *
 * whose items end at the columns 11, 21, 37, 45 and 56.
 */
static void add_all_kinds_of_items(argx_command_t *root)
{
    NAOS_REQUIRE_TRUE(argx_command_add_flag(root, 'h', "help", NULL) == ARGX_ERROR_NONE);
    add_value(root, 'o', "output", argx_type_string(), "FILE", true);
    add_positional(root, "input", ARGX_ARITY_REQUIRED);
    add_positional(root, "extra", ARGX_ARITY_ZERO_OR_MORE);
}

/*
 * A line that does not fit the width is broken between two items, and each
 * line after the first starts with 7 blanks, the width of "Usage: ". An
 * item that still fits stays on the line: at the width 30, "<input>" ends
 * at the column 30.
 */
NAOS_TEST(narrow_width_breaks_between_items_with_indent_of_seven)
{
    argx_help_options_t *options;
    argx_command_t *root = create_root();

    add_all_kinds_of_items(root);
    validate(root);

    options = create_options(30);
    check_usage_with(root, options,
                     "Usage: tool [OPTIONS]\n"
                     "       --output <FILE> <input>\n"
                     "       [extra]...\n");

    /* One column less, and "<input>" no longer fits behind the option. */
    argx_help_options_set_width(options, 29);
    check_usage_with(root, options,
                     "Usage: tool [OPTIONS]\n"
                     "       --output <FILE>\n"
                     "       <input> [extra]...\n");

    /* Wide enough for everything: one line, as without a limit. */
    argx_help_options_set_width(options, 56);
    check_usage_with(root, options, "Usage: tool [OPTIONS] --output <FILE> <input> [extra]...\n");

    argx_help_options_destroy(options);
    destroy_root(root);
}

/*
 * "[COMMAND]" is an item like the others: it stays on the line when it
 * fits and starts a new line when it does not. The whole line has 43
 * bytes.
 */
NAOS_TEST(command_marker_moves_to_next_line_when_it_does_not_fit)
{
    argx_help_options_t *options;
    argx_command_t *root = create_root();

    NAOS_REQUIRE_TRUE(argx_command_add_flag(root, 'h', "help", NULL) == ARGX_ERROR_NONE);
    add_value(root, '\0', "key", argx_type_string(), "KEY", true);
    add_sub(root, "build");
    validate(root);

    options = create_options(43);
    check_usage_with(root, options, "Usage: tool [OPTIONS] --key <KEY> [COMMAND]\n");

    argx_help_options_set_width(options, 42);
    check_usage_with(root, options,
                     "Usage: tool [OPTIONS] --key <KEY>\n"
                     "       [COMMAND]\n");

    argx_help_options_destroy(options);
    destroy_root(root);
}

/*
 * The boundary: an item whose last byte is at the column of the width
 * stays on the line; with a width one column smaller, it starts the next
 * line. "Usage: tool <source>" has 20 bytes and the whole line 29.
 */
NAOS_TEST(item_that_just_fits_stays_and_one_column_less_breaks)
{
    argx_help_options_t *options;
    argx_command_t *root = create_root();

    add_positional(root, "source", ARGX_ARITY_REQUIRED);
    add_positional(root, "target", ARGX_ARITY_REQUIRED);
    validate(root);

    options = create_options(29);
    check_usage_with(root, options, "Usage: tool <source> <target>\n");

    argx_help_options_set_width(options, 28);
    check_usage_with(root, options, "Usage: tool <source>\n"
                                    "       <target>\n");

    argx_help_options_set_width(options, 20);
    check_usage_with(root, options, "Usage: tool <source>\n"
                                    "       <target>\n");

    argx_help_options_set_width(options, 19);
    check_usage_with(root, options, "Usage: tool\n"
                                    "       <source>\n"
                                    "       <target>\n");

    argx_help_options_destroy(options);
    destroy_root(root);
}

/*
 * An item is not divided at a blank inside it: not the path, not a
 * required option between its name and its value, and not inside a display
 * name or a name that holds a blank. At the width 30, "Usage: My Tool
 * --output" and "--output <File Name> <In" would fit.
 */
NAOS_TEST(item_with_blank_inside_is_not_divided)
{
    argx_help_options_t *options;
    argx_command_t *root = NULL;

    argx_test_allocator_install();
    NAOS_REQUIRE_TRUE(argx_command_create("My Tool", &root) == ARGX_ERROR_NONE);

    add_value(root, 'o', "output", argx_type_string(), "File Name", true);
    add_positional(root, "In Put", ARGX_ARITY_REQUIRED);
    validate(root);

    options = create_options(30);
    check_usage_with(root, options,
                     "Usage: My Tool\n"
                     "       --output <File Name>\n"
                     "       <In Put>\n");

    /* The whole line has 44 bytes. */
    argx_help_options_set_width(options, 44);
    check_usage_with(root, options, "Usage: My Tool --output <File Name> <In Put>\n");

    argx_help_options_set_width(options, 43);
    check_usage_with(root, options,
                     "Usage: My Tool --output <File Name>\n"
                     "       <In Put>\n");

    argx_help_options_destroy(options);
    destroy_root(root);
}

/*
 * An item that is wider than the width is written as it is, on a line of
 * its own: the path of a subcommand, which is one item with "Usage: ", and
 * every other kind of item.
 */
NAOS_TEST(item_wider_than_width_is_written_as_it_is)
{
    argx_help_options_t *options;
    argx_command_t *root;
    argx_command_t *remote;
    argx_command_t *add;

    /* No line can hold two items; nothing comes before the first item. */
    root = create_root();
    add_all_kinds_of_items(root);
    validate(root);

    options = create_options(1);
    check_usage_with(root, options,
                     "Usage: tool\n"
                     "       [OPTIONS]\n"
                     "       --output <FILE>\n"
                     "       <input>\n"
                     "       [extra]...\n");
    argx_help_options_destroy(options);
    destroy_root(root);

    root = create_root();
    remote = add_sub(root, "remote");
    add = add_sub(remote, "add");
    add_value(add, 'c', "configuration", argx_type_string(), "FILE", true);
    add_positional(add, "name", ARGX_ARITY_REQUIRED);
    validate(root);

    options = create_options(1);
    check_usage_with(remote, options,
                     "Usage: tool remote\n"
                     "       [COMMAND]\n");

    /*
     * The path, 22 bytes, and the option, 29 bytes with the indent, are
     * wider than 20; "<name>" would fit behind neither.
     */
    argx_help_options_set_width(options, 20);
    check_usage_with(add, options,
                     "Usage: tool remote add\n"
                     "       --configuration <FILE>\n"
                     "       <name>\n");

    /*
     * An item that is too wide does not push the next one down when that
     * one still fits behind it.
     */
    argx_help_options_set_width(options, 36);
    check_usage_with(add, options,
                     "Usage: tool remote add\n"
                     "       --configuration <FILE> <name>\n");

    argx_help_options_destroy(options);
    destroy_root(root);
}

/*
 * Adds seven positionals. With every item on one line,
 *
 *     Usage: tool <argument-1> ... <argument-5> <x> <argument-7>
 *
 * "<x>" ends at the column 80 and the whole line has 93 bytes.
 */
static void add_positionals_beyond_80_columns(argx_command_t *root)
{
    add_positional(root, "argument-1", ARGX_ARITY_REQUIRED);
    add_positional(root, "argument-2", ARGX_ARITY_REQUIRED);
    add_positional(root, "argument-3", ARGX_ARITY_REQUIRED);
    add_positional(root, "argument-4", ARGX_ARITY_REQUIRED);
    add_positional(root, "argument-5", ARGX_ARITY_REQUIRED);
    add_positional(root, "x", ARGX_ARITY_REQUIRED);
    add_positional(root, "argument-7", ARGX_ARITY_REQUIRED);
}

/* The width 0 means no limit: the usage line is one line, however long. */
NAOS_TEST(width_zero_gives_one_line)
{
    argx_help_options_t *options;
    argx_command_t *root = create_root();

    add_positionals_beyond_80_columns(root);
    validate(root);

    options = create_options(0);
    check_usage_with(root, options,
                     "Usage: tool <argument-1> <argument-2> <argument-3> <argument-4> "
                     "<argument-5> <x> <argument-7>\n");

    argx_help_options_destroy(options);
    destroy_root(root);
}

/*
 * Without help options (NULL) the width is the default, 80, as with help
 * options that were just created: a line of exactly 80 bytes stays whole
 * and the next item starts a new line. A width that is set takes effect.
 */
NAOS_TEST(default_width_of_80_applies_without_options)
{
    static const char expected[] =
        "Usage: tool <argument-1> <argument-2> <argument-3> <argument-4> <argument-5> <x>\n"
        "       <argument-7>\n";
    argx_help_options_t *options = NULL;
    argx_command_t *root = create_root();

    /* The first line of the expected text has 80 bytes. */
    NAOS_REQUIRE_TRUE(strchr(expected, '\n') == (expected + 80));

    add_positionals_beyond_80_columns(root);
    validate(root);

    NAOS_REQUIRE_TRUE(argx_help_options_create(&options) == ARGX_ERROR_NONE);

    check_usage_with(root, NULL, expected);
    check_usage_with(root, options, expected);

    argx_help_options_set_width(options, 80);
    check_usage_with(root, options, expected);

    argx_help_options_set_width(options, 79);
    check_usage_with(root, options,
                     "Usage: tool <argument-1> <argument-2> <argument-3> <argument-4> "
                     "<argument-5>\n"
                     "       <x> <argument-7>\n");

    argx_help_options_destroy(options);
    destroy_root(root);
}

/*
 * The width of an item is the number of all its bytes, also when a name in
 * it holds a newline: "--mode <two\nlines>" counts as 18 columns, not as
 * the 6 of "lines>". The column that the next item is measured from is the
 * one of the output: after that item it counts from the newline inside the
 * name.
 */
NAOS_TEST(item_with_newline_inside_counts_all_its_bytes)
{
    argx_help_options_t *options;
    argx_command_t *root = create_root();

    add_value(root, 'm', "mode", argx_type_string(), "two\nlines", true);
    add_positional(root, "input", ARGX_ARITY_REQUIRED);
    validate(root);

    /* 11 + 1 + 18 is 30: the option just fits. */
    options = create_options(30);
    check_usage_with(root, options, "Usage: tool --mode <two\nlines> <input>\n");

    /*
     * One column less, and the option starts a new line. "<input>" stays
     * behind it: "lines>" ends at the column 6.
     */
    argx_help_options_set_width(options, 29);
    check_usage_with(root, options,
                     "Usage: tool\n"
                     "       --mode <two\nlines> <input>\n");

    /* "lines> <input>" has 14 bytes. */
    argx_help_options_set_width(options, 14);
    check_usage_with(root, options,
                     "Usage: tool\n"
                     "       --mode <two\nlines> <input>\n");

    argx_help_options_set_width(options, 13);
    check_usage_with(root, options,
                     "Usage: tool\n"
                     "       --mode <two\nlines>\n"
                     "       <input>\n");

    argx_help_options_destroy(options);
    destroy_root(root);
}

/* ---- The write function -------------------------------------------------- */

/*
 * When the write function reports a failure, the call returns
 * ARGX_ERROR_WRITE_FAILED, the write function is not called again, and
 * what it took before stays: the start of the text.
 */
NAOS_TEST(write_failure_is_reported_and_stops_the_calls)
{
    static const char expected[] = "Usage: tool [OPTIONS] --output <FILE> <input>\n";
    argx_command_t *root = create_root();
    sink_t whole;
    size_t n;

    NAOS_REQUIRE_TRUE(argx_command_add_flag(root, 'h', "help", NULL) == ARGX_ERROR_NONE);
    add_value(root, 'o', "output", argx_type_string(), "FILE", true);
    add_positional(root, "input", ARGX_ARITY_REQUIRED);
    validate(root);

    sink_init(&whole);
    NAOS_REQUIRE_TRUE(argx_help_write_usage(root, NULL, sink_write, &whole) == ARGX_ERROR_NONE);
    NAOS_CHECK_MEM_EQ(whole.text, whole.length, expected, strlen(expected));
    NAOS_REQUIRE_TRUE(whole.calls >= 2);

    /* Each call in turn is the one that fails. */
    for (n = 1; n <= whole.calls; ++n)
    {
        sink_t sink;

        sink_init(&sink);
        sink.fail_at = n;

        NAOS_CHECK_TRUE(argx_help_write_usage(root, NULL, sink_write, &sink) ==
                        ARGX_ERROR_WRITE_FAILED);
        NAOS_CHECK_UINT_EQ(sink.calls, n);
        NAOS_CHECK_UINT_EQ(sink.empty_calls, 0);
        NAOS_CHECK_TRUE(sink.length < strlen(expected));
        NAOS_CHECK_MEM_EQ(sink.text, sink.length, expected, sink.length);
    }

    NAOS_CHECK_TRUE(argx_command_is_validated(root));

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
    NAOS_ADD_TEST(narrow_width_breaks_between_items_with_indent_of_seven),
    NAOS_ADD_TEST(command_marker_moves_to_next_line_when_it_does_not_fit),
    NAOS_ADD_TEST(item_that_just_fits_stays_and_one_column_less_breaks),
    NAOS_ADD_TEST(item_with_blank_inside_is_not_divided),
    NAOS_ADD_TEST(item_wider_than_width_is_written_as_it_is),
    NAOS_ADD_TEST(width_zero_gives_one_line),
    NAOS_ADD_TEST(default_width_of_80_applies_without_options),
    NAOS_ADD_TEST(item_with_newline_inside_counts_all_its_bytes),
    NAOS_ADD_TEST(write_failure_is_reported_and_stops_the_calls)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(help_usage)
)
