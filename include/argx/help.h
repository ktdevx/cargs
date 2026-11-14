/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - help: the text that describes a command.
 *
 * This header declares the write function type, which is how help text
 * leaves the library: the library passes the text, part by part, to a
 * function that the caller provides, and the caller decides where it
 * goes. It also declares the help options, argx_help_options_t, which
 * control the text layout, and the functions that generate a command's
 * help, argx_help_write(), and its usage line alone,
 * argx_help_write_usage(). argx_help_format() and
 * argx_help_format_usage() put the same texts into a caller's array as a
 * string, so that the caller needs no write function.
 *
 * Ownership:
 *
 * - The caller creates the help options and destroys them with
 *   argx_help_options_destroy(). They are independent of every schema.
 * - A generating function borrows the schema and the help options for the
 *   call. It changes neither, keeps nothing allocated when it returns, and
 *   returns nothing that the caller has to free.
 *
 * Errors:
 *
 * - The generating functions return an argx_error_t. When the write
 *   function reports a failure, generation stops and the function returns
 *   ARGX_ERROR_WRITE_FAILED. The text passed before the failure stays
 *   where the write function put it.
 * - When an allocation fails, argx_help_write() and argx_help_format()
 *   return ARGX_ERROR_OUT_OF_MEMORY. What argx_help_write() passed
 *   before stays where the write function put it; the array of
 *   argx_help_format() then holds the empty string. Generating the usage
 *   line alone needs no memory.
 * - Text that does not fit into the array of argx_help_format() or
 *   argx_help_format_usage() is truncated, and that is not an error: the
 *   functions report the length of the whole text.
 * - A NULL pointer where none is allowed and a schema that is not
 *   validated are programming errors, not error codes: see each
 *   function's preconditions.
 */
#ifndef ARGX_HELP_H
#define ARGX_HELP_H

#include <argx/error.h>
#include <argx/export.h>
#include <argx/schema.h>

#include <stdbool.h>
#include <stddef.h>

/*
 * The write function: a function that the caller provides to receive the
 * text that the library generates.
 *
 * Each call follows this contract:
 *
 * - user_data is the pointer that the caller passed along with the write
 *   function. The library passes it on unchanged and never reads what it
 *   points to; it may be NULL.
 * - text points to length bytes: one part of the text. The part is not
 *   terminated: there is no '\0' after it, so read exactly length bytes.
 * - length is at least 1. The write function is never called with nothing
 *   to write.
 * - The calls come in order, from the start of the text: the parts,
 *   joined in call order, form the whole text.
 * - How many parts the text is split into, and where, is unspecified and
 *   may change between versions. Do not rely on a part being a line or a
 *   word.
 * - text is valid only during the call. Copy the bytes to keep them.
 * - Return true if the part was written and false if it was not. Once the
 *   write function has returned false, it is not called again for that
 *   text: generation stops and reports the failure. What was passed in
 *   earlier calls stays where the write function put it.
 * - The write function must not change or destroy the schema that the
 *   text is generated from.
 */
typedef bool (*argx_help_write_fn_t)(void *user_data, const char *text, size_t length);

/*
 * The help options: the layout settings of a help text. The contents are
 * not public, so later versions can add settings, each with its own
 * setter function, and programs built against earlier versions keep
 * working.
 *
 * All settings are counted in columns, where one byte is one column:
 *
 * - the width: the column that no line goes beyond, as long as the line
 *   can be broken before it. The default is 80. 0 means no limit.
 * - the indent: the number of blanks before each list item. The default
 *   is 2.
 * - the maximum column: the last column at which item descriptions may
 *   start. The default is 32. Half the width (when the width is not 0)
 *   and the indent + 2 also bound it: see
 *   argx_help_options_set_max_column().
 *
 * No setting is greater than 65535: a greater value is clamped to 65535.
 */
