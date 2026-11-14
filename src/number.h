/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - internal declarations for number conversion.
 *
 * Converts a string to a signed 64-bit integer or to a finite double. The
 * whole string must be the number. Nothing is skipped: neither leading
 * white space nor anything after the number.
 *
 * Neither function keeps state, prints anything or ends the process, and
 * both leave errno as it was before the call. The result does not depend
 * on the locale.
 */
#ifndef ARGX_NUMBER_H
#define ARGX_NUMBER_H

#include <argx/error.h>

#include <stdint.h>

/*
 * Converts text to a signed 64-bit integer. The only accepted form is
 * [+-]?[0-9]+: decimal digits with an optional leading sign. INT64_MIN and
 * INT64_MAX are accepted.
 *
 * On success, writes the integer to *out and returns ARGX_ERROR_NONE.
 * Returns ARGX_ERROR_INVALID_VALUE and leaves *out unchanged when text is
 * empty, contains a character the form does not allow (including white
 * space), or is outside the signed 64-bit integer range.
 *
 * Allocates nothing.
 *
 * Preconditions: text and out are not NULL. Both are checked with assert.
 */
argx_error_t argx_number_parse_int(const char *text, int64_t *out);

/*
 * Converts text to a finite double. The only accepted form is
 *
 *     [+-]? ( [0-9]+ ( "." [0-9]* )? | "." [0-9]+ ) ( [eE] [+-]? [0-9]+ )?
 *
 * that is, decimal notation with an optional exponent. The decimal point
 * is '.' in every locale. White space, spellings of infinity and
 * not-a-number, hexadecimal notation and ',' are not accepted.
 *
 * On success, writes the value to *out and returns ARGX_ERROR_NONE. An
 * input too close to 0 for a double succeeds with the value strtod gives
 * for it. Otherwise leaves *out unchanged and returns:
 *
 * - ARGX_ERROR_INVALID_VALUE when text does not match the form, or when
 *   its value is beyond the finite range of a double.
 * - ARGX_ERROR_OUT_OF_MEMORY when the copy of a long input cannot be
 *   allocated. The value is computed from a copy of text. A short input's
 *   copy needs no allocation, and a long input's copy is freed before the
 *   function returns.
 *
 * Preconditions: text and out are not NULL. Both are checked with assert.
 */
argx_error_t argx_number_parse_float(const char *text, double *out);

#endif /* ARGX_NUMBER_H */
