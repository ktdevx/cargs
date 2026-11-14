/*
 * Tests of the internal functions for the conversion of a wide string to
 * UTF-8: "utf8.h".
 *
 * The functions keep no state and allocate nothing, so the tests need no
 * allocator.
 *
 * A wide string is UTF-16 where WCHAR_MAX <= 0xFFFF and UTF-32 elsewhere.
 * The inputs that differ between the two are chosen with the same
 * condition as in the library, and every test runs checks in both.
 */
#include "utf8.h"

#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* The bytes of U+FFFD, which replaces a unit that cannot be converted. */
#define REPLACEMENT "\xEF\xBF\xBD"

/* The size of the output of check_convert(): enough for every test. */
#define OUT_SIZE ((size_t)64)

/*
 * A byte that the conversion never writes: it marks the bytes of the
 * output that were not written.
 */
#define UNTOUCHED '\x7E'

/*
 * The wide units of a code point above U+FFFF, for an initialiser: a pair
 * of surrogates in UTF-16, and one unit in UTF-32.
 */
#if WCHAR_MAX <= 0xFFFF
#define U10000 (wchar_t)0xD800, (wchar_t)0xDC00
#define U1F600 (wchar_t)0xD83D, (wchar_t)0xDE00
#define U10FFFF (wchar_t)0xDBFF, (wchar_t)0xDFFF
#else
#define U10000 (wchar_t)0x10000
#define U1F600 (wchar_t)0x1F600
#define U10FFFF (wchar_t)0x10FFFF
#endif

/*
 * text converts to the expected_length bytes of expected, and is valid or
 * not as expected. The two functions count the same number of bytes, the
 * output is terminated, and nothing is written after the terminator.
 */
static void check_convert(const wchar_t *text, const char *expected, size_t expected_length, bool expected_valid)
{
    char out[OUT_SIZE];
    bool valid = !expected_valid; /* the function must write it either way */
    size_t length;
    size_t written;
    size_t i;

    length = argx_utf8_length(text, &valid);
    NAOS_CHECK_UINT_EQ(length, expected_length);
    NAOS_CHECK_TRUE(valid == expected_valid);
    NAOS_REQUIRE_TRUE(length < OUT_SIZE);

    memset(out, UNTOUCHED, sizeof out);
    written = argx_utf8_write(text, out);
    NAOS_CHECK_UINT_EQ(written, length);
    NAOS_REQUIRE_TRUE(written < OUT_SIZE);
    NAOS_CHECK_MEM_EQ(out, written + 1, expected, expected_length + 1);
    for (i = written + 1; i < OUT_SIZE; ++i)
    {
        NAOS_CHECK_TRUE(out[i] == UNTOUCHED);
    }
}

/* text is valid and converts to the string expected. */
static void check_valid(const wchar_t *text, const char *expected)
{
    check_convert(text, expected, strlen(expected), true);
}

/* text is not valid and converts to the string expected. */
static void check_invalid(const wchar_t *text, const char *expected)
{
    check_convert(text, expected, strlen(expected), false);
}

/* ---- The boundaries of the lengths ------------------------------------ */

/*
 * An empty string is valid, has the length 0, and is written as the
 * terminator alone.
 */
NAOS_TEST(empty_string)
{
    static const wchar_t text[] = { 0 };

    check_valid(text, "");
}

/* Up to U+007F a code point is one byte. */
NAOS_TEST(one_byte_boundaries)
{
    static const wchar_t first[] = { (wchar_t)0x01, 0 };
    static const wchar_t last[] = { (wchar_t)0x7F, 0 };

    check_valid(L"a", "a");
    check_valid(L"--name=value", "--name=value");
    check_valid(first, "\x01");
    check_valid(last, "\x7F");
}

/* U+0080 to U+07FF is two bytes. */
NAOS_TEST(two_byte_boundaries)
{
    static const wchar_t first[] = { (wchar_t)0x80, 0 };
    static const wchar_t e_acute[] = { (wchar_t)0xE9, 0 };
    static const wchar_t last[] = { (wchar_t)0x7FF, 0 };

    check_valid(first, "\xC2\x80");
    check_valid(e_acute, "\xC3\xA9");
    check_valid(last, "\xDF\xBF");
}

