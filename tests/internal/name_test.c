/*
 * Tests of the internal functions for the rules of names: "name.h".
 *
 * The functions keep no state and allocate nothing, so the tests need no
 * allocator.
 */
#include "name.h"

#include <naos/naos.h>

#include <stddef.h>

/* ---- The short name --------------------------------------------------- */

/* Every ASCII letter, of either case, is a short name. */
NAOS_TEST(short_accepts_letters)
{
    const char letters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    size_t i;

    for (i = 0; i < sizeof letters - 1; ++i)
    {
        NAOS_CHECK_TRUE(clargs_name_is_short(letters[i]));
    }
    NAOS_CHECK_UINT_EQ(sizeof letters - 1, 52);
}

/* Every ASCII digit is a short name. */
NAOS_TEST(short_accepts_digits)
{
    const char digits[] = "0123456789";
    size_t i;

    for (i = 0; i < sizeof digits - 1; ++i)
    {
        NAOS_CHECK_TRUE(clargs_name_is_short(digits[i]));
    }
    NAOS_CHECK_UINT_EQ(sizeof digits - 1, 10);
}

/* '-' and '_' may continue a long name, but neither is a short name. */
NAOS_TEST(short_rejects_hyphen_and_underscore)
{
    NAOS_CHECK_FALSE(clargs_name_is_short('-'));
    NAOS_CHECK_FALSE(clargs_name_is_short('_'));
}

/*
 * Other printable characters, the space and the terminator ('\0' means
 * "no short name" to the callers) are not short names.
 */
NAOS_TEST(short_rejects_other_characters)
{
    NAOS_CHECK_FALSE(clargs_name_is_short('?'));
    NAOS_CHECK_FALSE(clargs_name_is_short(' '));
    NAOS_CHECK_FALSE(clargs_name_is_short('\0'));
    NAOS_CHECK_FALSE(clargs_name_is_short('='));
    NAOS_CHECK_FALSE(clargs_name_is_short('\t'));
}

/* The characters next to the ranges of the letters and the digits. */
NAOS_TEST(short_rejects_the_neighbours_of_the_ranges)
{
    NAOS_CHECK_FALSE(clargs_name_is_short('/')); /* before '0' */
    NAOS_CHECK_FALSE(clargs_name_is_short(':')); /* after '9' */
    NAOS_CHECK_FALSE(clargs_name_is_short('@')); /* before 'A' */
    NAOS_CHECK_FALSE(clargs_name_is_short('[')); /* after 'Z' */
    NAOS_CHECK_FALSE(clargs_name_is_short('`')); /* before 'a' */
    NAOS_CHECK_FALSE(clargs_name_is_short('{')); /* after 'z' */
}

/*
 * A byte outside ASCII is not a short name, whether char is signed or
 * not, and whatever the locale calls a letter.
 */
NAOS_TEST(short_rejects_bytes_outside_ascii)
{
    NAOS_CHECK_FALSE(clargs_name_is_short('\x80'));
    NAOS_CHECK_FALSE(clargs_name_is_short('\xC1')); /* 'A' + 0x80 */
    NAOS_CHECK_FALSE(clargs_name_is_short('\xE9')); /* e acute in Latin-1 */
    NAOS_CHECK_FALSE(clargs_name_is_short('\xFF'));
    NAOS_CHECK_FALSE(clargs_name_is_short((char)0x7F));
}

/* ---- The long name ---------------------------------------------------- */

/* A long name has two characters or more. */
NAOS_TEST(long_rejects_empty_and_one_character)
{
    NAOS_CHECK_FALSE(clargs_name_is_long(""));
    NAOS_CHECK_FALSE(clargs_name_is_long("a"));
    NAOS_CHECK_FALSE(clargs_name_is_long("7"));
    NAOS_CHECK_FALSE(clargs_name_is_long("-"));
}

NAOS_TEST(long_accepts_two_characters_and_more)
{
    NAOS_CHECK_TRUE(clargs_name_is_long("ab"));
    NAOS_CHECK_TRUE(clargs_name_is_long("a1"));
    NAOS_CHECK_TRUE(clargs_name_is_long("1a"));
    NAOS_CHECK_TRUE(clargs_name_is_long("42"));
    NAOS_CHECK_TRUE(clargs_name_is_long("All"));
    NAOS_CHECK_TRUE(clargs_name_is_long("output"));
    NAOS_CHECK_TRUE(clargs_name_is_long("azAZ09"));
}

/* The first character is a letter or a digit. */
NAOS_TEST(long_rejects_leading_hyphen_or_underscore)
{
    NAOS_CHECK_FALSE(clargs_name_is_long("-a"));
    NAOS_CHECK_FALSE(clargs_name_is_long("_a"));
    NAOS_CHECK_FALSE(clargs_name_is_long("--"));
    NAOS_CHECK_FALSE(clargs_name_is_long("-verbose"));
    NAOS_CHECK_FALSE(clargs_name_is_long("_verbose"));
}

