/*
 * CLArgs - the schema: the commands, options and positionals that a program
 * takes on its command line.
 *
 * A schema is a tree of commands. The root command stands for the program;
 * each command can have subcommands, to any depth. A command also has
 * options and positionals. The schema only tells what the command line
 * looks like: it knows nothing of an argv.
 *
 * Ownership:
 *
 * - The root command owns the whole tree: every subcommand, option and
 *   positional and every string in it. clargs_command_destroy() of the root
 *   frees all of it; nothing else in the tree is destroyed on its own.
 * - Every string that is passed in is copied. The caller may change or free
 *   its string after the call.
 * - The handles and the strings that the functions return are valid until
 *   the root is destroyed. The caller does not free them. A handle does not
 *   change when more is added to the tree.
 *
 * Errors:
 *
 * - A function that adds or sets something returns a clargs_error_t. When
 *   it fails, the schema is exactly as it was before the call, and an
 *   output is NULL.
 * - A function that only reads takes a const handle, changes nothing and
 *   cannot fail. As long as the schema is not being changed, such functions
 *   may be called from several threads at the same time. Changing one
 *   schema from several threads at the same time is not supported.
 * - A NULL pointer where none is allowed and an index that is out of range
 *   are errors of the program, not error codes: see the preconditions of
 *   each function.
 *
 * Names are compared byte by byte: upper and lower case are different.
 */
#ifndef CLARGS_SCHEMA_H
#define CLARGS_SCHEMA_H

#include <clargs/error.h>
#include <clargs/export.h>
#include <clargs/type.h>

#include <stdbool.h>
#include <stddef.h>

/* A command: the root of a schema, or a subcommand. The contents are not
 * public. */
typedef struct clargs_command clargs_command_t;

/* An option of a command. The contents are not public. */
typedef struct clargs_option clargs_option_t;

/* A positional of a command. The contents are not public. */
typedef struct clargs_positional clargs_positional_t;

/*
 * The kind of an option.
 *
 * Later versions may add kinds. Write code that may receive a value it does
 * not know: give a switch over clargs_option_kind_t a default label.
 */
typedef enum clargs_option_kind {
    CLARGS_OPTION_FLAG,      /* Whether it was given. */
    CLARGS_OPTION_NEGATABLE, /* Switched with --name and --no-name. */
    CLARGS_OPTION_COUNT,     /* How many times it was given. */
    CLARGS_OPTION_VALUE      /* Takes one value. */
} clargs_option_kind_t;

/*
 * How many arguments a positional takes.
 *
 * Later versions may add kinds. Write code that may receive a value it does
 * not know: give a switch over clargs_arity_t a default label.
 */
typedef enum clargs_arity {
    CLARGS_ARITY_REQUIRED,     /* Exactly one. */
    CLARGS_ARITY_OPTIONAL,     /* None or one. */
    CLARGS_ARITY_ZERO_OR_MORE, /* None or more. */
    CLARGS_ARITY_ONE_OR_MORE   /* One or more. */
} clargs_arity_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Commands ----------------------------------------------------------- */

/*
 * Creates a root command with the name name and writes it to *out. The new
 * command is empty: it has no description, no option, no positional and no
 * subcommand.
 *
 * The name is copied. Any string that is not empty is the name of a root;
 * the rule of the names of subcommands does not apply to it.
 *
 * The caller owns the root and destroys it with clargs_command_destroy().
 *
 * Returns CLARGS_ERROR_NONE on success. On failure, nothing is created,
 * nothing stays allocated, and *out is NULL:
 *
 * - CLARGS_ERROR_INVALID_NAME: name is the empty string.
 * - CLARGS_ERROR_OUT_OF_MEMORY: an allocation failed.
 *
 * Preconditions:
 *
 * - name is not NULL. Checked with assert.
 * - out is not NULL. Checked with assert.
 */
CLARGS_API clargs_error_t clargs_command_create(const char *name, clargs_command_t **out);

/*
 * Destroys a root command and everything below it: its subcommands, to any
 * depth, the options, the positionals and all the strings. Every handle and
 * every string that was read from the tree is invalid afterwards. Does
 * nothing for NULL.
 *
 * The value types that the tree uses are not destroyed; they belong to the
 * caller.
 *
 * The depth of the recursion is the depth of the tree.
 *
 * Preconditions:
 *
 * - root is NULL, or a root that clargs_command_create() created and that
 *   has not been destroyed. A subcommand is never passed: it is destroyed
 *   with its root. That root is not a subcommand is checked with assert.
 */
CLARGS_API void clargs_command_destroy(clargs_command_t *root);

