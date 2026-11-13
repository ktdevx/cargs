/*
 * Tests of the headings of the options in the help: the "Options:" list of
 * clargs_help_write() of <clargs/help.h>.
 *
 * Each test builds a schema, validates it, has the help of one of its
 * commands written and compares the text with the expected string, by its
 * length and by its bytes. The write function of the tests appends what it
 * is given to an array of a fixed size.
 *
 * With the default settings "the indent + the width of the widest heading
 * + 2" is at most 32 in every schema of this file, so the texts do not
 * depend on the limit of the description column. No option of this file is
 * required, has a default value, a value type with choices or an initial
 * state that is on: the description of an option is its description text
 * only.
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

/* Sets the description of option, when description is not NULL; it must
 * succeed. */
static void describe_option(clargs_option_t *option, const char *description)
{
    NAOS_REQUIRE_TRUE(option != NULL);
    if (description != NULL) {
        NAOS_REQUIRE_TRUE(clargs_option_set_description(option, description) == CLARGS_ERROR_NONE);
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
    describe_option(option, description);
}

/* Adds a negatable flag that must succeed; its initial state stays off. */
static void add_negatable(clargs_command_t *command, char short_name, const char *long_name,
                          const char *description)
{
    clargs_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_negatable(command, short_name, long_name, &option) ==
                      CLARGS_ERROR_NONE);
    describe_option(option, description);
}

/* Adds a count that must succeed. */
static void add_count(clargs_command_t *command, char short_name, const char *long_name,
                      const char *description)
{
    clargs_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_count(command, short_name, long_name, &option) ==
                      CLARGS_ERROR_NONE);
    describe_option(option, description);
}

/* Adds an option with a value of type that must succeed. value_name is
 * the display name of the value, or NULL to set none. The option is not
 * required and has no default value. */
static void add_value(clargs_command_t *command, char short_name, const char *long_name,
                      const clargs_type_t *type, const char *value_name, const char *description)
{
    clargs_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_value(command, short_name, long_name, type, &option) ==
                      CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(option != NULL);
    if (value_name != NULL) {
        NAOS_REQUIRE_TRUE(clargs_option_set_value_name(option, value_name) == CLARGS_ERROR_NONE);
    }
    describe_option(option, description);
}

/* The conversion function of the user-defined value types of the tests: no
 * test converts a value with it. */
static clargs_error_t convert_nothing(const char *text, void *value, void *context)
{
    (void)text;
    (void)value;
    (void)context;
    return CLARGS_ERROR_NONE;
}

/* Creates a user-defined value type with the name given; it must succeed.
 * The test destroys it after the schema that uses it. */
