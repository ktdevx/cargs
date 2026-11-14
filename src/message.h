/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - internal declarations for the message of a parse error.
 *
 * A message is one line of English that can be shown to the user of the
 * program as it is. It has no line break, no prefix such as the name of the
 * program, and no full stop. The wording of each kind:
 *
 *     kind                    wording
 *     UNKNOWN_OPTION          unknown option '<name>'
 *     UNKNOWN_SUBCOMMAND      unknown command '<value>'
 *     MISSING_VALUE           option '<name>' requires a value
 *     UNEXPECTED_VALUE        option '<name>' does not take a value
 *     INVALID_VALUE           invalid value '<value>' for option '<name>' (expected <type>)
 *       for a positional      invalid value '<value>' for argument '<name>' (expected <type>)
 *       of a default value    "invalid default value" in place of "invalid value"
 *     MISSING_OPTION          missing required option '<name>'
 *     MISSING_POSITIONAL      missing required argument '<name>'
 *     UNEXPECTED_POSITIONAL   unexpected argument '<value>'
 *     INVALID_ENCODING        invalid character encoding in argument '<value>'
 *
 * <name> is name_prefix followed by the first name_length bytes of name,
 * <value> is value and <type> is type_name. The strings are put in as they
 * are: nothing is escaped, replaced or cut.
 *
 * The function knows neither the schema nor the result: it gets the kind
 * and the strings only.
 */
#ifndef ARGX_MESSAGE_H
#define ARGX_MESSAGE_H

#include <argx/error.h>
#include <argx/parse.h>

#include <stdbool.h>
#include <stddef.h>

/*
 * What a message is built from. The strings are only read during the
 * call.
 */
typedef struct argx_message_parts
{
    argx_parse_error_kind_t kind;
    argx_source_t source;    /* ARGX_SOURCE_DEFAULT: the wording of a default value */
    bool for_positional;     /* INVALID_VALUE: the wording of a positional argument */
    const char *name_prefix; /* "--", "-" or "". Never NULL */
    const char *name;        /* The name. NULL for a kind that has no <name> */
    size_t name_length;      /* The length of name, which need not end at a terminator */
    const char *value;       /* The value or the element. NULL for a kind that has no <value> */
    const char *type_name;   /* The name of the value type. Used by INVALID_VALUE only */
} argx_message_parts_t;

/*
 * Builds the message of a parse error from parts and writes it to *out, as
 * a string with a terminator in memory of the allocator that is set. The
 * message takes one allocation. source and for_positional are read for
 * ARGX_PARSE_ERROR_INVALID_VALUE only.
 *
 * The caller owns the message and frees it with argx_free()
 * ("alloc_internal.h").
 *
 * Returns ARGX_ERROR_NONE on success. Returns ARGX_ERROR_OUT_OF_MEMORY
 * when the allocation fails or the length of the message does not fit in
 * size_t; *out is NULL then and nothing is left allocated.
 *
 * Preconditions, checked with assert:
 *
 * - parts and out are not NULL.
 * - parts->kind is one of the kinds of the table above; it is not
 *   ARGX_PARSE_ERROR_NONE.
 * - The strings that the wording of the kind uses are not NULL:
 *   name_prefix and name for <name>, value for <value>, type_name for
 *   <type>.
 */
argx_error_t argx_message_build(const argx_message_parts_t *parts, char **out);

#endif /* ARGX_MESSAGE_H */
