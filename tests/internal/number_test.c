/*
 * Tests of the internal functions for the conversion of numbers:
 * "number.h".
 *
 * The conversion of a floating-point number copies its input, and a long
 * input is copied into memory of the allocator. The tests use the test
 * allocator ("../support/test_allocator.h") to see that: its count of
 * requests shows whether a conversion allocated, its count of live blocks
 * shows that the copy is freed before the function returns, and its count
 * of violations shows a call that the library must never make.
 *
 * The state of the test allocator, the allocator that is set, errno and the
 * locale are shared by all the tests of this executable. Every test that
 * changes one of them puts it back before it ends.
 */
#include <cline/cline.h>

#include "number.h"

#include "../support/test_allocator.h"

#include <naos/naos.h>

#include <errno.h>
#include <float.h>
#include <locale.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * The value that an output holds before a conversion that must fail: the
 * conversion must leave it there.
 */
#define INT_SENTINEL INT64_C(-7777)
#define FLOAT_SENTINEL (-7777.25)

/*
 * The length of the long inputs. It is far more than the array that the
 * conversion of a floating-point number keeps for short inputs.
 */
#define LONG_LENGTH 300

/*
 * The end of the tests that install the test allocator: it has no live
 * block and saw no violation of the allocator contract.
 */
static void check_clean(void)
{
    NAOS_CHECK_UINT_EQ(cline_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_violations(), 0);
}

/* text is an integer and its value is expected. */
static void check_int(const char *text, int64_t expected)
{
    int64_t value = INT_SENTINEL;

    NAOS_CHECK_INT_EQ(cline_number_parse_int(text, &value), CLINE_ERROR_NONE);
    NAOS_CHECK_INT_EQ(value, expected);
}

/* text is not an integer, and the output is left as it was. */
static void check_int_invalid(const char *text)
{
    int64_t value = INT_SENTINEL;

    NAOS_CHECK_INT_EQ(cline_number_parse_int(text, &value), CLINE_ERROR_INVALID_VALUE);
    NAOS_CHECK_INT_EQ(value, INT_SENTINEL);
}

/* text is a floating-point number and its value is exactly expected. */
static void check_float(const char *text, double expected)
{
    double value = FLOAT_SENTINEL;

    NAOS_CHECK_INT_EQ(cline_number_parse_float(text, &value), CLINE_ERROR_NONE);
    NAOS_CHECK_TRUE(value == expected);
}

/*
 * text is not a finite floating-point number, and the output is left as it
 * was.
 */
static void check_float_invalid(const char *text)
{
    double value = FLOAT_SENTINEL;

    NAOS_CHECK_INT_EQ(cline_number_parse_float(text, &value), CLINE_ERROR_INVALID_VALUE);
    NAOS_CHECK_TRUE(value == FLOAT_SENTINEL);
}

/*
 * Writes a long number into buffer, which has LONG_LENGTH + 1 bytes:
 * "1.000...0005". Its value is a little more than 1.
 */
static void fill_long_fraction(char *buffer)
{
    memset(buffer, '0', LONG_LENGTH);
    buffer[0] = '1';
    buffer[1] = '.';
    buffer[LONG_LENGTH - 1] = '5';
    buffer[LONG_LENGTH] = '\0';
}

/* ---- The integer ------------------------------------------------------ */

/* Decimal digits, with one sign in front or none. */
NAOS_TEST(int_accepts_decimal_digits)
{
    check_int("0", 0);
    check_int("+0", 0);
    check_int("-0", 0);
    check_int("7", 7);
    check_int("42", 42);
    check_int("+42", 42);
    check_int("-42", -42);
    check_int("1234567890", INT64_C(1234567890));
    /* Zeros in front change nothing: the number is still decimal. */
    check_int("007", 7);
    check_int("-010", -10);
    check_int("00000000000000000000000000000000000001", 1);
}

/* The smallest and the largest value of a signed 64-bit integer. */
NAOS_TEST(int_accepts_the_limits)
{
    check_int("9223372036854775807", INT64_MAX);
    check_int("+9223372036854775807", INT64_MAX);
    check_int("-9223372036854775808", INT64_MIN);
    check_int("9223372036854775806", INT64_MAX - 1);
    check_int("-9223372036854775807", INT64_MIN + 1);
    check_int("-0000009223372036854775808", INT64_MIN);
}

/*
 * One more than the largest value, one less than the smallest, and far
 * beyond both.
 */
