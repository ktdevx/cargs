/*
 * SPDX-FileCopyrightText: 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - internal declarations for the conversion of a wide string to
 * UTF-8.
 *
 * How a wide string is read depends on the width of wchar_t, which is
 * decided at compile time by comparing WCHAR_MAX of <stdint.h>:
 *
 * - WCHAR_MAX <= 0xFFFF (MSVC): the string is UTF-16. A high surrogate
 *   (U+D800 to U+DBFF) followed by a low surrogate (U+DC00 to U+DFFF) is
 *   one code point. A surrogate that is not part of such a pair cannot be
 *   converted.
 * - otherwise (Linux, macOS): the string is UTF-32. A negative value, a
 *   value in the range of the surrogates (U+D800 to U+DFFF) and a value
 *   above U+10FFFF cannot be converted.
 *
 * Each unit that cannot be converted is replaced with U+FFFD (the three
 * bytes EF BF BD), and the conversion goes on with the next unit.
 *
 * The functions keep no state and allocate nothing: the caller asks for
 * the length first, prepares the bytes, and then has them written. They use
 * neither the API of the operating system nor the conversion functions of
 * the C standard library, so the result does not depend on the locale.
 */
#ifndef ARGX_UTF8_H
#define ARGX_UTF8_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Returns the number of bytes of text in UTF-8, without the terminator.
 * Each unit that cannot be converted counts as the three bytes of U+FFFD.
 *
 * *valid is always written, so the caller need not set it before the call:
 * true when every unit of text could be converted (an empty string too),
 * false when one unit or more was replaced with U+FFFD.
 *
 * The function does not check that the sum fits in size_t: a caller that
 * cannot rule that out by the length of text checks it itself.
 *
 * Preconditions: text and valid are not NULL. They are checked with assert.
 */
size_t argx_utf8_length(const wchar_t *text, bool *valid);

/*
 * Writes text in UTF-8 to out and terminates it with '\0'. Returns the
 * number of bytes written, without the terminator: the same number that
 * argx_utf8_length() returns for the same text. Each unit that cannot be
 * converted is written as U+FFFD; whether there was one is reported by
 * argx_utf8_length() only.
 *
 * Preconditions: text and out are not NULL. They are checked with assert.
 * out has argx_utf8_length(text, ...) + 1 bytes or more, and does not
 * overlap text; these are not checked.
 */
size_t argx_utf8_write(const wchar_t *text, char *out);

#endif /* ARGX_UTF8_H */
