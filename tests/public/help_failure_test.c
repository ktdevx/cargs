/*
 * Tests of the failure of an allocation, across the help: the functions of
 * <clargs/help.h> that allocate.
 *
 * The help options are the only object of the help that the caller creates
 * and destroys. The tests check that creating and destroying them leaves
 * nothing allocated, that a creation whose allocation fails returns
 * CLARGS_ERROR_OUT_OF_MEMORY, writes NULL to its output and keeps no
 * block, that destroying NULL does nothing, and that the functions that
 * set a setting never allocate.
 *
 * A generation of a help allocates only the cell text of an option or of a
 * positional: when the first cell that is not empty is built, and again
 * whenever a cell is longer than every cell before it. The tests take a
 * schema with annotations and long descriptions, count the requests of a
 * generation, and make each of them fail in turn, for clargs_help_write()
 * and for clargs_help_format(). Every failure returns
 * CLARGS_ERROR_OUT_OF_MEMORY and keeps no block; the write function has
 * received a start of the text that ends with a whole line; the array of
 * clargs_help_format() holds the empty string, or is untouched with the
 * size 0, and the length is 0. After every failure the schema reads as
 * before, and every generation gives the text it gave before. The usage
 * line alone is generated without a request.
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

/* The most allocation requests that an operation is given to fail in. */
#define REQUEST_LIMIT 100

/*
 * The size of the text that a sink holds: more than the longest text of
 * this file.
 */
#define SINK_SIZE ((size_t)4096)

/*
 * The bytes before the array that clargs_help_format() is given, and the
 * least number of bytes behind it: they must stay as they were.
 */
#define GUARD_SIZE ((size_t)16)

/* The size of the area that holds the guard bytes and the array. */
#define AREA_SIZE (SINK_SIZE + 2 * GUARD_SIZE)

/*
 * The byte that fills the area before a call. No text of this file has
 * it.
 */
#define SENTINEL ((char)0x7E)

/*
 * The value of a length before a call: no text of this file is that
 * long.
 */
#define LENGTH_UNSET ((size_t)54321)

/*
 * The size of the text of a picture of the schema, and the most pointers
 * that a picture holds.
 */
#define PICTURE_SIZE ((size_t)8192)
#define PICTURE_POINTERS ((size_t)256)

/*
 * The number of the generations that are compared: two commands, each
 * without help options and with help options that are not the default.
 */
#define CASE_COUNT ((size_t)4)

/*
 * The least number of requests that a generation of the help of the
 * schema of this file makes: so many allocation points fail, at least.
 */
#define LEAST_REQUESTS ((size_t)4)

/*
 * The end of every test: the test allocator has no live block and saw no
 * violation of the allocator contract.
 */
static void check_clean(void)
{
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
}

/*
 * Help options that are not a result of clargs_help_options_create(): what
 * an output holds before the call, so that a call that does not write its
 * output is seen. It is never read through.
 */
static clargs_help_options_t *stale_options(void)
{
    /* Aligned for an object of any type, as allocated memory is. */
    static union
    {
        long long integer;
        long double number;
        void *pointer;
        void (*function)(void);
    } object;

    return (clargs_help_options_t *)(void *)&object;
}

