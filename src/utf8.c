/*
 * CLArgs - the conversion of a wide string to UTF-8.
 */
#include "utf8.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

/* The largest number of bytes of one code point in UTF-8. */
#define UTF8_MAX_BYTES 4

/* The code point that replaces a unit that cannot be converted. */
#define REPLACEMENT 0xFFFDUL

/*
 * Reads one code point at text, which is not at the terminator. Returns the
 * number of units read: 1 or 2. A unit that cannot be converted is read
 * alone, *code_point is set to U+FFFD and *valid to false. *valid is not
 * written otherwise.
 *
 * A code point is held in an unsigned long, which has 32 bits or more.
 */
#if WCHAR_MAX <= 0xFFFF

/* UTF-16. */
static size_t decode(const wchar_t *text, unsigned long *code_point, bool *valid)
{
    unsigned long unit = (unsigned long)text[0];
    unsigned long next;

    if (unit < 0xD800UL || unit > 0xDFFFUL)
    {
        *code_point = unit;
        return 1;
    }
    if (unit <= 0xDBFFUL)
    {
        /*
         * A high surrogate. The next unit exists: it is a unit or the
         * terminator, and the terminator is not a low surrogate.
         */
        next = (unsigned long)text[1];
        if (next >= 0xDC00UL && next <= 0xDFFFUL)
        {
            *code_point = 0x10000UL + ((unit - 0xD800UL) << 10) + (next - 0xDC00UL);
            return 2;
        }
    }
    /*
     * A high surrogate without a low one after it, or a low surrogate
     * without a high one before it.
     */
    *code_point = REPLACEMENT;
    *valid = false;
    return 1;
}

#else

/* UTF-32. */
static size_t decode(const wchar_t *text, unsigned long *code_point, bool *valid)
{
    /*
     * Where wchar_t is signed, a negative value becomes a value above
     * U+10FFFF here: no comparison with 0 is needed, and none is written,
     * because it would be always false where wchar_t is unsigned.
     */
    unsigned long unit = (unsigned long)text[0];

    if (unit > 0x10FFFFUL || (unit >= 0xD800UL && unit <= 0xDFFFUL))
    {
        *code_point = REPLACEMENT;
        *valid = false;
        return 1;
    }
    *code_point = unit;
    return 1;
}

#endif

/*
 * Writes the bytes of code_point in UTF-8 to bytes and returns their
 * number: 1 to 4. code_point is U+10FFFF or less and not a surrogate, which
 * decode() guarantees.
 */
static size_t encode(unsigned long code_point, unsigned char bytes[UTF8_MAX_BYTES])
{
    if (code_point < 0x80UL)
    {
        bytes[0] = (unsigned char)code_point;
        return 1;
    }
    if (code_point < 0x800UL)
    {
        bytes[0] = (unsigned char)(0xC0UL | (code_point >> 6));
        bytes[1] = (unsigned char)(0x80UL | (code_point & 0x3FUL));
        return 2;
    }
    if (code_point < 0x10000UL)
    {
        bytes[0] = (unsigned char)(0xE0UL | (code_point >> 12));
        bytes[1] = (unsigned char)(0x80UL | ((code_point >> 6) & 0x3FUL));
        bytes[2] = (unsigned char)(0x80UL | (code_point & 0x3FUL));
        return 3;
    }
    bytes[0] = (unsigned char)(0xF0UL | (code_point >> 18));
    bytes[1] = (unsigned char)(0x80UL | ((code_point >> 12) & 0x3FUL));
    bytes[2] = (unsigned char)(0x80UL | ((code_point >> 6) & 0x3FUL));
    bytes[3] = (unsigned char)(0x80UL | (code_point & 0x3FUL));
    return 4;
}

/*
 * The two functions below walk the string with the same decode() and count
 * with the same encode(), so they count the same number of bytes.
 */

size_t clargs_utf8_length(const wchar_t *text, bool *valid)
{
    unsigned char bytes[UTF8_MAX_BYTES];
    unsigned long code_point;
    size_t length = 0;

    assert(text != NULL);
    assert(valid != NULL);

    *valid = true;
    while (*text != 0)
    {
        text += decode(text, &code_point, valid);
        length += encode(code_point, bytes);
    }
    return length;
}

size_t clargs_utf8_write(const wchar_t *text, char *out)
{
    unsigned char bytes[UTF8_MAX_BYTES];
    unsigned long code_point;
    size_t length = 0;
    size_t count;
    bool valid = true; /* reported by clargs_utf8_length() only */

    assert(text != NULL);
    assert(out != NULL);

    while (*text != 0)
    {
        text += decode(text, &code_point, &valid);
        count = encode(code_point, bytes);
        /*
         * The bytes are copied as they are: no conversion of a value above
         * 127 to char, which may be signed.
         */
        memcpy(out + length, bytes, count);
        length += count;
    }
    out[length] = '\0';
    return length;
}
