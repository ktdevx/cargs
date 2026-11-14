/*
 * Tests of the layout of the help: the description column of
 * clargs_help_write() of <clargs/help.h>, its limit, the items whose
 * heading is too wide for it, the breaking of the descriptions into lines,
 * the settings of the help options, and the shape of the lines.
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
 * - A description is broken at its blanks where a word would go beyond
 *   W, never inside a word; what follows a break or a newline of the
 *   description starts at C, or at 0 for the description of the command.
 * - A description that holds nothing but blanks and newlines counts as not
 *   set; the one of a subcommand is judged by its first line.
 * - In the trailing run of a description, the blanks and newlines behind
 *   its last other byte, the blanks are ignored and the newlines are kept.
 * - A setting above 65535 works as 65535.
 *
 * A command of a schema has subcommands or positionals, not both, so a
 * help has two lists at most: "Commands:" and "Options:", or "Arguments:"
 * and "Options:".
 *
 * Every text that a test of this file has generated goes through
 * generate(), which checks its shape with check_shape(): no line ends with
 * a blank, there is no '\r', the text ends with one "\n", and it has no
 * empty line at its start or at its end.
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
#include <stdlib.h>
#include <string.h>

/* The size of the text that the sink of most tests holds. */
#define SINK_SIZE ((size_t)1024)

/*
 * The size of the text that the sink of the tests of the greatest settings
 * holds: two lines of 65535 columns and more, and some short lines.
 */
#define BIG_SINK_SIZE ((size_t)140000)

/* The greatest value that a setting keeps. */
#define SETTING_MAX ((size_t)65535)

/* What the write function of the tests has received. */
typedef struct sink
{
    char *text;         /* the parts, joined in the order of the calls */
    size_t size;        /* the number of bytes that text has room for */
    size_t length;      /* the number of bytes of text */
    size_t calls;       /* the number of calls, the failed one included */
    size_t empty_calls; /* the calls with the length 0 or without a text */
    size_t fail_at;     /* the call that reports a failure; 0: none */
} sink_t;

/* Starts a sink that keeps its text in the size bytes of buffer. */
static void sink_init(sink_t *sink, char *buffer, size_t size)
{
    memset(sink, 0, sizeof *sink);
    sink->text = buffer;
    sink->size = size;
}

/*
 * Appends the part to the sink. The call number fail_at takes nothing and
 * reports a failure, and so would every later call.
 */