NAOS_TEST(creating_and_destroying_help_options_leaves_nothing_allocated)
{
    clargs_help_options_t *options = stale_options();

    clargs_test_allocator_install();

    NAOS_CHECK_TRUE(clargs_help_options_create(&options) == CLARGS_ERROR_NONE);
    NAOS_CHECK_TRUE(options != NULL);
    NAOS_CHECK_TRUE(options != stale_options());
    NAOS_CHECK_TRUE(clargs_test_allocator_requests() >= 1);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), clargs_test_allocator_requests());

    if (options != stale_options())
    {
        clargs_help_options_destroy(options);
    }

    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(creating_help_options_fails_cleanly_at_every_allocation_point)
{
    bool completed = false;
    size_t n;

    clargs_test_allocator_install();

    for (n = 1; n <= REQUEST_LIMIT; ++n)
    {
        clargs_help_options_t *options = stale_options();
        clargs_error_t error;

        clargs_test_allocator_reset();
        clargs_test_allocator_fail_at(n);

        error = clargs_help_options_create(&options);

        if (error == CLARGS_ERROR_NONE)
        {
            /*
             * The creation ended before the request to fail was reached:
             * every allocation point has failed once. The creation is not
             * an operation without requests.
             */
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), n - 1);
            NAOS_CHECK_TRUE(n > 1);
            NAOS_CHECK_TRUE(options != NULL);
            NAOS_CHECK_TRUE(options != stale_options());
            if (options != stale_options())
            {
                clargs_help_options_destroy(options);
            }
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
            completed = true;
            break;
        }

        /*
         * The nth request failed, and the creation reported it: the output
         * is NULL and no block is kept.
         */
        NAOS_CHECK_TRUE(error == CLARGS_ERROR_OUT_OF_MEMORY);
        NAOS_CHECK_TRUE(options == NULL);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), n);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
    }

    NAOS_CHECK_TRUE(completed);

    /* After the failures, a creation succeeds as before. */
    {
        clargs_help_options_t *options = stale_options();

        clargs_test_allocator_reset();
        NAOS_CHECK_TRUE(clargs_help_options_create(&options) == CLARGS_ERROR_NONE);
        NAOS_CHECK_TRUE(options != NULL);
        if (options != stale_options())
        {
            clargs_help_options_destroy(options);
        }
    }

    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(destroying_null_help_options_does_nothing)
{
    clargs_test_allocator_install();

    clargs_help_options_destroy(NULL);
    clargs_help_options_destroy(NULL);

    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 0);

    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(setting_help_options_never_allocates)
{
    clargs_help_options_t *options = NULL;
    size_t requests;
    size_t live;

    clargs_test_allocator_install();

    NAOS_REQUIRE_TRUE(clargs_help_options_create(&options) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(options != NULL);
    requests = clargs_test_allocator_requests();
    live = clargs_test_allocator_live();
    NAOS_CHECK_UINT_EQ(live, 1);

    /*
     * An allocation would fail, and none is made: ordinary values, 0, the
     * limit, a value above the limit and the greatest size_t, each setting
     * set more than once.
     */
    clargs_test_allocator_fail_at(requests + 1);

    clargs_help_options_set_width(options, 100);
    clargs_help_options_set_width(options, 0);
    clargs_help_options_set_width(options, 65535);
    clargs_help_options_set_width(options, 65536);
    clargs_help_options_set_width(options, (size_t)-1);

    clargs_help_options_set_indent(options, 4);
    clargs_help_options_set_indent(options, 0);
    clargs_help_options_set_indent(options, 65535);
    clargs_help_options_set_indent(options, 65536);
    clargs_help_options_set_indent(options, (size_t)-1);

    clargs_help_options_set_max_column(options, 24);
    clargs_help_options_set_max_column(options, 0);
    clargs_help_options_set_max_column(options, 65535);
    clargs_help_options_set_max_column(options, 65536);
    clargs_help_options_set_max_column(options, (size_t)-1);

    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);

    clargs_help_options_destroy(options);

    clargs_test_allocator_uninstall();
    check_clean();
}

/* ---- The generations ------------------------------------------------------ */

/* What the write function of the tests has received. */
typedef struct sink
{
    char text[SINK_SIZE]; /* the parts, joined in the order of the calls */
    size_t length;        /* the number of bytes of text */
    size_t calls;         /* the number of calls */
    size_t empty_calls;   /* the calls with the length 0 or without a text */
} sink_t;

/* Starts an empty sink. */
static void sink_init(sink_t *sink)
{
    memset(sink, 0, sizeof *sink);
}

/*
 * Appends the part to the sink. A part that does not fit is a failure of
 * the write: no text of this file is that long.
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
    if (length > SINK_SIZE - sink->length)
    {
        return false;
    }

    memcpy(sink->text + sink->length, text, length);
    sink->length += length;
    return true;
}

/*
 * A picture of a schema: everything that the functions of <clargs/schema.h>
 * and <clargs/type.h> tell about it, as a text, and the pointers that they
 * return, in the order of the reading. Two pictures of a schema that did
 * not change are equal byte by byte.
 */
typedef struct picture
{
    char text[PICTURE_SIZE];
    size_t length;
    const void *pointers[PICTURE_POINTERS];
    size_t pointer_count;
    bool overflow; /* the text or the pointers did not fit */
} picture_t;

/* Appends the string to the text of the picture. */
static void put(picture_t *picture, const char *text)
{
    size_t length = strlen(text);

    if (length > PICTURE_SIZE - picture->length)
    {
        picture->overflow = true;
        return;
    }
    memcpy(picture->text + picture->length, text, length);
    picture->length += length;
}

/* Appends the pointer to the pointers of the picture. */
static void put_pointer(picture_t *picture, const void *pointer)
{
    if (picture->pointer_count == PICTURE_POINTERS)
    {
        picture->overflow = true;
        return;
    }
    picture->pointers[picture->pointer_count] = pointer;
    ++picture->pointer_count;
}

/*
 * Appends the label and the string, in brackets, or "none" for NULL; the
 * pointer of the string goes to the pointers.
 */
