/*
 * CLArgs - help: the text that describes a command.
 *
 * This header holds the type of the write function, which is how the text
 * of a help message leaves the library: the library passes the text, part
 * by part, to a function of the caller, and the caller decides where it
 * goes.
 */
#ifndef CLARGS_HELP_H
#define CLARGS_HELP_H

#include <stdbool.h>
#include <stddef.h>

/*
 * The write function: a function of the caller that receives the text the
 * library generates.
 *
 * The contract of a call:
 *
 * - user_data is the pointer that the caller gave together with the write
 *   function. The library passes it on as it is and never reads what it
 *   points to; it may be NULL.
 * - text points to length bytes, one part of the text. The part is not
 *   terminated: there is no '\0' after it, so read exactly length bytes.
 * - length is 1 or more. The write function is never called with nothing
 *   to write.
 * - The calls come in order, from the start of the text: the parts, joined
 *   in the order of the calls, are the whole text.
 * - Into how many parts the text is divided, and where, is not specified
 *   and may change between versions. Do not rely on a part being a line or
 *   a word.
 * - text is valid only during the call. Copy the bytes to keep them.
 * - Return true when the part was written and false when it was not. After
 *   the write function has returned false it is not called again for that
 *   text: the generation stops and reports the failure. What was passed in
 *   the earlier calls stays where the write function put it.
 * - The write function must not change or destroy the schema that the text
 *   is generated from.
 */
typedef bool (*clargs_help_write_fn_t)(void *user_data, const char *text, size_t length);

#endif /* CLARGS_HELP_H */
