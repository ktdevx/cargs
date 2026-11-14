/*
 * SPDX-FileCopyrightText: 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * Tests of the internal functions for the shape of an argument: "lex.h".
 *
 * The functions keep no state and allocate nothing, so the tests need no
 * allocator.
 */
#include "lex.h"

#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>

/* ---- The classification ----------------------------------------------- */

/* An empty string does not start with '-': it is not an option. */
NAOS_TEST(classify_empty_is_plain)
{
    NAOS_CHECK_TRUE(argx_lex_classify("") == ARGX_LEX_PLAIN);
}

/* A single '-' is not an option. */
NAOS_TEST(classify_single_hyphen_is_plain)
{
    NAOS_CHECK_TRUE(argx_lex_classify("-") == ARGX_LEX_PLAIN);
}

/*
 * An argument that does not start with '-' is not an option, wherever
 * else it has a '-' or a '='.
 */
NAOS_TEST(classify_without_leading_hyphen_is_plain)
{
    NAOS_CHECK_TRUE(argx_lex_classify("x") == ARGX_LEX_PLAIN);
    NAOS_CHECK_TRUE(argx_lex_classify("x-y") == ARGX_LEX_PLAIN);
    NAOS_CHECK_TRUE(argx_lex_classify("x--y") == ARGX_LEX_PLAIN);
    NAOS_CHECK_TRUE(argx_lex_classify("x-") == ARGX_LEX_PLAIN);
    NAOS_CHECK_TRUE(argx_lex_classify(" -x") == ARGX_LEX_PLAIN);
    NAOS_CHECK_TRUE(argx_lex_classify("1") == ARGX_LEX_PLAIN);
    NAOS_CHECK_TRUE(argx_lex_classify(".5") == ARGX_LEX_PLAIN);
    NAOS_CHECK_TRUE(argx_lex_classify("=x") == ARGX_LEX_PLAIN);
    NAOS_CHECK_TRUE(argx_lex_classify("\xC3\xA9") == ARGX_LEX_PLAIN);
}

/* "--" alone ends the options. */
NAOS_TEST(classify_double_hyphen_is_terminator)
{
    NAOS_CHECK_TRUE(argx_lex_classify("--") == ARGX_LEX_TERMINATOR);
}

/*
 * "--" and one character or more is a long option, whatever the
 * characters are: a third '-', a '=' and a byte outside ASCII too.
 */
NAOS_TEST(classify_double_hyphen_and_more_is_long)
{
    NAOS_CHECK_TRUE(argx_lex_classify("--x") == ARGX_LEX_LONG);
    NAOS_CHECK_TRUE(argx_lex_classify("---") == ARGX_LEX_LONG);
    NAOS_CHECK_TRUE(argx_lex_classify("--=x") == ARGX_LEX_LONG);
    NAOS_CHECK_TRUE(argx_lex_classify("--=") == ARGX_LEX_LONG);
    NAOS_CHECK_TRUE(argx_lex_classify("--name") == ARGX_LEX_LONG);
    NAOS_CHECK_TRUE(argx_lex_classify("--name=value") == ARGX_LEX_LONG);
    NAOS_CHECK_TRUE(argx_lex_classify("--1") == ARGX_LEX_LONG);
    NAOS_CHECK_TRUE(argx_lex_classify("-- ") == ARGX_LEX_LONG);
    NAOS_CHECK_TRUE(argx_lex_classify("--\xC3\xA9") == ARGX_LEX_LONG);
}

/*
 * '-' and a character that is not '-' is a cluster of short options. The
 * classification does not look at the schema: the forms of a negative
 * number are clusters too.
 */