static void put_text(picture_t *picture, const char *label, const char *text)
{
    put(picture, label);
    put_pointer(picture, text);
    if (text == NULL)
    {
        put(picture, "none");
    }
    else
    {
        put(picture, "[");
        put(picture, text);
        put(picture, "]");
    }
}

/* Appends the label and the number, in decimal digits. */
static void put_count(picture_t *picture, const char *label, size_t count)
{
    char digits[32];
    size_t at = sizeof digits - 1;

    digits[at] = '\0';
    do
    {
        --at;
        digits[at] = (char)('0' + (int)(count % 10));
        count /= 10;
    } while (count != 0 && at != 0);

    put(picture, label);
    put(picture, digits + at);
}

/*
 * Appends the label and the value type: its name, its kind, the size of
 * its value and its choices; "none" for NULL.
 */
static void put_type(picture_t *picture, const char *label, const clargs_type_t *type)
{
    size_t index;

    put(picture, label);
    put_pointer(picture, type);
    if (type == NULL)
    {
        put(picture, "none");
        return;
    }
    put_text(picture, "{name=", clargs_type_name(type));
    put_count(picture, " kind=", (size_t)clargs_type_kind(type));
    put_count(picture, " size=", clargs_type_value_size(type));
    put_count(picture, " choices=", clargs_type_choice_count(type));
    for (index = 0; index < clargs_type_choice_count(type); ++index)
    {
        put_text(picture, " ", clargs_type_choice_at(type, index));
    }
    put(picture, "}");
}

