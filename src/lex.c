/*
 * CLine - the shape of an argument.
 */
#include "lex.h"

#include <assert.h>

/*
 * An ASCII digit. The comparison is with the ASCII range, not with
 * <ctype.h>, which follows the locale. A byte outside ASCII is not in the
 * range, whether char is signed or not.
 */
static bool is_digit(char c)
{
    return (c >= '0') && (c <= '9');
}

cline_lex_kind_t cline_lex_classify(const char *arg)
{
    assert(arg != NULL);

    /*
     * "", and an argument that does not start with '-'. The terminator of
     * the string is read before any character after it.
     */
    if (arg[0] != '-')
    {
        return CLINE_LEX_PLAIN;
    }
    /* "-" alone. */
    if (arg[1] == '\0')
    {
        return CLINE_LEX_PLAIN;
    }
    if (arg[1] != '-')
    {
        return CLINE_LEX_SHORT;
    }
    return (arg[2] == '\0') ? CLINE_LEX_TERMINATOR : CLINE_LEX_LONG;
}

bool cline_lex_is_negative_number(const char *arg)
{
    assert(arg != NULL);

    if (arg[0] != '-')
    {
        return false;
    }
    /*
     * arg[1] exists: it is a character or the terminator. arg[2] is read
     * only after arg[1] was seen to be '.'.
     */
    if (is_digit(arg[1]))
    {
        return true;
    }
    return (arg[1] == '.') && is_digit(arg[2]);
}

void cline_lex_split_long(const char *arg, const char **name, size_t *name_length, const char **value)
{
    const char *p;

    assert(arg != NULL);
    assert(name != NULL);
    assert(name_length != NULL);
    assert(value != NULL);
    assert(cline_lex_classify(arg) == CLINE_LEX_LONG);

    for (p = arg + 2; (*p != '\0') && (*p != '='); ++p)
    {
    }

    *name = arg + 2;
    *name_length = (size_t)(p - (arg + 2));
    *value = (*p == '=') ? (p + 1) : NULL;
}