typedef struct argx_help_options argx_help_options_t;

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Creates help options and writes them to *out. Every setting has its
 * default value: width 80, indent 2, maximum column 32.
 *
 * The caller owns the help options and destroys them with
 * argx_help_options_destroy().
 *
 * Returns ARGX_ERROR_NONE, or ARGX_ERROR_OUT_OF_MEMORY when the
 * allocation fails. When the call fails, *out is NULL and nothing is left
 * allocated.
 *
 * Preconditions:
 *
 * - out is not NULL. Checked with assert.
 */
ARGX_API argx_error_t argx_help_options_create(argx_help_options_t **out);

/*
 * Destroys help options and frees their memory. Does nothing for NULL.
 *
 * Preconditions:
 *
 * - options is NULL, or help options that argx_help_options_create()
 *   created and that have not been destroyed. Not checked.
 * - No other thread is using the help options. Not checked.
 */
ARGX_API void argx_help_options_destroy(argx_help_options_t *options);

/*
 * Sets the width, in columns. 0 means no limit: the text is then broken
 * only at newlines. A width greater than 65535 is clamped to 65535. The
 * default is 80.
 *
 * A width that is not 0 also limits the column at which item descriptions
 * start: see argx_help_options_set_max_column().
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
ARGX_API void argx_help_options_set_width(argx_help_options_t *options, size_t width);

/*
 * Sets the indent: the number of blanks before each list item. An indent
 * greater than 65535 is clamped to 65535. The default is 2.
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
ARGX_API void argx_help_options_set_indent(argx_help_options_t *options, size_t indent);

/*
 * Sets the maximum column: the last column at which the descriptions of
 * list items may start. A column greater than 65535 is clamped to 65535.
 * The default is 32.
 *
 * The limit that a help text uses is the smaller of the maximum column
 * and, when the width is not 0, half the width, rounded down; but it is
 * never less than the indent + 2. argx_help_write() describes how the
 * limit is used.
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
ARGX_API void argx_help_options_set_max_column(argx_help_options_t *options, size_t column);

/*
 * Generates command's usage line, such as
 *
 *     Usage: tool remote add [OPTIONS] --output <FILE> <input> [extra]...
 *
 * and passes it to write_fn, together with user_data. The text ends with
 * a '\n'. Its items, in order:
 *
 * - "Usage: " and the command path: the names of the commands from the
 *   root down to command, separated by single blanks.
 * - "[OPTIONS]", when command has an option that is not required: a flag,
 *   a negatable flag, a count, or an option with a value that is not
 *   required.
 * - Each required option of command, in the order the options were added:
 *   "--" and the long name, or "-" and the short name when the option has
 *   no long name, then a blank and the value name between "<" and ">".
 *   When no value name is set, the value type's name is used instead.
 * - Each positional of command, in the order the positionals were added:
 *   "<name>" for one that takes exactly one argument, "[name]" for zero
 *   or one, "[name]..." for zero or more, "<name>..." for one or more.
 * - "[COMMAND]", when command has a subcommand.
 *
 * Items are separated by one blank. When an item would then go beyond the
 * width in the help options, a newline and 7 blanks (the width of
 * "Usage: ") replace that blank: the usage line is broken into several
 * lines, and the library adds no blank at the end of a line. An item is
 * never split, not even at a blank inside it: an item wider than the
 * width is written as is, on a line of its own, and the first item always
 * starts the text. An item's width is its number of bytes. With width 0,
 * all items are on one line.
 *
 * This holds for names without a newline. A newline inside a positional
 * name, a value name, a user-defined value type's name or the root's name
 * is written as is, so the item itself spans more than one line. The item
 * still counts as one item with all its bytes, while the output column
 * starts again after the newline. The usage line can therefore break
 * before such an item even though the part before its newline would fit.
 * When the root's name starts with a newline, the blank of "Usage: " ends
 * the first line. Keep newlines out of names.
 *
 * Only what command itself has is shown: not its parent's options, and
 * nothing from its subcommands. The only schema strings the usage line
 * shows are names, including value names and value type names; they are
 * written as is, byte by byte.
 *
 * options are the help options, or NULL to use the default of every
 * setting. The call does not change them.
 *
 * The function allocates no memory, does not change the schema, and may be
 * called from several threads at the same time for the same schema and
 * help options.
 *
 * Returns ARGX_ERROR_NONE on success, and ARGX_ERROR_WRITE_FAILED when
 * write_fn returns false. write_fn is then not called again, and the text
 * it received before the failure stays where it put it: the output may
 * hold the start of the usage line.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 * - The schema that command belongs to is validated: see
 *   argx_command_is_validated(). Checked with assert.
 * - write_fn is not NULL. Checked with assert.
 * - options may be NULL. If not, they have not been destroyed, and no other
 *   thread is changing them during the call. Not checked.
 * - The schema is not changed or destroyed during the call, not even by
 *   write_fn. Not checked.
 * - user_data may be NULL.
 */
