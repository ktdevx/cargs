/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * Tests of the help: argx_help_write() of <argx/help.h>.
 *
 * Each test builds a schema, validates it, has the help of one of its
 * commands written and compares the text with the expected string, by its
 * length and by its bytes. The write function of the tests appends what it
 * is given to an array of a fixed size.
 *
 * A command of a schema has subcommands or positionals, not both, so a
 * help of this file has two lists at most: "Commands:" or "Arguments:",
 * and "Options:".
 *
 * With the default settings "the indent + the width of the widest heading
 * + 2" is at most 32 in each schema of this file; where a test sets a
 * width, it is also at most half that width. So the texts do not depend on
 * the limit of the description column. No option of this file is required,
 * has a default value, a value type with choices or an initial state that
 * is on: the description of an option is its description text only. The
 * headings of the options have their own tests, in help_item_test.c.
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
#define SINK_SIZE ((size_t)1024)

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

/* Sets the description of command; it must succeed. */
static void describe(argx_command_t *command, const char *description)
{
    NAOS_REQUIRE_TRUE(argx_command_set_description(command, description) == ARGX_ERROR_NONE);
}

/*
 * Adds a subcommand that must succeed, and returns it. description is its
 * description, or NULL to set none.
 */
static argx_command_t *add_sub(argx_command_t *command, const char *name,
                               const char *description)
{
    argx_command_t *sub = NULL;

    NAOS_REQUIRE_TRUE(argx_command_add_subcommand(command, name, &sub) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(sub != NULL);
    if (description != NULL)
    {
        describe(sub, description);
    }
    return sub;
}

/*
 * Adds a positional of strings that must succeed. description is its
 * description, or NULL to set none.
 */
static void add_positional(argx_command_t *command, const char *name, argx_arity_t arity,
                           const char *description)
{
    argx_positional_t *positional = NULL;

    NAOS_REQUIRE_TRUE(argx_command_add_positional(command, name, argx_type_string(), arity,
                                                  &positional) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(positional != NULL);
    if (description != NULL)
    {
        NAOS_REQUIRE_TRUE(argx_positional_set_description(positional, description) ==
                          ARGX_ERROR_NONE);
    }
}

/*
 * Adds a flag that must succeed. short_name is '\0' and long_name is NULL
 * for a name that the option does not have; description is its
 * description, or NULL to set none.
 */
static void add_flag(argx_command_t *command, char short_name, const char *long_name,
                     const char *description)
{
    argx_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(argx_command_add_flag(command, short_name, long_name, &option) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(option != NULL);
    if (description != NULL)
    {
        NAOS_REQUIRE_TRUE(argx_option_set_description(option, description) == ARGX_ERROR_NONE);
    }
}

/*
 * Adds a count that must succeed, with the names and the description as
 * for add_flag().
 */
static void add_count(argx_command_t *command, char short_name, const char *long_name,
                      const char *description)
{
    argx_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(argx_command_add_count(command, short_name, long_name, &option) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(option != NULL);
    if (description != NULL)
    {
        NAOS_REQUIRE_TRUE(argx_option_set_description(option, description) == ARGX_ERROR_NONE);
    }
}

/*
 * Adds an option with a value of strings that must succeed, with the
 * display name value_name. The option is not required and has no default
 * value.
 */
static void add_value(argx_command_t *command, char short_name, const char *long_name,
                      const char *value_name, const char *description)
{
    argx_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(argx_command_add_value(command, short_name, long_name, argx_type_string(),
                                             &option) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(option != NULL);
    NAOS_REQUIRE_TRUE(argx_option_set_value_name(option, value_name) == ARGX_ERROR_NONE);
    if (description != NULL)
    {
        NAOS_REQUIRE_TRUE(argx_option_set_description(option, description) == ARGX_ERROR_NONE);
    }
}

/* Validates the tree of root; it must have no fault. */
static void validate(argx_command_t *root)
{
    NAOS_REQUIRE_TRUE(argx_command_validate(root, NULL, NULL, NULL) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_command_is_validated(root));
}

/*
 * Creates help options with the width and the indent given; the maximum
 * column keeps its default value.
 */
static argx_help_options_t *create_options(size_t width, size_t indent)
{
    argx_help_options_t *options = NULL;

    NAOS_REQUIRE_TRUE(argx_help_options_create(&options) == ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(options != NULL);
    argx_help_options_set_width(options, width);
    argx_help_options_set_indent(options, indent);
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
 * Returns the number of the empty lines of the length bytes of text: the
 * newlines at the start of the text or right behind another newline.
 */
static size_t count_empty_lines(const char *text, size_t length)
{
    size_t count = 0;
    size_t index;

    for (index = 0; index < length; ++index)
    {
        if ((text[index] == '\n') && ((index == 0) || (text[index - 1] == '\n')))
        {
            ++count;
        }
    }
    return count;
}

/*
 * Checks the shape of a help text: no line ends with a blank, there is no
 * '\r', the text ends with one "\n", and it neither starts nor ends with
 * an empty line.
 */
static void check_shape(const char *text, size_t length)
{
    NAOS_CHECK_TRUE(!has_blank_at_line_end(text, length));
    NAOS_CHECK_TRUE(memchr(text, '\r', length) == NULL);
    NAOS_REQUIRE_TRUE(length >= 2);
    NAOS_CHECK_TRUE(text[0] != '\n');
    NAOS_CHECK_TRUE(text[length - 1] == '\n');
    NAOS_CHECK_TRUE(text[length - 2] != '\n');
}

/*
 * Has the help of command written with options and checks that the call
 * succeeds, that the text is exactly the string expected, that the text has
 * the shape of a help text, that no call of the write function was empty,
 * that the generation left nothing allocated and violated no rule of the
 * allocator, and that the schema is still validated. The generation may
 * allocate: for the cell text of an option or of a positional.
 */
static void check_help_with(const argx_command_t *command, const argx_help_options_t *options,
                            const char *expected)
{
    sink_t sink;
    size_t live = argx_test_allocator_live();

    sink_init(&sink);

    NAOS_CHECK_TRUE(argx_help_write(command, options, sink_write, &sink) == ARGX_ERROR_NONE);

    NAOS_CHECK_UINT_EQ(sink.length, strlen(expected));
    NAOS_CHECK_MEM_EQ(sink.text, sink.length, expected, strlen(expected));
    check_shape(sink.text, sink.length);
    check_shape(expected, strlen(expected));
    NAOS_CHECK_UINT_EQ(sink.empty_calls, 0);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_live(), live);
    NAOS_CHECK_UINT_EQ(argx_test_allocator_violations(), 0);
    NAOS_CHECK_TRUE(argx_command_is_validated(command));
}

/* The same without help options: every setting has its default value. */
static void check_help(const argx_command_t *command, const char *expected)
{
    check_help_with(command, NULL, expected);
}

/* ---- Whole texts --------------------------------------------------------- */

/*
 * A root with a description and subcommands: the usage line, the
 * description, and the list of the subcommands with its title, each part
 * behind one empty line.
 */
NAOS_TEST(root_with_description_and_subcommands)
{
    argx_command_t *root = create_root();

    describe(root, "Manage the remote repositories.");
    add_sub(root, "add", "Add a remote");
    add_sub(root, "remove", "Remove a remote");
    add_sub(root, "list", "List the remotes");
    validate(root);

    check_help(root,
               "Usage: tool [COMMAND]\n"
               "\n"
               "Manage the remote repositories.\n"
               "\n"
               "Commands:\n"
               "  add     Add a remote\n"
               "  remove  Remove a remote\n"
               "  list    List the remotes\n");

    destroy_root(root);
}

/*
 * A subcommand of the second level with positionals: the usage line has
 * the path from the root, and the list is the one of the positionals, with
 * the heading of each arity.
 */
NAOS_TEST(second_level_subcommand_with_positionals)
{
    argx_command_t *root = create_root();
    argx_command_t *remote = add_sub(root, "remote", "Manage the remotes");
    argx_command_t *add = add_sub(remote, "add", "Add a remote.");

    describe(root, "The tool.");
    add_positional(add, "name", ARGX_ARITY_REQUIRED, "Name of the remote");
    add_positional(add, "url", ARGX_ARITY_OPTIONAL, "Address of the remote");
    add_positional(add, "tags", ARGX_ARITY_ZERO_OR_MORE, "Tags to set");
    validate(root);

    check_help(add,
               "Usage: tool remote add <name> [url] [tags]...\n"
               "\n"
               "Add a remote.\n"
               "\n"
               "Arguments:\n"
               "  <name>     Name of the remote\n"
               "  [url]      Address of the remote\n"
               "  [tags]...  Tags to set\n");

    /*
     * The command between the two: its own description and its own
     * subcommand.
     */
    check_help(remote,
               "Usage: tool remote [COMMAND]\n"
               "\n"
               "Manage the remotes\n"
               "\n"
               "Commands:\n"
               "  add  Add a remote.\n");

    destroy_root(root);
}

/* ---- The parts that are left out ------------------------------------------ */

/*
 * A command without a description: the first list follows the usage line
 * behind one empty line.
 */
NAOS_TEST(command_without_description)
{
    static const char expected[] = "Usage: tool [COMMAND]\n"
                                   "\n"
                                   "Commands:\n"
                                   "  add  Add a remote\n";
    argx_command_t *root = create_root();

    add_sub(root, "add", "Add a remote");
    validate(root);

    check_help(root, expected);
    NAOS_CHECK_UINT_EQ(count_empty_lines(expected, strlen(expected)), 1);

    destroy_root(root);
}

/* A command with nothing at all: the help is the usage line. */
NAOS_TEST(command_with_nothing_is_only_the_usage_line)
{
    argx_command_t *root = create_root();

    validate(root);
    check_help(root, "Usage: tool\n");

    destroy_root(root);
}

/* A command with a description only: no list and no title of a list. */
NAOS_TEST(command_with_description_only)
{
    argx_command_t *root = create_root();

    describe(root, "Do one thing.");
    validate(root);

    check_help(root,
               "Usage: tool\n"
               "\n"
               "Do one thing.\n");

    destroy_root(root);
}

/*
 * One empty line stands between two parts, and none at the start or at the
 * end: two with the description and a list, one with a list only, none
 * with the usage line only.
 */
NAOS_TEST(parts_are_separated_by_one_empty_line)
{
    argx_command_t *root = create_root();
    argx_command_t *run = add_sub(root, "run", "Run it");
    argx_command_t *stop = add_sub(root, "stop", NULL);
    argx_command_t *wait = add_sub(root, "wait", NULL);
    sink_t sink;

    describe(root, "Do things.");
    add_positional(run, "x", ARGX_ARITY_OPTIONAL, "A thing");
    add_positional(stop, "y", ARGX_ARITY_OPTIONAL, "Another thing");
    describe(stop, "Stop it.");
    validate(root);

    check_help(root,
               "Usage: tool [COMMAND]\n"
               "\n"
               "Do things.\n"
               "\n"
               "Commands:\n"
               "  run   Run it\n"
               "  stop  Stop it.\n"
               "  wait\n");
    sink_init(&sink);
    NAOS_REQUIRE_TRUE(argx_help_write(root, NULL, sink_write, &sink) == ARGX_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(count_empty_lines(sink.text, sink.length), 2);

    check_help(stop,
               "Usage: tool stop [y]\n"
               "\n"
               "Stop it.\n"
               "\n"
               "Arguments:\n"
               "  [y]  Another thing\n");
    sink_init(&sink);
    NAOS_REQUIRE_TRUE(argx_help_write(stop, NULL, sink_write, &sink) == ARGX_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(count_empty_lines(sink.text, sink.length), 2);

    /*
     * The description of a subcommand in the list of its parent is the
     * description of the command in its own help.
     */
    check_help(run,
               "Usage: tool run [x]\n"
               "\n"
               "Run it\n"
               "\n"
               "Arguments:\n"
               "  [x]  A thing\n");

    check_help(wait, "Usage: tool wait\n");
    sink_init(&sink);
    NAOS_REQUIRE_TRUE(argx_help_write(wait, NULL, sink_write, &sink) == ARGX_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(count_empty_lines(sink.text, sink.length), 0);

    destroy_root(root);

    /* A list only: one. */
    root = create_root();
    add_positional(root, "x", ARGX_ARITY_OPTIONAL, "A thing");
    validate(root);

    check_help(root,
               "Usage: tool [x]\n"
               "\n"
               "Arguments:\n"
               "  [x]  A thing\n");
    sink_init(&sink);
    NAOS_REQUIRE_TRUE(argx_help_write(root, NULL, sink_write, &sink) == ARGX_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(count_empty_lines(sink.text, sink.length), 1);

    destroy_root(root);
}

/* ---- The items ------------------------------------------------------------ */

/*
 * The items of a list stand in the order in which they were added, not in
 * the order of their names.
 */
NAOS_TEST(items_come_in_definition_order)
{
    argx_command_t *root = create_root();
    argx_command_t *zeta = add_sub(root, "zeta", "Last letter");

    add_sub(root, "alpha", "First letter");
    add_sub(root, "mid", "Between");
    add_positional(zeta, "second", ARGX_ARITY_REQUIRED, "Added first");
    add_positional(zeta, "first", ARGX_ARITY_ONE_OR_MORE, "Added second");
    validate(root);

    check_help(root,
               "Usage: tool [COMMAND]\n"
               "\n"
               "Commands:\n"
               "  zeta   Last letter\n"
               "  alpha  First letter\n"
               "  mid    Between\n");

    check_help(zeta,
               "Usage: tool zeta <second> <first>...\n"
               "\n"
               "Last letter\n"
               "\n"
               "Arguments:\n"
               "  <second>    Added first\n"
               "  <first>...  Added second\n");

    destroy_root(root);
}

/*
 * Only the subcommands of the command itself are listed: not those of its
 * subcommands and not the positionals of one; the headings of those also
 * do not count for the description column.
 */
NAOS_TEST(grandchildren_are_not_listed)
{
    argx_command_t *root = create_root();
    argx_command_t *remote = add_sub(root, "remote", "Manage the remotes");
    argx_command_t *add = add_sub(remote, "add-a-new-remote", "Add a remote");

    add_positional(add, "where-to-look", ARGX_ARITY_OPTIONAL, "Where to look");
    add_sub(root, "log", "Show the history");
    validate(root);

    check_help(root,
               "Usage: tool [COMMAND]\n"
               "\n"
               "Commands:\n"
               "  remote  Manage the remotes\n"
               "  log     Show the history\n");

    destroy_root(root);
}

/*
 * The description of a subcommand in the list ends before its first
 * newline. In the help of the subcommand itself it is whole.
 */
NAOS_TEST(subcommand_description_is_cut_at_first_newline)
{
    argx_command_t *root = create_root();
    argx_command_t *add = add_sub(root, "add", "Add a remote\n\nThe remote is not fetched.");

    add_sub(root, "list", "List the remotes\n");
    validate(root);

    check_help(root,
               "Usage: tool [COMMAND]\n"
               "\n"
               "Commands:\n"
               "  add   Add a remote\n"
               "  list  List the remotes\n");

    check_help(add,
               "Usage: tool add\n"
               "\n"
               "Add a remote\n"
               "\n"
               "The remote is not fetched.\n");

    destroy_root(root);
}

/*
 * The description of a positional is written whole: after a newline in it
 * the text goes on at the description column.
 */
NAOS_TEST(positional_description_is_written_whole)
{
    argx_command_t *root = create_root();

    add_positional(root, "input", ARGX_ARITY_REQUIRED, "File to read\nor - for the input\n");
    add_positional(root, "x", ARGX_ARITY_OPTIONAL, "A thing");
    validate(root);

    check_help(root,
               "Usage: tool <input> [x]\n"
               "\n"
               "Arguments:\n"
               "  <input>  File to read\n"
               "           or - for the input\n"
               "  [x]      A thing\n");

    destroy_root(root);
}

/*
 * A description that is the empty string, or that is empty without the
 * newlines at its end, is treated as not set: no empty paragraph for the
 * command, and a line with the heading only for an item.
 */
NAOS_TEST(empty_description_is_not_written)
{
    argx_command_t *root = create_root();
    argx_command_t *add;

    describe(root, "");
    add = add_sub(root, "add", "");
    add_sub(root, "remove", "\n\n");
    add_sub(root, "rename", "\nOnly on the second line");
    add_sub(root, "list", "List the remotes");
    add_positional(add, "x", ARGX_ARITY_OPTIONAL, "");
    add_positional(add, "y", ARGX_ARITY_OPTIONAL, "\n");
    add_positional(add, "z", ARGX_ARITY_OPTIONAL, "A thing");
    validate(root);

    check_help(root,
               "Usage: tool [COMMAND]\n"
               "\n"
               "Commands:\n"
               "  add\n"
               "  remove\n"
               "  rename\n"
               "  list    List the remotes\n");

    check_help(add,
               "Usage: tool add [x] [y] [z]\n"
               "\n"
               "Arguments:\n"
               "  [x]\n"
               "  [y]\n"
               "  [z]  A thing\n");

    destroy_root(root);

    /* The description of the command is only newlines. */
    root = create_root();
    describe(root, "\n\n");
    add_sub(root, "add", "Add a remote");
    validate(root);

    check_help(root,
               "Usage: tool [COMMAND]\n"
               "\n"
               "Commands:\n"
               "  add  Add a remote\n");

    destroy_root(root);
}

/*
 * An item without a description is a line with its heading only; its
 * heading still counts for the description column of the others.
 */
NAOS_TEST(item_without_description_is_heading_only)
{
    argx_command_t *root = create_root();
    argx_command_t *add = add_sub(root, "add", "Add a remote");

    add_sub(root, "remove-all", NULL);
    add_positional(add, "name", ARGX_ARITY_REQUIRED, NULL);
    add_positional(add, "x", ARGX_ARITY_OPTIONAL, "A thing");
    add_positional(add, "rest", ARGX_ARITY_ZERO_OR_MORE, NULL);
    validate(root);

    check_help(root,
               "Usage: tool [COMMAND]\n"
               "\n"
               "Commands:\n"
               "  add         Add a remote\n"
               "  remove-all\n");

    check_help(add,
               "Usage: tool add <name> [x] [rest]...\n"
               "\n"
               "Add a remote\n"
               "\n"
               "Arguments:\n"
               "  <name>\n"
               "  [x]        A thing\n"
               "  [rest]...\n");

    destroy_root(root);
}

/*
 * The newlines at the end of the description of a command are not written:
 * the next part follows behind one empty line. The newlines inside it
 * are kept.
 */
NAOS_TEST(command_description_keeps_inner_newlines_and_drops_trailing_ones)
{
    argx_command_t *root = create_root();

    describe(root, "First line.\nSecond line.\n\nSecond paragraph.\n\n");
    add_sub(root, "add", "Add a remote");
    validate(root);

    check_help(root,
               "Usage: tool [COMMAND]\n"
               "\n"
               "First line.\n"
               "Second line.\n"
               "\n"
               "Second paragraph.\n"
               "\n"
               "Commands:\n"
               "  add  Add a remote\n");

    destroy_root(root);
}

/*
 * The width of a heading is the number of all its bytes, also when the
 * name holds a newline: "<a\nb>" counts as 5 columns, not as the 2 of
 * "b>", so the description column is 2 + 5 + 2 and not the 2 + 4 + 2 that
 * "<in>" alone would give. The strings are written as they are.
 */
NAOS_TEST(heading_with_newline_inside_counts_all_its_bytes)
{
    argx_command_t *root = create_root();

    add_positional(root, "a\nb", ARGX_ARITY_REQUIRED, "Two lines");
    add_positional(root, "in", ARGX_ARITY_REQUIRED, "One line");
    validate(root);

    check_help(root,
               "Usage: tool <a\nb> <in>\n"
               "\n"
               "Arguments:\n"
               "  <a\nb>       Two lines\n"
               "  <in>   One line\n");

    destroy_root(root);
}

/* ---- The settings --------------------------------------------------------- */

/*
 * Without help options the width is 80: the description of the command is
 * broken there, and so is the description of an item, which goes on at
 * the description column.
 */
NAOS_TEST(descriptions_wrap_at_default_width)
{
    argx_command_t *root = create_root();

    describe(root, "This description of the command is long enough to go beyond the "
                   "eighty columns of the default width.");
    add_sub(root, "add", "This description of the subcommand is long enough to go beyond "
                         "the eighty columns as well.");
    validate(root);

    check_help(root,
               "Usage: tool [COMMAND]\n"
               "\n"
               "This description of the command is long enough to go beyond the eighty columns\n"
               "of the default width.\n"
               "\n"
               "Commands:\n"
               "  add  This description of the subcommand is long enough to go beyond the eighty\n"
               "       columns as well.\n");

    destroy_root(root);
}

/*
 * The width and the indent of the help options are used: the descriptions
 * are broken at the width, the items start behind the indent, and the
 * description column follows from the indent.
 */
NAOS_TEST(width_and_indent_of_options_are_used)
{
    argx_command_t *root = create_root();
    argx_command_t *add = add_sub(root, "add", "Add a remote to the list of the known remotes");
    argx_help_options_t *options = create_options(40, 4);

    describe(root, "Manage the remote repositories of the project.");
    add_positional(add, "x", ARGX_ARITY_OPTIONAL, "The name that the remote is known by");
    validate(root);

    /* The column is 4 + 3 + 2 = 9, at most half the width. */
    check_help_with(root, options,
                    "Usage: tool [COMMAND]\n"
                    "\n"
                    "Manage the remote repositories of the\n"
                    "project.\n"
                    "\n"
                    "Commands:\n"
                    "    add  Add a remote to the list of the\n"
                    "         known remotes\n");
    check_help_with(add, options,
                    "Usage: tool add [x]\n"
                    "\n"
                    "Add a remote to the list of the known\n"
                    "remotes\n"
                    "\n"
                    "Arguments:\n"
                    "    [x]  The name that the remote is\n"
                    "         known by\n");

    /* The indent 0: the items start the line. */
    argx_help_options_set_indent(options, 0);
    check_help_with(root, options,
                    "Usage: tool [COMMAND]\n"
                    "\n"
                    "Manage the remote repositories of the\n"
                    "project.\n"
                    "\n"
                    "Commands:\n"
                    "add  Add a remote to the list of the\n"
                    "     known remotes\n");
    check_help_with(add, options,
                    "Usage: tool add [x]\n"
                    "\n"
                    "Add a remote to the list of the known\n"
                    "remotes\n"
                    "\n"
                    "Arguments:\n"
                    "[x]  The name that the remote is known\n"
                    "     by\n");

    /* The width 0: nothing is broken. */
    argx_help_options_set_width(options, 0);
    argx_help_options_set_indent(options, 2);
    check_help_with(root, options,
                    "Usage: tool [COMMAND]\n"
                    "\n"
                    "Manage the remote repositories of the project.\n"
                    "\n"
                    "Commands:\n"
                    "  add  Add a remote to the list of the known remotes\n");
    check_help_with(add, options,
                    "Usage: tool add [x]\n"
                    "\n"
                    "Add a remote to the list of the known remotes\n"
                    "\n"
                    "Arguments:\n"
                    "  [x]  The name that the remote is known by\n");

    argx_help_options_destroy(options);
    destroy_root(root);
}

/* Help options as they were created give the text of no help options. */
NAOS_TEST(null_options_are_the_default_options)
{
    static const char expected_root[] = "Usage: tool [COMMAND]\n"
                                        "\n"
                                        "Do things.\n"
                                        "\n"
                                        "Commands:\n"
                                        "  run  Run it\n";
    static const char expected_run[] = "Usage: tool run [x]\n"
                                       "\n"
                                       "Run it\n"
                                       "\n"
                                       "Arguments:\n"
                                       "  [x]  A thing\n";
    argx_command_t *root = create_root();
    argx_command_t *run = add_sub(root, "run", "Run it");
    argx_help_options_t *options = NULL;

    describe(root, "Do things.");
    add_positional(run, "x", ARGX_ARITY_OPTIONAL, "A thing");
    validate(root);

    NAOS_REQUIRE_TRUE(argx_help_options_create(&options) == ARGX_ERROR_NONE);

    check_help(root, expected_root);
    check_help_with(root, options, expected_root);
    check_help(run, expected_run);
    check_help_with(run, options, expected_run);

    argx_help_options_destroy(options);
    destroy_root(root);
}

/* ---- The usage line -------------------------------------------------------- */

/*
 * Checks that the text of argx_help_write_usage() for command and
 * options is the start of the text of argx_help_write(), and that the
 * help goes on behind it.
 */
static void check_usage_is_head(const argx_command_t *command,
                                const argx_help_options_t *options)
{
    sink_t usage;
    sink_t help;

    sink_init(&usage);
    sink_init(&help);

    NAOS_REQUIRE_TRUE(argx_help_write_usage(command, options, sink_write, &usage) ==
                      ARGX_ERROR_NONE);
    NAOS_REQUIRE_TRUE(argx_help_write(command, options, sink_write, &help) == ARGX_ERROR_NONE);

    NAOS_REQUIRE_TRUE(usage.length != 0);
    NAOS_REQUIRE_TRUE(help.length > usage.length);
    NAOS_CHECK_MEM_EQ(help.text, usage.length, usage.text, usage.length);
    /* The usage line is followed by the empty line before the next part. */
    NAOS_CHECK_TRUE(help.text[usage.length] == '\n');
}

/*
 * The text of the usage line alone is the start of the help, with the
 * same help options: on one line, and broken into several.
 */
NAOS_TEST(usage_alone_is_the_head_of_the_help)
{
    argx_command_t *root = create_root();
    argx_command_t *remote = add_sub(root, "remote", "Manage the remotes");
    argx_help_options_t *options = create_options(30, 2);

    describe(root, "The tool.");
    add_positional(remote, "name", ARGX_ARITY_REQUIRED, "Name of the remote");
    add_positional(remote, "url", ARGX_ARITY_OPTIONAL, "Address");
    add_positional(remote, "tags", ARGX_ARITY_ZERO_OR_MORE, "Tags to set");
    validate(root);

    check_usage_is_head(root, NULL);
    check_usage_is_head(remote, NULL);
    check_usage_is_head(root, options);
    check_usage_is_head(remote, options);

    /*
     * With the width 30 the usage line of the subcommand has two lines.
     * The column is 2 + 9 + 2 = 13, at most half the width.
     */
    check_help_with(remote, options,
                    "Usage: tool remote <name>\n"
                    "       [url] [tags]...\n"
                    "\n"
                    "Manage the remotes\n"
                    "\n"
                    "Arguments:\n"
                    "  <name>     Name of the\n"
                    "             remote\n"
                    "  [url]      Address\n"
                    "  [tags]...  Tags to set\n");

    argx_help_options_destroy(options);
    destroy_root(root);
}

/* ---- Repeating, and the schema --------------------------------------------- */

/*
 * What the reading functions give for the schema of
 * same_input_gives_same_text_and_schema_is_unchanged.
 */
typedef struct snapshot
{
    bool validated;
    const char *root_name;
    const char *root_description;
    const argx_command_t *root_parent;
    size_t options;
    size_t positionals;
    size_t subcommands;
    size_t sub_positionals;
    size_t sub_subcommands;
    const argx_command_t *sub;
    const char *sub_name;
    const char *sub_description;
    const argx_command_t *sub_parent;
    const argx_positional_t *positional;
    const char *positional_name;
    const char *positional_description;
    const char *positional_default;
    argx_arity_t positional_arity;
    const argx_type_t *positional_type;
} snapshot_t;

static snapshot_t take_snapshot(const argx_command_t *root)
{
    snapshot_t snapshot;

    memset(&snapshot, 0, sizeof snapshot);
    snapshot.validated = argx_command_is_validated(root);
    snapshot.root_name = argx_command_name(root);
    snapshot.root_description = argx_command_description(root);
    snapshot.root_parent = argx_command_parent(root);
    snapshot.options = argx_command_option_count(root);
    snapshot.positionals = argx_command_positional_count(root);
    snapshot.subcommands = argx_command_subcommand_count(root);
    snapshot.sub = argx_command_subcommand_at(root, 0);
    snapshot.sub_name = argx_command_name(snapshot.sub);
    snapshot.sub_description = argx_command_description(snapshot.sub);
    snapshot.sub_parent = argx_command_parent(snapshot.sub);
    snapshot.sub_positionals = argx_command_positional_count(snapshot.sub);
    snapshot.sub_subcommands = argx_command_subcommand_count(snapshot.sub);
    snapshot.positional = argx_command_positional_at(snapshot.sub, 0);
    snapshot.positional_name = argx_positional_name(snapshot.positional);
    snapshot.positional_description = argx_positional_description(snapshot.positional);
    snapshot.positional_default = argx_positional_default(snapshot.positional);
    snapshot.positional_arity = argx_positional_arity(snapshot.positional);
    snapshot.positional_type = argx_positional_type(snapshot.positional);
    return snapshot;
}

/*
 * Checks that the reading functions give for root what they gave for
 * before: the same objects, the same strings at the same places with the
 * same bytes.
 */
static void check_snapshot(const argx_command_t *root, const snapshot_t *before)
{
    snapshot_t after = take_snapshot(root);

    NAOS_CHECK_TRUE(after.validated == before->validated);
    NAOS_CHECK_TRUE(after.root_name == before->root_name);
    NAOS_CHECK_STR_EQ(after.root_name, "tool");
    NAOS_CHECK_TRUE(after.root_description == before->root_description);
    NAOS_CHECK_STR_EQ(after.root_description, "Do things.\n\nIn two paragraphs.\n");
    NAOS_CHECK_TRUE(after.root_parent == NULL);
    NAOS_CHECK_UINT_EQ(after.options, before->options);
    NAOS_CHECK_UINT_EQ(after.positionals, before->positionals);
    NAOS_CHECK_UINT_EQ(after.subcommands, before->subcommands);
    NAOS_CHECK_UINT_EQ(after.sub_positionals, before->sub_positionals);
    NAOS_CHECK_UINT_EQ(after.sub_subcommands, before->sub_subcommands);
    NAOS_CHECK_TRUE(after.sub == before->sub);
    NAOS_CHECK_TRUE(after.sub_name == before->sub_name);
    NAOS_CHECK_STR_EQ(after.sub_name, "run");
    NAOS_CHECK_TRUE(after.sub_description == before->sub_description);
    NAOS_CHECK_STR_EQ(after.sub_description, "Run it\nwith care");
    NAOS_CHECK_TRUE(after.sub_parent == root);
    NAOS_CHECK_TRUE(after.positional == before->positional);
    NAOS_CHECK_TRUE(after.positional_name == before->positional_name);
    NAOS_CHECK_STR_EQ(after.positional_name, "x");
    NAOS_CHECK_TRUE(after.positional_description == before->positional_description);
    NAOS_CHECK_STR_EQ(after.positional_description, "A thing\n");
    NAOS_CHECK_TRUE(after.positional_default == before->positional_default);
    NAOS_CHECK_TRUE(after.positional_arity == before->positional_arity);
    NAOS_CHECK_TRUE(after.positional_type == before->positional_type);
}

/*
 * Two generations with the same input give the same bytes, and a
 * generation changes nothing of the schema: the reading functions give the
 * same before and after, and the schema is still validated.
 */
NAOS_TEST(same_input_gives_same_text_and_schema_is_unchanged)
{
    static const char expected[] = "Usage: tool [COMMAND]\n"
                                   "\n"
                                   "Do things.\n"
                                   "\n"
                                   "In two paragraphs.\n"
                                   "\n"
                                   "Commands:\n"
                                   "  run  Run it\n";
    argx_command_t *root = create_root();
    argx_command_t *run = add_sub(root, "run", "Run it\nwith care");
    argx_help_options_t *options = create_options(60, 2);
    snapshot_t before;
    sink_t first;
    sink_t second;

    describe(root, "Do things.\n\nIn two paragraphs.\n");
    add_positional(run, "x", ARGX_ARITY_OPTIONAL, "A thing\n");
    validate(root);

    before = take_snapshot(root);
    NAOS_REQUIRE_TRUE(before.validated);

    sink_init(&first);
    sink_init(&second);
    NAOS_REQUIRE_TRUE(argx_help_write(root, options, sink_write, &first) == ARGX_ERROR_NONE);
    check_snapshot(root, &before);
    NAOS_REQUIRE_TRUE(argx_help_write(root, options, sink_write, &second) == ARGX_ERROR_NONE);
    check_snapshot(root, &before);

    NAOS_CHECK_UINT_EQ(first.length, strlen(expected));
    NAOS_CHECK_MEM_EQ(first.text, first.length, expected, strlen(expected));
    NAOS_CHECK_UINT_EQ(second.length, first.length);
    NAOS_CHECK_MEM_EQ(second.text, second.length, first.text, first.length);

    /* The help of the subcommand does not change the schema either. */
    check_help_with(run, options,
                    "Usage: tool run [x]\n"
                    "\n"
                    "Run it\n"
                    "with care\n"
                    "\n"
                    "Arguments:\n"
                    "  [x]  A thing\n");
    check_snapshot(root, &before);

    argx_help_options_destroy(options);
    destroy_root(root);
}

/* ---- The write function fails ---------------------------------------------- */

/*
 * Whichever call of the write function fails: the generation returns
 * ARGX_ERROR_WRITE_FAILED, the write function is not called again, and
 * what it took before is the start of the text.
 */
NAOS_TEST(write_failure_is_reported_and_stops_the_calls)
{
    static const char expected[] = "Usage: tool [COMMAND]\n"
                                   "\n"
                                   "Do things.\n"
                                   "\n"
                                   "Commands:\n"
                                   "  run   Run it\n"
                                   "  stop  Stop it\n";
    argx_command_t *root = create_root();
    sink_t whole;
    size_t requests;
    size_t n;

    describe(root, "Do things.");
    add_sub(root, "run", "Run it");
    add_sub(root, "stop", "Stop it");
    validate(root);

    sink_init(&whole);
    NAOS_REQUIRE_TRUE(argx_help_write(root, NULL, sink_write, &whole) == ARGX_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(whole.length, strlen(expected));
    NAOS_CHECK_MEM_EQ(whole.text, whole.length, expected, strlen(expected));
    NAOS_REQUIRE_TRUE(whole.calls >= 2);

    requests = argx_test_allocator_requests();

    /* Each call in turn is the one that fails. */
    for (n = 1; n <= whole.calls; ++n)
    {
        sink_t sink;

        sink_init(&sink);
        sink.fail_at = n;

        NAOS_CHECK_TRUE(argx_help_write(root, NULL, sink_write, &sink) ==
                        ARGX_ERROR_WRITE_FAILED);
        NAOS_CHECK_UINT_EQ(sink.calls, n);
        NAOS_CHECK_UINT_EQ(sink.empty_calls, 0);
        NAOS_CHECK_TRUE(sink.length < strlen(expected));
        NAOS_CHECK_MEM_EQ(sink.text, sink.length, expected, sink.length);
    }

    NAOS_CHECK_UINT_EQ(argx_test_allocator_requests(), requests);
    NAOS_CHECK_TRUE(argx_command_is_validated(root));

    /* The schema is as usable as before: the next generation succeeds. */
    check_help(root, expected);

    destroy_root(root);
}

/* ---- The list of the options ---------------------------------------------- */

/*
 * A root with a description, subcommands and options: the usage line, the
 * description, "Commands:" and "Options:", in this order, each part behind
 * one empty line. The descriptions of both lists start at one column.
 */
NAOS_TEST(root_with_description_subcommands_and_options)
{
    argx_command_t *root = create_root();

    describe(root, "Manage the remote repositories.");
    add_sub(root, "add", "Add a remote");
    add_sub(root, "remove", "Remove a remote");
    add_flag(root, 'v', "verbose", "Say more");
    add_flag(root, '\0', "dry-run", "Change nothing");
    add_value(root, 'c', "config", "FILE", "Read the settings from FILE");
    validate(root);

    check_help(root,
               "Usage: tool [OPTIONS] [COMMAND]\n"
               "\n"
               "Manage the remote repositories.\n"
               "\n"
               "Commands:\n"
               "  add                  Add a remote\n"
               "  remove               Remove a remote\n"
               "\n"
               "Options:\n"
               "  -v, --verbose        Say more\n"
               "      --dry-run        Change nothing\n"
               "  -c, --config <FILE>  Read the settings from FILE\n");

    destroy_root(root);
}

/*
 * A subcommand of the second level with positionals and options: the path
 * from the root in the usage line, the description, "Arguments:" and
 * "Options:", in this order.
 */
NAOS_TEST(second_level_subcommand_with_positionals_and_options)
{
    argx_command_t *root = create_root();
    argx_command_t *remote = add_sub(root, "remote", "Manage the remotes");
    argx_command_t *add = add_sub(remote, "add", "Add a remote.");

    describe(root, "The tool.");
    add_positional(add, "name", ARGX_ARITY_REQUIRED, "Name of the remote");
    add_positional(add, "url", ARGX_ARITY_OPTIONAL, "Address of the remote");
    add_flag(add, 'f', "force", "Replace an existing remote");
    add_value(add, 't', "track", "BRANCH", "Branch to track");
    validate(root);

    check_help(add,
               "Usage: tool remote add [OPTIONS] <name> [url]\n"
               "\n"
               "Add a remote.\n"
               "\n"
               "Arguments:\n"
               "  <name>                Name of the remote\n"
               "  [url]                 Address of the remote\n"
               "\n"
               "Options:\n"
               "  -f, --force           Replace an existing remote\n"
               "  -t, --track <BRANCH>  Branch to track\n");

    destroy_root(root);
}

/*
 * A command with options only: "Options:" is the only list, behind one
 * empty line, with and without a description of the command.
 */
NAOS_TEST(command_with_options_only)
{
    argx_command_t *root = create_root();
    argx_command_t *run = add_sub(root, "run", NULL);
    sink_t sink;

    add_flag(run, 'q', "quiet", "Say nothing");
    validate(root);

    check_help(run,
               "Usage: tool run [OPTIONS]\n"
               "\n"
               "Options:\n"
               "  -q, --quiet  Say nothing\n");

    sink_init(&sink);
    NAOS_CHECK_TRUE(argx_help_write(run, NULL, sink_write, &sink) == ARGX_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(count_empty_lines(sink.text, sink.length), 1);

    describe(run, "Run it.");
    validate(root);

    check_help(run,
               "Usage: tool run [OPTIONS]\n"
               "\n"
               "Run it.\n"
               "\n"
               "Options:\n"
               "  -q, --quiet  Say nothing\n");

    destroy_root(root);
}

/*
 * Only the options of the command itself are listed: not those of its
 * parent, not those of its subcommands. The description column follows
 * from the headings of the command itself as well: 17 for the root, 20 for
 * "run" and 15 for "fast".
 */
NAOS_TEST(options_of_other_commands_are_not_listed)
{
    argx_command_t *root = create_root();
    argx_command_t *run = add_sub(root, "run", "Run it");
    argx_command_t *fast = add_sub(run, "fast", "Run it fast");

    add_flag(root, 'v', "verbose", "Say more");
    add_flag(run, 'k', "keep-going", "Go on after a failure");
    add_flag(fast, '\0', "turbo", "Go faster");
    validate(root);

    check_help(root,
               "Usage: tool [OPTIONS] [COMMAND]\n"
               "\n"
               "Commands:\n"
               "  run            Run it\n"
               "\n"
               "Options:\n"
               "  -v, --verbose  Say more\n");

    check_help(run,
               "Usage: tool run [OPTIONS] [COMMAND]\n"
               "\n"
               "Run it\n"
               "\n"
               "Commands:\n"
               "  fast              Run it fast\n"
               "\n"
               "Options:\n"
               "  -k, --keep-going  Go on after a failure\n");

    check_help(fast,
               "Usage: tool run fast [OPTIONS]\n"
               "\n"
               "Run it fast\n"
               "\n"
               "Options:\n"
               "      --turbo  Go faster\n");

    destroy_root(root);
}

/*
 * The options stand in the order in which they were added, whatever their
 * names and kinds are.
 */
NAOS_TEST(options_come_in_definition_order)
{
    argx_command_t *root = create_root();

    add_flag(root, 'z', NULL, "Last letter");
    add_count(root, '\0', "alpha", "First letter");
    add_value(root, 'm', "middle", "N", "Middle letter");
    validate(root);

    check_help(root,
               "Usage: tool [OPTIONS]\n"
               "\n"
               "Options:\n"
               "  -z                Last letter\n"
               "      --alpha...    First letter\n"
               "  -m, --middle <N>  Middle letter\n");

    destroy_root(root);
}

/*
 * "Commands:" and "Options:" have one description column, from the widest
 * heading of both: an option that is wider than every subcommand moves the
 * descriptions of the subcommands, to 2 + 22 + 2, and a subcommand that is
 * wider than every option moves the descriptions of the options, to
 * 2 + 19 + 2.
 */
NAOS_TEST(commands_and_options_share_the_description_column)
{
    argx_command_t *root = create_root();

    add_sub(root, "a", "First");
    add_flag(root, '\0', "long-option-name", "Wide");
    validate(root);

    check_help(root,
               "Usage: tool [OPTIONS] [COMMAND]\n"
               "\n"
               "Commands:\n"
               "  a                       First\n"
               "\n"
               "Options:\n"
               "      --long-option-name  Wide\n");

    destroy_root(root);

    root = create_root();
    add_sub(root, "a-long-command-name", "Wide");
    add_flag(root, 'q', NULL, "Quiet");
    validate(root);

    check_help(root,
               "Usage: tool [OPTIONS] [COMMAND]\n"
               "\n"
               "Commands:\n"
               "  a-long-command-name  Wide\n"
               "\n"
               "Options:\n"
               "  -q                   Quiet\n");

    destroy_root(root);
}

/*
 * "Arguments:" and "Options:" have one description column as well: an
 * option that is wider than every positional moves the descriptions of the
 * positionals, to 2 + 22 + 2, and a positional that is wider than every
 * option moves the descriptions of the options, to 2 + 25 + 2.
 */
NAOS_TEST(arguments_and_options_share_the_description_column)
{
    argx_command_t *root = create_root();

    add_positional(root, "in", ARGX_ARITY_REQUIRED, "Input");
    add_flag(root, '\0', "long-option-name", "Wide");
    validate(root);

    check_help(root,
               "Usage: tool [OPTIONS] <in>\n"
               "\n"
               "Arguments:\n"
               "  <in>                    Input\n"
               "\n"
               "Options:\n"
               "      --long-option-name  Wide\n");

    destroy_root(root);

    root = create_root();
    add_positional(root, "a-long-argument-name", ARGX_ARITY_ZERO_OR_MORE, "Wide");
    add_flag(root, 'q', NULL, "Quiet");
    validate(root);

    check_help(root,
               "Usage: tool [OPTIONS] [a-long-argument-name]...\n"
               "\n"
               "Arguments:\n"
               "  [a-long-argument-name]...  Wide\n"
               "\n"
               "Options:\n"
               "  -q                         Quiet\n");

    destroy_root(root);
}

NAOS_SUITE(help,
    NAOS_ADD_TEST(root_with_description_and_subcommands),
    NAOS_ADD_TEST(second_level_subcommand_with_positionals),
    NAOS_ADD_TEST(command_without_description),
    NAOS_ADD_TEST(command_with_nothing_is_only_the_usage_line),
    NAOS_ADD_TEST(command_with_description_only),
    NAOS_ADD_TEST(parts_are_separated_by_one_empty_line),
    NAOS_ADD_TEST(items_come_in_definition_order),
    NAOS_ADD_TEST(grandchildren_are_not_listed),
    NAOS_ADD_TEST(subcommand_description_is_cut_at_first_newline),
    NAOS_ADD_TEST(positional_description_is_written_whole),
    NAOS_ADD_TEST(empty_description_is_not_written),
    NAOS_ADD_TEST(item_without_description_is_heading_only),
    NAOS_ADD_TEST(command_description_keeps_inner_newlines_and_drops_trailing_ones),
    NAOS_ADD_TEST(heading_with_newline_inside_counts_all_its_bytes),
    NAOS_ADD_TEST(descriptions_wrap_at_default_width),
    NAOS_ADD_TEST(width_and_indent_of_options_are_used),
    NAOS_ADD_TEST(null_options_are_the_default_options),
    NAOS_ADD_TEST(usage_alone_is_the_head_of_the_help),
    NAOS_ADD_TEST(same_input_gives_same_text_and_schema_is_unchanged),
    NAOS_ADD_TEST(write_failure_is_reported_and_stops_the_calls),
    NAOS_ADD_TEST(root_with_description_subcommands_and_options),
    NAOS_ADD_TEST(second_level_subcommand_with_positionals_and_options),
    NAOS_ADD_TEST(command_with_options_only),
    NAOS_ADD_TEST(options_of_other_commands_are_not_listed),
    NAOS_ADD_TEST(options_come_in_definition_order),
    NAOS_ADD_TEST(commands_and_options_share_the_description_column),
    NAOS_ADD_TEST(arguments_and_options_share_the_description_column)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(help)
)