NAOS_TEST(int_rejects_values_out_of_range)
{
    check_int_invalid("9223372036854775808");
    check_int_invalid("+9223372036854775808");
    check_int_invalid("-9223372036854775809");
    check_int_invalid("9223372036854775810");
    check_int_invalid("-9223372036854775810");
    check_int_invalid("10000000000000000000");
    check_int_invalid("-10000000000000000000");
    check_int_invalid("18446744073709551616"); /* 2^64: wraps to 0 */
    check_int_invalid("18446744073709551617"); /* 2^64 + 1: wraps to 1 */
    check_int_invalid("99999999999999999999999999999999999999");
    check_int_invalid("-99999999999999999999999999999999999999");
}

/* No digit at all. */
NAOS_TEST(int_rejects_empty_and_sign_only)
{
    check_int_invalid("");
    check_int_invalid("+");
    check_int_invalid("-");
    check_int_invalid("+-");
    check_int_invalid("--1");
    check_int_invalid("++1");
    check_int_invalid("-+1");
    check_int_invalid("1-");
    check_int_invalid("1+1");
}

/* White space is not skipped, neither before nor after the digits. */
NAOS_TEST(int_rejects_white_space)
{
    check_int_invalid(" ");
    check_int_invalid(" 1");
    check_int_invalid("1 ");
    check_int_invalid(" 1 ");
    check_int_invalid("- 1");
    check_int_invalid("1 2");
    check_int_invalid("\t1");
    check_int_invalid("1\n");
    check_int_invalid("\n1");
    check_int_invalid("1\r");
    check_int_invalid("\v1");
    check_int_invalid("\f1");
}

/* Other notations of a number, and other characters. */
NAOS_TEST(int_rejects_other_characters)
{
    check_int_invalid("0x10");
    check_int_invalid("0X10");
    check_int_invalid("1.0");
    check_int_invalid("1.");
    check_int_invalid(".1");
    check_int_invalid("1e3");
    check_int_invalid("1,000");
    check_int_invalid("1_000");
    check_int_invalid("12a");
    check_int_invalid("a12");
    check_int_invalid("abc");
    check_int_invalid("1L");
    check_int_invalid("1u");
    check_int_invalid("/");     /* before '0' */
    check_int_invalid(":");     /* after '9' */
    check_int_invalid("1/");
    check_int_invalid("1:");
    check_int_invalid("\xB1");  /* '1' + 0x80 */
    check_int_invalid("1\xFF");
}

/*
 * The conversion of an integer allocates nothing, however long the input
 * is.
 */
NAOS_TEST(int_allocates_nothing)
{
    char text[LONG_LENGTH + 1];

    memset(text, '0', LONG_LENGTH);
    text[LONG_LENGTH - 1] = '9';
    text[LONG_LENGTH] = '\0';

    cline_test_allocator_install();

    check_int("123", 123);
    check_int(text, 9);
    check_int_invalid("9223372036854775808");
    text[0] = '1';
    check_int_invalid(text);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_requests(), 0);

    cline_test_allocator_uninstall();
    check_clean();
}

/* ---- The floating-point number ---------------------------------------- */

/*
 * The decimal notation: digits, with a '.' before, between or after them,
 * or with none.
 */
NAOS_TEST(float_accepts_decimal_notation)
{
    check_float("1", 1.0);
    check_float("1.", 1.0);
    check_float(".5", 0.5);
    check_float("1.5", 1.5);
    check_float("0", 0.0);
    check_float("0.0", 0.0);
    check_float("-0", 0.0);
    check_float("+1.5", 1.5);
    check_float("-1.5", -1.5);
    check_float("-.5", -0.5);
    check_float("+.5", 0.5);
    check_float("-1.", -1.0);
    check_float("007.25", 7.25);
    check_float("123456.75", 123456.75);
    check_float("9007199254740993", 9007199254740992.0); /* 2^53 + 1 */
}

