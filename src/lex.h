/*
 * CLArgs - internal declarations for the shape of an argument.
 *
 * An argument of the command line is classified by its shape alone: an
 * argument that is not an option, a long option, a cluster of short
 * options, or the terminator of the options. The functions do not know the
 * schema. They do not know either whether the argument is after the
 * terminator or was taken as the value of an option: the caller decides
 * that before it calls.
 *
 * The functions keep no state and allocate nothing. They compare the
 * characters with the ASCII ranges and do not use <ctype.h>, so the result
 * does not depend on the locale: a byte outside ASCII is never a digit.
 */
#ifndef CLARGS_LEX_H
#define CLARGS_LEX_H

#include <stdbool.h>
#include <stddef.h>

/* The shape of an argument. */
typedef enum clargs_lex_kind
{
    CLARGS_LEX_PLAIN,     /* not an option: "-", "" and an argument that does not start with '-' */
    CLARGS_LEX_LONG,      /* "--" and one character or more */
    CLARGS_LEX_SHORT,     /* '-' and one character or more, the first not '-' */
    CLARGS_LEX_TERMINATOR /* "--" alone */
} clargs_lex_kind_t;

/*
 * Returns the shape of arg. An argument with the form of a negative number
 * ("-1", "-.5") is CLARGS_LEX_SHORT: see clargs_lex_is_negative_number().
 *
 * Precondition: arg is not NULL. It is checked with assert.
 */
clargs_lex_kind_t clargs_lex_classify(const char *arg);

/*
 * Returns true when arg has the form of a negative number: '-' and an ASCII
 * digit, or '-', '.' and an ASCII digit. The characters after that digit
 * are not looked at, so "-1x" has the form too. Every argument for which
 * this returns true is CLARGS_LEX_SHORT for clargs_lex_classify().
 *
 * Precondition: arg is not NULL. It is checked with assert.
 */
bool clargs_lex_is_negative_number(const char *arg);

/*
 * Splits a long option into its name and its value. *name is set to the
 * character after "--", and *name_length to the length up to the first '='
 * (up to the end when there is no '='). *value is set to the character
 * after the first '=', or to NULL when there is no '='. The value may be
 * an empty string, and the name may have the length 0 ("--=x").
 *
 * *name and *value point into arg: nothing is copied, and the name is not
 * terminated at name_length.
 *
 * Preconditions: arg, name, name_length and value are not NULL. They are
 * checked with assert. arg is an argument for which clargs_lex_classify()
 * returned CLARGS_LEX_LONG; this is checked with assert as well.
 */
void clargs_lex_split_long(const char *arg, const char **name, size_t *name_length, const char **value);

#endif /* CLARGS_LEX_H */
