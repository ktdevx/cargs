/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - internal declarations for the shape of an argument.
 *
 * A command-line argument is classified by its shape alone: an argument
 * that is not an option, a long option, a bundle of short options, or the
 * terminator ("--"). The functions do not know the schema. Nor do they know
 * whether the argument comes after the terminator or was taken as an
 * option's value; the caller decides that before calling.
 *
 * The functions keep no state and allocate nothing. They compare
 * characters against ASCII ranges instead of using <ctype.h>, so the result
 * does not depend on the locale: a byte outside ASCII is never a digit.
 */
#ifndef ARGX_LEX_H
#define ARGX_LEX_H

#include <stdbool.h>
#include <stddef.h>

/* The shape of an argument. */
typedef enum argx_lex_kind
{
    ARGX_LEX_PLAIN,     /* not an option: "-", "" and any argument not starting with '-' */
    ARGX_LEX_LONG,      /* "--" followed by one or more characters */
    ARGX_LEX_SHORT,     /* '-' followed by one or more characters, the first not '-' */
    ARGX_LEX_TERMINATOR /* "--" alone */
} argx_lex_kind_t;

/*
 * Returns the shape of arg. An argument that looks like a negative number
 * ("-1", "-.5") is ARGX_LEX_SHORT; see argx_lex_is_negative_number().
 *
 * Precondition: arg is not NULL. It is checked with assert.
 */
argx_lex_kind_t argx_lex_classify(const char *arg);

/*
 * Returns true when arg has the form of a negative number: '-' followed by
 * an ASCII digit, or "-." followed by an ASCII digit. The characters after
 * that digit are not examined, so "-1x" has the form too.
 * argx_lex_classify() returns ARGX_LEX_SHORT for every argument for which
 * this returns true.
 *
 * Precondition: arg is not NULL. It is checked with assert.
 */
bool argx_lex_is_negative_number(const char *arg);

/*
 * Splits a long option into its name and value. *name is set to the
 * character after "--", and *name_length to the length up to the first '='
 * (or to the end when there is no '='). *value is set to the character
 * after the first '=', or to NULL when there is no '='. The value may be
 * an empty string, and the name may have length 0 ("--=x").
 *
 * *name and *value point into arg: nothing is copied, and the name is not
 * terminated at name_length.
 *
 * Preconditions: arg, name, name_length and value are not NULL. They are
 * checked with assert. arg is an argument for which argx_lex_classify()
 * returned ARGX_LEX_LONG; this is checked with assert as well.
 */
void argx_lex_split_long(const char *arg, const char **name, size_t *name_length, const char **value);

#endif /* ARGX_LEX_H */