/*
 * U+0800 to U+FFFF is three bytes. The code points next to the range of
 * the surrogates are valid.
 */
NAOS_TEST(three_byte_boundaries)
{
    static const wchar_t first[] = { (wchar_t)0x800, 0 };
    static const wchar_t hiragana_a[] = { (wchar_t)0x3042, 0 };
    static const wchar_t before_surrogates[] = { (wchar_t)0xD7FF, 0 };
    static const wchar_t after_surrogates[] = { (wchar_t)0xE000, 0 };
    static const wchar_t replacement[] = { (wchar_t)0xFFFD, 0 };
    static const wchar_t last[] = { (wchar_t)0xFFFF, 0 };

    check_valid(first, "\xE0\xA0\x80");
    check_valid(hiragana_a, "\xE3\x81\x82");
    check_valid(before_surrogates, "\xED\x9F\xBF");
    check_valid(after_surrogates, "\xEE\x80\x80");
    /* U+FFFD in the input is a character like any other: it is valid. */
    check_valid(replacement, REPLACEMENT);
    check_valid(last, "\xEF\xBF\xBF");
}

/*
 * U+10000 to U+10FFFF is four bytes. In UTF-16 each is a pair of
 * surrogates, which becomes one code point.
 */
NAOS_TEST(four_byte_boundaries)
{
    static const wchar_t first[] = { U10000, 0 };
    static const wchar_t emoji[] = { U1F600, 0 };
    static const wchar_t last[] = { U10FFFF, 0 };

    check_valid(first, "\xF0\x90\x80\x80");
    check_valid(emoji, "\xF0\x9F\x98\x80");
    check_valid(last, "\xF4\x8F\xBF\xBF");
}

/*
 * A string with code points of every length: the lengths add up, and the
 * order is kept.
 */
NAOS_TEST(mixed_lengths)
{
    static const wchar_t text[] = {
        (wchar_t)'a', (wchar_t)0xE9, (wchar_t)0x3042, U1F600, U10000, (wchar_t)'z', 0
    };

    check_valid(text, "a" "\xC3\xA9" "\xE3\x81\x82" "\xF0\x9F\x98\x80" "\xF0\x90\x80\x80" "z");
}

/* ---- The units that cannot be converted ------------------------------- */

/* valid is written for a valid string too: the caller need not set it. */
NAOS_TEST(valid_is_written_both_ways)
{
    static const wchar_t bad[] = { (wchar_t)0xDC00, 0 };
    bool valid = false;

    NAOS_CHECK_UINT_EQ(argx_utf8_length(L"abc", &valid), 3);
    NAOS_CHECK_TRUE(valid);

    valid = true;
    NAOS_CHECK_UINT_EQ(argx_utf8_length(bad, &valid), 3);
    NAOS_CHECK_FALSE(valid);
}

/*
 * A surrogate alone cannot be converted in either encoding: it becomes
 * U+FFFD, and the characters around it are kept.
 */
NAOS_TEST(lone_surrogate_is_replaced)
{
    static const wchar_t high[] = { (wchar_t)0xD800, 0 };
    static const wchar_t high_last[] = { (wchar_t)0xDBFF, 0 };
    static const wchar_t low[] = { (wchar_t)0xDC00, 0 };
    static const wchar_t low_last[] = { (wchar_t)0xDFFF, 0 };
    static const wchar_t high_between[] = { (wchar_t)'a', (wchar_t)0xD800, (wchar_t)'b', 0 };
    static const wchar_t low_between[] = { (wchar_t)'a', (wchar_t)0xDFFF, (wchar_t)'b', 0 };
    static const wchar_t high_at_end[] = { (wchar_t)'a', (wchar_t)0xDBFF, 0 };

    check_invalid(high, REPLACEMENT);
    check_invalid(high_last, REPLACEMENT);
    check_invalid(low, REPLACEMENT);
    check_invalid(low_last, REPLACEMENT);
    check_invalid(high_between, "a" REPLACEMENT "b");
    check_invalid(low_between, "a" REPLACEMENT "b");
    check_invalid(high_at_end, "a" REPLACEMENT);
}

