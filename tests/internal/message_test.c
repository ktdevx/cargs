/*
 * Tests of the internal function that builds the message of a parse error:
 * "message.h".
 *
 * The tests use the test allocator ("../support/test_allocator.h") to see
 * the requests that clargs_message_build makes: its count of requests
 * shows that a message takes one allocation, its count of live blocks
 * shows that a message is freed with clargs_free and that a failed build
 * leaves nothing behind, and its count of violations shows a call that the
 * library must never make.
 *
 * The state of the test allocator is shared by all the tests of this
 * executable. Every test starts with an install (which resets), frees what
 * it allocated, sets the default allocator back before it ends, and ends
 * with check_clean().
 */
#include <clargs/clargs.h>

#include "alloc_internal.h"
#include "message.h"

#include "../support/test_allocator.h"

#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* The end of every test: the test allocator has no live block and saw no
 * violation of the allocator contract. */
static void check_clean(void)
{
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
}

/* The parts of a message of the kind, for an error on the command line,
 * with no strings: a test sets the strings that the wording of the kind
 * uses. */
static clargs_message_parts_t parts_of(clargs_parse_error_kind_t kind)
{
    clargs_message_parts_t parts;

    parts.kind = kind;
    parts.source = CLARGS_SOURCE_COMMAND_LINE;
    parts.for_positional = false;
    parts.name_prefix = "";
    parts.name = NULL;
    parts.name_length = 0;
    parts.value = NULL;
    parts.type_name = NULL;
    return parts;
}

/* Sets the name of the parts: the prefix and the whole string name. */
static void set_name(clargs_message_parts_t *parts, const char *prefix, const char *name)
{
    parts->name_prefix = prefix;
    parts->name = name;
    parts->name_length = strlen(name);
}

/* Builds the message of the parts and checks that it is exactly expected:
 * the same bytes with the terminator, made with one request, and freed
 * with clargs_free. The test allocator is installed. */
static void check_message(const clargs_message_parts_t *parts, const char *expected)
{
    char *message = NULL;
    size_t requests = clargs_test_allocator_requests();
    size_t live = clargs_test_allocator_live();

    NAOS_CHECK_INT_EQ(clargs_message_build(parts, &message), CLARGS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests + 1);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live + 1);
    NAOS_CHECK_TRUE(message != NULL);
    if (message != NULL) {
        NAOS_CHECK_STR_EQ(message, expected);
        NAOS_CHECK_UINT_EQ(strlen(message), strlen(expected));
        clargs_free(message);
    }
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), live);
}

/* ---- The public enumerations ------------------------------------------ */

/* The values are in the order of the design, from 0: a value that is added
 * later comes after them and changes none. */
NAOS_TEST(enumerations_keep_their_order)
{
    NAOS_CHECK_INT_EQ(CLARGS_SOURCE_NONE, 0);
    NAOS_CHECK_INT_EQ(CLARGS_SOURCE_DEFAULT, 1);
    NAOS_CHECK_INT_EQ(CLARGS_SOURCE_COMMAND_LINE, 2);

    NAOS_CHECK_INT_EQ(CLARGS_PARSE_ERROR_NONE, 0);
    NAOS_CHECK_INT_EQ(CLARGS_PARSE_ERROR_UNKNOWN_OPTION, 1);
    NAOS_CHECK_INT_EQ(CLARGS_PARSE_ERROR_UNKNOWN_SUBCOMMAND, 2);
    NAOS_CHECK_INT_EQ(CLARGS_PARSE_ERROR_MISSING_VALUE, 3);
    NAOS_CHECK_INT_EQ(CLARGS_PARSE_ERROR_UNEXPECTED_VALUE, 4);
    NAOS_CHECK_INT_EQ(CLARGS_PARSE_ERROR_INVALID_VALUE, 5);
    NAOS_CHECK_INT_EQ(CLARGS_PARSE_ERROR_MISSING_OPTION, 6);
    NAOS_CHECK_INT_EQ(CLARGS_PARSE_ERROR_MISSING_POSITIONAL, 7);
    NAOS_CHECK_INT_EQ(CLARGS_PARSE_ERROR_UNEXPECTED_POSITIONAL, 8);
    NAOS_CHECK_INT_EQ(CLARGS_PARSE_ERROR_INVALID_ENCODING, 9);
}