static bool sink_write(void *user_data, const char *text, size_t length)
{
    sink_t *sink = user_data;

    ++sink->calls;
    if (text == NULL || length == 0)
    {
        ++sink->empty_calls;
        return true;
    }
    if (sink->fail_at != 0 && sink->calls >= sink->fail_at)
    {
        return false;
    }
    if (length > sink->size - sink->length)
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

/*
 * Adds a subcommand that must succeed, and returns it. description is its
 * description, or NULL to set none.
 */
static clargs_command_t *add_sub(clargs_command_t *command, const char *name,
                                 const char *description)
{
    clargs_command_t *sub = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_subcommand(command, name, &sub) == CLARGS_ERROR_NONE);
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
static void add_positional(clargs_command_t *command, const char *name, clargs_arity_t arity,
                           const char *description)
{
    clargs_positional_t *positional = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_positional(command, name, clargs_type_string(), arity,
                                                    &positional) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(positional != NULL);
    if (description != NULL)
    {
        NAOS_REQUIRE_TRUE(clargs_positional_set_description(positional, description) ==
                          CLARGS_ERROR_NONE);
    }
}

/*
 * Adds a positional of strings that takes none or one argument and has the
 * default value given; it must succeed. description is its description, or
 * NULL to set none.
 */
static void add_positional_default(clargs_command_t *command, const char *name,
                                   const char *default_value, const char *description)
{
    clargs_positional_t *positional = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_positional(command, name, clargs_type_string(),
                                                    CLARGS_ARITY_OPTIONAL,
                                                    &positional) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(positional != NULL);
    NAOS_REQUIRE_TRUE(clargs_positional_set_default(positional, default_value) ==
                      CLARGS_ERROR_NONE);
    if (description != NULL)
    {
        NAOS_REQUIRE_TRUE(clargs_positional_set_description(positional, description) ==
                          CLARGS_ERROR_NONE);
    }
}

/*
 * Adds a flag that must succeed. short_name is '\0' and long_name is NULL
 * for a name that the option does not have; description is its
 * description, or NULL to set none.
 */
static void add_flag(clargs_command_t *command, char short_name, const char *long_name,
                     const char *description)
{
    clargs_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_flag(command, short_name, long_name, &option) ==
                      CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(option != NULL);
    if (description != NULL)
    {
        NAOS_REQUIRE_TRUE(clargs_option_set_description(option, description) == CLARGS_ERROR_NONE);
    }
}

/*
 * Adds an option with a value of strings that must succeed, with the
 * display name value_name. The option is not required. default_value is
 * its default value, or NULL to set none.
 */
static void add_value(clargs_command_t *command, char short_name, const char *long_name,
                      const char *value_name, const char *default_value,
                      const char *description)
{
    clargs_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_value(command, short_name, long_name, clargs_type_string(),
                                               &option) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(option != NULL);
    NAOS_REQUIRE_TRUE(clargs_option_set_value_name(option, value_name) == CLARGS_ERROR_NONE);
    if (default_value != NULL)
    {
        NAOS_REQUIRE_TRUE(clargs_option_set_default(option, default_value) == CLARGS_ERROR_NONE);
    }
    if (description != NULL)
    {
        NAOS_REQUIRE_TRUE(clargs_option_set_description(option, description) == CLARGS_ERROR_NONE);
    }
}

/* Validates the tree of root; it must have no fault. */
static void validate(clargs_command_t *root)
{
    NAOS_REQUIRE_TRUE(clargs_command_validate(root, NULL, NULL, NULL) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_command_is_validated(root));
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
        if (text[index] == ' ' && (index + 1 == length || text[index + 1] == '\n'))
        {
            return true;
        }
    }
    return false;
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
 * Has the help of command written with options to sink, which was started
 * with sink_init(). Every text of this file is generated here. Checks that
 * the call succeeds, that the text has the shape of a help text, that no
 * call of the write function was empty, that the generation left nothing
 * allocated and violated no rule of the allocator, and that the schema is
 * still validated. The generation may allocate: for the cell text of an
 * option or of a positional.
 */
static void generate(const clargs_command_t *command, const clargs_help_options_t *options,
                     sink_t *sink)
{
    size_t live = clargs_test_allocator_live();

    NAOS_CHECK_TRUE(clargs_help_write(command, options, sink_write, sink) == CLARGS_ERROR_NONE);

    check_shape(sink->text, sink->length);
    NAOS_CHECK_UINT_EQ(sink->empty_calls, 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
    NAOS_CHECK_TRUE(clargs_command_is_validated(command));
}

/*
 * Has the help of command written with options, checks the generation as
 * generate() does, and checks that the text is exactly the string
 * expected: by its length and by its bytes.
 */
static void check_help_with(const clargs_command_t *command, const clargs_help_options_t *options,
                            const char *expected)
{
    char buffer[SINK_SIZE];
    sink_t sink;

    sink_init(&sink, buffer, sizeof buffer);
    generate(command, options, &sink);

    NAOS_CHECK_UINT_EQ(sink.length, strlen(expected));
    NAOS_CHECK_MEM_EQ(sink.text, sink.length, expected, strlen(expected));
    check_shape(expected, strlen(expected));
}

/*
 * Creates help options that must succeed, with the width, the indent and
 * the maximum column given.
 */
static clargs_help_options_t *create_options(size_t width, size_t indent, size_t max_column)
{
    clargs_help_options_t *options = NULL;

    NAOS_REQUIRE_TRUE(clargs_help_options_create(&options) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(options != NULL);
    clargs_help_options_set_width(options, width);
    clargs_help_options_set_indent(options, indent);
    clargs_help_options_set_max_column(options, max_column);
    return options;
}

/*
 * Has the help of command written with help options that have the width,
 * the indent and the maximum column given, and checks it as
 * check_help_with() does. The help options are destroyed again.
 */
static void check_layout(const clargs_command_t *command, size_t width, size_t indent,
                         size_t max_column, const char *expected)
{
    clargs_help_options_t *options = create_options(width, indent, max_column);

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

/* ---- The breaking of the descriptions ------------------------------------ */

/*
 * With the width 30 the limit is 15, the column of the widest heading.
 * The descriptions of the subcommands and of the option are broken between
 * two words where the next word would go beyond the column 30, and go on at
 * the column 15; "copy up to date" ends exactly at the column 30. The
 * description of the command is broken in the same way and goes on at the
 * column 0. Of the description of a subcommand only the first line is
 * there.
 */
NAOS_TEST(long_descriptions_break_between_words)
{
    clargs_command_t *root = create_root();

    describe(root, "A tool that keeps the remote copies of a project in step");
    add_sub(root, "add", "Add a remote copy to the list");
    add_sub(root, "sync", "Bring every copy up to date\nSecond line, not shown");
    add_flag(root, 'q', "quiet", "Print nothing but the errors");
    validate(root);

    check_layout(root, 30, 2, 32,
                 "Usage: tool [OPTIONS]\n"
                 "       [COMMAND]\n"
                 "\n"
                 "A tool that keeps the remote\n"
                 "copies of a project in step\n"
                 "\n"
                 "Commands:\n"
                 "  add          Add a remote\n"
                 "               copy to the\n"
                 "               list\n"
                 "  sync         Bring every\n"
                 "               copy up to date\n"
                 "\n"
                 "Options:\n"
                 "  -q, --quiet  Print nothing\n"
                 "               but the errors\n");

    destroy_root(root);
}

/*
 * The description of a positional is broken in the same way: with the
 * width 30 it goes on at the column 11, the one of the heading.
 */
NAOS_TEST(long_description_of_an_argument_breaks_between_words)
{
    clargs_command_t *root = create_root();

    add_positional(root, "input", CLARGS_ARITY_REQUIRED, "The file that is read and then checked");
    validate(root);

    check_layout(root, 30, 2, 32,
                 "Usage: tool <input>\n"
                 "\n"
                 "Arguments:\n"
                 "  <input>  The file that is\n"
                 "           read and then\n"
                 "           checked\n");

    destroy_root(root);
}

/*
 * With the width 40 the limit is 20. The description of an item whose
 * heading is too wide starts on the next line at the column 20, and its
 * second line starts there as well, as do the lines of the other item.
 */
NAOS_TEST(long_description_below_a_long_heading_breaks_between_words)
{
    clargs_command_t *root = create_root();

    add_flag(root, 'v', "verbose", "Print more about every step that is taken");
    add_value(root, 'o', "output-directory", "DIR", NULL, "Write the files there and not here");
    validate(root);

    check_layout(root, 40, 2, 32,
                 "Usage: tool [OPTIONS]\n"
                 "\n"
                 "Options:\n"
                 "  -v, --verbose     Print more about\n"
                 "                    every step that is\n"
                 "                    taken\n"
                 "  -o, --output-directory <DIR>\n"
                 "                    Write the files\n"
                 "                    there and not here\n");

    destroy_root(root);
}

/*
 * A word that is wider than the room is written as it is, on a line of
 * its own, and goes beyond the width 30: the first word of the description
 * of the command, and a word in the middle of the description of an
 * option. The word behind it starts the next line.
 */
NAOS_TEST(word_wider_than_the_room_is_not_divided)
{
    clargs_command_t *root = create_root();

    describe(root, "Supercalifragilisticexpialidocious word");
    add_flag(root, 'u', "url", "See https://example.invalid/a/very/long/path now");
    validate(root);

    check_layout(root, 30, 2, 32,
                 "Usage: tool [OPTIONS]\n"
                 "\n"
                 "Supercalifragilisticexpialidocious\n"
                 "word\n"
                 "\n"
                 "Options:\n"
                 "  -u, --url  See\n"
                 "             https://example.invalid/a/very/long/path\n"
                 "             now\n");

    destroy_root(root);
}

/*
 * A newline in a description ends the line, and what follows starts at
 * the description column, or at 0 for the description of the command. Two
 * newlines give an empty line, which holds no blank. The blanks at the
 * start of a line of the description are kept.
 */
NAOS_TEST(newlines_of_a_description_end_the_lines)
{
    clargs_command_t *root = create_root();

    describe(root, "The first paragraph.\n\nThe second one,\n  with a line of its own.");
    add_positional(root, "input", CLARGS_ARITY_REQUIRED, "Read it\n\nAfter an empty line");
    add_flag(root, 'v', "verbose", "First line\nSecond line\n\n  After an empty line");
    validate(root);

    check_help_with(root, NULL,
                    "Usage: tool [OPTIONS] <input>\n"
                    "\n"
                    "The first paragraph.\n"
                    "\n"
                    "The second one,\n"
                    "  with a line of its own.\n"
                    "\n"
                    "Arguments:\n"
                    "  <input>        Read it\n"
                    "\n"
                    "                 After an empty line\n"
                    "\n"
                    "Options:\n"
                    "  -v, --verbose  First line\n"
                    "                 Second line\n"
                    "\n"
                    "                   After an empty line\n");

    destroy_root(root);
}

/*
 * With the width 0 nothing is broken: the usage line and the descriptions
 * go beyond the column 80, and only the newline of a description ends a
 * line. The same schema with the width 80 has them broken.
 */
NAOS_TEST(width_zero_breaks_at_newlines_only)
{
    clargs_command_t *root = create_root();

    describe(root, "This sentence is longer than eighty columns and it stays on one line when "
                   "the width is zero.");
    add_positional(root, "a-positional-with-a-long-name", CLARGS_ARITY_REQUIRED, NULL);
    add_positional(root, "another-positional-long-name", CLARGS_ARITY_REQUIRED, NULL);
    add_flag(root, 'v', "verbose",
             "Print more about every step that is taken, also about the steps that change "
             "nothing\nA line of its own");
    validate(root);

    check_layout(root, 0, 2, 32,
                 "Usage: tool [OPTIONS] <a-positional-with-a-long-name> "
                 "<another-positional-long-name>\n"
                 "\n"
                 "This sentence is longer than eighty columns and it stays on one line when "
                 "the width is zero.\n"
                 "\n"
                 "Arguments:\n"
                 "  <a-positional-with-a-long-name>\n"
                 "  <another-positional-long-name>\n"
                 "\n"
                 "Options:\n"
                 "  -v, --verbose                 Print more about every step that is taken, "
                 "also about the steps that change nothing\n"
                 "                                A line of its own\n");

    check_layout(root, 80, 2, 32,
                 "Usage: tool [OPTIONS] <a-positional-with-a-long-name>\n"
                 "       <another-positional-long-name>\n"
                 "\n"
                 "This sentence is longer than eighty columns and it stays on one line when the\n"
                 "width is zero.\n"
                 "\n"
                 "Arguments:\n"
                 "  <a-positional-with-a-long-name>\n"
                 "  <another-positional-long-name>\n"
                 "\n"
                 "Options:\n"
                 "  -v, --verbose                 Print more about every step that is taken, also\n"
                 "                                about the steps that change nothing\n"
                 "                                A line of its own\n");

    destroy_root(root);
}

/* ---- The settings --------------------------------------------------------- */

/*
 * The indent stands before every item of both lists, and the description
 * column moves with it. The maximum column is a limit only: 12 sends the
 * description of the widest heading to the next line and draws the others
 * to the column 12, and 40 changes nothing, as the widest heading asks for
 * the column 17.
 */
NAOS_TEST(indent_and_maximum_column_shape_the_lists)
{
    clargs_command_t *root = create_root();

    add_sub(root, "add", "Add a remote");
    add_sub(root, "remove", "Remove a remote");
    add_flag(root, 'v', "verbose", "Say more");
    validate(root);

    check_layout(root, 80, 2, 32,
                 "Usage: tool [OPTIONS] [COMMAND]\n"
                 "\n"
                 "Commands:\n"
                 "  add            Add a remote\n"
                 "  remove         Remove a remote\n"
                 "\n"
                 "Options:\n"
                 "  -v, --verbose  Say more\n");

    check_layout(root, 80, 6, 32,
                 "Usage: tool [OPTIONS] [COMMAND]\n"
                 "\n"
                 "Commands:\n"
                 "      add            Add a remote\n"
                 "      remove         Remove a remote\n"
                 "\n"
                 "Options:\n"
                 "      -v, --verbose  Say more\n");

    check_layout(root, 80, 0, 32,
                 "Usage: tool [OPTIONS] [COMMAND]\n"
                 "\n"
                 "Commands:\n"
                 "add            Add a remote\n"
                 "remove         Remove a remote\n"
                 "\n"
                 "Options:\n"
                 "-v, --verbose  Say more\n");

    check_layout(root, 80, 2, 12,
                 "Usage: tool [OPTIONS] [COMMAND]\n"
                 "\n"
                 "Commands:\n"
                 "  add       Add a remote\n"
                 "  remove    Remove a remote\n"
                 "\n"
                 "Options:\n"
                 "  -v, --verbose\n"
                 "            Say more\n");

    check_layout(root, 80, 2, 40,
                 "Usage: tool [OPTIONS] [COMMAND]\n"
                 "\n"
                 "Commands:\n"
                 "  add            Add a remote\n"
                 "  remove         Remove a remote\n"
                 "\n"
                 "Options:\n"
                 "  -v, --verbose  Say more\n");

    destroy_root(root);
}

/*
 * NULL for the help options and help options as they were created give
 * the same text, the one of the default settings: the description of the
 * command is broken at the width 80, the items have the indent 2, and the
 * description column is the maximum column 32. The same text again for
 * help options that had the default values set.
 */
NAOS_TEST(null_and_new_options_give_the_same_text)
{
    static const char expected[] =
        "Usage: tool [OPTIONS]\n"
        "\n"
        "Keeps the remote copies of a project in step with the local one and reports\n"
        "every difference that it finds.\n"
        "\n"
        "Options:\n"
        "  -v, --verbose                 Say more\n"
        "      --a-rather-long-name-of-option <FILE>\n"
        "                                Write to FILE\n";
    clargs_command_t *root = create_root();
    clargs_help_options_t *options = NULL;
    char null_buffer[SINK_SIZE];
    char new_buffer[SINK_SIZE];
    sink_t null_sink;
    sink_t new_sink;

    describe(root, "Keeps the remote copies of a project in step with the local one and reports "
                   "every difference that it finds.");
    add_flag(root, 'v', "verbose", "Say more");
    add_value(root, '\0', "a-rather-long-name-of-option", "FILE", NULL, "Write to FILE");
    validate(root);

    NAOS_REQUIRE_TRUE(clargs_help_options_create(&options) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(options != NULL);

    check_help_with(root, NULL, expected);
    check_help_with(root, options, expected);
    check_layout(root, 80, 2, 32, expected);

    /* The two texts, one against the other. */
    sink_init(&null_sink, null_buffer, sizeof null_buffer);
    sink_init(&new_sink, new_buffer, sizeof new_buffer);
    generate(root, NULL, &null_sink);
    generate(root, options, &new_sink);
    NAOS_CHECK_UINT_EQ(null_sink.length, new_sink.length);
    NAOS_CHECK_MEM_EQ(null_sink.text, null_sink.length, new_sink.text, new_sink.length);

    clargs_help_options_destroy(options);
    destroy_root(root);
}

/*
 * One set of help options serves any number of generations, for any
 * command: the help of the root and the one of a subcommand, each twice
 * and in turn, have the same text every time. The settings are the width
 * 30, the indent 4 and the maximum column 12. The description of the
 * subcommand is whole in its own help and cut to its first break in the
 * list of the root.
 */
NAOS_TEST(one_options_object_serves_many_generations)
{
    static const char expected_root[] =
        "Usage: tool [OPTIONS]\n"
        "       [COMMAND]\n"
        "\n"
        "Commands:\n"
        "    remote  Manage the remote\n"
        "            copies of the\n"
        "            project\n"
        "\n"
        "Options:\n"
        "    -v, --verbose\n"
        "            Say more\n";
    static const char expected_sub[] =
        "Usage: tool remote <name>\n"
        "\n"
        "Manage the remote copies of\n"
        "the project\n"
        "\n"
        "Arguments:\n"
        "    <name>  The name of the\n"
        "            remote copy\n";
    clargs_command_t *root = create_root();
    clargs_command_t *remote = add_sub(root, "remote", "Manage the remote copies of the project");
    clargs_help_options_t *options;

    add_flag(root, 'v', "verbose", "Say more");
    add_positional(remote, "name", CLARGS_ARITY_REQUIRED, "The name of the remote copy");
    validate(root);

    options = create_options(30, 4, 12);

    check_help_with(root, options, expected_root);
    check_help_with(root, options, expected_root);
    check_help_with(remote, options, expected_sub);
    check_help_with(root, options, expected_root);
    check_help_with(remote, options, expected_sub);

    clargs_help_options_destroy(options);
    destroy_root(root);
}

/* ---- The greatest settings ------------------------------------------------ */

/*
 * One piece of a long text: the bytes of literal and, behind them, count
 * times the byte fill.
 */
typedef struct piece
{
    const char *literal;
    char fill;
    size_t count;
} piece_t;

/*
 * Checks that the length bytes of text are exactly the pieces, one behind
 * the other: by the length and by every byte. A text of some 65535 columns
 * is so compared without a literal of that size.
 */
static void check_pieces(const char *text, size_t length, const piece_t *pieces, size_t count)
{
    size_t expected = 0;
    size_t position = 0;
    size_t index;

    for (index = 0; index < count; ++index)
    {
        expected += strlen(pieces[index].literal) + pieces[index].count;
    }
    NAOS_REQUIRE_TRUE(length == expected);

    for (index = 0; index < count; ++index)
    {
        size_t literal = strlen(pieces[index].literal);
        size_t wrong = 0;
        size_t offset;

        NAOS_CHECK_MEM_EQ(text + position, literal, pieces[index].literal, literal);
        position += literal;

        for (offset = 0; offset < pieces[index].count; ++offset)
        {
            if (text[position + offset] != pieces[index].fill)
            {
                ++wrong;
            }
        }
        NAOS_CHECK_UINT_EQ(wrong, 0);
        position += pieces[index].count;
    }
}

/*
 * Has the help of command written with help options that have the width,
 * the indent and the maximum column given, into the BIG_SINK_SIZE bytes of
 * buffer, and checks the generation as generate() does and the text with
 * check_pieces().
 */
static void check_big_layout(const clargs_command_t *command, size_t width, size_t indent,
                             size_t max_column, char *buffer, const piece_t *pieces, size_t count)
{
    clargs_help_options_t *options = create_options(width, indent, max_column);
    sink_t sink;

    sink_init(&sink, buffer, BIG_SINK_SIZE);
    generate(command, options, &sink);
    check_pieces(sink.text, sink.length, pieces, count);

    clargs_help_options_destroy(options);
}

/*
 * The values above the limit that the tests set: the first one, another,
 * and the greatest size_t.
 */
static const size_t beyond_the_limit[] = { (size_t)65536, (size_t)70000, (size_t)-1 };

#define BEYOND_COUNT (sizeof beyond_the_limit / sizeof beyond_the_limit[0])

/*
 * A width above 65535 works as 65535. The description of the command is
 * a word of 65533 bytes, a blank and "bb". With the width 65535 "bb" would
 * end at the column 65536 and goes to the next line; with the width 65536
 * it would stay. A width shows only on a line of that length, so the
 * description is that long; it is built here and not a literal.
 */
NAOS_TEST(width_above_the_limit_works_as_the_limit)
{
    static const piece_t pieces[] = {
        { "Usage: tool\n\n", 'x', (size_t)65533 },
        { "\nbb\n", ' ', 0 },
    };
    static const piece_t one_line[] = {
        { "Usage: tool\n\n", 'x', (size_t)65533 },
        { " bb\n", ' ', 0 },
    };
    char *description = malloc((size_t)65533 + 4);
    char *buffer = malloc(BIG_SINK_SIZE);
    clargs_command_t *root;
    size_t index;

    NAOS_REQUIRE_TRUE(description != NULL);
    NAOS_REQUIRE_TRUE(buffer != NULL);
    memset(description, 'x', (size_t)65533);
    memcpy(description + 65533, " bb", 4);

    root = create_root();
    describe(root, description);
    validate(root);

    check_big_layout(root, SETTING_MAX, 2, 32, buffer, pieces, 2);
    for (index = 0; index < BEYOND_COUNT; ++index)
    {
        check_big_layout(root, beyond_the_limit[index], 2, 32, buffer, pieces, 2);
    }

    /* One column less breaks the line as well; no limit does not. */
    check_big_layout(root, SETTING_MAX - 1, 2, 32, buffer, pieces, 2);
    check_big_layout(root, 0, 2, 32, buffer, one_line, 2);

    destroy_root(root);
    free(buffer);
    free(description);
}

/*
 * An indent above 65535 works as 65535: the item starts behind 65535
 * blanks, and the description column, the indent + 2 as the least limit,
 * is 65537. The blanks are counted, not written out here: the text is
 * compared piece by piece. The width is 0, so that nothing else bounds
 * the column.
 */
NAOS_TEST(indent_above_the_limit_works_as_the_limit)
{
    static const piece_t pieces[] = {
        { "Usage: tool [COMMAND]\n\nCommands:\n", ' ', (size_t)65535 },
        { "a\n", ' ', (size_t)65537 },
        { "Add\n", ' ', 0 },
    };
    static const piece_t one_less[] = {
        { "Usage: tool [COMMAND]\n\nCommands:\n", ' ', (size_t)65534 },
        { "a\n", ' ', (size_t)65536 },
        { "Add\n", ' ', 0 },
    };
    char *buffer = malloc(BIG_SINK_SIZE);
    clargs_command_t *root;
    size_t index;

    NAOS_REQUIRE_TRUE(buffer != NULL);

    root = create_root();
    add_sub(root, "a", "Add");
    validate(root);

    check_big_layout(root, 0, SETTING_MAX, 32, buffer, pieces, 3);
    for (index = 0; index < BEYOND_COUNT; ++index)
    {
        check_big_layout(root, 0, beyond_the_limit[index], 32, buffer, pieces, 3);
    }
    check_big_layout(root, 0, SETTING_MAX - 1, 32, buffer, one_less, 3);

    destroy_root(root);
    free(buffer);
}

/*
 * A maximum column above 65535 works as 65535. With the width 0 and the
 * indent 65533 the heading "a" asks for the column 65536. The maximum
 * column 65535 is less: the description goes to the next line, at the
 * column 65535. A maximum column of 65536 would leave it on the line of
 * the heading, as the indent 65532 shows, which asks for 65535.
 */
NAOS_TEST(maximum_column_above_the_limit_works_as_the_limit)
{
    static const piece_t pieces[] = {
        { "Usage: tool [COMMAND]\n\nCommands:\n", ' ', (size_t)65533 },
        { "a\n", ' ', (size_t)65535 },
        { "Add\n", ' ', 0 },
    };
    static const piece_t same_line[] = {
        { "Usage: tool [COMMAND]\n\nCommands:\n", ' ', (size_t)65532 },
        { "a  Add\n", ' ', 0 },
    };
    char *buffer = malloc(BIG_SINK_SIZE);
    clargs_command_t *root;
    size_t index;

    NAOS_REQUIRE_TRUE(buffer != NULL);

    root = create_root();
    add_sub(root, "a", "Add");
    validate(root);

    check_big_layout(root, 0, 65533, SETTING_MAX, buffer, pieces, 3);
    for (index = 0; index < BEYOND_COUNT; ++index)
    {
        check_big_layout(root, 0, 65533, beyond_the_limit[index], buffer, pieces, 3);
        check_big_layout(root, 0, 65532, beyond_the_limit[index], buffer, same_line, 2);
    }

    destroy_root(root);
    free(buffer);
}

/* ---- Descriptions of blanks and newlines only ---------------------------- */

/*
 * Checks that description, as the description of a command, is not
 * written: the help is the usage line alone, and with a list one empty
 * line stands between the usage line and the list.
 */
static void check_blank_command_description(const char *description)
{
    clargs_command_t *root = create_root();

    describe(root, description);
    validate(root);
    check_help_with(root, NULL, "Usage: tool\n");
    destroy_root(root);

    root = create_root();
    describe(root, description);
    add_sub(root, "add", "Add");
    add_flag(root, 'v', NULL, "Say more");
    validate(root);
    check_help_with(root, NULL,
                    "Usage: tool [OPTIONS] [COMMAND]\n"
                    "\n"
                    "Commands:\n"
                    "  add  Add\n"
                    "\n"
                    "Options:\n"
                    "  -v   Say more\n");
    destroy_root(root);
}

/*
 * A description of a command that holds nothing but blanks and newlines
 * counts as not set: no paragraph and no empty line stands for it.
 */
NAOS_TEST(blank_description_of_the_command_is_not_written)
{
    check_blank_command_description("   ");
    check_blank_command_description(" \n");
    check_blank_command_description("\n \n");
    check_blank_command_description("  \n\n   \n ");
}

/*
 * A description of an option that holds nothing but blanks and newlines
 * counts as not set: the line holds the heading only, behind a heading
 * that fits and behind one that is too wide, and no empty line follows it,
 * also not behind the last item of the text.
 */
NAOS_TEST(blank_description_of_an_option_is_heading_only)
{
    clargs_command_t *root = create_root();

    add_flag(root, 'v', "verbose", "   ");
    add_value(root, 'o', "output-directory", "DIR", NULL, " \n");
    add_flag(root, 'q', "quiet", "Say less");
    add_flag(root, '\0', "a-flag-with-a-long-name", "\n \n");
    validate(root);

    check_layout(root, 80, 2, 20,
                 "Usage: tool [OPTIONS]\n"
                 "\n"
                 "Options:\n"
                 "  -v, --verbose\n"
                 "  -o, --output-directory <DIR>\n"
                 "  -q, --quiet       Say less\n"
                 "      --a-flag-with-a-long-name\n");

    destroy_root(root);
}

/* The same for the positionals. */
NAOS_TEST(blank_description_of_an_argument_is_heading_only)
{
    clargs_command_t *root = create_root();

    add_positional(root, "a-long-positional-name", CLARGS_ARITY_REQUIRED, "   ");
    add_positional(root, "input", CLARGS_ARITY_REQUIRED, "  \n  ");
    add_positional(root, "another-long-positional", CLARGS_ARITY_REQUIRED, "\n \n");
    validate(root);

    check_layout(root, 80, 2, 20,
                 "Usage: tool <a-long-positional-name> <input> <another-long-positional>\n"
                 "\n"
                 "Arguments:\n"
                 "  <a-long-positional-name>\n"
                 "  <input>\n"
                 "  <another-long-positional>\n");

    destroy_root(root);
}

/*
 * The description of a subcommand is judged by its first line, the part
 * that a list shows: when that holds nothing but blanks, the line holds
 * the name only, whatever follows the first newline.
 */
NAOS_TEST(blank_first_line_of_a_subcommand_is_heading_only)
{
    clargs_command_t *root = create_root();

    add_sub(root, "synchronize-everything", "   ");
    add_sub(root, "add", " \nNot shown");
    add_sub(root, "rm", "Remove");
    add_sub(root, "ls", "");
    add_sub(root, "another-rather-long-name", "  \nNot shown");
    validate(root);

    check_layout(root, 80, 2, 20,
                 "Usage: tool [COMMAND]\n"
                 "\n"
                 "Commands:\n"
                 "  synchronize-everything\n"
                 "  add\n"
                 "  rm                Remove\n"
                 "  ls\n"
                 "  another-rather-long-name\n");

    destroy_root(root);
}

/*
 * An item whose description holds nothing but blanks and newlines shows
 * its annotations in the place of the description: at the description
 * column, without a blank before them and without an empty line, on the
 * line of the heading or, behind a heading that is too wide, on the next
 * line.
 */
NAOS_TEST(blank_description_leaves_the_annotations_alone)
{
    clargs_command_t *root = create_root();

    add_positional_default(root, "out", "o", " \n");
    add_positional_default(root, "a-long-optional-name", "x", "   ");
    add_value(root, 'n', "num", "N", "1", " \n");
    add_value(root, 'o', "output-directory", "DIR", "out", "   ");
    add_value(root, '\0', "a-long-name-of-option", "FILE", "f", "\n \n");
    validate(root);

    check_layout(root, 80, 2, 20,
                 "Usage: tool [OPTIONS] [out] [a-long-optional-name]\n"
                 "\n"
                 "Arguments:\n"
                 "  [out]             [default: o]\n"
                 "  [a-long-optional-name]\n"
                 "                    [default: x]\n"
                 "\n"
                 "Options:\n"
                 "  -n, --num <N>     [default: 1]\n"
                 "  -o, --output-directory <DIR>\n"
                 "                    [default: out]\n"
                 "      --a-long-name-of-option <FILE>\n"
                 "                    [default: f]\n");

    destroy_root(root);
}

/* ---- The blanks of the trailing run of a description ----------------- */

/*
 * Builds the items of a test of the trailing run, each with the
 * description given, on root.
 */
typedef void (*trailing_build_t)(clargs_command_t *root, const char *description);

/* The command has the description, and nothing else. */
static void build_command_alone(clargs_command_t *root, const char *description)
{
    describe(root, description);
}

/* The command has the description, and a flag follows it. */
static void build_command_and_option(clargs_command_t *root, const char *description)
{
    describe(root, description);
    add_flag(root, 'v', NULL, "Say more");
}

/* A flag with a heading that fits, then the last, one that is too wide. */
static void build_flags_long_last(clargs_command_t *root, const char *description)
{
    add_flag(root, 'q', "quiet", description);
    add_flag(root, '\0', "a-flag-with-a-long-name", description);
}

/* The same flags, the one with the heading that fits last. */
static void build_flags_short_last(clargs_command_t *root, const char *description)
{
    add_flag(root, '\0', "a-flag-with-a-long-name", description);
    add_flag(root, 'q', "quiet", description);
}

/* Options with a default value: a heading that fits, then one too wide. */
static void build_defaults_long_last(clargs_command_t *root, const char *description)
{
    add_value(root, 'n', "num", "N", "1", description);
    add_value(root, '\0', "a-long-name-of-option", "FILE", "f", description);
}

/* The same options, the one with the heading that fits last. */
static void build_defaults_short_last(clargs_command_t *root, const char *description)
{
    add_value(root, '\0', "a-long-name-of-option", "FILE", "f", description);
    add_value(root, 'n', "num", "N", "1", description);
}

/* Positionals: a heading that fits, then one too wide. */
static void build_arguments_long_last(clargs_command_t *root, const char *description)
{
    add_positional(root, "input", CLARGS_ARITY_REQUIRED, description);
    add_positional(root, "another-long-positional", CLARGS_ARITY_REQUIRED, description);
}

/* The same positionals, the one with the heading that fits last. */
static void build_arguments_short_last(clargs_command_t *root, const char *description)
{
    add_positional(root, "another-long-positional", CLARGS_ARITY_REQUIRED, description);
    add_positional(root, "input", CLARGS_ARITY_REQUIRED, description);
}

/*
 * Optional positionals with a default value: a heading that fits, then
 * one too wide.
 */
static void build_argument_defaults(clargs_command_t *root, const char *description)
{
    add_positional_default(root, "out", "o", description);
    add_positional_default(root, "a-long-optional-name", "x", description);
}

/*
 * Builds a root with build and description, and has its help written with
 * the width 80, the indent 2 and the maximum column 20 to sink, as
 * generate() does: which checks the shape of the text.
 */
static void generate_trailing(trailing_build_t build, const char *description, sink_t *sink)
{
    clargs_command_t *root = create_root();
    clargs_help_options_t *options = create_options(80, 2, 20);

    build(root, description);
    validate(root);
    generate(root, options, sink);

    clargs_help_options_destroy(options);
    destroy_root(root);
}

/*
 * Checks that the help built with build and description is the text of
 * the one built with build and reduced, byte for byte.
 */
static void check_same_text(trailing_build_t build, const char *description, const char *reduced)
{
    char buffer[SINK_SIZE];
    char reduced_buffer[SINK_SIZE];
    sink_t sink;
    sink_t reduced_sink;

    sink_init(&sink, buffer, sizeof buffer);
    sink_init(&reduced_sink, reduced_buffer, sizeof reduced_buffer);
    generate_trailing(build, description, &sink);
    generate_trailing(build, reduced, &reduced_sink);

    NAOS_CHECK_UINT_EQ(sink.length, reduced_sink.length);
    NAOS_CHECK_MEM_EQ(sink.text, sink.length, reduced_sink.text, reduced_sink.length);
}

/* Checks that the help built with build and description is expected. */
static void check_trailing(trailing_build_t build, const char *description, const char *expected)
{
    clargs_command_t *root = create_root();

    build(root, description);
    validate(root);
    check_layout(root, 80, 2, 20, expected);
    destroy_root(root);
}

/*
 * In the trailing run of a description, the blanks and newlines behind its
 * last other byte, the blanks are ignored and the newlines are kept: each
 * description gives the text of the one without those blanks, for the
 * description of the command and for those of the options and of the
 * positionals, behind a heading that fits and behind one that is too wide,
 * with annotations and without, and on the last item of the text.
 */
NAOS_TEST(blanks_of_the_trailing_run_are_ignored)
{
    static const trailing_build_t builds[] = {
        build_command_alone,      build_command_and_option,  build_flags_long_last,
        build_flags_short_last,   build_defaults_long_last,  build_defaults_short_last,
        build_arguments_long_last, build_arguments_short_last, build_argument_defaults,
    };
    static const char *const descriptions[][2] = {
        {"foo\n  ", "foo\n"},
        {"foo\n \n", "foo\n\n"},
        {"foo  ", "foo"},
        {"foo \n ", "foo\n"},
        {"foo\n  \n\n", "foo\n\n\n"},
        {"foo\n  \nbar \n  \n", "foo\n  \nbar\n\n"},
    };
    size_t build;
    size_t index;

    for (build = 0; build < sizeof builds / sizeof builds[0]; ++build)
    {
        for (index = 0; index < sizeof descriptions / sizeof descriptions[0]; ++index)
        {
            check_same_text(builds[build], descriptions[index][0], descriptions[index][1]);
        }
    }
}

/*
 * The description of the command with blanks in its trailing run: the
 * text ends with the line of the description, or the list follows it
 * behind one empty line.
 */
NAOS_TEST(blanks_behind_the_description_of_the_command)
{
    check_trailing(build_command_alone, "foo\n  ", "Usage: tool\n\nfoo\n");
    check_trailing(build_command_alone, "foo\n \n", "Usage: tool\n\nfoo\n");
    check_trailing(build_command_and_option, "foo \n ",
                   "Usage: tool [OPTIONS]\n"
                   "\n"
                   "foo\n"
                   "\n"
                   "Options:\n"
                   "  -v  Say more\n");
}

/*
 * Items without annotations: no empty line follows an item whose
 * description has blanks in its trailing run, also not the last one.
 */
NAOS_TEST(blanks_behind_the_description_of_an_item)
{
    check_trailing(build_flags_long_last, "foo\n  ",
                   "Usage: tool [OPTIONS]\n"
                   "\n"
                   "Options:\n"
                   "  -q, --quiet       foo\n"
                   "      --a-flag-with-a-long-name\n"
                   "                    foo\n");
    check_trailing(build_flags_short_last, "foo\n \n",
                   "Usage: tool [OPTIONS]\n"
                   "\n"
                   "Options:\n"
                   "      --a-flag-with-a-long-name\n"
                   "                    foo\n"
                   "  -q, --quiet       foo\n");
    check_trailing(build_arguments_short_last, "foo \n ",
                   "Usage: tool <another-long-positional> <input>\n"
                   "\n"
                   "Arguments:\n"
                   "  <another-long-positional>\n"
                   "                    foo\n"
                   "  <input>           foo\n");
}

/*
 * Items with a default value: the trailing run keeps its newlines. The
 * annotation starts on the line behind the description, at the
 * description column, and each newline of the run beyond the first puts
 * an empty line before it; blanks alone put the annotation on the line of
 * the description.
 */
NAOS_TEST(blanks_behind_a_description_with_annotations)
{
    check_trailing(build_defaults_long_last, "foo\n  ",
                   "Usage: tool [OPTIONS]\n"
                   "\n"
                   "Options:\n"
                   "  -n, --num <N>     foo\n"
                   "                    [default: 1]\n"
                   "      --a-long-name-of-option <FILE>\n"
                   "                    foo\n"
                   "                    [default: f]\n");
    check_trailing(build_defaults_short_last, "foo\n \n",
                   "Usage: tool [OPTIONS]\n"
                   "\n"
                   "Options:\n"
                   "      --a-long-name-of-option <FILE>\n"
                   "                    foo\n"
                   "\n"
                   "                    [default: f]\n"
                   "  -n, --num <N>     foo\n"
                   "\n"
                   "                    [default: 1]\n");
    check_trailing(build_defaults_long_last, "foo\n  \n\n",
                   "Usage: tool [OPTIONS]\n"
                   "\n"
                   "Options:\n"
                   "  -n, --num <N>     foo\n"
                   "\n"
                   "\n"
                   "                    [default: 1]\n"
                   "      --a-long-name-of-option <FILE>\n"
                   "                    foo\n"
                   "\n"
                   "\n"
                   "                    [default: f]\n");
    check_trailing(build_argument_defaults, "foo  ",
                   "Usage: tool [out] [a-long-optional-name]\n"
                   "\n"
                   "Arguments:\n"
                   "  [out]             foo [default: o]\n"
                   "  [a-long-optional-name]\n"
                   "                    foo [default: x]\n");
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
    NAOS_ADD_TEST(width_of_a_heading_with_newline_decides),
    NAOS_ADD_TEST(long_descriptions_break_between_words),
    NAOS_ADD_TEST(long_description_of_an_argument_breaks_between_words),
    NAOS_ADD_TEST(long_description_below_a_long_heading_breaks_between_words),
    NAOS_ADD_TEST(word_wider_than_the_room_is_not_divided),
    NAOS_ADD_TEST(newlines_of_a_description_end_the_lines),
    NAOS_ADD_TEST(width_zero_breaks_at_newlines_only),
    NAOS_ADD_TEST(indent_and_maximum_column_shape_the_lists),
    NAOS_ADD_TEST(null_and_new_options_give_the_same_text),
    NAOS_ADD_TEST(one_options_object_serves_many_generations),
    NAOS_ADD_TEST(width_above_the_limit_works_as_the_limit),
    NAOS_ADD_TEST(indent_above_the_limit_works_as_the_limit),
    NAOS_ADD_TEST(maximum_column_above_the_limit_works_as_the_limit),
    NAOS_ADD_TEST(blank_description_of_the_command_is_not_written),
    NAOS_ADD_TEST(blank_description_of_an_option_is_heading_only),
    NAOS_ADD_TEST(blank_description_of_an_argument_is_heading_only),
    NAOS_ADD_TEST(blank_first_line_of_a_subcommand_is_heading_only),
    NAOS_ADD_TEST(blank_description_leaves_the_annotations_alone),
    NAOS_ADD_TEST(blanks_of_the_trailing_run_are_ignored),
    NAOS_ADD_TEST(blanks_behind_the_description_of_the_command),
    NAOS_ADD_TEST(blanks_behind_the_description_of_an_item),
    NAOS_ADD_TEST(blanks_behind_a_description_with_annotations)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(help_layout)
)
