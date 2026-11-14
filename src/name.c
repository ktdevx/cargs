/*
 * SPDX-FileCopyrightText: 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - the rules of names.
 */
#include "name.h"

#include <assert.h>
#include <stddef.h>

/*
 * The first character of a name: an ASCII letter or digit. The comparisons
 * are with the ASCII ranges, not with <ctype.h>, which follows the locale.
 * A byte outside ASCII is in no range, whether char is signed or not.
 */
static bool is_name_start(char c)
{
    return ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9'));
}

/* A character after the first: also '-' and '_'. */
static bool is_name_continue(char c)
{
    return is_name_start(c) || (c == '-') || (c == '_');
}

/*
 * The rule that the long name and the subcommand name share: the first
 * character starts a name and all the others continue it. False for an
 * empty string.
 */
static bool is_name(const char *name)
{
    const char *p;

    if (!is_name_start(name[0]))
    {
        return false;
    }
    for (p = name + 1; *p != '\0'; ++p)
    {
        if (!is_name_continue(*p))
        {
            return false;
        }
    }
    return true;
}

bool argx_name_is_short(char name)
{
    return is_name_start(name);
}

bool argx_name_is_long(const char *name)
{
    assert(name != NULL);

    /*
     * is_name() saw a first character, so name[1] exists: it is the second
     * character or the terminator.
     */
    return is_name(name) && (name[1] != '\0');
}

bool argx_name_is_subcommand(const char *name)
{
    assert(name != NULL);

    return is_name(name);
}
