/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - the conversion of numbers.
 */
#include "number.h"

#include "alloc_internal.h"

#include <assert.h>
#include <errno.h>
#include <float.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * The size of the array that holds the copy of a short floating-point
 * input. A longer copy comes from the allocator.
 */
#define SHORT_COPY_SIZE 64

/*
 * The size of the array that holds the decimal point of the locale and its
 * terminator. A decimal point is one character, which may take several
 * bytes.
 */
#define DECIMAL_POINT_SIZE 16

/*
 * An ASCII digit. The comparison is with the ASCII range, not with
 * <ctype.h>, which follows the locale.
 */
static bool is_digit(char c)
{
    return (c >= '0') && (c <= '9');
}

argx_error_t argx_number_parse_int(const char *text, int64_t *out)
{
    const char *p;
    bool negative;
    int64_t value;

    assert(text != NULL);
    assert(out != NULL);

    p = text;
    negative = false;
    if ((*p == '+') || (*p == '-'))
    {
        negative = *p == '-';
        ++p;
    }
    if (!is_digit(*p))
    {
        return ARGX_ERROR_INVALID_VALUE;
    }

    /*
     * The value is built as a negative number: the negative range has one
     * value more than the positive one, so INT64_MIN is built like any
     * other value. Before each step, the check tells whether
     * value * 10 - digit would still be INT64_MIN or more. The division
     * rounds towards 0, which is upwards here: exactly the bound.
     */
    value = 0;
    for (; *p != '\0'; ++p)
    {
        int digit;

        if (!is_digit(*p))
        {
            return ARGX_ERROR_INVALID_VALUE;
        }
        digit = *p - '0';
        if (value < ((INT64_MIN + digit) / 10))
        {
            return ARGX_ERROR_INVALID_VALUE;
        }
        value = (value * 10) - digit;
    }

    if (!negative)
    {
        /* -INT64_MIN is one more than INT64_MAX. */
        if (value == INT64_MIN)
        {
            return ARGX_ERROR_INVALID_VALUE;
        }
        value = -value;
    }

    *out = value;
    return ARGX_ERROR_NONE;
}

/*
 * Returns true when text has the shape of a floating-point number:
 *
 *     [+-]? ( [0-9]+ ( "." [0-9]* )? | "." [0-9]+ ) ( [eE] [+-]? [0-9]+ )?
 *
 * Then *length is the length of text, and *point is the index of its '.',
 * or *length when it has none.
 */
static bool scan_float(const char *text, size_t *length, size_t *point)
{
    const char *p = text;
    const char *dot = NULL;
    bool has_digit = false;

    if ((*p == '+') || (*p == '-'))
    {
        ++p;
    }
    for (; is_digit(*p); ++p)
    {
        has_digit = true;
    }
    if (*p == '.')
    {
        dot = p;
        ++p;
        for (; is_digit(*p); ++p)
        {
            has_digit = true;
        }
    }
    /* A digit before the '.' or after it, or both. */
    if (!has_digit)
    {
        return false;
    }
    if ((*p == 'e') || (*p == 'E'))
    {
        ++p;
        if ((*p == '+') || (*p == '-'))
        {
            ++p;
        }
        if (!is_digit(*p))
        {
            return false;
        }
        for (; is_digit(*p); ++p)
        {
            /* The digits of the exponent. */
        }
    }
    if (*p != '\0')
    {
        return false;
    }

    *length = (size_t)(p - text);
    *point = (dot != NULL) ? (size_t)(dot - text) : *length;
    return true;
}

/*
 * Writes the decimal point of the current locale and a terminator to
 * buffer, which has DECIMAL_POINT_SIZE bytes, and returns its length.
 *
 * The decimal point is what stands between the two digits of 0.5 when it
 * is formatted. localeconv is not used: its result is a structure that
 * later calls may overwrite. If the formatted number is not a '0', a
 * decimal point and a '5', the decimal point is taken to be ".".
 */
static size_t decimal_point(char *buffer)
{
    char formatted[DECIMAL_POINT_SIZE + 2];
    int written;
    size_t length;

    written = snprintf(formatted, sizeof formatted, "%.1f", 0.5);
    if ((written < 3) || ((size_t)written >= sizeof formatted) ||
        (formatted[0] != '0') || (formatted[written - 1] != '5'))
    {
        buffer[0] = '.';
        buffer[1] = '\0';
        return 1;
    }

    /*
     * written is less than DECIMAL_POINT_SIZE + 2, so the decimal point and
     * its terminator fit.
     */
    length = (size_t)written - 2;
    memcpy(buffer, formatted + 1, length);
    buffer[length] = '\0';
    return length;
}

argx_error_t argx_number_parse_float(const char *text, double *out)
{
    char short_copy[SHORT_COPY_SIZE];
    char point_text[DECIMAL_POINT_SIZE];
    char *copy;
    char *end;
    size_t length;
    size_t point;
    size_t point_length;
    size_t copy_length;
    double value;
    bool complete;
    int saved_errno;

    assert(text != NULL);
    assert(out != NULL);

    if (!scan_float(text, &length, &point))
    {
        return ARGX_ERROR_INVALID_VALUE;
    }

    /* snprintf and strtod may write errno. */
    saved_errno = errno;

    /*
     * strtod reads the decimal point of the current locale, so it gets a
     * copy of the input with that in the place of the '.'. The length does
     * not overflow: the input is an object that exists, and the decimal
     * point adds a few bytes at most.
     */
    point_length = decimal_point(point_text);
    copy_length = (point < length) ? (length - 1 + point_length) : length;
    if (copy_length < sizeof short_copy)
    {
        copy = short_copy;
    }
    else
    {
        copy = argx_malloc(copy_length + 1);
        if (copy == NULL)
        {
            errno = saved_errno;
            return ARGX_ERROR_OUT_OF_MEMORY;
        }
    }

    memcpy(copy, text, point);
    if (point < length)
    {
        memcpy(copy + point, point_text, point_length);
        memcpy(copy + point + point_length, text + point + 1, length - point - 1);
    }
    copy[copy_length] = '\0';

    value = strtod(copy, &end);
    complete = end == (copy + copy_length);

    if (copy != short_copy)
    {
        argx_free(copy);
    }
    errno = saved_errno;

    /*
     * strtod read the whole copy, unless the locale changed between the
     * two calls above. A value beyond the finite range comes back as an
     * infinity (HUGE_VAL, with either sign).
     */
    if (!complete || (value > DBL_MAX) || (value < -DBL_MAX))
    {
        return ARGX_ERROR_INVALID_VALUE;
    }

    *out = value;
    return ARGX_ERROR_NONE;
}
