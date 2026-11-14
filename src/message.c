/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - parse error messages.
 */
#include "message.h"

#include "alloc_internal.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

/*
 * The most pieces a message can have. The invalid value wording has four
 * fixed texts, plus the value, the prefix, the name and the type name.
 */
#define MAX_PIECES 8

/* One piece of a message: length bytes at text, with no terminator. */
typedef struct piece
{
    const char *text;
    size_t length;
} piece_t;

/* The pieces of a message, in order. */
typedef struct pieces
{
    piece_t items[MAX_PIECES];
    size_t count;
} pieces_t;

/* Appends the length bytes at text. */
static void add_bytes(pieces_t *pieces, const char *text, size_t length)
{
    assert(pieces->count < MAX_PIECES);

    pieces->items[pieces->count].text = text;
    pieces->items[pieces->count].length = length;
    ++pieces->count;
}

/* Appends the string text, without its terminator. */
static void add_string(pieces_t *pieces, const char *text)
{
    assert(text != NULL);

    add_bytes(pieces, text, strlen(text));
}

/* Appends <name>: the prefix and the name. */
static void add_name(pieces_t *pieces, const argx_message_parts_t *parts)
{
    assert(parts->name != NULL);

    add_string(pieces, parts->name_prefix);
    add_bytes(pieces, parts->name, parts->name_length);
}

/* Appends a fixed text from the wording. */
#define ADD_TEXT(pieces, literal) add_bytes((pieces), (literal), sizeof(literal) - 1)

/*
 * Collects the pieces of the kind's wording. See the table in
 * "message.h".
 */
static void collect(pieces_t *pieces, const argx_message_parts_t *parts)
{
    pieces->count = 0;

    switch (parts->kind)
    {
    case ARGX_PARSE_ERROR_UNKNOWN_OPTION:
        ADD_TEXT(pieces, "unknown option '");
        add_name(pieces, parts);
        ADD_TEXT(pieces, "'");
        break;
    case ARGX_PARSE_ERROR_UNKNOWN_SUBCOMMAND:
        ADD_TEXT(pieces, "unknown command '");
        add_string(pieces, parts->value);
        ADD_TEXT(pieces, "'");
        break;
    case ARGX_PARSE_ERROR_MISSING_VALUE:
        ADD_TEXT(pieces, "option '");
        add_name(pieces, parts);
        ADD_TEXT(pieces, "' requires a value");
        break;
    case ARGX_PARSE_ERROR_UNEXPECTED_VALUE:
        ADD_TEXT(pieces, "option '");
        add_name(pieces, parts);
        ADD_TEXT(pieces, "' does not take a value");
        break;
    case ARGX_PARSE_ERROR_INVALID_VALUE:
        if (parts->source == ARGX_SOURCE_DEFAULT)
        {
            ADD_TEXT(pieces, "invalid default value '");
        }
        else
        {
            ADD_TEXT(pieces, "invalid value '");
        }
        add_string(pieces, parts->value);
        if (parts->for_positional)
        {
            ADD_TEXT(pieces, "' for argument '");
        }
        else
        {
            ADD_TEXT(pieces, "' for option '");
        }
        add_name(pieces, parts);
        ADD_TEXT(pieces, "' (expected ");
        add_string(pieces, parts->type_name);
        ADD_TEXT(pieces, ")");
        break;
    case ARGX_PARSE_ERROR_MISSING_OPTION:
        ADD_TEXT(pieces, "missing required option '");
        add_name(pieces, parts);
        ADD_TEXT(pieces, "'");
        break;
    case ARGX_PARSE_ERROR_MISSING_POSITIONAL:
        ADD_TEXT(pieces, "missing required argument '");
        add_name(pieces, parts);
        ADD_TEXT(pieces, "'");
        break;
    case ARGX_PARSE_ERROR_UNEXPECTED_POSITIONAL:
        ADD_TEXT(pieces, "unexpected argument '");
        add_string(pieces, parts->value);
        ADD_TEXT(pieces, "'");
        break;
    case ARGX_PARSE_ERROR_INVALID_ENCODING:
        ADD_TEXT(pieces, "invalid character encoding in argument '");
        add_string(pieces, parts->value);
        ADD_TEXT(pieces, "'");
        break;
    case ARGX_PARSE_ERROR_NONE:
    default:
        /* Ruled out by the precondition. */
        break;
    }
}

argx_error_t argx_message_build(const argx_message_parts_t *parts, char **out)
{
    pieces_t pieces;
    size_t length = 0;
    size_t i;
    char *message;
    char *end;

    assert(parts != NULL);
    assert(out != NULL);
    assert(parts->kind != ARGX_PARSE_ERROR_NONE);
    assert(parts->kind <= ARGX_PARSE_ERROR_INVALID_ENCODING);

    *out = NULL;

    /* Count first: the total length, then the terminator. */
    collect(&pieces, parts);
    for (i = 0; i < pieces.count; ++i)
    {
        if (pieces.items[i].length > (SIZE_MAX - length))
        {
            return ARGX_ERROR_OUT_OF_MEMORY;
        }
        length += pieces.items[i].length;
    }

    if (length == SIZE_MAX)
    {
        return ARGX_ERROR_OUT_OF_MEMORY;
    }

    message = argx_malloc(length + 1);
    if (message == NULL)
    {
        return ARGX_ERROR_OUT_OF_MEMORY;
    }

    end = message;
    for (i = 0; i < pieces.count; ++i)
    {
        if (pieces.items[i].length > 0)
        {
            memcpy(end, pieces.items[i].text, pieces.items[i].length);
            end += pieces.items[i].length;
        }
    }
    *end = '\0';

    *out = message;

    return ARGX_ERROR_NONE;
}