/* After the first character, '-' and '_' are allowed, at the end too. */
NAOS_TEST(long_accepts_inner_hyphen_and_underscore)
{
    NAOS_CHECK_TRUE(clargs_name_is_long("dry-run"));
    NAOS_CHECK_TRUE(clargs_name_is_long("dry_run"));
    NAOS_CHECK_TRUE(clargs_name_is_long("a-"));
    NAOS_CHECK_TRUE(clargs_name_is_long("a_"));
    NAOS_CHECK_TRUE(clargs_name_is_long("a--b__c"));
    NAOS_CHECK_TRUE(clargs_name_is_long("no-color"));
}

NAOS_TEST(long_rejects_space_and_equals)
{
    NAOS_CHECK_FALSE(clargs_name_is_long("dry run"));
    NAOS_CHECK_FALSE(clargs_name_is_long(" ab"));
    NAOS_CHECK_FALSE(clargs_name_is_long("ab "));
    NAOS_CHECK_FALSE(clargs_name_is_long("key=value"));
    NAOS_CHECK_FALSE(clargs_name_is_long("=ab"));
    NAOS_CHECK_FALSE(clargs_name_is_long("ab="));
    NAOS_CHECK_FALSE(clargs_name_is_long("a?"));
    NAOS_CHECK_FALSE(clargs_name_is_long("a.b"));
}

/*
 * Bytes outside ASCII are rejected at the start, in the middle and at the
 * end.
 */
NAOS_TEST(long_rejects_bytes_outside_ascii)
{
    NAOS_CHECK_FALSE(clargs_name_is_long("\xC3\xA9t\xC3\xA9")); /* UTF-8 */
    NAOS_CHECK_FALSE(clargs_name_is_long("caf\xC3\xA9"));
    NAOS_CHECK_FALSE(clargs_name_is_long("a\xE9" "b")); /* Latin-1 */
    NAOS_CHECK_FALSE(clargs_name_is_long("ab\xFF"));
    NAOS_CHECK_FALSE(clargs_name_is_long("\x80" "ab"));
}

/* ---- The subcommand name ---------------------------------------------- */

NAOS_TEST(subcommand_rejects_empty)
{
    NAOS_CHECK_FALSE(clargs_name_is_subcommand(""));
}

/* Unlike a long name, one character is enough. */
NAOS_TEST(subcommand_accepts_one_character)
{
    NAOS_CHECK_TRUE(clargs_name_is_subcommand("a"));
    NAOS_CHECK_TRUE(clargs_name_is_subcommand("Z"));
    NAOS_CHECK_TRUE(clargs_name_is_subcommand("7"));
    NAOS_CHECK_FALSE(clargs_name_is_subcommand("-"));
    NAOS_CHECK_FALSE(clargs_name_is_subcommand("_"));
    NAOS_CHECK_FALSE(clargs_name_is_subcommand(" "));
    NAOS_CHECK_FALSE(clargs_name_is_subcommand("\xE9"));
}

/* The same rule of characters as the long name. */
NAOS_TEST(subcommand_follows_the_rule_of_the_long_name)
{
    NAOS_CHECK_TRUE(clargs_name_is_subcommand("add"));
    NAOS_CHECK_TRUE(clargs_name_is_subcommand("cherry-pick"));
    NAOS_CHECK_TRUE(clargs_name_is_subcommand("merge_base"));
    NAOS_CHECK_TRUE(clargs_name_is_subcommand("a-"));
    NAOS_CHECK_FALSE(clargs_name_is_subcommand("-add"));
    NAOS_CHECK_FALSE(clargs_name_is_subcommand("_add"));
    NAOS_CHECK_FALSE(clargs_name_is_subcommand("re mote"));
    NAOS_CHECK_FALSE(clargs_name_is_subcommand("a=b"));
    NAOS_CHECK_FALSE(clargs_name_is_subcommand("caf\xC3\xA9"));
}

NAOS_SUITE(short_name,
    NAOS_ADD_TEST(short_accepts_letters),
    NAOS_ADD_TEST(short_accepts_digits),
    NAOS_ADD_TEST(short_rejects_hyphen_and_underscore),
    NAOS_ADD_TEST(short_rejects_other_characters),
    NAOS_ADD_TEST(short_rejects_the_neighbours_of_the_ranges),
    NAOS_ADD_TEST(short_rejects_bytes_outside_ascii)
)

NAOS_SUITE(long_name,
    NAOS_ADD_TEST(long_rejects_empty_and_one_character),
    NAOS_ADD_TEST(long_accepts_two_characters_and_more),
    NAOS_ADD_TEST(long_rejects_leading_hyphen_or_underscore),
    NAOS_ADD_TEST(long_accepts_inner_hyphen_and_underscore),
    NAOS_ADD_TEST(long_rejects_space_and_equals),
    NAOS_ADD_TEST(long_rejects_bytes_outside_ascii)
)

NAOS_SUITE(subcommand_name,
    NAOS_ADD_TEST(subcommand_rejects_empty),
    NAOS_ADD_TEST(subcommand_accepts_one_character),
    NAOS_ADD_TEST(subcommand_follows_the_rule_of_the_long_name)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(short_name),
    NAOS_ADD_SUITE(long_name),
    NAOS_ADD_SUITE(subcommand_name)
)