ARGX_API argx_error_t argx_help_write_usage(const argx_command_t *command,
                                            const argx_help_options_t *options,
                                            argx_help_write_fn_t write_fn, void *user_data);

/*
 * Generates command's help, such as
 *
 *     Usage: tool remote [COMMAND]
 *
 *     Manage the remote repositories.
 *
 *     Commands:
 *       add     Add a remote
 *       remove  Remove a remote
 *
 * and passes it to write_fn, together with user_data. command is the root
 * or a subcommand at any level. The text ends with a '\n', its lines are
 * separated by '\n' only, and the library adds no '\r' and no blank at the
 * end of a line. A '\r' in a schema string, a blank at the end of a name
 * and a blank before a newline inside a name are written as is. When the
 * root's name starts with a newline, the blank of "Usage: " ends the first
 * line. The parts of the help, in order:
 *
 * - The usage line: byte for byte the text of argx_help_write_usage() for
 *   the same command and help options.
 * - command's description, from the start of the line, without its
 *   trailing newlines.
 * - "Commands:" and, each on its own line, the subcommands of command
 *   itself, not their subcommands: the name and the first line of the
 *   description, that is, the part before its first newline.
 * - "Arguments:" and, each on its own line, command's positionals: the
 *   heading, written as in the usage line, the whole description and the
 *   annotations.
 * - "Options:" and, each on its own line, the options of command itself,
 *   not those of its parent or its subcommands: the heading, the whole
 *   description and the annotations.
 *
 * An option's heading starts with its names: "-s, --long" with both
 * names, "-s" with only a short name, and four blanks and "--long" with
 * only a long name, so that the long names line up. A negatable flag's
 * long name is written as "--[no-]long". A count has "..." right after the
 * names. An option with a value has, after the names, a blank and the
 * value name between "<" and ">", as in "-o, --output <FILE>"; when no
 * value name is set, the value type's name is used instead. Only the
 * schema's options are listed: none is added, and an option that the
 * schema defines for help, such as "--help", is listed like any other.
 *
 * The annotations of an option or positional follow its description, in
 * this order, each after one blank:
 *
 * - "[choices: a, b, c]": for an option with a value or a positional whose
 *   value type has choices. The choices appear in the order they were
 *   defined, separated by ", ".
 * - "[default: VALUE]": for an option with a value or a positional that
 *   has a default value. VALUE is the string that was set, not converted,
 *   and it is broken as part of the description: see below.
 *   "[default: on]": for a negatable flag whose initial state is on. One
 *   whose initial state is off has no annotation.
 * - "[required]": for a required option. A positional has none: its
 *   heading shows whether it is required.
 *
 * An item without a description shows its annotations in place of the
 * description. When the description ends with a newline, the annotations
 * start on the next line, without the blank; each additional trailing
 * newline puts an empty line before them. The trailing newlines of a
 * description without annotations are not written. In the run of blanks
 * and newlines at the end of a description, after its last other byte,
 * the blanks are ignored and the newlines are kept: "foo\n  " gives the
 * same text as "foo\n". A subcommand has no annotations.
 *
 * A description that contains nothing but blanks and newlines counts as
 * not set: an empty one, and one such as " \n". In the subcommand list, a
 * subcommand's description is judged by its first line alone: when that
 * line holds nothing but blanks, the subcommand counts as having no
 * description there.
 *
 * A part with nothing to show is left out, together with its title: a
 * description that is not set, a list without items. One empty line
 * separates two parts, and there is none at the start or the end of the
 * text. A description that starts with a newline or with a line of only
 * blanks keeps that newline or that line: this can add empty lines to the
 * text, or move the description or the annotations. List items appear in
 * the order they were added.
 *
 * An item starts with the indent from the help options. Its description
 * starts at the description column, which is the same for all the lists of
 * one help text: the indent + the width of the help's widest heading + 2,
 * or the column limit when that is less. The limit is the smaller of the
 * maximum column from the help options and, when the width is not 0, half
 * the width, rounded down; but it is never less than the indent + 2. A
 * heading's width is its number of bytes.
 *
 * The description starts on the heading's line when at least 2 blanks
 * stand between the heading and the description column. After a heading
 * that is too wide for that, the line ends, and the description starts on
 * the next line, at the description column:
 *
 *     Options:
 *       -v, --verbose                 Say more
 *           --a-rather-long-name-of-option <FILE>
 *                                     Write to FILE
 *
 * An item with neither a description nor annotations is a line with only
 * its heading, however wide the heading is, and no empty line follows it.
 *
 * Descriptions are broken at blanks so that no line goes beyond the width
 * in the help options; a word that is too long is not split. A newline in
 * a description ends the line, and the text after it starts at the column
 * where the description started. With width 0, lines end only at those
 * newlines. The annotations are broken as part of the description,
 * including at blanks inside an annotation.
 *
 * All of this assumes names without a newline. A newline inside a
 * positional name, a value name, a user-defined value type's name or the
 * root's name is written as is, while a heading still counts with all its
 * bytes: what is said here about columns and empty lines then need not
 * hold. Keep newlines out of names.
 *
 * The fixed words of the text are English. Schema strings are not
 * converted. Names, including value names and value type names, are
 * written as is, byte by byte. A description and its annotations,
 * including the default value and the choices, are broken as described
 * above, so the blanks at the end of their lines, those before a newline
 * inside them and those where they are broken are not written.
 *
 * options are the help options, or NULL to use the default of every
 * setting. The call does not change them.
 *
 * The function does not change the schema and keeps no state: the same
 * schema, command and help options always give the same text. Any memory
 * it allocates, with the allocator that is set, is freed before it
 * returns, whatever it returns. It may be called from several threads at
 * the same time for the same schema and help options.
 *
 * Returns ARGX_ERROR_NONE on success. On failure, generation stops,
 * write_fn is not called again, and the text it received before the
 * failure stays where it put it: the output may hold the start of the
 * help.
 *
 * - ARGX_ERROR_WRITE_FAILED: write_fn returned false.
 * - ARGX_ERROR_OUT_OF_MEMORY: an allocation failed. The text then ends
 *   before the line of the option or positional that the memory was for.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 * - The schema that command belongs to is validated: see
 *   argx_command_is_validated(). Checked with assert.
 * - write_fn is not NULL. Checked with assert.
 * - options may be NULL. If not, they have not been destroyed, and no other
 *   thread is changing them during the call. Not checked.
 * - The schema is not changed or destroyed during the call, not even by
 *   write_fn. Not checked.
 * - user_data may be NULL.
 */