/* Appends the option with all that can be read of it. */
static void put_option(picture_t *picture, const clargs_option_t *option)
{
    char short_name[2];

    short_name[0] = clargs_option_short_name(option);
    short_name[1] = '\0';

    put_pointer(picture, option);
    put_pointer(picture, clargs_option_command(option));
    put_count(picture, "  option kind=", (size_t)clargs_option_kind(option));
    put(picture, " short=[");
    put(picture, short_name);
    put(picture, "]");
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

/* Appends the positional with all that can be read of it. */
static void put_positional(picture_t *picture, const clargs_positional_t *positional)
{
    put_pointer(picture, positional);
    put_pointer(picture, clargs_positional_command(positional));
    put_text(picture, "  positional ", clargs_positional_name(positional));
    put_count(picture, " arity=", (size_t)clargs_positional_arity(positional));
    put_type(picture, " type=", clargs_positional_type(positional));
    put_text(picture, " description=", clargs_positional_description(positional));
    put_text(picture, " default=", clargs_positional_default(positional));
    put(picture, "\n");
}

/* Appends the command, and then every command below it. */
static void put_command(picture_t *picture, const clargs_command_t *command)
{
    size_t index;

    put_pointer(picture, command);
    put_pointer(picture, clargs_command_parent(command));
    put_text(picture, "command ", clargs_command_name(command));
    put_text(picture, " description=", clargs_command_description(command));
    put_count(picture, " validated=", clargs_command_is_validated(command) ? 1u : 0u);
    put_count(picture, " options=", clargs_command_option_count(command));
    put_count(picture, " positionals=", clargs_command_positional_count(command));
    put_count(picture, " subcommands=", clargs_command_subcommand_count(command));
    put(picture, "\n");

    for (index = 0; index < clargs_command_option_count(command); ++index)
    {
        put_option(picture, clargs_command_option_at(command, index));
    }
    for (index = 0; index < clargs_command_positional_count(command); ++index)
    {
        put_positional(picture, clargs_command_positional_at(command, index));
    }
    for (index = 0; index < clargs_command_subcommand_count(command); ++index)
    {
        put_command(picture, clargs_command_subcommand_at(command, index));
    }
}

/* Makes picture the picture of the tree of root. */
static void draw(picture_t *picture, const clargs_command_t *root)
{
    memset(picture, 0, sizeof *picture);
    put_command(picture, root);
    NAOS_REQUIRE_TRUE(!picture->overflow);
}

/*
 * One generation that is compared: a command, help options, and the text
 * and the number of requests of a generation without a failure.
 */
typedef struct help_case
{
    const clargs_command_t *command;
    const clargs_help_options_t *options; /* NULL: the default settings */
    char text[SINK_SIZE];                 /* the help */
    size_t length;
    size_t requests;                      /* of one generation of the help */
    char usage[SINK_SIZE];                /* the usage line */
    size_t usage_length;
} help_case_t;

/*
 * The schema of the tests, the help options that are not the default, and
 * what was read and generated before any failure.
 */
typedef struct fixture
{
    clargs_type_t *format; /* the choices of --format */
    clargs_type_t *target; /* the choices of <target> and of --mode */
    clargs_command_t *root;
    clargs_help_options_t *narrow; /* width 40, indent 4, column 20 at most */
    help_case_t cases[CASE_COUNT];
    picture_t before;
} fixture_t;

/*
 * Adds an option with a value to command and sets what is not NULL or
 * false; it must succeed.
 */
static void add_value(clargs_command_t *command, char short_name, const char *long_name,
                      const clargs_type_t *type, const char *value_name, const char *default_value,
                      bool required, const char *description)
{
    clargs_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_value(command, short_name, long_name, type, &option) ==
                      CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(option != NULL);
    if (value_name != NULL)
    {
        NAOS_REQUIRE_TRUE(clargs_option_set_value_name(option, value_name) == CLARGS_ERROR_NONE);
    }
    if (default_value != NULL)
    {
        NAOS_REQUIRE_TRUE(clargs_option_set_default(option, default_value) == CLARGS_ERROR_NONE);
    }
    if (required)
    {
        NAOS_REQUIRE_TRUE(clargs_option_set_required(option, true) == CLARGS_ERROR_NONE);
    }
    NAOS_REQUIRE_TRUE(clargs_option_set_description(option, description) == CLARGS_ERROR_NONE);
}

/*
 * Adds a positional to command, with a default value when default_value is
 * not NULL; it must succeed.
 */
static void add_positional(clargs_command_t *command, const char *name, const clargs_type_t *type,
                           clargs_arity_t arity, const char *default_value,
                           const char *description)
{
    clargs_positional_t *positional = NULL;

    NAOS_REQUIRE_TRUE(clargs_command_add_positional(command, name, type, arity, &positional) ==
                      CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(positional != NULL);
    if (default_value != NULL)
    {
        NAOS_REQUIRE_TRUE(clargs_positional_set_default(positional, default_value) ==
                          CLARGS_ERROR_NONE);
    }
    NAOS_REQUIRE_TRUE(clargs_positional_set_description(positional, description) ==
                      CLARGS_ERROR_NONE);
}

/*
 * Builds and validates the schema of the tests.
 *
 * The root "tool" has two subcommands and five options; the subcommand
 * "build" has two positionals and two options. Both have every annotation:
 * choices, default values, a required option, and a negatable flag whose
 * initial state is on. In each of the two helps the cells get longer from
 * item to item, up to some hundred bytes, so that the cell text is
 * allocated with the first item and grows several times after it.
 */
static void build_schema(fixture_t *fixture)
{
    static const char *const formats[] = {"text", "json", "xml"};
    static const char *const targets[] = {"debug", "release", "profile"};
    clargs_command_t *build = NULL;
    clargs_command_t *clean = NULL;
    clargs_option_t *option = NULL;

    NAOS_REQUIRE_TRUE(clargs_type_create_choice(formats, 3, &fixture->format) ==
                      CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_type_create_choice(targets, 3, &fixture->target) ==
                      CLARGS_ERROR_NONE);

    NAOS_REQUIRE_TRUE(clargs_command_create("tool", &fixture->root) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(fixture->root != NULL);
    NAOS_REQUIRE_TRUE(clargs_command_set_description(
                          fixture->root,
                          "Builds the parts of a project and keeps the results of every build, "
                          "so that a part that did not change is not built a second time.\n"
                          "\n"
                          "Every command reads the settings of the project first.") ==
                      CLARGS_ERROR_NONE);

    NAOS_REQUIRE_TRUE(clargs_command_add_flag(fixture->root, 'v', "verbose", &option) ==
                      CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_option_set_description(option, "Say more") == CLARGS_ERROR_NONE);

    NAOS_REQUIRE_TRUE(clargs_command_add_negatable(fixture->root, '\0', "color", &option) ==
                      CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_option_set_initial(option, true) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_option_set_description(option, "Use colors in the messages") ==
                      CLARGS_ERROR_NONE);

    add_value(fixture->root, 'f', "format", fixture->format, "FMT", "text", false,
              "The format of the report that is written when a command ends, for a person "
              "to read or for another program to take apart");
    add_value(fixture->root, 'o', "output", clargs_type_string(), "FILE", NULL, true,
              "The file that the report is written to. The directories of the file are "
              "created when they do not exist, and a file of the same name is replaced "
              "without a question.\n"
              "The name is taken as it is given: it is not looked up in the directory of "
              "the project, and no ending is added to it, whatever the format of the "
              "report is.");
    add_value(fixture->root, 'j', "jobs", clargs_type_int(), "N", "4", false,
              "How many parts are built at a time");

    NAOS_REQUIRE_TRUE(clargs_command_add_subcommand(fixture->root, "build", &build) ==
                      CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_command_set_description(
                          build, "Builds the parts that changed\nThe other parts are kept.") ==
                      CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_command_add_subcommand(fixture->root, "clean", &clean) ==
                      CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_command_set_description(clean, "Removes the results of the builds") ==
                      CLARGS_ERROR_NONE);

    add_positional(build, "target", fixture->target, CLARGS_ARITY_REQUIRED, NULL, "What to build");
    add_positional(build, "label", clargs_type_string(), CLARGS_ARITY_OPTIONAL, "latest",
                   "The label that the results of this build are kept under, so that a later "
                   "build finds them again and does not build the same parts once more");
    NAOS_REQUIRE_TRUE(clargs_command_add_negatable(build, 'c', "cache", &option) ==
                      CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_option_set_initial(option, true) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_option_set_description(option, "Keep the results") ==
                      CLARGS_ERROR_NONE);
    add_value(build, 'm', "mode", fixture->target, NULL, NULL, true,
              "How the parts are built. The mode decides which checks are compiled into the "
              "parts and how much time the compiler may spend on each of them; the parts of "
              "two modes are kept apart, so a change of the mode builds every part once.\n"
              "A part that was built in another mode is never taken for a part of this "
              "mode, also when its sources did not change in between.");

    NAOS_REQUIRE_TRUE(clargs_command_validate(fixture->root, NULL, NULL, NULL) ==
                      CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(clargs_command_is_validated(fixture->root));
}

/*
 * Checks that the length bytes of a are the same text as the b_length
 * bytes of b: the same length and the same bytes.
 */
static void check_same_text(const char *a, size_t length, const char *b, size_t b_length)
{
    NAOS_CHECK_UINT_EQ(length, b_length);
    NAOS_CHECK_MEM_EQ(a, length, b, b_length);
}

/*
 * Starts a test: installs the test allocator, builds the schema and the
 * help options, takes the picture of the schema, and generates the help and
 * the usage line of every case once, counting the requests of the help.
 */
static void fixture_start(fixture_t *fixture)
{
    const clargs_command_t *build;
    size_t index;

    memset(fixture, 0, sizeof *fixture);
    clargs_test_allocator_install();

    build_schema(fixture);
    build = clargs_command_find_subcommand(fixture->root, "build");
    NAOS_REQUIRE_TRUE(build != NULL);

    NAOS_REQUIRE_TRUE(clargs_help_options_create(&fixture->narrow) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(fixture->narrow != NULL);
    clargs_help_options_set_width(fixture->narrow, 40);
    clargs_help_options_set_indent(fixture->narrow, 4);
    clargs_help_options_set_max_column(fixture->narrow, 20);

    fixture->cases[0].command = fixture->root;
    fixture->cases[0].options = NULL;
    fixture->cases[1].command = fixture->root;
    fixture->cases[1].options = fixture->narrow;
    fixture->cases[2].command = build;
    fixture->cases[2].options = NULL;
    fixture->cases[3].command = build;
    fixture->cases[3].options = fixture->narrow;

    draw(&fixture->before, fixture->root);

    for (index = 0; index < CASE_COUNT; ++index)
    {
        help_case_t *item = &fixture->cases[index];
        size_t live = clargs_test_allocator_live();
        size_t requests = clargs_test_allocator_requests();
        sink_t sink;

        sink_init(&sink);
        NAOS_REQUIRE_TRUE(clargs_help_write(item->command, item->options, sink_write, &sink) ==
                          CLARGS_ERROR_NONE);
        item->requests = clargs_test_allocator_requests() - requests;
        memcpy(item->text, sink.text, sink.length);
        item->length = sink.length;

        /*
         * The generation is one that allocates, and more than once or
         * twice: the cell text grows while the lists are written.
         */
        NAOS_REQUIRE_TRUE(item->requests >= LEAST_REQUESTS);
        NAOS_REQUIRE_TRUE(item->requests < REQUEST_LIMIT);
        NAOS_REQUIRE_TRUE(item->length >= 2);
        NAOS_CHECK_TRUE(item->text[item->length - 1] == '\n');
        NAOS_CHECK_UINT_EQ(sink.empty_calls, 0);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);

        sink_init(&sink);
        NAOS_REQUIRE_TRUE(clargs_help_write_usage(item->command, item->options, sink_write,
                                                  &sink) == CLARGS_ERROR_NONE);
        memcpy(item->usage, sink.text, sink.length);
        item->usage_length = sink.length;
        NAOS_REQUIRE_TRUE(item->usage_length >= 2);
        NAOS_CHECK_TRUE(item->usage[item->usage_length - 1] == '\n');

        /* The help starts with the usage line. */
        NAOS_REQUIRE_TRUE(item->usage_length < item->length);
        NAOS_CHECK_MEM_EQ(item->text, item->usage_length, item->usage, item->usage_length);
    }

    /*
     * The settings change the text: a text that is as before proves that
     * the help options are as before.
     */
    NAOS_CHECK_TRUE(fixture->cases[0].length != fixture->cases[1].length ||
                    memcmp(fixture->cases[0].text, fixture->cases[1].text,
                           fixture->cases[0].length) != 0);
    NAOS_CHECK_TRUE(fixture->cases[2].length != fixture->cases[3].length ||
                    memcmp(fixture->cases[2].text, fixture->cases[3].text,
                           fixture->cases[2].length) != 0);

    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
}

/*
 * Ends a test: destroys what fixture_start() created; nothing stays
 * allocated.
 */
static void fixture_end(fixture_t *fixture)
{
    clargs_help_options_destroy(fixture->narrow);
    clargs_command_destroy(fixture->root);
    clargs_type_destroy(fixture->target);
    clargs_type_destroy(fixture->format);
    clargs_test_allocator_uninstall();
    check_clean();
}

/*
 * Checks that nothing has changed since fixture_start(): the schema reads
 * the same, pointers included, and is validated; and every case, the ones
 * with the help options included, generates the text it generated before,
 * through the write function and into an array.
 */
static void check_unchanged(const fixture_t *fixture)
{
    picture_t now;
    size_t live = clargs_test_allocator_live();
    size_t index;

    draw(&now, fixture->root);
    check_same_text(now.text, now.length, fixture->before.text, fixture->before.length);
    NAOS_CHECK_UINT_EQ(now.pointer_count, fixture->before.pointer_count);
    NAOS_CHECK_MEM_EQ(now.pointers, sizeof now.pointers, fixture->before.pointers,
                      sizeof fixture->before.pointers);
    NAOS_CHECK_TRUE(clargs_command_is_validated(fixture->root));

    for (index = 0; index < CASE_COUNT; ++index)
    {
        const help_case_t *item = &fixture->cases[index];
        size_t requests = clargs_test_allocator_requests();
        char buffer[SINK_SIZE];
        size_t length = LENGTH_UNSET;
        sink_t sink;

        sink_init(&sink);
        NAOS_CHECK_TRUE(clargs_help_write(item->command, item->options, sink_write, &sink) ==
                        CLARGS_ERROR_NONE);
        check_same_text(sink.text, sink.length, item->text, item->length);
        NAOS_CHECK_UINT_EQ(sink.empty_calls, 0);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests() - requests, item->requests);

        memset(buffer, SENTINEL, sizeof buffer);
        NAOS_CHECK_TRUE(clargs_help_format(item->command, item->options, buffer, sizeof buffer,
                                           &length) == CLARGS_ERROR_NONE);
        NAOS_CHECK_UINT_EQ(length, item->length);
        NAOS_REQUIRE_TRUE(memchr(buffer, '\0', sizeof buffer) != NULL);
        check_same_text(buffer, strlen(buffer), item->text, item->length);

        sink_init(&sink);
        NAOS_CHECK_TRUE(clargs_help_write_usage(item->command, item->options, sink_write,
                                                &sink) == CLARGS_ERROR_NONE);
        check_same_text(sink.text, sink.length, item->usage, item->usage_length);
    }

    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
}

/* How clargs_help_format() is called in a check. */
typedef struct format_call
{
    size_t size;      /* the size of the array */
    bool null_buffer; /* the array is NULL; the size is 0 then */
    bool with_length; /* the length is asked for */
} format_call_t;

/*
 * Makes the nth request of a clargs_help_format() of the case fail, with
 * an array of call.size bytes in the middle of an area of sentinel bytes.
 * The call returns CLARGS_ERROR_OUT_OF_MEMORY after n requests; the array
 * holds the empty string, or is untouched with the size 0; no byte outside
 * the array is written; the length, when it is asked for, is 0; nothing
 * stays allocated.
 */
static void check_format_failure(const help_case_t *item, size_t nth, format_call_t call)
{
    char area[AREA_SIZE];
    char want[AREA_SIZE];
    size_t length = LENGTH_UNSET;
    size_t live = clargs_test_allocator_live();
    size_t requests = clargs_test_allocator_requests();
    clargs_error_t error;

    NAOS_REQUIRE_TRUE(call.size <= AREA_SIZE - 2 * GUARD_SIZE);
    NAOS_REQUIRE_TRUE(!call.null_buffer || call.size == 0);

    memset(area, SENTINEL, sizeof area);
    memset(want, SENTINEL, sizeof want);

    clargs_test_allocator_fail_at(requests + nth);
    error = clargs_help_format(item->command, item->options,
                               call.null_buffer ? NULL : area + GUARD_SIZE, call.size,
                               call.with_length ? &length : NULL);
    clargs_test_allocator_fail_at(0);

    NAOS_CHECK_TRUE(error == CLARGS_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests() - requests, nth);
    NAOS_CHECK_UINT_EQ(length, call.with_length ? (size_t)0 : LENGTH_UNSET);

    if (call.size != 0)
    {
        /*
         * The empty string. The bytes of the array behind its '\0' are not
         * specified: the start of the text may have been there.
         */
        NAOS_CHECK_TRUE(area[GUARD_SIZE] == '\0');
    }
    /*
     * The bytes before the array and behind it; with the size 0 these are
     * all the bytes of the area.
     */
    NAOS_CHECK_MEM_EQ(area, GUARD_SIZE, want, GUARD_SIZE);
    NAOS_CHECK_MEM_EQ(area + GUARD_SIZE + call.size, sizeof area - GUARD_SIZE - call.size,
                      want + GUARD_SIZE + call.size, sizeof want - GUARD_SIZE - call.size);

    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
}

/*
 * Every allocation of a generation through the write function fails once.
 * The number of the allocations is the one that was counted, not one that
 * the test knows.
 */
NAOS_TEST(writing_a_help_fails_cleanly_at_every_allocation_point)
{
    fixture_t fixture;
    size_t index;

    fixture_start(&fixture);

    for (index = 0; index < CASE_COUNT; ++index)
    {
        const help_case_t *item = &fixture.cases[index];
        size_t shortest = item->length;
        size_t failures = 0;
        bool completed = false;
        size_t n;

        for (n = 1; n <= REQUEST_LIMIT; ++n)
        {
            size_t live = clargs_test_allocator_live();
            size_t requests = clargs_test_allocator_requests();
            clargs_error_t error;
            sink_t sink;

            sink_init(&sink);
            clargs_test_allocator_fail_at(requests + n);
            error = clargs_help_write(item->command, item->options, sink_write, &sink);
            clargs_test_allocator_fail_at(0);

            if (error == CLARGS_ERROR_NONE)
            {
                /*
                 * The generation ended before the request to fail was
                 * reached: every allocation point has failed once.
                 */
                NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests() - requests, n - 1);
                NAOS_CHECK_UINT_EQ(n - 1, item->requests);
                check_same_text(sink.text, sink.length, item->text, item->length);
                NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);
                completed = true;
                break;
            }

            /*
             * The nth request failed, and the generation reported it and
             * made no request after it. No block is kept.
             */
            ++failures;
            NAOS_CHECK_TRUE(error == CLARGS_ERROR_OUT_OF_MEMORY);
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests() - requests, n);
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);
            NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);

            /*
             * What the write function received is a start of the text, and
             * not all of it. The usage line came before the first request,
             * and the generation stopped before the heading of the item
             * whose cell could not be built: the last line is whole.
             */
            NAOS_CHECK_UINT_EQ(sink.empty_calls, 0);
            NAOS_REQUIRE_TRUE(sink.length < item->length);
            NAOS_CHECK_MEM_EQ(sink.text, sink.length, item->text, sink.length);
            NAOS_REQUIRE_TRUE(sink.length >= item->usage_length);
            NAOS_CHECK_TRUE(sink.text[sink.length - 1] == '\n');
            if (sink.length < shortest)
            {
                shortest = sink.length;
            }

            /*
             * The schema and the help options are as they were, and every
             * generation gives its text again.
             */
            check_unchanged(&fixture);
        }

        NAOS_CHECK_TRUE(completed);
        NAOS_CHECK_UINT_EQ(failures, item->requests);
        NAOS_CHECK_TRUE(failures >= LEAST_REQUESTS);
        /*
         * The first request is made for the first item of a list: more
         * than the usage line was written before it.
         */
        NAOS_CHECK_TRUE(shortest > item->usage_length);
    }

    fixture_end(&fixture);
}