/*
 * Adds a subcommand with the name name to command, after the subcommands
 * that command has already, and writes it to *out if out is not NULL. The
 * new subcommand is empty, like a new root, and can get subcommands of its
 * own: the depth has no limit.
 *
 * The name is copied. It has one character or more; the first is an ASCII
 * letter or digit, the others are ASCII letters, digits, '-' or '_'.
 *
 * The subcommand belongs to the tree and is destroyed with the root.
 *
 * Returns CLARGS_ERROR_NONE on success. On failure, the tree is as it was
 * before the call, nothing of the call stays allocated, and *out is NULL.
 * The conditions are checked in this order:
 *
 * - CLARGS_ERROR_INVALID_NAME: name does not follow the rule; the empty
 *   string does not.
 * - CLARGS_ERROR_INVALID_DEFINITION: command has a positional. A command
 *   has positionals or subcommands, not both.
 * - CLARGS_ERROR_NAME_CONFLICT: command has a subcommand of the same name
 *   already. The same name can be used in another command.
 * - CLARGS_ERROR_OUT_OF_MEMORY: an allocation failed.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 * - name is not NULL. Checked with assert.
 * - out may be NULL, when the handle is not needed.
 */
CLARGS_API clargs_error_t clargs_command_add_subcommand(clargs_command_t *command, const char *name,
                                                        clargs_command_t **out);

/*
 * Sets the description of command to a copy of text. A description that
 * was set before is replaced. The empty string can be set; it is not the
 * same as no description.
 *
 * Returns CLARGS_ERROR_NONE on success, and CLARGS_ERROR_OUT_OF_MEMORY when
 * the allocation fails; the command then keeps the description it had.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 * - text is not NULL. Checked with assert.
 */
CLARGS_API clargs_error_t clargs_command_set_description(clargs_command_t *command, const char *text);

/*
 * Returns the name of command. Never NULL and never empty.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 */
CLARGS_API const char *clargs_command_name(const clargs_command_t *command);

/*
 * Returns the description of command, or NULL when none has been set.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 */
CLARGS_API const char *clargs_command_description(const clargs_command_t *command);

/*
 * Returns the command that command is a subcommand of, or NULL when command
 * is a root.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 */
CLARGS_API const clargs_command_t *clargs_command_parent(const clargs_command_t *command);

/*
 * Return the number of the options, positionals and subcommands of command
 * itself (not of its subcommands), and the one at index, counted from 0.
 * The order is the order in which they were added.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 * - For the functions that take an index: index is less than the matching
 *   number. Checked with assert.
 */
CLARGS_API size_t clargs_command_option_count(const clargs_command_t *command);
CLARGS_API const clargs_option_t *clargs_command_option_at(const clargs_command_t *command, size_t index);
CLARGS_API size_t clargs_command_positional_count(const clargs_command_t *command);
CLARGS_API const clargs_positional_t *clargs_command_positional_at(const clargs_command_t *command,
                                                                   size_t index);
CLARGS_API size_t clargs_command_subcommand_count(const clargs_command_t *command);
CLARGS_API const clargs_command_t *clargs_command_subcommand_at(const clargs_command_t *command,
                                                                size_t index);

/*
 * Returns the option of command whose short name is name, or NULL when
 * there is none. Only the options of command itself are looked at, not
 * those of its parent or of its subcommands. Upper and lower case are
 * different. For '\0', which is not a name, the result is NULL.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 */
CLARGS_API const clargs_option_t *clargs_command_find_short(const clargs_command_t *command, char name);

/*
 * Returns the option of command whose long name, or whose negated name
 * ("no-" and the long name, for a negatable flag), is the first length
 * bytes of name, or NULL when there is none. The string at name does not
 * have to end after length bytes, so a name can be looked up in place in a
 * longer string such as "name=value". The whole name is compared, byte by
 * byte: upper and lower case are different, and neither a part of a name
 * nor a longer string matches.
 *
 * Only the options of command itself are looked at, not those of its parent
 * or of its subcommands.
 *
 * When an option is found and negated is not NULL, *negated is false if
 * the long name matched and true if the negated name matched. When no
 * option is found, *negated is not written.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 * - name is not NULL and points to at least length bytes. That it is not
 *   NULL is checked with assert.
 * - negated may be NULL.
 */
CLARGS_API const clargs_option_t *clargs_command_find_long(const clargs_command_t *command,
                                                           const char *name, size_t length,
                                                           bool *negated);

/*
 * Returns the subcommand of command whose name is name, or NULL when there
 * is none. Only the subcommands of command itself are looked at, not those
 * of its subcommands. The whole name is compared, byte by byte: upper and
 * lower case are different.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 * - name is not NULL. Checked with assert.
 */
CLARGS_API const clargs_command_t *clargs_command_find_subcommand(const clargs_command_t *command,
                                                                  const char *name);

/* ---- Options ------------------------------------------------------------ */