/* ---- The wording of each kind ----------------------------------------- */

NAOS_TEST(unknown_option)
{
    clargs_message_parts_t parts = parts_of(CLARGS_PARSE_ERROR_UNKNOWN_OPTION);

    clargs_test_allocator_install();

    set_name(&parts, "--", "colour");
    check_message(&parts, "unknown option '--colour'");

    set_name(&parts, "-", "x");
    check_message(&parts, "unknown option '-x'");

    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(unknown_subcommand)
{
    clargs_message_parts_t parts = parts_of(CLARGS_PARSE_ERROR_UNKNOWN_SUBCOMMAND);

    clargs_test_allocator_install();

    parts.value = "comit";
    check_message(&parts, "unknown command 'comit'");

    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(missing_value)
{
    clargs_message_parts_t parts = parts_of(CLARGS_PARSE_ERROR_MISSING_VALUE);

    clargs_test_allocator_install();

    set_name(&parts, "--", "output");
    check_message(&parts, "option '--output' requires a value");

    set_name(&parts, "-", "o");
    check_message(&parts, "option '-o' requires a value");

    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(unexpected_value)
{
    clargs_message_parts_t parts = parts_of(CLARGS_PARSE_ERROR_UNEXPECTED_VALUE);

    clargs_test_allocator_install();

    /* The name as it was written: a negated name keeps its "no-". */
    set_name(&parts, "--", "no-color");
    check_message(&parts, "option '--no-color' does not take a value");

    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(invalid_value_for_an_option)
{
    clargs_message_parts_t parts = parts_of(CLARGS_PARSE_ERROR_INVALID_VALUE);

    clargs_test_allocator_install();

    set_name(&parts, "--", "jobs");
    parts.value = "abc";
    parts.type_name = "integer";
    check_message(&parts, "invalid value 'abc' for option '--jobs' (expected integer)");

    set_name(&parts, "-", "j");
    check_message(&parts, "invalid value 'abc' for option '-j' (expected integer)");

    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(invalid_value_for_a_positional)
{
    clargs_message_parts_t parts = parts_of(CLARGS_PARSE_ERROR_INVALID_VALUE);

    clargs_test_allocator_install();

    parts.for_positional = true;
    set_name(&parts, "", "COUNT");
    parts.value = "x";
    parts.type_name = "integer";
    check_message(&parts, "invalid value 'x' for argument 'COUNT' (expected integer)");

    clargs_test_allocator_uninstall();
    check_clean();
}

/* A default value that cannot be converted: "invalid default value". */
NAOS_TEST(invalid_default_value_for_an_option)
{
    clargs_message_parts_t parts = parts_of(CLARGS_PARSE_ERROR_INVALID_VALUE);

    clargs_test_allocator_install();

    parts.source = CLARGS_SOURCE_DEFAULT;
    set_name(&parts, "--", "jobs");
    parts.value = "many";
    parts.type_name = "integer";
    check_message(&parts, "invalid default value 'many' for option '--jobs' (expected integer)");

    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(invalid_default_value_for_a_positional)
{
    clargs_message_parts_t parts = parts_of(CLARGS_PARSE_ERROR_INVALID_VALUE);

    clargs_test_allocator_install();

    parts.source = CLARGS_SOURCE_DEFAULT;
    parts.for_positional = true;
    set_name(&parts, "", "LEVEL");
    parts.value = "loud";
    parts.type_name = "choice";
    check_message(&parts, "invalid default value 'loud' for argument 'LEVEL' (expected choice)");

    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(missing_option)
{
    clargs_message_parts_t parts = parts_of(CLARGS_PARSE_ERROR_MISSING_OPTION);

    clargs_test_allocator_install();

    set_name(&parts, "--", "output");
    check_message(&parts, "missing required option '--output'");

    set_name(&parts, "-", "o");
    check_message(&parts, "missing required option '-o'");

    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(missing_positional)
{
    clargs_message_parts_t parts = parts_of(CLARGS_PARSE_ERROR_MISSING_POSITIONAL);

    clargs_test_allocator_install();

    set_name(&parts, "", "FILE");
    check_message(&parts, "missing required argument 'FILE'");

    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(unexpected_positional)
{
    clargs_message_parts_t parts = parts_of(CLARGS_PARSE_ERROR_UNEXPECTED_POSITIONAL);

    clargs_test_allocator_install();

    parts.value = "extra";
    check_message(&parts, "unexpected argument 'extra'");

    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(invalid_encoding)
{
    clargs_message_parts_t parts = parts_of(CLARGS_PARSE_ERROR_INVALID_ENCODING);

    clargs_test_allocator_install();

    /* The element with U+FFFD in place of what could not be converted. */
    parts.value = "caf\xEF\xBF\xBD";
    check_message(&parts, "invalid character encoding in argument 'caf\xEF\xBF\xBD'");

    clargs_test_allocator_uninstall();
    check_clean();
}

/* ---- What the wording does not depend on ------------------------------ */

/* The source chooses the wording of CLARGS_PARSE_ERROR_INVALID_VALUE only,
 * and for_positional too: the other kinds read neither. */
NAOS_TEST(source_and_for_positional_change_only_an_invalid_value)
{
    clargs_message_parts_t parts = parts_of(CLARGS_PARSE_ERROR_MISSING_OPTION);

    clargs_test_allocator_install();

    parts.source = CLARGS_SOURCE_DEFAULT;
    parts.for_positional = true;
    set_name(&parts, "--", "output");
    check_message(&parts, "missing required option '--output'");

    parts.source = CLARGS_SOURCE_NONE;
    check_message(&parts, "missing required option '--output'");

    /* Only CLARGS_SOURCE_DEFAULT is a default value. */
    parts = parts_of(CLARGS_PARSE_ERROR_INVALID_VALUE);
    parts.source = CLARGS_SOURCE_NONE;
    set_name(&parts, "--", "jobs");
    parts.value = "abc";
    parts.type_name = "integer";
    check_message(&parts, "invalid value 'abc' for option '--jobs' (expected integer)");

    clargs_test_allocator_uninstall();
    check_clean();
}

/* The strings that a kind does not use are not read: they may be NULL, and
 * what they hold does not get into the message. */
NAOS_TEST(unused_strings_are_ignored)
{
    clargs_message_parts_t parts = parts_of(CLARGS_PARSE_ERROR_UNKNOWN_OPTION);

    clargs_test_allocator_install();

    set_name(&parts, "--", "colour");
    parts.value = "red";
    parts.type_name = "string";
    check_message(&parts, "unknown option '--colour'");

    parts = parts_of(CLARGS_PARSE_ERROR_UNEXPECTED_POSITIONAL);
    set_name(&parts, "--", "colour");
    parts.value = "extra";
    parts.type_name = "string";
    check_message(&parts, "unexpected argument 'extra'");

    clargs_test_allocator_uninstall();
    check_clean();
}

/* ---- The name --------------------------------------------------------- */

/* The name ends after name_length bytes, not at the terminator: the name
 * of "--output=file" is the six bytes of "output". */
NAOS_TEST(name_ends_at_its_length)
{
    const char element[] = "--output=file";
    const char bundle[] = "-abc";
    clargs_message_parts_t parts = parts_of(CLARGS_PARSE_ERROR_UNEXPECTED_VALUE);

    clargs_test_allocator_install();

    parts.name_prefix = "--";
    parts.name = element + 2;
    parts.name_length = 6;
    check_message(&parts, "option '--output' does not take a value");

    /* One character of a bundle: "-abc" names 'b'. */
    parts = parts_of(CLARGS_PARSE_ERROR_UNKNOWN_OPTION);
    parts.name_prefix = "-";
    parts.name = bundle + 2;
    parts.name_length = 1;
    check_message(&parts, "unknown option '-b'");

    /* The length is used in every wording that has a name. */
    parts = parts_of(CLARGS_PARSE_ERROR_INVALID_VALUE);
    parts.name_prefix = "--";
    parts.name = "jobs=abc";
    parts.name_length = 4;
    parts.value = "abc";
    parts.type_name = "integer";
    check_message(&parts, "invalid value 'abc' for option '--jobs' (expected integer)");

    clargs_test_allocator_uninstall();
    check_clean();
}

/* A name of the length 0 is an empty name, and the name is not read. */
NAOS_TEST(name_of_the_length_zero)
{
    clargs_message_parts_t parts = parts_of(CLARGS_PARSE_ERROR_UNKNOWN_OPTION);

    clargs_test_allocator_install();

    parts.name_prefix = "--";
    parts.name = "=value";
    parts.name_length = 0;
    check_message(&parts, "unknown option '--'");

    clargs_test_allocator_uninstall();
    check_clean();
}

/* ---- The strings are not changed -------------------------------------- */

/* A value is put in as it is: quotes, a percent sign, a backslash, bytes
 * that are not ASCII and an empty string are neither escaped nor replaced
 * nor cut. */
NAOS_TEST(strings_are_embedded_as_they_are)
{
    clargs_message_parts_t parts = parts_of(CLARGS_PARSE_ERROR_UNEXPECTED_POSITIONAL);

    clargs_test_allocator_install();

    parts.value = "it's \"100%s\" \\n";
    check_message(&parts, "unexpected argument 'it's \"100%s\" \\n'");

    parts.value = "\x80\xFF\x01 \xE3\x81\x82";
    check_message(&parts, "unexpected argument '\x80\xFF\x01 \xE3\x81\x82'");

    parts.value = "";
    check_message(&parts, "unexpected argument ''");

    parts = parts_of(CLARGS_PARSE_ERROR_INVALID_VALUE);
    set_name(&parts, "--", "na'me%d");
    parts.value = "";
    parts.type_name = "my type (v2)";
    check_message(&parts, "invalid value '' for option '--na'me%d' (expected my type (v2))");

    clargs_test_allocator_uninstall();
    check_clean();
}

/* A long value is copied whole. */
NAOS_TEST(long_strings_are_copied_whole)
{
    static const char head[] = "unknown command '";
    char value[4096];
    char expected[sizeof head + sizeof value];
    clargs_message_parts_t parts = parts_of(CLARGS_PARSE_ERROR_UNKNOWN_SUBCOMMAND);
    size_t i;

    for (i = 0; i < sizeof value - 1; ++i) {
        value[i] = (char)('a' + i % 26);
    }
    value[sizeof value - 1] = '\0';

    memcpy(expected, head, sizeof head - 1);
    memcpy(expected + sizeof head - 1, value, sizeof value - 1);
    expected[sizeof head - 1 + sizeof value - 1] = '\'';
    expected[sizeof head - 1 + sizeof value] = '\0';

    clargs_test_allocator_install();

    parts.value = value;
    check_message(&parts, expected);

    clargs_test_allocator_uninstall();
    check_clean();
}

/* ---- The form of the message ------------------------------------------ */

/* One line: no line break, no prefix, no full stop. Every wording starts
 * with a lower case letter of its own text and ends with the last
 * character of the table. */
NAOS_TEST(message_is_one_line_without_a_full_stop)
{
    static const clargs_parse_error_kind_t kinds[] = {
        CLARGS_PARSE_ERROR_UNKNOWN_OPTION,     CLARGS_PARSE_ERROR_UNKNOWN_SUBCOMMAND,
        CLARGS_PARSE_ERROR_MISSING_VALUE,      CLARGS_PARSE_ERROR_UNEXPECTED_VALUE,
        CLARGS_PARSE_ERROR_INVALID_VALUE,      CLARGS_PARSE_ERROR_MISSING_OPTION,
        CLARGS_PARSE_ERROR_MISSING_POSITIONAL, CLARGS_PARSE_ERROR_UNEXPECTED_POSITIONAL,
        CLARGS_PARSE_ERROR_INVALID_ENCODING
    };
    size_t i;

    clargs_test_allocator_install();

    for (i = 0; i < sizeof kinds / sizeof kinds[0]; ++i) {
        clargs_message_parts_t parts = parts_of(kinds[i]);
        char *message = NULL;
        size_t length;

        set_name(&parts, "--", "name");
        parts.value = "value";
        parts.type_name = "type";

        NAOS_CHECK_INT_EQ(clargs_message_build(&parts, &message), CLARGS_ERROR_NONE);
        NAOS_REQUIRE_TRUE(message != NULL);

        length = strlen(message);
        NAOS_CHECK_TRUE(length > 0);
        NAOS_CHECK_TRUE(strchr(message, '\n') == NULL);
        NAOS_CHECK_TRUE(strchr(message, '\r') == NULL);
        NAOS_CHECK_TRUE(strchr(message, ':') == NULL);
        NAOS_CHECK_TRUE(message[0] >= 'a' && message[0] <= 'z');
        NAOS_CHECK_TRUE(message[length - 1] != '.');
        NAOS_CHECK_TRUE(message[length - 1] != ' ');

        clargs_free(message);
    }

    clargs_test_allocator_uninstall();
    check_clean();
}

/* ---- Failures --------------------------------------------------------- */

/* When the allocation fails, the result is CLARGS_ERROR_OUT_OF_MEMORY, *out
 * is NULL and nothing stays allocated. The next build works again. */
NAOS_TEST(allocation_failure_is_reported)
{
    clargs_message_parts_t parts = parts_of(CLARGS_PARSE_ERROR_INVALID_VALUE);
    char unrelated = 'x';
    char *message = &unrelated;

    set_name(&parts, "--", "jobs");
    parts.value = "abc";
    parts.type_name = "integer";

    clargs_test_allocator_install();
    clargs_test_allocator_fail_at(1);

    NAOS_CHECK_INT_EQ(clargs_message_build(&parts, &message), CLARGS_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_TRUE(message == NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
    NAOS_CHECK_INT_EQ(unrelated, 'x');

    /* Only the first request fails. */
    check_message(&parts, "invalid value 'abc' for option '--jobs' (expected integer)");

    clargs_test_allocator_uninstall();
    check_clean();
}

/* A length that does not fit in size_t is an allocation failure: nothing
 * is requested and no string is read past its real end. The name is not
 * read before the lengths have been added, so a length that is not the
 * real one shows it. */
NAOS_TEST(length_overflow_is_an_allocation_failure)
{
    clargs_message_parts_t parts = parts_of(CLARGS_PARSE_ERROR_UNKNOWN_OPTION);
    char unrelated = 'x';
    char *message = &unrelated;

    parts.name_prefix = "--";
    parts.name = "colour";
    parts.name_length = SIZE_MAX;

    clargs_test_allocator_install();

    NAOS_CHECK_INT_EQ(clargs_message_build(&parts, &message), CLARGS_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_TRUE(message == NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 0);

    /* The sum without the terminator fits, the terminator does not. */
    message = &unrelated;
    parts.name_length = SIZE_MAX - strlen("unknown option '--'");
    NAOS_CHECK_INT_EQ(clargs_message_build(&parts, &message), CLARGS_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_TRUE(message == NULL);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 0);

    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_SUITE(enumerations,
    NAOS_ADD_TEST(enumerations_keep_their_order)
)

NAOS_SUITE(wording,
    NAOS_ADD_TEST(unknown_option),
    NAOS_ADD_TEST(unknown_subcommand),
    NAOS_ADD_TEST(missing_value),
    NAOS_ADD_TEST(unexpected_value),
    NAOS_ADD_TEST(invalid_value_for_an_option),
    NAOS_ADD_TEST(invalid_value_for_a_positional),
    NAOS_ADD_TEST(invalid_default_value_for_an_option),
    NAOS_ADD_TEST(invalid_default_value_for_a_positional),
    NAOS_ADD_TEST(missing_option),
    NAOS_ADD_TEST(missing_positional),
    NAOS_ADD_TEST(unexpected_positional),
    NAOS_ADD_TEST(invalid_encoding),
    NAOS_ADD_TEST(source_and_for_positional_change_only_an_invalid_value),
    NAOS_ADD_TEST(unused_strings_are_ignored)
)

NAOS_SUITE(embedding,
    NAOS_ADD_TEST(name_ends_at_its_length),
    NAOS_ADD_TEST(name_of_the_length_zero),
    NAOS_ADD_TEST(strings_are_embedded_as_they_are),
    NAOS_ADD_TEST(long_strings_are_copied_whole),
    NAOS_ADD_TEST(message_is_one_line_without_a_full_stop)
)

NAOS_SUITE(failure,
    NAOS_ADD_TEST(allocation_failure_is_reported),
    NAOS_ADD_TEST(length_overflow_is_an_allocation_failure)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(enumerations),
    NAOS_ADD_SUITE(wording),
    NAOS_ADD_SUITE(embedding),
    NAOS_ADD_SUITE(failure)
)