/*
 * Every allocation of a generation into an array fails once, for every
 * kind of array: one that holds all of the text, ones that hold a part of
 * it, one of one byte, and the size 0 with an array and with NULL; each of
 * them with and without the length.
 */
NAOS_TEST(formatting_a_help_fails_cleanly_at_every_allocation_point)
{
    fixture_t fixture;
    size_t index;

    fixture_start(&fixture);

    for (index = 0; index < CASE_COUNT; ++index)
    {
        const help_case_t *item = &fixture.cases[index];
        size_t n;

        NAOS_REQUIRE_TRUE(item->length + 1 <= AREA_SIZE - 2 * GUARD_SIZE);
        NAOS_REQUIRE_TRUE(item->length / 2 > 20);

        /*
         * The requests of a generation were counted by fixture_start(): the
         * 1st to the last of them fail in turn.
         */
        for (n = 1; n <= item->requests; ++n)
        {
            format_call_t call;
            int with_length;

            for (with_length = 0; with_length < 2; ++with_length)
            {
                call.with_length = with_length != 0;
                call.null_buffer = false;

                /*
                 * Room for all of the text: its start was written before
                 * the allocation failed.
                 */
                call.size = item->length + 1;
                check_format_failure(item, n, call);
                /*
                 * Room for a part of it: a part that ends in the lists,
                 * and one that ends in the usage line.
                 */
                call.size = item->length / 2;
                check_format_failure(item, n, call);
                call.size = 20;
                check_format_failure(item, n, call);
                call.size = 1;
                check_format_failure(item, n, call);

                /* The size 0: nothing is written. */
                call.size = 0;
                check_format_failure(item, n, call);
                call.null_buffer = true;
                check_format_failure(item, n, call);
            }

            /*
             * The schema and the help options are as they were, and every
             * generation gives its text again.
             */
            check_unchanged(&fixture);
        }

        /*
         * There is no request behind the last one that failed: with the
         * next one set to fail, the generation succeeds.
         */
        {
            char buffer[SINK_SIZE];
            size_t length = LENGTH_UNSET;
            size_t requests = clargs_test_allocator_requests();

            memset(buffer, SENTINEL, sizeof buffer);
            clargs_test_allocator_fail_at(requests + item->requests + 1);
            NAOS_CHECK_TRUE(clargs_help_format(item->command, item->options, buffer,
                                               sizeof buffer, &length) == CLARGS_ERROR_NONE);
            clargs_test_allocator_fail_at(0);

            NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests() - requests, item->requests);
            NAOS_CHECK_UINT_EQ(length, item->length);
            NAOS_REQUIRE_TRUE(memchr(buffer, '\0', sizeof buffer) != NULL);
            check_same_text(buffer, strlen(buffer), item->text, item->length);
        }
    }

    fixture_end(&fixture);
}

