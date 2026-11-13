/*
 * CLArgs - help: the text that describes a command.
 *
 * This header holds the type of the write function, which is how the text
 * of a help message leaves the library: the library passes the text, part
 * by part, to a function of the caller, and the caller decides where it
 * goes. It also holds the help options, clargs_help_options_t: the
 * settings of the layout of the text.
 *
 * Ownership:
 *
 * - The caller creates the help options and destroys them with
 *   clargs_help_options_destroy(). They are independent of every schema.
 *
 * Errors:
 *
 * - A NULL pointer where none is allowed is an error of the program, not
 *   an error code: see the preconditions of each function.
 */
#ifndef CLARGS_HELP_H
#define CLARGS_HELP_H

#include <clargs/error.h>
#include <clargs/export.h>

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

/*
 * The help options: the settings of the layout of a help text. The contents
 * are not public, so later versions can add settings, each with a function
 * that sets it, and programs that were built before keep working.
 *
 * The settings, all counted in columns, where one byte is one column:
 *
 * - the width: the column that no line goes beyond when it can be broken
 *   before. The default is 80. 0 means that there is no limit.
 * - the indent: the blanks before each item of a list. The default is 2.
 * - the maximum column: the last column at which the descriptions of the
 *   items may start. The default is 32.
 *
 * No setting is greater than 65535: a greater value is kept as 65535.
 */
typedef struct clargs_help_options clargs_help_options_t;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Creates help options and writes them to *out. Every setting has its
 * default value: the width 80, the indent 2, the maximum column 32.
 *
 * The caller owns the help options and destroys them with
 * clargs_help_options_destroy().
 *
 * Returns CLARGS_ERROR_NONE, or CLARGS_ERROR_OUT_OF_MEMORY when the
 * allocation fails. When the call fails, *out is NULL and nothing is left
 * allocated.
 *
 * Preconditions:
 *
 * - out is not NULL. Checked with assert.
 */
CLARGS_API clargs_error_t clargs_help_options_create(clargs_help_options_t **out);

/*
 * Destroys help options and frees their memory. Does nothing for NULL.
 *
 * Preconditions:
 *
 * - options is NULL, or help options that clargs_help_options_create()
 *   created and that have not been destroyed. Not checked.
 * - No other thread is using the help options. Not checked.
 */
CLARGS_API void clargs_help_options_destroy(clargs_help_options_t *options);

/*
 * Sets the width, in columns. 0 means that there is no limit: the text is
 * then broken only where it has a newline. A width greater than 65535 is
 * kept as 65535. The default is 80.
 *
 * The function allocates no memory and cannot fail.
 *
 * Preconditions:
 *
 * - options is not NULL. Checked with assert.
 * - No other thread is using the help options during the call. Not
 *   checked. Help options that are not being changed may be used by
 *   several threads at the same time.
 */
CLARGS_API void clargs_help_options_set_width(clargs_help_options_t *options, size_t width);

/*
 * Sets the indent: the number of blanks before each item of a list. An
 * indent greater than 65535 is kept as 65535. The default is 2.
 *
 * The function allocates no memory and cannot fail.
 *
 * Preconditions:
 *
 * - options is not NULL. Checked with assert.
 * - No other thread is using the help options during the call. Not
 *   checked. Help options that are not being changed may be used by
 *   several threads at the same time.
 */
CLARGS_API void clargs_help_options_set_indent(clargs_help_options_t *options, size_t indent);

/*
 * Sets the maximum column: the last column at which the descriptions of
 * the items of a list may start. A column greater than 65535 is kept as
 * 65535. The default is 32.
 *
 * The function allocates no memory and cannot fail.
 *
 * Preconditions:
 *
 * - options is not NULL. Checked with assert.
 * - No other thread is using the help options during the call. Not
 *   checked. Help options that are not being changed may be used by
 *   several threads at the same time.
 */
CLARGS_API void clargs_help_options_set_max_column(clargs_help_options_t *options, size_t column);

#ifdef __cplusplus
}
#endif

#endif /* CLARGS_HELP_H */