ARGX_API argx_error_t argx_help_write(const argx_command_t *command,
                                      const argx_help_options_t *options,
                                      argx_help_write_fn_t write_fn, void *user_data);

/*
 * Generates command's help, byte for byte the text of argx_help_write()
 * for the same command and help options, and puts it into buffer, a
 * caller's array of size bytes, as a string.
 *
 * When size is not 0, the first bytes of the text, as many as fit but at
 * most size - 1, are written to buffer, followed immediately by a '\0':
 * buffer then holds a string. When the call succeeds, nothing is written
 * after that '\0'. Nothing is ever written outside the array. When size
 * is 0, nothing is written to buffer, and buffer may be NULL.
 *
 * When length is not NULL, *length is set to the length of the whole
 * text, not counting the '\0', whether or not the text fitted.
 *
 * Text that does not fit is truncated, and the call still succeeds. The
 * text was truncated when *length is size or more; truncated text may end
 * in the middle of a line. To get the whole text, call the function
 * twice: first with size 0 and buffer NULL to get the length, then with
 * an array of *length + 1 bytes.
 *
 * options are the help options, or NULL to use the default of every
 * setting. The call does not change them.
 *
 * The function does not change the schema and keeps no state. Any memory
 * it allocates, with the allocator that is set, is freed before it
 * returns, whatever it returns. It may be called from several threads at
 * the same time for the same schema and help options, each with its own
 * buffer.
 *
 * Returns ARGX_ERROR_NONE on success, including when the text was
 * truncated, and ARGX_ERROR_OUT_OF_MEMORY when an allocation fails. When
 * the call fails, buffer holds the empty string if size is not 0, and is
 * not written to if size is 0; *length, when length is not NULL, is 0.
 * The bytes of the array after the '\0' of that empty string are then
 * unspecified: they may hold part of the text generated before the
 * failure. The function never returns ARGX_ERROR_WRITE_FAILED.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 * - The schema that command belongs to is validated: see
 *   argx_command_is_validated(). Checked with assert.
 * - buffer is not NULL when size is not 0. Checked with assert. buffer may
 *   be NULL when size is 0.
 * - buffer points to an array of size bytes or more. Not checked.
 * - options may be NULL. If not, they have not been destroyed, and no other
 *   thread is changing them during the call. Not checked.
 * - The schema is not changed or destroyed during the call. Not checked.
 * - length may be NULL.
 */