/* The notation with an exponent. */
NAOS_TEST(float_accepts_exponent_notation)
{
    double value = FLOAT_SENTINEL;

    check_float("1E10", 1e10);
    check_float("1e10", 1e10);
    check_float("1e+10", 1e10);
    check_float("1e0", 1.0);
    check_float("1.e2", 100.0);
    check_float(".5e1", 5.0);
    check_float("2.5E-1", 0.25);
    check_float("1e007", 1e7);
    check_float("0e999", 0.0);

    NAOS_CHECK_INT_EQ(cline_number_parse_float("-1.5e-3", &value), CLINE_ERROR_NONE);
    NAOS_CHECK_DOUBLE_NEAR(value, -1.5e-3, 1e-15);

    /* Near the largest finite value. */
    value = FLOAT_SENTINEL;
    NAOS_CHECK_INT_EQ(cline_number_parse_float("1.7976931348623157e308", &value),
        CLINE_ERROR_NONE);
    NAOS_CHECK_TRUE(value == DBL_MAX);
    value = FLOAT_SENTINEL;
    NAOS_CHECK_INT_EQ(cline_number_parse_float("-1.7976931348623157e308", &value),
        CLINE_ERROR_NONE);
    NAOS_CHECK_TRUE(value == -DBL_MAX);
}

/*
 * An input that is too close to 0 for a double succeeds, with the value
 * that the standard conversion gives: 0 or a number next to it.
 */
NAOS_TEST(float_accepts_underflow)
{
    double value = FLOAT_SENTINEL;

    NAOS_CHECK_INT_EQ(cline_number_parse_float("1e-400", &value), CLINE_ERROR_NONE);
    NAOS_CHECK_TRUE(value >= 0.0);
    NAOS_CHECK_TRUE(value < DBL_MIN);

    value = FLOAT_SENTINEL;
    NAOS_CHECK_INT_EQ(cline_number_parse_float("-1e-400", &value), CLINE_ERROR_NONE);
    NAOS_CHECK_TRUE(value <= 0.0);
    NAOS_CHECK_TRUE(value > -DBL_MIN);
}

/* A value beyond the finite range fails: the result would be infinite. */
NAOS_TEST(float_rejects_values_out_of_range)
{
    check_float_invalid("1e400");
    check_float_invalid("-1e400");
    check_float_invalid("+1e400");
    check_float_invalid("1e309");
    check_float_invalid("1.8e308");
    check_float_invalid("1e99999999999999999999");
}

/* Not the shape of a number: no digit, or a part that is not complete. */
NAOS_TEST(float_rejects_incomplete_shapes)
{
    check_float_invalid("");
    check_float_invalid(".");
    check_float_invalid("+");
    check_float_invalid("-");
    check_float_invalid("+.");
    check_float_invalid("-.");
    check_float_invalid("e5");
    check_float_invalid(".e5");
    check_float_invalid("-e5");
    check_float_invalid("1e");
    check_float_invalid("1e+");
    check_float_invalid("1e-");
    check_float_invalid("1E");
    check_float_invalid("1.5e");
    check_float_invalid("1..5");
    check_float_invalid("1.5.2");
    check_float_invalid("1e5.2");
    check_float_invalid("1e5e2");
    check_float_invalid("1e.5");
    check_float_invalid("--1");
    check_float_invalid("+-1");
    check_float_invalid("1-");
    check_float_invalid("1e--5");
}

/*
 * The spellings of infinity and of not-a-number that the standard
 * conversion takes.
 */
NAOS_TEST(float_rejects_inf_and_nan)
{
    check_float_invalid("inf");
    check_float_invalid("INF");
    check_float_invalid("Inf");
    check_float_invalid("+inf");
    check_float_invalid("-inf");
    check_float_invalid("infinity");
    check_float_invalid("INFINITY");
    check_float_invalid("-infinity");
    check_float_invalid("nan");
    check_float_invalid("NAN");
    check_float_invalid("NaN");
    check_float_invalid("-nan");
    check_float_invalid("nan(1)");
    check_float_invalid("nan()");
}

/* The hexadecimal notation that the standard conversion takes. */
NAOS_TEST(float_rejects_hexadecimal)
{
    check_float_invalid("0x1p3");
    check_float_invalid("0X1P3");
    check_float_invalid("0x10");
    check_float_invalid("0x1.8p1");
    check_float_invalid("-0x1p3");
    check_float_invalid("1p3");
}

/*
 * ',' is neither a decimal point nor a separator, and other characters are
 * no part of a number.
 */
NAOS_TEST(float_rejects_other_characters)
{
    check_float_invalid("1,5");
    check_float_invalid(",5");
    check_float_invalid("1,");
    check_float_invalid("1,000.5");
    check_float_invalid("1_000");
    check_float_invalid("1.5f");
    check_float_invalid("1.5L");
    check_float_invalid("1d5");
    check_float_invalid("abc");
    check_float_invalid("1.5x");
    check_float_invalid("x1.5");
    check_float_invalid("/");
    check_float_invalid(":");
    check_float_invalid("1\xAE" "5"); /* '.' + 0x80 */
    check_float_invalid("\xB1");
}

