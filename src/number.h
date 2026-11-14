/*
 * ArgX - internal declarations for the conversion of numbers.
 *
 * The conversion of a string to a signed 64-bit integer and to a finite
 * double. The whole string must be the number: nothing is skipped, neither
 * white space in front nor anything after the number.
 *
 * Neither function keeps state, prints anything or ends the process, and
 * both leave errno as it was before the call. The result does not depend on
 * the locale.
 */
#ifndef ARGX_NUMBER_H
#define ARGX_NUMBER_H

#include <argx/error.h>

#include <stdint.h>

/*
 * Converts text to a signed 64-bit integer. The only shape that is taken
 * is [+-]?[0-9]+: decimal digits, with one sign in front or none. INT64_MIN
 * and INT64_MAX are taken.
 *
 * On success, writes the integer to *out and returns ARGX_ERROR_NONE.
 * Returns ARGX_ERROR_INVALID_VALUE and leaves *out as it was when text is
 * empty, has a character that the shape does not allow (white space
 * included), or is a value outside the range of a signed 64-bit integer.
 *
 * Allocates nothing.
 *
 * Preconditions: text and out are not NULL. Both are checked with assert.
 */
argx_error_t argx_number_parse_int(const char *text, int64_t *out);

/*
 * Converts text to a finite double. The only shape that is taken is
 *
 *     [+-]? ( [0-9]+ ( "." [0-9]* )? | "." [0-9]+ ) ( [eE] [+-]? [0-9]+ )?
 *
 * the decimal notation, with an exponent or without. The decimal point is
 * '.', whatever the locale is. White space, the spellings of infinity and
 * of not-a-number, the hexadecimal notation and ',' are not taken.
 *
 * On success, writes the value to *out and returns ARGX_ERROR_NONE. An
 * input that is too close to 0 for a double succeeds with the value that
 * strtod gives for it. Otherwise leaves *out as it was and returns:
 *
 * - ARGX_ERROR_INVALID_VALUE when text does not have the shape, or when
 *   its value is beyond the finite range of a double.
 * - ARGX_ERROR_OUT_OF_MEMORY when the copy of a long input cannot be
 *   allocated. The value is computed from a copy of text; the copy of a
 *   short input needs no allocation, and the copy of a long one is freed
 *   before the function returns.
 *
 * Preconditions: text and out are not NULL. Both are checked with assert.
 */
argx_error_t argx_number_parse_float(const char *text, double *out);

#endif /* ARGX_NUMBER_H */
