/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - internal declarations for the name rules.
 *
 * The rules for option and subcommand names. A name starts with an ASCII
 * letter or digit and continues with ASCII letters, digits, '-' and '_'.
 * Letters of both cases are allowed, and names are case-sensitive.
 *
 * The functions keep no state and allocate nothing. They compare
 * characters against the ASCII ranges instead of using <ctype.h>, so the
 * result does not depend on the locale. A byte outside ASCII is never part
 * of a name.
 */
#ifndef ARGX_NAME_H
#define ARGX_NAME_H

#include <stdbool.h>

/*
 * Returns true when name is a valid option short name: one ASCII letter
 * or digit. Returns false for every other character, '-', '_' and '\0'
 * included.
 */
bool argx_name_is_short(char name);

/*
 * Returns true when name is a valid option long name: two or more
 * characters, the first an ASCII letter or digit, the rest ASCII letters,
 * digits, '-' or '_'. Returns false for an empty string.
 *
 * Precondition: name is not NULL. It is checked with assert.
 */
bool argx_name_is_long(const char *name);

/*
 * Returns true when name is a valid subcommand name: one or more
 * characters, following the same character rules as a long name. Returns
 * false for an empty string.
 *
 * Precondition: name is not NULL. It is checked with assert.
 */
bool argx_name_is_subcommand(const char *name);

#endif /* ARGX_NAME_H */