/* White space is not skipped, neither before nor after the number. */
NAOS_TEST(float_rejects_white_space)
{
    check_float_invalid(" ");
    check_float_invalid(" 1.5");
    check_float_invalid("1.5 ");
    check_float_invalid(" 1.5 ");
    check_float_invalid("- 1.5");
    check_float_invalid("1 .5");
    check_float_invalid("1. 5");
    check_float_invalid("1e 5");
    check_float_invalid("\t1.5");
    check_float_invalid("1.5\n");
    check_float_invalid("\n1.5");
    check_float_invalid("1.5\r");
    check_float_invalid("\v1.5");
    check_float_invalid("\f1.5");
}

/* ---- The copy of the input -------------------------------------------- */

/* A short input is copied into an array of the function: no request. */
NAOS_TEST(float_short_input_allocates_nothing)
{
    cline_test_allocator_install();

    check_float("1.5", 1.5);
    check_float("-2.5e-1", -0.25);
    check_float_invalid("1e400");
    check_float_invalid("1,5");
    NAOS_CHECK_UINT_EQ(cline_test_allocator_requests(), 0);

    cline_test_allocator_uninstall();
    check_clean();
}

/*
 * A long input is copied into memory of the allocator, in one request, and
 * the copy is freed before the function returns.
 */
NAOS_TEST(float_long_input_is_copied_and_freed)
{
    char text[LONG_LENGTH + 1];
    double value = FLOAT_SENTINEL;

    fill_long_fraction(text);

    cline_test_allocator_install();

    NAOS_CHECK_INT_EQ(cline_number_parse_float(text, &value), CLINE_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_live(), 0);
    NAOS_CHECK_DOUBLE_NEAR(value, 1.0, 1e-9);

    /* Without a '.': 1 and LONG_LENGTH - 1 zeros, in the finite range. */
    memset(text, '0', LONG_LENGTH);
    text[0] = '1';
    value = FLOAT_SENTINEL;
    NAOS_CHECK_INT_EQ(cline_number_parse_float(text, &value), CLINE_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_requests(), 2);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_live(), 0);
    NAOS_CHECK_TRUE(value > 1e298);
    NAOS_CHECK_TRUE(value < 1e300);

    cline_test_allocator_uninstall();
    check_clean();
}

/* A long input beyond the finite range fails, and its copy is freed. */
NAOS_TEST(float_long_input_out_of_range_frees_the_copy)
{
    char text[LONG_LENGTH + 1];

    /*
     * 9 and LONG_LENGTH - 1 nines, then "e99" over the last digits: far
     * more than the largest finite value.
     */
    memset(text, '9', LONG_LENGTH);
    text[LONG_LENGTH - 3] = 'e';
    text[LONG_LENGTH] = '\0';

    cline_test_allocator_install();

    check_float_invalid(text);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_live(), 0);

    cline_test_allocator_uninstall();
    check_clean();
}

/*
 * The shape is checked before the copy is made: a long input that is not a
 * number fails without a request.
 */
NAOS_TEST(float_long_invalid_input_allocates_nothing)
{
    char text[LONG_LENGTH + 1];

    fill_long_fraction(text);

    cline_test_allocator_install();

    text[LONG_LENGTH - 1] = 'x';
    check_float_invalid(text);
    text[LONG_LENGTH - 1] = ' ';
    check_float_invalid(text);
    text[LONG_LENGTH - 1] = '5';
    text[0] = ' ';
    check_float_invalid(text);
    text[0] = '1';
    text[LONG_LENGTH / 2] = ',';
    check_float_invalid(text);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_requests(), 0);

    cline_test_allocator_uninstall();
    check_clean();
}

/*
 * When the allocation of the copy fails, the conversion reports that, not
 * an invalid value, and leaves the output as it was.
 */