/*
 * Every unit that cannot be converted is replaced on its own, and one
 * valid character after them does not make the string valid again.
 */
NAOS_TEST(each_bad_unit_is_replaced)
{
    /*
     * A low surrogate and then a high one: not a pair in UTF-16, and two
     * values in the range of the surrogates in UTF-32.
     */
    static const wchar_t reversed[] = { (wchar_t)0xDC00, (wchar_t)0xD800, (wchar_t)'x', 0 };
    static const wchar_t two_high[] = { (wchar_t)0xD800, (wchar_t)0xD800, (wchar_t)'x', 0 };

    check_invalid(reversed, REPLACEMENT REPLACEMENT "x");
    check_invalid(two_high, REPLACEMENT REPLACEMENT "x");
}

#if WCHAR_MAX <= 0xFFFF

/*
 * UTF-16: only a high surrogate directly followed by a low one is a pair.
 * A high surrogate before a pair is replaced, and the pair is kept.
 */
NAOS_TEST(encoding_specific_bad_units)
{
    static const wchar_t high_then_pair[] = { (wchar_t)0xD800, (wchar_t)0xD800, (wchar_t)0xDC00, 0 };
    static const wchar_t pair_then_low[] = { (wchar_t)0xD800, (wchar_t)0xDC00, (wchar_t)0xDC00, 0 };
    static const wchar_t high_then_bmp[] = { (wchar_t)0xD800, (wchar_t)0x3042, (wchar_t)0xDC00, 0 };

    check_invalid(high_then_pair, REPLACEMENT "\xF0\x90\x80\x80");
    check_invalid(pair_then_low, "\xF0\x90\x80\x80" REPLACEMENT);
    check_invalid(high_then_bmp, REPLACEMENT "\xE3\x81\x82" REPLACEMENT);
}

#else

/*
 * UTF-32: a pair of surrogates is not a code point, and a value above
 * U+10FFFF or below 0 cannot be converted.
 */
NAOS_TEST(encoding_specific_bad_units)
{
    static const wchar_t pair[] = { (wchar_t)0xD800, (wchar_t)0xDC00, 0 };
    static const wchar_t above[] = { (wchar_t)0x110000, 0 };
    static const wchar_t above_between[] = { (wchar_t)'a', (wchar_t)0x110000, (wchar_t)'b', 0 };
    static const wchar_t largest[] = { WCHAR_MAX, 0 };
#if WCHAR_MIN < 0
    static const wchar_t minus_one[] = { (wchar_t)-1, 0 };
    static const wchar_t smallest[] = { WCHAR_MIN, (wchar_t)'b', 0 };
#endif

    check_invalid(pair, REPLACEMENT REPLACEMENT);
    check_invalid(above, REPLACEMENT);
    check_invalid(above_between, "a" REPLACEMENT "b");
    check_invalid(largest, REPLACEMENT);
#if WCHAR_MIN < 0
    check_invalid(minus_one, REPLACEMENT);
    check_invalid(smallest, REPLACEMENT "b");
#endif
}

#endif

/* ---- The registration -------------------------------------------------- */

NAOS_SUITE(boundaries,
    NAOS_ADD_TEST(empty_string),
    NAOS_ADD_TEST(one_byte_boundaries),
    NAOS_ADD_TEST(two_byte_boundaries),
    NAOS_ADD_TEST(three_byte_boundaries),
    NAOS_ADD_TEST(four_byte_boundaries),
    NAOS_ADD_TEST(mixed_lengths)
)

NAOS_SUITE(bad_units,
    NAOS_ADD_TEST(valid_is_written_both_ways),
    NAOS_ADD_TEST(lone_surrogate_is_replaced),
    NAOS_ADD_TEST(each_bad_unit_is_replaced),
    NAOS_ADD_TEST(encoding_specific_bad_units)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(boundaries),
    NAOS_ADD_SUITE(bad_units)
)
