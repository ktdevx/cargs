/*
 * Tests of the layout of the help: the description column of
 * clargs_help_write() of <clargs/help.h>, its limit, and the items whose
 * heading is too wide for it.
 *
 * Each test builds a schema, validates it, has the help of its root
 * written with help options and compares the text with the expected
 * string, by its length and by its bytes. The write function of the tests
 * appends what it is given to an array of a fixed size.
 *
 * The rules that the tests pin down, with the indent I, the width W, the
 * maximum column M of the help options and the width w of a heading:
 *
 * - The limit is M or, when W is not 0 and W / 2, rounded down, is less,
 *   W / 2; but I + 2 when that is greater.
 * - The description column C is I + the widest w of the help + 2, or the
 *   limit when that is less. It is the same for both lists of a help.
 * - An item with I + w + 2 <= C has its description on the line of the
 *   heading. Any other item has a line with the heading only, and the
 *   description on the next line, from C on.
 *
 * A command of a schema has subcommands or positionals, not both, so a
 * help has two lists at most: "Commands:" and "Options:", or "Arguments:"
 * and "Options:".
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
#define SINK_SIZE ((size_t)1024)

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

/* Sets the description of command; it must succeed. */
static void describe(clargs_command_t *command, const char *description)
{
    NAOS_REQUIRE_TRUE(clargs_command_set_description(command, description) == CLARGS_ERROR_NONE);
}

/* Adds a subcommand that must succeed, and returns it. description is its
 * description, or NULL to set none. */
static clargs_command_t *add_sub(clargs_command_t *command, const char *name,
                                 const char *description)
{
    clargs_command_t *sub = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_subcommand(command, name, &sub) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(sub != NULL);
    if (description != NULL) {
        describe(sub, description);
    }
    return sub;
}

/* Adds a positional of strings that must succeed. description is its
 * description, or NULL to set none. */
static void add_positional(clargs_command_t *command, const char *name, clargs_arity_t arity,
                           const char *description)
{
    clargs_positional_t *positional = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_positional(command, name, clargs_type_string(), arity,
                                                    &positional) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(positional != NULL);
    if (description != NULL) {
        NAOS_REQUIRE_TRUE(clargs_positional_set_description(positional, description) ==
                          CLARGS_ERROR_NONE);
    }
}

/* Adds a flag that must succeed. short_name is '\0' and long_name is NULL
 * for a name that the option does not have; description is its
 * description, or NULL to set none. */
static void add_flag(clargs_command_t *command, char short_name, const char *long_name,
                     const char *description)
{
    clargs_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_flag(command, short_name, long_name, &option) ==
                      CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(option != NULL);
    if (description != NULL) {
        NAOS_REQUIRE_TRUE(clargs_option_set_description(option, description) == CLARGS_ERROR_NONE);
    }
}

/* Adds an option with a value of strings that must succeed, with the
 * display name value_name. The option is not required. default_value is
 * its default value, or NULL to set none. */
static void add_value(clargs_command_t *command, char short_name, const char *long_name,
                      const char *value_name, const char *default_value,
                      const char *description)
{
    clargs_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_value(command, short_name, long_name, clargs_type_string(),
                                               &option) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(option != NULL);
    NAOS_REQUIRE_TRUE(clargs_option_set_value_name(option, value_name) == CLARGS_ERROR_NONE);
    if (default_value != NULL) {
        NAOS_REQUIRE_TRUE(clargs_option_set_default(option, default_value) == CLARGS_ERROR_NONE);
    }
    if (description != NULL) {
        NAOS_REQUIRE_TRUE(clargs_option_set_description(option, description) == CLARGS_ERROR_NONE);
    }
}

/* Validates the tree of root; it must have no fault. */
static void validate(clargs_command_t *root)
{
    NAOS_REQUIRE_TRUE(clargs_command_validate(root, NULL, NULL, NULL) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_command_is_validated(root));
}

/* Returns whether a line of the length bytes of text ends with a blank: a
 * blank before a newline, or at the end of the text. */
static bool has_blank_at_line_end(const char *text, size_t length)
{
    size_t index;

    for (index = 0; index < length; ++index) {
        if (text[index] == ' ' && (index + 1 == length || text[index + 1] == '\n')) {
            return true;
        }
    }
    return false;
}

