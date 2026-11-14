/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - internal declarations for parse error messages.
 *
 * A message is one line of English that can be shown to the program's
 * user as is. It has no line break, no prefix such as the program name,
 * and no full stop. The wording for each kind:
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
 * <value> is value, and <type> is type_name. The strings are inserted as
 * is: nothing is escaped, replaced or truncated.
 *
 * The function knows neither the schema nor the result. It receives only
 * the kind and the strings.
 */
#ifndef ARGX_MESSAGE_H
#define ARGX_MESSAGE_H

#include <argx/error.h>
#include <argx/parse.h>

#include <stdbool.h>
#include <stddef.h>

/*
 * The parts a message is built from. The strings are only read during the
 * call.
 */
typedef struct argx_message_parts
{
    argx_parse_error_kind_t kind;
    argx_source_t source;    /* ARGX_SOURCE_DEFAULT: default value wording */
    bool for_positional;     /* INVALID_VALUE: positional argument wording */
    const char *name_prefix; /* "--", "-" or "". Never NULL */
    const char *name;        /* The name. NULL for a kind that has no <name> */
    size_t name_length;      /* Length of name, which need not be terminated */
    const char *value;       /* The value or element. NULL for a kind that has no <value> */
    const char *type_name;   /* The value type name. Used by INVALID_VALUE only */
} argx_message_parts_t;

/*
 * Builds a parse error message from parts and writes it to *out as a
 * terminated string allocated with the current allocator. The message
 * takes one allocation. source and for_positional are read only for
 * ARGX_PARSE_ERROR_INVALID_VALUE.
 *
 * The caller owns the message and frees it with argx_free()
 * ("alloc_internal.h").
 *
 * Returns ARGX_ERROR_NONE on success. Returns ARGX_ERROR_OUT_OF_MEMORY
 * when the allocation fails or the message length does not fit in a
 * size_t. In that case *out is NULL and nothing is left allocated.
 *
 * Preconditions, checked with assert:
 *
 * - parts and out are not NULL.
 * - parts->kind is one of the kinds in the table above. It is not
 *   ARGX_PARSE_ERROR_NONE.
 * - The strings that the kind's wording uses are not NULL: name_prefix
 *   and name for <name>, value for <value>, type_name for <type>.
 */
argx_error_t argx_message_build(const argx_message_parts_t *parts, char **out);

#endif /* ARGX_MESSAGE_H */