ARGX_API argx_error_t argx_help_format(const argx_command_t *command,
                                       const argx_help_options_t *options, char *buffer,
                                       size_t size, size_t *length);

/*
 * Generates command's usage line, byte for byte the text of
 * argx_help_write_usage() for the same command and help options, and puts
 * it into buffer, a caller's array of size bytes, as a string.
 *
 * buffer, size and length work as in argx_help_format(). When size is not
 * 0, the first bytes of the text, at most size - 1, are written to buffer,
 * followed immediately by a '\0', and nothing else is written. When size
 * is 0, nothing is written, and buffer may be NULL. When length is not
 * NULL, *length is set to the length of the whole text, not counting the
 * '\0'. Text that does not fit is truncated, and the call still succeeds:
 * the text was truncated when *length is size or more.
 *
 * options are the help options, or NULL to use the default of every
 * setting. The call does not change them.
 *
 * The function allocates no memory, does not change the schema, and may be
 * called from several threads at the same time for the same schema and
 * help options, each with its own buffer.
 *
 * Returns ARGX_ERROR_NONE, including when the text was truncated. The
 * function cannot fail: buffer and *length are always set as described
 * above.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 * - The schema that command belongs to is validated: see
 *   argx_command_is_validated(). Checked with assert.
 * - buffer is not NULL when size is not 0. Checked with assert. buffer may
 *   be NULL when size is 0.
 * - buffer points to an array of size bytes or more. Not checked.
 * - options may be NULL. If not, they have not been destroyed, and no other
 *   thread is changing them during the call. Not checked.
 * - The schema is not changed or destroyed during the call. Not checked.
 * - length may be NULL.
 */
ARGX_API argx_error_t argx_help_format_usage(const argx_command_t *command,
                                             const argx_help_options_t *options,
                                             char *buffer, size_t size, size_t *length);

#ifdef __cplusplus
}
#endif

#endif /* ARGX_HELP_H */