static clargs_type_t *create_type(const char *name)
{
    clargs_type_t *type = NULL;

    NAOS_REQUIRE_TRUE(clargs_type_create(name, sizeof(int), convert_nothing, NULL, NULL, &type) ==
                      CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(type != NULL);
    return type;
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
        NAOS_REQUIRE_TRUE(clargs_command_set_description(sub, description) == CLARGS_ERROR_NONE);
    }
    return sub;
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
 * that nothing was allocated and that the schema is still validated.
 */
static void check_help_with(const clargs_command_t *command, const clargs_help_options_t *options,
                            const char *expected)
{
    sink_t sink;
    size_t requests = clargs_test_allocator_requests();

    sink_init(&sink);

    NAOS_CHECK_TRUE(clargs_help_write(command, options, sink_write, &sink) == CLARGS_ERROR_NONE);

    NAOS_CHECK_UINT_EQ(sink.length, strlen(expected));
    NAOS_CHECK_MEM_EQ(sink.text, sink.length, expected, strlen(expected));
    check_shape(sink.text, sink.length);
    check_shape(expected, strlen(expected));
    NAOS_CHECK_UINT_EQ(sink.empty_calls, 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests);
    NAOS_CHECK_TRUE(clargs_command_is_validated(command));
}

/* The same without help options: every setting has its default value. */
static void check_help(const clargs_command_t *command, const char *expected)
{
    check_help_with(command, NULL, expected);
}

/* ---- The names ------------------------------------------------------------ */

/* The three forms of the names, for a flag: "-s, --long" with both names,
 * "-s" with a short name only, and four blanks and "--long" with a long
 * name only, so that the long names stand one below the other. */
NAOS_TEST(flag_heading_has_three_forms_of_names)
{
    clargs_command_t *root = create_root();

    add_flag(root, 'v', "verbose", "Say more");
    add_flag(root, 'q', NULL, "Say less");
    add_flag(root, '\0', "force", "Do it anyway");
    validate(root);

    check_help(root,
               "Usage: tool [OPTIONS]\n"
               "\n"
               "Options:\n"
               "  -v, --verbose  Say more\n"
               "  -q             Say less\n"
               "      --force    Do it anyway\n");

    destroy_root(root);
}

/* The four blanks of a long name without a short name belong to the
 * heading, also when no option of the list has a short name: they count
 * for the description column, 2 + (4 + 7) + 2. */
NAOS_TEST(long_name_alone_keeps_its_four_blanks)
{
    clargs_command_t *root = create_root();

    add_flag(root, '\0', "force", "Do it anyway");
    add_flag(root, '\0', "all", "Take everything");
    validate(root);

    check_help(root,
               "Usage: tool [OPTIONS]\n"
               "\n"
               "Options:\n"
               "      --force  Do it anyway\n"
               "      --all    Take everything\n");

    destroy_root(root);
}

/* ---- The kinds ------------------------------------------------------------ */

/* A negatable flag: the long name is written as "--[no-]long", with and
 * without a short name. A negatable flag always has a long name. */
NAOS_TEST(negatable_flag_heading_shows_the_negated_form)
{
    clargs_command_t *root = create_root();

    add_negatable(root, 'c', "color", "Use colors");
    add_negatable(root, '\0', "pager", "Page the output");
    add_flag(root, '\0', "plain", "No markup");
    validate(root);

    check_help(root,
               "Usage: tool [OPTIONS]\n"
               "\n"
               "Options:\n"
               "  -c, --[no-]color  Use colors\n"
               "      --[no-]pager  Page the output\n"
               "      --plain       No markup\n");

    destroy_root(root);
}

/* A count: "..." right behind the names, in each of their three forms. */
NAOS_TEST(count_heading_ends_with_dots)
{
    clargs_command_t *root = create_root();

    add_count(root, 'v', "verbose", "Say more");
    add_count(root, 'd', NULL, "Debug");
    add_count(root, '\0', "trace", "Trace the calls");
    validate(root);

    check_help(root,
               "Usage: tool [OPTIONS]\n"
               "\n"
               "Options:\n"
               "  -v, --verbose...  Say more\n"
               "  -d...             Debug\n"
               "      --trace...    Trace the calls\n");

    destroy_root(root);
}

/* An option with a value: behind the names, in each of their three forms,
 * a blank and the display name of the value between "<" and ">". The
 * display name that was set is used whatever the value type is. */
NAOS_TEST(value_heading_shows_the_display_name)
{
    clargs_command_t *root = create_root();

    add_value(root, 'o', "output", clargs_type_string(), "FILE", "Write to FILE");
    add_value(root, 'n', NULL, clargs_type_string(), "NUM", "Repeat NUM times");
    add_value(root, '\0', "config", clargs_type_string(), "PATH", "Read the settings");
    add_value(root, 'j', "jobs", clargs_type_int(), "N", "Run N jobs");
    validate(root);

    check_help(root,
               "Usage: tool [OPTIONS]\n"
               "\n"
               "Options:\n"
               "  -o, --output <FILE>  Write to FILE\n"
               "  -n <NUM>             Repeat NUM times\n"
               "      --config <PATH>  Read the settings\n"
               "  -j, --jobs <N>       Run N jobs\n");

    destroy_root(root);
}

/* An option with a value and without a display name: the name of the value
 * type stands in its place, for the built-in types and for a user-defined
 * type. */
NAOS_TEST(value_heading_without_display_name_shows_the_type_name)
{
    clargs_command_t *root = create_root();
    clargs_type_t *port = create_type("port");

    add_value(root, 's', "name", clargs_type_string(), NULL, "Name to use");
    add_value(root, '\0', "count", clargs_type_int(), NULL, "How many");
    add_value(root, 'r', NULL, clargs_type_float(), NULL, "Ratio");
    add_value(root, 'p', "port", port, NULL, "Port to use");
    validate(root);

    check_help(root,
               "Usage: tool [OPTIONS]\n"
               "\n"
               "Options:\n"
               "  -s, --name <string>    Name to use\n"
               "      --count <integer>  How many\n"
               "  -r <float>             Ratio\n"
               "  -p, --port <port>      Port to use\n");

    clargs_command_destroy(root);
    clargs_type_destroy(port);
    clargs_test_allocator_uninstall();
    check_clean();
}

/* The four kinds in one list, each with both names. */
NAOS_TEST(all_kinds_stand_in_one_list)
{
    clargs_command_t *root = create_root();

    add_flag(root, 'f', "force", "A flag");
    add_negatable(root, 'c', "color", "A negatable flag");
    add_count(root, 'v', "verbose", "A count");
    add_value(root, 'o', "output", clargs_type_string(), "FILE", "A value");
    validate(root);

    check_help(root,
               "Usage: tool [OPTIONS]\n"
               "\n"
               "Options:\n"
               "  -f, --force          A flag\n"
               "  -c, --[no-]color     A negatable flag\n"
               "  -v, --verbose...     A count\n"
               "  -o, --output <FILE>  A value\n");

    destroy_root(root);
}

/* ---- What is listed -------------------------------------------------------- */

/* A flag that the user defined as "--help" is an option like every other:
 * it is listed where it was added, in the usual form. The library adds no
 * option of its own: a command whose schema has no "--help" shows none, and
 * a command without options has no list. */
NAOS_TEST(help_flag_of_the_schema_is_listed_and_none_is_added)
{
    clargs_command_t *root = create_root();
    clargs_command_t *run = add_sub(root, "run", "Run it");
    clargs_command_t *show = add_sub(root, "show", "Show it");

    add_flag(root, 'h', "help", "Show this help");
    add_flag(root, 'V', "version", "Show the version");
    add_flag(run, 'n', "dry-run", "Change nothing");
    validate(root);

    check_help(root,
               "Usage: tool [OPTIONS] [COMMAND]\n"
               "\n"
               "Commands:\n"
               "  run            Run it\n"
               "  show           Show it\n"
               "\n"
               "Options:\n"
               "  -h, --help     Show this help\n"
               "  -V, --version  Show the version\n");

    check_help(run,
               "Usage: tool run [OPTIONS]\n"
               "\n"
               "Run it\n"
               "\n"
               "Options:\n"
               "  -n, --dry-run  Change nothing\n");

    check_help(show,
               "Usage: tool show\n"
               "\n"
               "Show it\n");

    destroy_root(root);
}

/* ---- The bytes ------------------------------------------------------------- */

/* The strings of the schema are written as they are: upper and lower case
 * in the names, in a display name and in the name of a user-defined type
 * stay, a blank in a display name stays, and bytes that are not ASCII pass
 * through. A width is a number of bytes: the three bytes of the display
 * name of "-x" count as three columns. */
NAOS_TEST(bytes_of_the_schema_are_written_unchanged)
{
    clargs_command_t *root = create_root();
    clargs_type_t *port = create_type("Port Number");

    add_flag(root, 'V', "Verbose", "Be VERBOSE");
    add_flag(root, 'v', "version", "Print the Version");
    add_value(root, 'O', "OutPut", clargs_type_string(), "File Name", "Write to a File");
    add_value(root, 'x', NULL, clargs_type_string(), "\xE5\x80\xA4", "\xE8\xAA\xAC\xE6\x98\x8E");
    add_value(root, '\0', "port", port, NULL, "Port to Use");
    validate(root);

    check_help(root,
               "Usage: tool [OPTIONS]\n"
               "\n"
               "Options:\n"
               "  -V, --Verbose             Be VERBOSE\n"
               "  -v, --version             Print the Version\n"
               "  -O, --OutPut <File Name>  Write to a File\n"
               "  -x <\xE5\x80\xA4>                  \xE8\xAA\xAC\xE6\x98\x8E\n"
               "      --port <Port Number>  Port to Use\n");

    clargs_command_destroy(root);
    clargs_type_destroy(port);
    clargs_test_allocator_uninstall();
    check_clean();
}

/* The width of the heading of an option is the number of all its bytes,
 * also when the display name holds a newline: "-o <A\nB>" counts as 8
 * columns, not as the 2 of "B>", so the description column is 2 + 8 + 2. */
NAOS_TEST(option_heading_with_newline_inside_counts_all_its_bytes)
{
    clargs_command_t *root = create_root();

    add_value(root, 'o', NULL, clargs_type_string(), "A\nB", "Two lines");
    add_flag(root, 'q', NULL, "One line");
    validate(root);

    check_help(root,
               "Usage: tool [OPTIONS]\n"
               "\n"
               "Options:\n"
               "  -o <A\nB>          Two lines\n"
               "  -q        One line\n");

    destroy_root(root);
}

/* ---- The descriptions ------------------------------------------------------ */

/* The description of an option is its whole description text: a newline
 * inside it ends the line and what follows starts at the description
 * column, and the newlines at its end are dropped. An option without a
 * description, or with one that is empty without the newlines at its end,
 * is a line with the heading only. */
NAOS_TEST(option_description_is_written_whole)
{
    clargs_command_t *root = create_root();

    add_flag(root, 'a', NULL, NULL);
    add_flag(root, 'b', NULL, "");
    add_flag(root, 'c', NULL, "First line\nSecond line\n");
    add_flag(root, 'd', NULL, "\n");
    validate(root);

    check_help(root,
               "Usage: tool [OPTIONS]\n"
               "\n"
               "Options:\n"
               "  -a\n"
               "  -b\n"
               "  -c  First line\n"
               "      Second line\n"
               "  -d\n");

    destroy_root(root);
}

NAOS_SUITE(help_item,
    NAOS_ADD_TEST(flag_heading_has_three_forms_of_names),
    NAOS_ADD_TEST(long_name_alone_keeps_its_four_blanks),
    NAOS_ADD_TEST(negatable_flag_heading_shows_the_negated_form),
    NAOS_ADD_TEST(count_heading_ends_with_dots),
    NAOS_ADD_TEST(value_heading_shows_the_display_name),
    NAOS_ADD_TEST(value_heading_without_display_name_shows_the_type_name),
    NAOS_ADD_TEST(all_kinds_stand_in_one_list),
    NAOS_ADD_TEST(help_flag_of_the_schema_is_listed_and_none_is_added),
    NAOS_ADD_TEST(bytes_of_the_schema_are_written_unchanged),
    NAOS_ADD_TEST(option_heading_with_newline_inside_counts_all_its_bytes),
    NAOS_ADD_TEST(option_description_is_written_whole)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(help_item)
)