NAOS_TEST(classify_hyphen_and_other_is_short)
{
    NAOS_CHECK_TRUE(argx_lex_classify("-x") == ARGX_LEX_SHORT);
    NAOS_CHECK_TRUE(argx_lex_classify("-xyz") == ARGX_LEX_SHORT);
    NAOS_CHECK_TRUE(argx_lex_classify("-1") == ARGX_LEX_SHORT);
    NAOS_CHECK_TRUE(argx_lex_classify("-.5") == ARGX_LEX_SHORT);
    NAOS_CHECK_TRUE(argx_lex_classify("-.") == ARGX_LEX_SHORT);
    NAOS_CHECK_TRUE(argx_lex_classify("-1x") == ARGX_LEX_SHORT);
    NAOS_CHECK_TRUE(argx_lex_classify("-x-") == ARGX_LEX_SHORT);
    NAOS_CHECK_TRUE(argx_lex_classify("-x--") == ARGX_LEX_SHORT);
    NAOS_CHECK_TRUE(argx_lex_classify("-=") == ARGX_LEX_SHORT);
    NAOS_CHECK_TRUE(argx_lex_classify("-o=value") == ARGX_LEX_SHORT);
    NAOS_CHECK_TRUE(argx_lex_classify("- ") == ARGX_LEX_SHORT);
    NAOS_CHECK_TRUE(argx_lex_classify("-\xC3\xA9") == ARGX_LEX_SHORT);
}

/* ---- The form of a negative number ------------------------------------ */

/* '-' and a digit, whatever follows the digit. */
NAOS_TEST(negative_number_accepts_hyphen_and_digit)
{
    const char digits[] = "0123456789";
    char arg[3];
    size_t i;

    arg[0] = '-';
    arg[2] = '\0';
    for (i = 0; i < (sizeof digits - 1); ++i)
    {
        arg[1] = digits[i];
        NAOS_CHECK_TRUE(argx_lex_is_negative_number(arg));
    }
    NAOS_CHECK_TRUE(argx_lex_is_negative_number("-1"));
    NAOS_CHECK_TRUE(argx_lex_is_negative_number("-42"));
    NAOS_CHECK_TRUE(argx_lex_is_negative_number("-1.5"));
    NAOS_CHECK_TRUE(argx_lex_is_negative_number("-1x"));
    NAOS_CHECK_TRUE(argx_lex_is_negative_number("-1e9"));
}

/* '-', '.' and a digit, whatever follows the digit. */
NAOS_TEST(negative_number_accepts_hyphen_dot_and_digit)
{
    NAOS_CHECK_TRUE(argx_lex_is_negative_number("-.5"));
    NAOS_CHECK_TRUE(argx_lex_is_negative_number("-.0"));
    NAOS_CHECK_TRUE(argx_lex_is_negative_number("-.9"));
    NAOS_CHECK_TRUE(argx_lex_is_negative_number("-.5x"));
}

/* A '.' with no digit after it is not the form of a number. */
NAOS_TEST(negative_number_rejects_dot_without_digit)
{
    NAOS_CHECK_FALSE(argx_lex_is_negative_number("-."));
    NAOS_CHECK_FALSE(argx_lex_is_negative_number("-.x"));
    NAOS_CHECK_FALSE(argx_lex_is_negative_number("-.."));
    NAOS_CHECK_FALSE(argx_lex_is_negative_number("-..5"));
    NAOS_CHECK_FALSE(argx_lex_is_negative_number("-.-1"));
}

/* Arguments without a leading '-', the '-' alone and the long forms. */
NAOS_TEST(negative_number_rejects_other_shapes)
{
    NAOS_CHECK_FALSE(argx_lex_is_negative_number(""));
    NAOS_CHECK_FALSE(argx_lex_is_negative_number("-"));
    NAOS_CHECK_FALSE(argx_lex_is_negative_number("--"));
    NAOS_CHECK_FALSE(argx_lex_is_negative_number("--1"));
    NAOS_CHECK_FALSE(argx_lex_is_negative_number("---"));
    NAOS_CHECK_FALSE(argx_lex_is_negative_number("1"));
    NAOS_CHECK_FALSE(argx_lex_is_negative_number(".5"));
    NAOS_CHECK_FALSE(argx_lex_is_negative_number("x"));
    NAOS_CHECK_FALSE(argx_lex_is_negative_number("x-1"));
    NAOS_CHECK_FALSE(argx_lex_is_negative_number("+1"));
}

/*
 * A character that is not a digit right after the '-'. The characters
 * next to the range of the digits, and bytes outside ASCII, are not
 * digits.
 */