/*
 * The usage line alone is generated without a request, for the root and
 * for a subcommand, with and without help options, through the write
 * function and into an array. A request would fail: the next one is set to
 * fail before every call, and none is made.
 */
NAOS_TEST(generating_a_usage_line_never_allocates)
{
    fixture_t fixture;
    size_t index;

    fixture_start(&fixture);

    for (index = 0; index < CASE_COUNT; ++index)
    {
        const help_case_t *item = &fixture.cases[index];
        size_t live = clargs_test_allocator_live();
        size_t requests = clargs_test_allocator_requests();
        char buffer[SINK_SIZE];
        size_t length = LENGTH_UNSET;
        sink_t sink;

        clargs_test_allocator_fail_at(requests + 1);

        sink_init(&sink);
        NAOS_CHECK_TRUE(clargs_help_write_usage(item->command, item->options, sink_write,
                                                &sink) == CLARGS_ERROR_NONE);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests);
        check_same_text(sink.text, sink.length, item->usage, item->usage_length);
        NAOS_CHECK_UINT_EQ(sink.empty_calls, 0);

        memset(buffer, SENTINEL, sizeof buffer);
        NAOS_CHECK_TRUE(clargs_help_format_usage(item->command, item->options, buffer,
                                                 sizeof buffer, &length) == CLARGS_ERROR_NONE);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests);
        NAOS_CHECK_UINT_EQ(length, item->usage_length);
        NAOS_REQUIRE_TRUE(memchr(buffer, '\0', sizeof buffer) != NULL);
        check_same_text(buffer, strlen(buffer), item->usage, item->usage_length);

        /* Also an array that cuts the text, the size 0 and no length. */
        NAOS_CHECK_TRUE(clargs_help_format_usage(item->command, item->options, buffer, 5,
                                                 NULL) == CLARGS_ERROR_NONE);
        length = LENGTH_UNSET;
        NAOS_CHECK_TRUE(clargs_help_format_usage(item->command, item->options, NULL, 0,
                                                 &length) == CLARGS_ERROR_NONE);
        NAOS_CHECK_UINT_EQ(length, item->usage_length);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests);

        clargs_test_allocator_fail_at(0);

        NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
    }

    fixture_end(&fixture);
}

NAOS_SUITE(help_failure,
    NAOS_ADD_TEST(creating_and_destroying_help_options_leaves_nothing_allocated),
    NAOS_ADD_TEST(creating_help_options_fails_cleanly_at_every_allocation_point),
    NAOS_ADD_TEST(destroying_null_help_options_does_nothing),
    NAOS_ADD_TEST(setting_help_options_never_allocates),
    NAOS_ADD_TEST(writing_a_help_fails_cleanly_at_every_allocation_point),
    NAOS_ADD_TEST(formatting_a_help_fails_cleanly_at_every_allocation_point),
    NAOS_ADD_TEST(generating_a_usage_line_never_allocates)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(help_failure)
)
