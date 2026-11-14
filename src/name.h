/*
 * SPDX-FileCopyrightText: 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - internal declarations for the rules of names.
 *
 * The rules of the names of options and subcommands. A name starts with an
 * ASCII letter or digit, and continues with ASCII letters, digits, '-' and
 * '_'. Letters of both cases are allowed, and the case is kept apart.
 *
 * The functions keep no state and allocate nothing. They compare the
 * characters with the ASCII ranges and do not use <ctype.h>, so the result
 * does not depend on the locale: a byte outside ASCII is never part of a
 * name.
 */
#ifndef ARGX_NAME_H
#define ARGX_NAME_H

#include <stdbool.h>

/*
 * Returns true when name is a short name of an option: one ASCII letter or
 * digit. Returns false for every other character, '-', '_' and '\0'
 * included.
 */
bool argx_name_is_short(char name);

/*
 * Returns true when name is a long name of an option: two characters or
 * more, the first an ASCII letter or digit, the others ASCII letters,
 * digits, '-' or '_'. Returns false for an empty string.
 *
 * Precondition: name is not NULL. It is checked with assert.
 */
bool argx_name_is_long(const char *name);

/*
 * Returns true when name is a name of a subcommand: one character or more,
 * with the same rule of characters as a long name. Returns false for an
 * empty string.
 *
 * Precondition: name is not NULL. It is checked with assert.
 */
bool argx_name_is_subcommand(const char *name);

#endif /* ARGX_NAME_H */