NAOS_TEST(negative_number_rejects_hyphen_and_non_digit)
{
    NAOS_CHECK_FALSE(argx_lex_is_negative_number("-x"));
    NAOS_CHECK_FALSE(argx_lex_is_negative_number("-xyz"));
    NAOS_CHECK_FALSE(argx_lex_is_negative_number("-x1"));
    NAOS_CHECK_FALSE(argx_lex_is_negative_number("-/")); /* before '0' */
    NAOS_CHECK_FALSE(argx_lex_is_negative_number("-:")); /* after '9' */
    NAOS_CHECK_FALSE(argx_lex_is_negative_number("-./"));
    NAOS_CHECK_FALSE(argx_lex_is_negative_number("-.:"));
    NAOS_CHECK_FALSE(argx_lex_is_negative_number("- 1"));
    NAOS_CHECK_FALSE(argx_lex_is_negative_number("-e1"));
    NAOS_CHECK_FALSE(argx_lex_is_negative_number("-\xB1")); /* '1' + 0x80 */
    NAOS_CHECK_FALSE(argx_lex_is_negative_number("-.\xB5"));
}

/*
 * Every argument with the form of a negative number is classified as a
 * cluster of short options.
 */
NAOS_TEST(negative_number_is_classified_as_short)
{
    const char *const args[] = { "-1", "-.5", "-1x", "-0", "-9.25", "-.0e1" };
    size_t i;

    for (i = 0; i < (sizeof args / sizeof args[0]); ++i)
    {
        NAOS_CHECK_TRUE(argx_lex_is_negative_number(args[i]));
        NAOS_CHECK_TRUE(argx_lex_classify(args[i]) == ARGX_LEX_SHORT);
    }
}

/* ---- The name and the value of a long option -------------------------- */

/* Without '=': the name runs to the end and there is no value. */
NAOS_TEST(split_long_without_equals)
{
    const char *arg = "--name";
    const char *name = NULL;
    size_t name_length = 99;
    const char *value = arg;

    argx_lex_split_long(arg, &name, &name_length, &value);

    NAOS_CHECK_TRUE(name == (arg + 2));
    NAOS_CHECK_UINT_EQ(name_length, 4);
    NAOS_CHECK_TRUE(value == NULL);
}

/* The shortest long option: one character after "--". */
NAOS_TEST(split_long_one_character)
{
    const char *arg = "--x";
    const char *name = NULL;
    size_t name_length = 99;
    const char *value = arg;

    argx_lex_split_long(arg, &name, &name_length, &value);

    NAOS_CHECK_TRUE(name == (arg + 2));
    NAOS_CHECK_UINT_EQ(name_length, 1);
    NAOS_CHECK_TRUE(value == NULL);
}

/* "---": the third '-' is the name. */
NAOS_TEST(split_long_triple_hyphen)
{
    const char *arg = "---";
    const char *name = NULL;
    size_t name_length = 99;
    const char *value = arg;

    argx_lex_split_long(arg, &name, &name_length, &value);

    NAOS_CHECK_TRUE(name == (arg + 2));
    NAOS_CHECK_UINT_EQ(name_length, 1);
    NAOS_CHECK_TRUE(value == NULL);
}

/* One '=': the name is before it, the value is right after it. */
NAOS_TEST(split_long_with_one_equals)
{
    const char *arg = "--name=value";
    const char *name = NULL;
    size_t name_length = 99;
    const char *value = NULL;

    argx_lex_split_long(arg, &name, &name_length, &value);

    NAOS_CHECK_TRUE(name == (arg + 2));
    NAOS_CHECK_UINT_EQ(name_length, 4);
    NAOS_CHECK_TRUE(value == (arg + 7));
    NAOS_CHECK_STR_EQ(value, "value");
}

