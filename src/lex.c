/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - the shape of an argument.
 */
#include "lex.h"

#include <assert.h>

/*
 * Tests for an ASCII digit. It compares against the ASCII range instead of
 * using <ctype.h>, which follows the locale. A byte outside ASCII is not
 * in the range, whether char is signed or not.
 */
static bool is_digit(char c)
{
    return (c >= '0') && (c <= '9');
}

argx_lex_kind_t argx_lex_classify(const char *arg)
{
    assert(arg != NULL);

    /*
     * "" and any argument that does not start with '-'. The string's
     * terminating '\0' is read before any character after it.
     */
    if (arg[0] != '-')
    {
        return ARGX_LEX_PLAIN;
    }

    /* "-" alone. */
    if (arg[1] == '\0')
    {
        return ARGX_LEX_PLAIN;
    }

    if (arg[1] != '-')
    {
        return ARGX_LEX_SHORT;
    }

    return (arg[2] == '\0') ? ARGX_LEX_TERMINATOR : ARGX_LEX_LONG;
}

bool argx_lex_is_negative_number(const char *arg)
{
    assert(arg != NULL);

    if (arg[0] != '-')
    {
        return false;
    }

    /*
     * arg[1] exists: it is a character or the terminating '\0'. arg[2] is
     * read only once arg[1] is known to be '.'.
     */
    if (is_digit(arg[1]))
    {
        return true;
    }

    return (arg[1] == '.') && is_digit(arg[2]);
}

void argx_lex_split_long(const char *arg, const char **name, size_t *name_length, const char **value)
{
    const char *p;

    assert(arg != NULL);
    assert(name != NULL);
    assert(name_length != NULL);
    assert(value != NULL);
    assert(argx_lex_classify(arg) == ARGX_LEX_LONG);

    for (p = arg + 2; (*p != '\0') && (*p != '='); ++p)
    {
    }

    *name = arg + 2;
    *name_length = (size_t)(p - (arg + 2));
    *value = (*p == '=') ? (p + 1) : NULL;
}