NAOS_TEST(float_long_input_allocation_failure)
{
    char text[LONG_LENGTH + 1];
    double value = FLOAT_SENTINEL;

    fill_long_fraction(text);

    cline_test_allocator_install();
    cline_test_allocator_fail_at(1);

    NAOS_CHECK_INT_EQ(cline_number_parse_float(text, &value), CLINE_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_TRUE(value == FLOAT_SENTINEL);
    /* The request that failed is the only one. */
    NAOS_CHECK_UINT_EQ(cline_test_allocator_requests(), 1);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_violations(), 0);

    /* The same input converts once the allocation succeeds again. */
    NAOS_CHECK_INT_EQ(cline_number_parse_float(text, &value), CLINE_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(cline_test_allocator_requests(), 2);
    NAOS_CHECK_DOUBLE_NEAR(value, 1.0, 1e-9);

    cline_test_allocator_uninstall();
    check_clean();
}

/* ---- errno ------------------------------------------------------------ */

/*
 * Calls both conversions, with inputs that succeed, that are invalid and
 * that are out of range (where the standard conversion writes errno), and
 * checks after each that errno is still expected.
 */
static void check_errno_stays(int expected)
{
    char text[LONG_LENGTH + 1];
    int64_t int_value = 0;
    double float_value = 0.0;

    errno = expected;
    (void)cline_number_parse_int("42", &int_value);
    NAOS_CHECK_INT_EQ(errno, expected);
    errno = expected;
    (void)cline_number_parse_int("abc", &int_value);
    NAOS_CHECK_INT_EQ(errno, expected);
    errno = expected;
    (void)cline_number_parse_int("9223372036854775808", &int_value);
    NAOS_CHECK_INT_EQ(errno, expected);

    errno = expected;
    (void)cline_number_parse_float("1.5", &float_value);
    NAOS_CHECK_INT_EQ(errno, expected);
    errno = expected;
    (void)cline_number_parse_float("abc", &float_value);
    NAOS_CHECK_INT_EQ(errno, expected);
    errno = expected;
    NAOS_CHECK_INT_EQ(cline_number_parse_float("1e400", &float_value),
        CLINE_ERROR_INVALID_VALUE);
    NAOS_CHECK_INT_EQ(errno, expected);
    errno = expected;
    NAOS_CHECK_INT_EQ(cline_number_parse_float("1e-400", &float_value), CLINE_ERROR_NONE);
    NAOS_CHECK_INT_EQ(errno, expected);

    /* A long input: the copy comes from the allocator. */
    fill_long_fraction(text);
    errno = expected;
    NAOS_CHECK_INT_EQ(cline_number_parse_float(text, &float_value), CLINE_ERROR_NONE);
    NAOS_CHECK_INT_EQ(errno, expected);
}

/*
 * errno is as it was before the call: 0 stays 0, and a value that an
 * earlier call left stays too.
 */
NAOS_TEST(errno_is_unchanged)
{
    int saved = errno;

    check_errno_stays(0);
    check_errno_stays(EDOM);
    check_errno_stays(ERANGE);

    errno = saved;
}

/* errno is as it was when the allocation of the copy fails. */
NAOS_TEST(errno_is_unchanged_by_an_allocation_failure)
{
    char text[LONG_LENGTH + 1];
    double value = FLOAT_SENTINEL;
    int saved = errno;

    fill_long_fraction(text);

    cline_test_allocator_install();
    cline_test_allocator_fail_at(1);

    errno = EDOM;
    NAOS_CHECK_INT_EQ(cline_number_parse_float(text, &value), CLINE_ERROR_OUT_OF_MEMORY);
    NAOS_CHECK_INT_EQ(errno, EDOM);
    errno = saved;

    cline_test_allocator_uninstall();
    check_clean();
}

/* ---- The locale ------------------------------------------------------- */

/* Returns true when the current locale writes the decimal point as ','. */
static bool decimal_point_is_comma(void)
{
    char formatted[32];

    (void)snprintf(formatted, sizeof formatted, "%.1f", 0.5);
    return strcmp(formatted, "0,5") == 0;
}

/*
 * What every locale must give: '.' is the decimal point and ',' is not,
 * for a short input and for a long one.
 */
static void check_point_only(void)
{
    char text[LONG_LENGTH + 1];
    double value = FLOAT_SENTINEL;

    check_float("1.5", 1.5);
    check_float("-0.25", -0.25);
    check_float(".5", 0.5);
    check_float("1.", 1.0);
    check_float("2.5e1", 25.0);
    check_float("15", 15.0);
    check_float_invalid("1,5");
    check_float_invalid(",5");
    check_float_invalid("1,");
    check_float_invalid("2,5e1");
    check_float_invalid("1e400");

    fill_long_fraction(text);
    NAOS_CHECK_INT_EQ(cline_number_parse_float(text, &value), CLINE_ERROR_NONE);
    NAOS_CHECK_TRUE(value > (1.0 - 1e-9));
    NAOS_CHECK_TRUE(value < (1.0 + 1e-9));
    text[1] = ',';
    check_float_invalid(text);
}

/* In the "C" locale, '.' is the decimal point. */
NAOS_TEST(float_in_the_c_locale)
{
    NAOS_CHECK_FALSE(decimal_point_is_comma());
    check_point_only();
}

/*
 * In a locale whose decimal point is ',', the conversion still takes '.'
 * only and gives the same values.
 *
 * The names of such locales differ between the systems, and a system may
 * have none of them: the test tries the names in turn. When none can be
 * set, the part under the locale does not run; the checks in the locale
 * that the test started in run in every case.
 */
NAOS_TEST(float_in_a_locale_with_a_comma)
{
    static const char *const candidates[] = {
        "de_DE.UTF-8", "de_DE.utf8", "de_DE", "de_DE@euro", "de_DE.ISO8859-1",
        "fr_FR.UTF-8", "fr_FR.utf8", "fr_FR", "fr_FR.ISO8859-1",
        "it_IT.UTF-8", "es_ES.UTF-8", "nl_NL.UTF-8", "pt_BR.UTF-8", "ru_RU.UTF-8",
        "de-DE", "fr-FR", "German_Germany.1252", "French_France.1252", "German", "French"
    };
    char original[256] = "C";
    const char *current;
    bool switched = false;
    size_t i;

    /* setlocale returns a string that a later call may overwrite. */
    current = setlocale(LC_NUMERIC, NULL);
    if ((current != NULL) && (strlen(current) < sizeof original))
    {
        memcpy(original, current, strlen(current) + 1);
    }

    /* Before the switch. */
    check_point_only();

    for (i = 0; (i < (sizeof candidates / sizeof candidates[0])) && !switched; ++i)
    {
        if (setlocale(LC_NUMERIC, candidates[i]) != NULL)
        {
            if (decimal_point_is_comma())
            {
                switched = true;
            }
            else
            {
                (void)setlocale(LC_NUMERIC, original);
            }
        }
    }

    if (switched)
    {
        check_point_only();
        (void)setlocale(LC_NUMERIC, original);
    }

    /* After the locale is put back. */
    NAOS_CHECK_FALSE(decimal_point_is_comma());
    check_float("1.5", 1.5);
    check_float_invalid("1,5");
}

NAOS_SUITE(integer,
    NAOS_ADD_TEST(int_accepts_decimal_digits),
    NAOS_ADD_TEST(int_accepts_the_limits),
    NAOS_ADD_TEST(int_rejects_values_out_of_range),
    NAOS_ADD_TEST(int_rejects_empty_and_sign_only),
    NAOS_ADD_TEST(int_rejects_white_space),
    NAOS_ADD_TEST(int_rejects_other_characters),
    NAOS_ADD_TEST(int_allocates_nothing)
)

NAOS_SUITE(floating_point,
    NAOS_ADD_TEST(float_accepts_decimal_notation),
    NAOS_ADD_TEST(float_accepts_exponent_notation),
    NAOS_ADD_TEST(float_accepts_underflow),
    NAOS_ADD_TEST(float_rejects_values_out_of_range),
    NAOS_ADD_TEST(float_rejects_incomplete_shapes),
    NAOS_ADD_TEST(float_rejects_inf_and_nan),
    NAOS_ADD_TEST(float_rejects_hexadecimal),
    NAOS_ADD_TEST(float_rejects_other_characters),
    NAOS_ADD_TEST(float_rejects_white_space)
)

NAOS_SUITE(copy,
    NAOS_ADD_TEST(float_short_input_allocates_nothing),
    NAOS_ADD_TEST(float_long_input_is_copied_and_freed),
    NAOS_ADD_TEST(float_long_input_out_of_range_frees_the_copy),
    NAOS_ADD_TEST(float_long_invalid_input_allocates_nothing),
    NAOS_ADD_TEST(float_long_input_allocation_failure)
)

NAOS_SUITE(errno_value,
    NAOS_ADD_TEST(errno_is_unchanged),
    NAOS_ADD_TEST(errno_is_unchanged_by_an_allocation_failure)
)

NAOS_SUITE(locale,
    NAOS_ADD_TEST(float_in_the_c_locale),
    NAOS_ADD_TEST(float_in_a_locale_with_a_comma)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(integer),
    NAOS_ADD_SUITE(floating_point),
    NAOS_ADD_SUITE(copy),
    NAOS_ADD_SUITE(errno_value),
    NAOS_ADD_SUITE(locale)
)