/* Two '=': the first one splits, the second belongs to the value. */
NAOS_TEST(split_long_with_two_equals)
{
    const char *arg = "--name=a=b";
    const char *name = NULL;
    size_t name_length = 99;
    const char *value = NULL;

    argx_lex_split_long(arg, &name, &name_length, &value);

    NAOS_CHECK_TRUE(name == (arg + 2));
    NAOS_CHECK_UINT_EQ(name_length, 4);
    NAOS_CHECK_TRUE(value == (arg + 7));
    NAOS_CHECK_STR_EQ(value, "a=b");
}

/* A '=' at the end: the value is there, and it is empty. */
NAOS_TEST(split_long_with_empty_value)
{
    const char *arg = "--name=";
    const char *name = NULL;
    size_t name_length = 99;
    const char *value = NULL;

    argx_lex_split_long(arg, &name, &name_length, &value);

    NAOS_CHECK_TRUE(name == (arg + 2));
    NAOS_CHECK_UINT_EQ(name_length, 4);
    NAOS_CHECK_TRUE(value == (arg + 7));
    NAOS_CHECK_STR_EQ(value, "");
}

/* "--=x": the name is empty and the value is "x". */
NAOS_TEST(split_long_with_empty_name)
{
    const char *arg = "--=x";
    const char *name = NULL;
    size_t name_length = 99;
    const char *value = NULL;

    argx_lex_split_long(arg, &name, &name_length, &value);

    NAOS_CHECK_TRUE(name == (arg + 2));
    NAOS_CHECK_UINT_EQ(name_length, 0);
    NAOS_CHECK_TRUE(value == (arg + 3));
    NAOS_CHECK_STR_EQ(value, "x");
}

/*
 * The value is not looked at: a value that starts with '=' or '-', and
 * bytes outside ASCII in the name and in the value, are kept as written.
 */
NAOS_TEST(split_long_keeps_the_bytes)
{
    const char *arg = "--n\xC3\xA9=-\xC3\xA9 ";
    const char *name = NULL;
    size_t name_length = 99;
    const char *value = NULL;

    argx_lex_split_long(arg, &name, &name_length, &value);
    NAOS_CHECK_TRUE(name == (arg + 2));
    NAOS_CHECK_UINT_EQ(name_length, 3);
    NAOS_CHECK_STR_EQ(value, "-\xC3\xA9 ");

    arg = "--a==";
    argx_lex_split_long(arg, &name, &name_length, &value);
    NAOS_CHECK_TRUE(name == (arg + 2));
    NAOS_CHECK_UINT_EQ(name_length, 1);
    NAOS_CHECK_STR_EQ(value, "=");
}

NAOS_SUITE(classify,
    NAOS_ADD_TEST(classify_empty_is_plain),
    NAOS_ADD_TEST(classify_single_hyphen_is_plain),
    NAOS_ADD_TEST(classify_without_leading_hyphen_is_plain),
    NAOS_ADD_TEST(classify_double_hyphen_is_terminator),
    NAOS_ADD_TEST(classify_double_hyphen_and_more_is_long),
    NAOS_ADD_TEST(classify_hyphen_and_other_is_short)
)

NAOS_SUITE(negative_number,
    NAOS_ADD_TEST(negative_number_accepts_hyphen_and_digit),
    NAOS_ADD_TEST(negative_number_accepts_hyphen_dot_and_digit),
    NAOS_ADD_TEST(negative_number_rejects_dot_without_digit),
    NAOS_ADD_TEST(negative_number_rejects_other_shapes),
    NAOS_ADD_TEST(negative_number_rejects_hyphen_and_non_digit),
    NAOS_ADD_TEST(negative_number_is_classified_as_short)
)

NAOS_SUITE(split_long,
    NAOS_ADD_TEST(split_long_without_equals),
    NAOS_ADD_TEST(split_long_one_character),
    NAOS_ADD_TEST(split_long_triple_hyphen),
    NAOS_ADD_TEST(split_long_with_one_equals),
    NAOS_ADD_TEST(split_long_with_two_equals),
    NAOS_ADD_TEST(split_long_with_empty_value),
    NAOS_ADD_TEST(split_long_with_empty_name),
    NAOS_ADD_TEST(split_long_keeps_the_bytes)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(classify),
    NAOS_ADD_SUITE(negative_number),
    NAOS_ADD_SUITE(split_long)
)