/*
 * Add an option to command, after the options that command has already,
 * and write it to *out if out is not NULL. There is one function for each
 * kind of option:
 *
 * - clargs_command_add_flag(): a flag, CLARGS_OPTION_FLAG.
 * - clargs_command_add_negatable(): a negatable flag,
 *   CLARGS_OPTION_NEGATABLE. Its negated name, "no-" and the long name, is
 *   made here; a long name "color" gives the negated name "no-color".
 * - clargs_command_add_count(): a count, CLARGS_OPTION_COUNT.
 * - clargs_command_add_value(): an option that takes one value,
 *   CLARGS_OPTION_VALUE. It is the only one with a value type, type. The
 *   value type is borrowed, not copied: the caller keeps it valid until the
 *   root of the tree is destroyed, and destroys it afterwards.
 *
 * An option has a short name, a long name, or both. short_name is '\0' for
 * an option without a short name, and long_name is NULL for an option
 * without a long name. A short name is one ASCII letter or digit. A long
 * name has two characters or more; the first is an ASCII letter or digit,
 * the others are ASCII letters, digits, '-' or '_'. The long name is
 * copied. A negatable flag always has a long name.
 *
 * The option belongs to the tree and is destroyed with the root.
 *
 * Return CLARGS_ERROR_NONE on success. On failure, the tree is as it was
 * before the call, nothing of the call stays allocated, and *out is NULL.
 * The conditions are checked in this order:
 *
 * - CLARGS_ERROR_INVALID_NAME: there is neither a short name nor a long
 *   name.
 * - CLARGS_ERROR_INVALID_NAME: a negatable flag has no long name.
 * - CLARGS_ERROR_INVALID_NAME: the short name or the long name does not
 *   follow the rule; the empty string is not a long name.
 * - CLARGS_ERROR_NAME_CONFLICT: command has an option with the same short
 *   name already.
 * - CLARGS_ERROR_NAME_CONFLICT: command has an option whose long name or
 *   negated name is the long name.
 * - CLARGS_ERROR_NAME_CONFLICT: for a negatable flag, command has an option
 *   whose long name or negated name is the negated name.
 * - CLARGS_ERROR_OUT_OF_MEMORY: an allocation failed.
 *
 * The same names can be used in another command, also in the parent and in
 * a subcommand of command.
 *
 * Preconditions:
 *
 * - command is not NULL. Checked with assert.
 * - short_name may be '\0' and long_name may be NULL, as described above.
 * - For clargs_command_add_value(): type is not NULL (checked with assert)
 *   and is a value type that has not been destroyed.
 * - out may be NULL, when the handle is not needed.
 */
CLARGS_API clargs_error_t clargs_command_add_flag(clargs_command_t *command, char short_name,
                                                  const char *long_name, clargs_option_t **out);
CLARGS_API clargs_error_t clargs_command_add_negatable(clargs_command_t *command, char short_name,
                                                       const char *long_name, clargs_option_t **out);
CLARGS_API clargs_error_t clargs_command_add_count(clargs_command_t *command, char short_name,
                                                   const char *long_name, clargs_option_t **out);
CLARGS_API clargs_error_t clargs_command_add_value(clargs_command_t *command, char short_name,
                                                   const char *long_name, const clargs_type_t *type,
                                                   clargs_option_t **out);

/*
 * Returns the command that option belongs to. Never NULL.
 *
 * Preconditions:
 *
 * - option is not NULL. Checked with assert.
 */
CLARGS_API const clargs_command_t *clargs_option_command(const clargs_option_t *option);

/*
 * Returns the kind of option.
 *
 * Preconditions:
 *
 * - option is not NULL. Checked with assert.
 */
CLARGS_API clargs_option_kind_t clargs_option_kind(const clargs_option_t *option);

/*
 * Returns the short name of option, or '\0' when it has none.
 *
 * Preconditions:
 *
 * - option is not NULL. Checked with assert.
 */
CLARGS_API char clargs_option_short_name(const clargs_option_t *option);

/*
 * Returns the long name of option, or NULL when it has none.
 *
 * Preconditions:
 *
 * - option is not NULL. Checked with assert.
 */
CLARGS_API const char *clargs_option_long_name(const clargs_option_t *option);

/*
 * Returns the negated name of option, "no-" and its long name, when option
 * is a negatable flag, and NULL for every other kind.
 *
 * Preconditions:
 *
 * - option is not NULL. Checked with assert.
 */
CLARGS_API const char *clargs_option_negated_name(const clargs_option_t *option);

/*
 * Returns the value type of option when option takes a value, and NULL for
 * every other kind. It is the value type that was passed to
 * clargs_command_add_value(); it belongs to the caller of that function.
 *
 * Preconditions:
 *
 * - option is not NULL. Checked with assert.
 */
CLARGS_API const clargs_type_t *clargs_option_type(const clargs_option_t *option);

#ifdef __cplusplus
}
#endif

#endif /* CLARGS_SCHEMA_H */
