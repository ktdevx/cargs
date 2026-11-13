/*
 * CLArgs - help: the text that describes a command.
 *
 * This header holds the type of the write function, which is how the text
 * of a help message leaves the library: the library passes the text, part
 * by part, to a function of the caller, and the caller decides where it
 * goes. It also holds the help options, clargs_help_options_t: the
 * settings of the layout of the text, and the function that generates the
 * usage line of a command, clargs_help_write_usage().
 *
 * Ownership:
 *
 * - The caller creates the help options and destroys them with
 *   clargs_help_options_destroy(). They are independent of every schema.
 * - A generation borrows the schema and the help options for the call. It
 *   changes neither, keeps nothing allocated when it returns, and returns
 *   nothing that the caller has to free.
 *
 * Errors:
 *
 * - A generation returns a clargs_error_t. When the write function reports
 *   a failure, the generation stops and returns CLARGS_ERROR_WRITE_FAILED;
 *   the part of the text that was passed before stays where the write
 *   function put it.
 * - A NULL pointer where none is allowed and a schema that is not validated
 *   are errors of the program, not error codes: see the preconditions of
 *   each function.
 */
#ifndef CLARGS_HELP_H
#define CLARGS_HELP_H

#include <clargs/error.h>
#include <clargs/export.h>
#include <clargs/schema.h>

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

/*
 * Generates the usage line of command, such as
 *
 *     Usage: tool remote add [OPTIONS] --output <FILE> <input> [extra]...
 *
 * and passes it to write_fn, together with user_data. The text ends with
 * one '\n'. Its items, in this order:
 *
 * - "Usage: " and the path of command: the names of the commands from the
 *   root down to command, separated by one blank each.
 * - "[OPTIONS]", when command has an option that is not required: a flag,
 *   a negatable flag, a count, or an option with a value that is not
 *   required.
 * - Each required option of command, in the order in which the options
 *   were added: "--" and the long name, or "-" and the short name when the
 *   option has no long name, then a blank and the display name of the value
 *   between "<" and ">". When no display name is set, the name of the value
 *   type stands in its place.
 * - Each positional of command, in the order in which the positionals were
 *   added: "<name>" for one that takes exactly one argument, "[name]" for
 *   none or one, "[name]..." for none or more, "<name>..." for one or more.
 * - "[COMMAND]", when command has a subcommand.
 *
 * Two items are separated by one blank. When the item would then go beyond
 * the width of the help options, a newline and 7 blanks, the width of
 * "Usage: ", stand in the place of that blank: the usage line is broken
 * into several lines, and no line ends with a blank. An item is never
 * divided, also not at a blank inside it: an item that is wider than the
 * width is written as it is, on a line of its own, and the first item
 * always starts the text. The width of an item is the number of its bytes.
 * With the width 0 the items are all on one line.
 *
 * Only what command itself has is shown: not the options of its parent,
 * and nothing of its subcommands. The strings of the schema are written as
 * they are, byte by byte.
 *
 * options are the help options, or NULL for the default value of every
 * setting. The call does not change them.
 *
 * The function allocates no memory, does not change the schema and may be
 * called for the same schema and help options from several threads at the
 * same time.
 *
 * Returns CLARGS_ERROR_NONE on success, and CLARGS_ERROR_WRITE_FAILED when
 * write_fn returns false. write_fn is then not called again, and the part
 * of the text that it took before the failure stays where it put it: the
 * output may hold the start of the usage line.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 * - The schema that command belongs to is validated: see
 *   clargs_command_is_validated(). Checked with assert.
 * - write_fn is not NULL. Checked with assert.
 * - options may be NULL. If not, they have not been destroyed, and no other
 *   thread is changing them during the call. Not checked.
 * - user_data may be NULL.
 */
CLARGS_API clargs_error_t clargs_help_write_usage(const clargs_command_t *command,
                                                  const clargs_help_options_t *options,
                                                  clargs_help_write_fn_t write_fn, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* CLARGS_HELP_H */