/* Checks the shape of a help text: no line ends with a blank, there is no
 * '\r', the text ends with one "\n", and it neither starts nor ends with
 * an empty line. */
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
static void check_help_with(const clargs_command_t *command, const clargs_help_options_t *options,
                            const char *expected)
{
    sink_t sink;
    size_t live = clargs_test_allocator_live();

    sink_init(&sink);

    NAOS_CHECK_TRUE(clargs_help_write(command, options, sink_write, &sink) == CLARGS_ERROR_NONE);

    NAOS_CHECK_UINT_EQ(sink.length, strlen(expected));
    NAOS_CHECK_MEM_EQ(sink.text, sink.length, expected, strlen(expected));
    check_shape(sink.text, sink.length);
    check_shape(expected, strlen(expected));
    NAOS_CHECK_UINT_EQ(sink.empty_calls, 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
    NAOS_CHECK_TRUE(clargs_command_is_validated(command));
}

/* Has the help of command written with help options that have the width,
 * the indent and the maximum column given, and checks it as
 * check_help_with() does. The help options are destroyed again. */
static void check_layout(const clargs_command_t *command, size_t width, size_t indent,
                         size_t max_column, const char *expected)
{
    clargs_help_options_t *options = NULL;

    NAOS_REQUIRE_TRUE(clargs_help_options_create(&options) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(options != NULL);
    clargs_help_options_set_width(options, width);
    clargs_help_options_set_indent(options, indent);
    clargs_help_options_set_max_column(options, max_column);

    check_help_with(command, options, expected);

    clargs_help_options_destroy(options);
}

/* ---- The limit of the column -------------------------------------------- */

/*
 * With the maximum column 12 a name of 11 bytes is too wide: the column
 * is 12, the name has a line of its own and its description starts on the
 * next line at the column 12. The shorter names keep their descriptions
 * on their lines. The subcommands are written without the cell text.
 */
NAOS_TEST(heading_beyond_the_limit_goes_to_its_own_line)
{
    clargs_command_t *root = create_root();

    add_sub(root, "add", "Add a remote");
    add_sub(root, "synchronize", "Synchronize all");
    add_sub(root, "rm", "Remove a remote");
    validate(root);

    check_layout(root, 80, 2, 12,
                 "Usage: tool [COMMAND]\n"
                 "\n"
                 "Commands:\n"
                 "  add       Add a remote\n"
                 "  synchronize\n"
                 "            Synchronize all\n"
                 "  rm        Remove a remote\n");

    destroy_root(root);
}

/*
 * With the default settings the limit is 32, as half of the width 80 is
 * more: an option whose heading has 41 bytes goes to a line of its own,
 * and all the descriptions start at the column 32. NULL stands for the
 * default settings.
 */
NAOS_TEST(default_limit_is_the_column_32)
{
    clargs_command_t *root = create_root();

    add_flag(root, 'v', "verbose", "Say more");
    add_value(root, '\0', "a-rather-long-name-of-option", "FILE", NULL, "Write to FILE");
    add_flag(root, 'h', "help", "Show the help");
    validate(root);

    check_help_with(root, NULL,
                    "Usage: tool [OPTIONS]\n"
                    "\n"
                    "Options:\n"
                    "  -v, --verbose                 Say more\n"
                    "      --a-rather-long-name-of-option <FILE>\n"
                    "                                Write to FILE\n"
                    "  -h, --help                    Show the help\n");

    destroy_root(root);
}

/*
 * The boundary, with the indent 2 and the column 12: behind a name of 8
 * bytes exactly 2 blanks reach the column, and the description stays on
 * the line. Behind a name of 9 bytes there is 1 column only, behind one
 * of 10 bytes none: both descriptions go to the next line.
 */
NAOS_TEST(two_blanks_keep_the_description_on_the_line)
{
    clargs_command_t *root = create_root();

    add_sub(root, "eightchr", "Two blanks");
    add_sub(root, "ninechars", "One blank");
    add_sub(root, "tenletters", "No blank");
    validate(root);

    check_layout(root, 80, 2, 12,
                 "Usage: tool [COMMAND]\n"
                 "\n"
                 "Commands:\n"
                 "  eightchr  Two blanks\n"
                 "  ninechars\n"
                 "            One blank\n"
                 "  tenletters\n"
                 "            No blank\n");

    destroy_root(root);
}

/*
 * Half of the width is the limit when it is less than the maximum
 * column, here the default 32. It is rounded down: the widths 20 and 21
 * both give the column 10, where a name of 6 bytes keeps its description
 * and one of 7 bytes does not; the width 22 gives the column 11, the one
 * of the widest name, and both names keep theirs.
 */
NAOS_TEST(half_of_the_width_is_the_limit)
{
    clargs_command_t *root = create_root();

    add_sub(root, "sixsix", "Six");
    add_sub(root, "sevens7", "Seven");
    validate(root);

    check_layout(root, 20, 2, 32,
                 "Usage: tool\n"
                 "       [COMMAND]\n"
                 "\n"
                 "Commands:\n"
                 "  sixsix  Six\n"
                 "  sevens7\n"
                 "          Seven\n");

    check_layout(root, 21, 2, 32,
                 "Usage: tool [COMMAND]\n"
                 "\n"
                 "Commands:\n"
                 "  sixsix  Six\n"
                 "  sevens7\n"
                 "          Seven\n");

    check_layout(root, 22, 2, 32,
                 "Usage: tool [COMMAND]\n"
                 "\n"
                 "Commands:\n"
                 "  sixsix   Six\n"
                 "  sevens7  Seven\n");

    destroy_root(root);
}

/*
 * The limit is the smaller of the two. A name of 40 bytes asks for the
 * column 44. The maximum column 100 and the width 80: half the width, 40,
 * is the column. The maximum column 36 and the width 80: the maximum is
 * the column.
 */
NAOS_TEST(limit_is_the_smaller_of_maximum_and_half_width)
{
    clargs_command_t *root = create_root();

    add_sub(root, "add", "Add");
    add_sub(root, "abcdefghijklmnopqrstuvwxyz0123456789abcd", "Long");
    validate(root);

    check_layout(root, 80, 2, 100,
                 "Usage: tool [COMMAND]\n"
                 "\n"
                 "Commands:\n"
                 "  add                                   Add\n"
                 "  abcdefghijklmnopqrstuvwxyz0123456789abcd\n"
                 "                                        Long\n");

    check_layout(root, 80, 2, 36,
                 "Usage: tool [COMMAND]\n"
                 "\n"
                 "Commands:\n"
                 "  add                               Add\n"
                 "  abcdefghijklmnopqrstuvwxyz0123456789abcd\n"
                 "                                    Long\n");

    destroy_root(root);
}

/*
 * With the width 0 there is no half: the maximum column alone is the
 * limit. With the default 32 a name of 28 bytes keeps its description
 * and one of 29 bytes does not.
 */
NAOS_TEST(width_zero_leaves_the_maximum_as_the_limit)
{
    clargs_command_t *root = create_root();

    add_sub(root, "abcdefghijklmnopqrstuvwxyz01", "Fits");
    add_sub(root, "abcdefghijklmnopqrstuvwxyz012", "Does not fit");
    validate(root);

    check_layout(root, 0, 2, 32,
                 "Usage: tool [COMMAND]\n"
                 "\n"
                 "Commands:\n"
                 "  abcdefghijklmnopqrstuvwxyz01  Fits\n"
                 "  abcdefghijklmnopqrstuvwxyz012\n"
                 "                                Does not fit\n");

    destroy_root(root);
}

/*
 * The name of 40 bytes, which half of the width 80 sends to a line of
 * its own (see limit_is_the_smaller_of_maximum_and_half_width), sets the
 * column, 44, when the width is 0 and the maximum column is 100.
 */
NAOS_TEST(width_zero_does_not_halve_a_great_maximum)
{
    clargs_command_t *root = create_root();

    add_sub(root, "add", "Add");
    add_sub(root, "abcdefghijklmnopqrstuvwxyz0123456789abcd", "Long");
    validate(root);

    check_layout(root, 0, 2, 100,
                 "Usage: tool [COMMAND]\n"
                 "\n"
                 "Commands:\n"
                 "  add                                       Add\n"
                 "  abcdefghijklmnopqrstuvwxyz0123456789abcd  Long\n");

    destroy_root(root);
}

/*
 * The limit is never less than the indent + 2. With the indent 4 the
 * column is 6 for the maximum column 3, for the maximum column 0, and
 * for the width 8, whose half is 4. No heading ends 2 columns before it,
 * so every description is on the next line. With the indent 0 and the
 * maximum column 0 the column is 2.
 */
NAOS_TEST(indent_plus_two_is_the_least_limit)
{
    clargs_command_t *root = create_root();

    add_sub(root, "a", "Add");
    add_sub(root, "bb", "Bee");
    validate(root);

    check_layout(root, 80, 4, 3,
                 "Usage: tool [COMMAND]\n"
                 "\n"
                 "Commands:\n"
                 "    a\n"
                 "      Add\n"
                 "    bb\n"
                 "      Bee\n");

    check_layout(root, 80, 4, 0,
                 "Usage: tool [COMMAND]\n"
                 "\n"
                 "Commands:\n"
                 "    a\n"
                 "      Add\n"
                 "    bb\n"
                 "      Bee\n");

    check_layout(root, 8, 4, 32,
                 "Usage: tool\n"
                 "       [COMMAND]\n"
                 "\n"
                 "Commands:\n"
                 "    a\n"
                 "      Add\n"
                 "    bb\n"
                 "      Bee\n");

    check_layout(root, 80, 0, 0,
                 "Usage: tool [COMMAND]\n"
                 "\n"
                 "Commands:\n"
                 "a\n"
                 "  Add\n"
                 "bb\n"
                 "  Bee\n");

    destroy_root(root);
}

/*
 * "Commands:" and "Options:", with the maximum column 20: a name that
 * is too wide goes to a line of its own, and the descriptions of both
 * lists start at the column 20.
 */
NAOS_TEST(long_command_leaves_the_column_of_the_options)
{
    clargs_command_t *root = create_root();

    add_sub(root, "add", "Add");
    add_sub(root, "synchronize-everything", "Sync");
    add_flag(root, 'v', "verbose", "Say more");
    add_flag(root, 'h', "help", "Show help");
    validate(root);

    check_layout(root, 80, 2, 20,
                 "Usage: tool [OPTIONS] [COMMAND]\n"
                 "\n"
                 "Commands:\n"
                 "  add               Add\n"
                 "  synchronize-everything\n"
                 "                    Sync\n"
                 "\n"
                 "Options:\n"
                 "  -v, --verbose     Say more\n"
                 "  -h, --help        Show help\n");

    destroy_root(root);
}

/*
 * "Commands:" and "Options:", with the maximum column 20: the heading
 * that is too wide is one of the options.
 */
NAOS_TEST(long_option_leaves_the_column_of_the_commands)
{
    clargs_command_t *root = create_root();

    add_sub(root, "add", "Add");
    add_sub(root, "remove", "Remove");
    add_flag(root, 'v', "verbose", "Say more");
    add_value(root, 'o', "output-directory", "DIR", NULL, "Write there");
    validate(root);

    check_layout(root, 80, 2, 20,
                 "Usage: tool [OPTIONS] [COMMAND]\n"
                 "\n"
                 "Commands:\n"
                 "  add               Add\n"
                 "  remove            Remove\n"
                 "\n"
                 "Options:\n"
                 "  -v, --verbose     Say more\n"
                 "  -o, --output-directory <DIR>\n"
                 "                    Write there\n");

    destroy_root(root);
}

/*
 * "Arguments:" and "Options:", with the maximum column 20: the heading
 * that is too wide is one of the positionals.
 */
NAOS_TEST(long_argument_leaves_the_column_of_the_options)
{
    clargs_command_t *root = create_root();

    add_positional(root, "input", CLARGS_ARITY_REQUIRED, "Read it");
    add_positional(root, "a-long-positional-name", CLARGS_ARITY_REQUIRED, "Long one");
    add_flag(root, 'v', "verbose", "Say more");
    validate(root);

    check_layout(root, 80, 2, 20,
                 "Usage: tool [OPTIONS] <input> <a-long-positional-name>\n"
                 "\n"
                 "Arguments:\n"
                 "  <input>           Read it\n"
                 "  <a-long-positional-name>\n"
                 "                    Long one\n"
                 "\n"
                 "Options:\n"
                 "  -v, --verbose     Say more\n");

    destroy_root(root);
}

/*
 * "Arguments:" and "Options:", with the maximum column 20: the heading
 * that is too wide is one of the options.
 */
NAOS_TEST(long_option_leaves_the_column_of_the_arguments)
{
    clargs_command_t *root = create_root();

    add_positional(root, "input", CLARGS_ARITY_REQUIRED, "Read it");
    add_flag(root, 'v', "verbose", "Say more");
    add_value(root, 'o', "output-directory", "DIR", NULL, "Write there");
    validate(root);

    check_layout(root, 80, 2, 20,
                 "Usage: tool [OPTIONS] <input>\n"
                 "\n"
                 "Arguments:\n"
                 "  <input>           Read it\n"
                 "\n"
                 "Options:\n"
                 "  -v, --verbose     Say more\n"
                 "  -o, --output-directory <DIR>\n"
                 "                    Write there\n");

    destroy_root(root);
}

/*
 * An option with a heading that is too wide and with annotations only:
 * they are its description and start on the next line. One with nothing
 * to show is a line with the heading only; no line follows it.
 */
NAOS_TEST(annotations_alone_go_to_the_next_line)
{
    clargs_command_t *root = create_root();

    add_value(root, 'o', "output-directory", "DIR", "out", NULL);
    add_flag(root, '\0', "a-flag-with-a-long-name", NULL);
    add_flag(root, 'v', "verbose", "Say more");
    validate(root);

    check_layout(root, 80, 2, 20,
                 "Usage: tool [OPTIONS]\n"
                 "\n"
                 "Options:\n"
                 "  -o, --output-directory <DIR>\n"
                 "                    [default: out]\n"
                 "      --a-flag-with-a-long-name\n"
                 "  -v, --verbose     Say more\n");

    destroy_root(root);
}

/*
 * A subcommand and a positional with a name that is too wide and without
 * a description: a line with the heading only, also as the last line of
 * the text.
 */
NAOS_TEST(long_heading_without_description_is_heading_only)
{
    clargs_command_t *root = create_root();

    add_sub(root, "synchronize-everything", NULL);
    add_sub(root, "add", "Add");
    add_sub(root, "another-rather-long-name", NULL);
    validate(root);

    check_layout(root, 80, 2, 20,
                 "Usage: tool [COMMAND]\n"
                 "\n"
                 "Commands:\n"
                 "  synchronize-everything\n"
                 "  add               Add\n"
                 "  another-rather-long-name\n");

    destroy_root(root);
}

/* The same for a positional, whose line is written from the cell text. */
NAOS_TEST(long_argument_without_description_is_heading_only)
{
    clargs_command_t *root = create_root();

    add_positional(root, "input", CLARGS_ARITY_REQUIRED, "Read it");
    add_positional(root, "a-long-positional-name", CLARGS_ARITY_REQUIRED, NULL);
    validate(root);

    check_layout(root, 80, 2, 20,
                 "Usage: tool <input> <a-long-positional-name>\n"
                 "\n"
                 "Arguments:\n"
                 "  <input>           Read it\n"
                 "  <a-long-positional-name>\n");

    destroy_root(root);
}

/*
 * The width of a heading with a newline inside a name is the number of all
 * its bytes, and it decides, not the column where the heading ends. With
 * the column 12: "<ab\ncd>", 7 bytes, keeps its description on its last
 * line; "<abcdefg\nh>", 11 bytes, does not, though it ends at the column
 * 2.
 */
NAOS_TEST(width_of_a_heading_with_newline_decides)
{
    clargs_command_t *root = create_root();

    add_positional(root, "ab\ncd", CLARGS_ARITY_REQUIRED, "Short");
    add_positional(root, "abcdefg\nh", CLARGS_ARITY_REQUIRED, "Long");
    validate(root);

    check_layout(root, 80, 2, 12,
                 "Usage: tool <ab\n"
                 "cd> <abcdefg\n"
                 "h>\n"
                 "\n"
                 "Arguments:\n"
                 "  <ab\n"
                 "cd>         Short\n"
                 "  <abcdefg\n"
                 "h>\n"
                 "            Long\n");

    destroy_root(root);
}

NAOS_SUITE(help_layout,
    NAOS_ADD_TEST(heading_beyond_the_limit_goes_to_its_own_line),
    NAOS_ADD_TEST(default_limit_is_the_column_32),
    NAOS_ADD_TEST(two_blanks_keep_the_description_on_the_line),
    NAOS_ADD_TEST(half_of_the_width_is_the_limit),
    NAOS_ADD_TEST(limit_is_the_smaller_of_maximum_and_half_width),
    NAOS_ADD_TEST(width_zero_leaves_the_maximum_as_the_limit),
    NAOS_ADD_TEST(width_zero_does_not_halve_a_great_maximum),
    NAOS_ADD_TEST(indent_plus_two_is_the_least_limit),
    NAOS_ADD_TEST(long_command_leaves_the_column_of_the_options),
    NAOS_ADD_TEST(long_option_leaves_the_column_of_the_commands),
    NAOS_ADD_TEST(long_argument_leaves_the_column_of_the_options),
    NAOS_ADD_TEST(long_option_leaves_the_column_of_the_arguments),
    NAOS_ADD_TEST(annotations_alone_go_to_the_next_line),
    NAOS_ADD_TEST(long_heading_without_description_is_heading_only),
    NAOS_ADD_TEST(long_argument_without_description_is_heading_only),
    NAOS_ADD_TEST(width_of_a_heading_with_newline_decides)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(help_layout)
)
