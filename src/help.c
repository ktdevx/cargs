/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - the help: the public functions that generate the text, and the
 * order of its parts.
 */
#include "help_internal.h"
#include "help_item.h"
#include "vec.h"
#include "wrap.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

/*
 * The state of one generation. It lives on the public function's stack
 * for the duration of one call.
 */
typedef struct help_context
{
    const argx_command_t *command; /* the command the text is about */
    size_t width;                  /* the width in columns; 0 means no limit */
    size_t indent;                 /* the blanks before each item of a list */
    size_t max_column;             /* the last column at which descriptions may start */
    size_t column;                 /* the description column; set before the lists are written */
    argx_out_t out;                /* where the text goes */
    /*
     * the cell text of one item, as bytes; reused for the next
     * item and freed before the generation returns
     */
    argx_vec_t text;
} help_context_t;

/*
 * Starts a generation for command. Takes the settings from options, or
 * their default values when options is NULL, and starts the output. The
 * cell text is empty; nothing is allocated.
 */
static void help_context_init(help_context_t *context, const argx_command_t *command,
                              const argx_help_options_t *options, argx_help_write_fn_t write_fn,
                              void *user_data)
{
    context->command = command;
    if (options != NULL)
    {
        context->width = options->width;
        context->indent = options->indent;
        context->max_column = options->max_column;
    }
    else
    {
        context->width = ARGX_HELP_DEFAULT_WIDTH;
        context->indent = ARGX_HELP_DEFAULT_INDENT;
        context->max_column = ARGX_HELP_DEFAULT_MAX_COLUMN;
    }
    context->column = 0;
    argx_out_init(&context->out, write_fn, user_data);
    argx_vec_init(&context->text, 1);
}

/*
 * Returns the result of a generation in which no allocation failed. It
 * depends only on whether the write function has reported a failure.
 */
static argx_error_t help_context_result(const help_context_t *context)
{
    return argx_out_failed(&context->out) ? ARGX_ERROR_WRITE_FAILED : ARGX_ERROR_NONE;
}

/*
 * Writes command's path: the command names from the root down to command,
 * separated by single blanks. Nothing is allocated: for each level, from
 * the root down, the parent chain is walked again from command.
 */
static void help_write_path(argx_out_t *out, const argx_command_t *command)
{
    const argx_command_t *ancestor;
    size_t depth = 0;
    size_t level;

    for (ancestor = argx_command_parent(command); ancestor != NULL;
         ancestor = argx_command_parent(ancestor))
    {
        ++depth;
    }

    for (level = depth + 1; level-- > 0;)
    {
        size_t step;

        ancestor = command;
        for (step = 0; step < level; ++step)
        {
            ancestor = argx_command_parent(ancestor);
        }

        if (level != depth)
        {
            argx_out_spaces(out, 1);
        }

        argx_help_item_string(out, argx_command_name(ancestor));
    }
}

/*
 * Returns whether command has an option that is not required: one that
 * takes no value, or one whose value need not be given.
 */
static bool help_has_optional_option(const argx_command_t *command)
{
    size_t count = argx_command_option_count(command);
    size_t index;

    for (index = 0; index < count; ++index)
    {
        if (!argx_option_is_required(argx_command_option_at(command, index)))
        {
            return true;
        }
    }

    return false;
}

/*
 * The blanks that start each continuation line of the usage line: the
 * width of "Usage: ".
 */
#define HELP_USAGE_INDENT ((size_t)7)

/* The kinds of usage line items that follow the path. */
typedef enum help_usage_kind
{
    HELP_USAGE_OPTIONS,    /* "[OPTIONS]" */
    HELP_USAGE_OPTION,     /* a required option */
    HELP_USAGE_POSITIONAL, /* a positional */
    HELP_USAGE_COMMAND     /* "[COMMAND]" */
} help_usage_kind_t;

/* One usage line item: a text that is never split. */
typedef struct help_usage_item
{
    help_usage_kind_t kind;
    const argx_option_t *option;         /* for HELP_USAGE_OPTION */
    const argx_positional_t *positional; /* for HELP_USAGE_POSITIONAL */
} help_usage_item_t;

/*
 * Writes item to out. This is the one place that knows an item's text,
 * for both output and measuring.
 */
static void help_usage_item_write(argx_out_t *out, const help_usage_item_t *item)
{
    switch (item->kind)
    {
    case HELP_USAGE_OPTIONS:
        argx_out_string(out, "[OPTIONS]");
        break;
    case HELP_USAGE_OPTION:
        argx_help_item_usage_option(out, item->option);
        break;
    case HELP_USAGE_POSITIONAL:
        argx_help_item_positional_heading(out, item->positional);
        break;
    case HELP_USAGE_COMMAND:
        argx_out_string(out, "[COMMAND]");
        break;
    }
}

/*
 * Returns a + b, or the greatest size_t value when the sum would exceed
 * it.
 */
static size_t help_add(size_t a, size_t b)
{
    return (b > ((size_t)-1 - a)) ? (size_t)-1 : (a + b);
}

/*
 * A write function that keeps nothing: it adds length to the size_t that
 * user_data points to. The sum stops at the greatest size_t value instead
 * of wrapping around.
 */
static bool help_count_write(void *user_data, const char *text, size_t length)
{
    size_t *total = user_data;

    (void)text;
    *total = help_add(*total, length);

    return true;
}

/*
 * Starts counter as an output that measures a width. Sets *width to 0;
 * every byte then written to counter adds 1 to it. Nothing is kept and
 * nothing is allocated.
 *
 * The width of a usage line item or a heading is the number of all its
 * bytes, including those before a newline inside a name and the newline
 * itself. The output column would not do, as it restarts at 0 after a
 * newline. No blank is remembered inside an item or a heading, so every
 * byte reaches the count.
 */
static void help_counter_init(argx_out_t *counter, size_t *width)
{
    *width = 0;
    argx_out_init(counter, help_count_write, width);
}

/*
 * Returns item's width: the number of all its bytes. The item is written
 * once to an output that only counts; see help_counter_init().
 */
static size_t help_usage_item_width(const help_usage_item_t *item)
{
    size_t width;
    argx_out_t counter;

    help_counter_init(&counter, &width);
    help_usage_item_write(&counter, item);

    return width;
}

/*
 * Separates item, the next usage line item, from the previous one. Uses
 * one blank or, when the width is not 0 and "current column + 1 + item
 * width" exceeds it, a newline and the usage line indent. An item wider
 * than the width is thus written as it is, on its own line.
 *
 * The current column is the output's column. After an item with a newline
 * inside a name, it counts from that newline: it is the length of the
 * last line written, not the previous item's width.
 */
static void help_usage_separate(help_context_t *context, const help_usage_item_t *item)
{
    argx_out_t *out = &context->out;

    if (context->width != 0)
    {
        size_t item_width = help_usage_item_width(item);

        /*
         * "column + 1 + item_width > width", written without a sum that
         * could wrap around. The subtraction happens only when
         * item_width is less than the width.
         */
        if ((item_width >= context->width) ||
            (argx_out_column(out) >= (context->width - item_width)))
        {
            argx_out_newline(out);
            argx_out_spaces(out, HELP_USAGE_INDENT);
            return;
        }
    }

    argx_out_spaces(out, 1);
}

/*
 * Writes item, a usage line item after the first, preceded by what
 * separates it from the previous item.
 */
static void help_usage_add(help_context_t *context, const help_usage_item_t *item)
{
    help_usage_separate(context, item);
    help_usage_item_write(&context->out, item);
}

/*
 * Writes the usage line of context's command, including the newline that
 * ends it. The items, in order: "Usage: " and the command path,
 * "[OPTIONS]", the required options, the positionals and "[COMMAND]".
 * The line is broken between two items where it would exceed the width;
 * no item is split.
 */
static void help_write_usage_line(help_context_t *context)
{
    const argx_command_t *command = context->command;
    argx_out_t *out = &context->out;
    help_usage_item_t item;
    size_t count;
    size_t index;

    /*
     * The first item has no separator and is not measured. The blank is
     * part of it: it is written even when the root command's name starts
     * with a newline.
     */
    argx_out_text(out, "Usage: ", 7);
    help_write_path(out, command);

    item.option = NULL;
    item.positional = NULL;

    if (help_has_optional_option(command))
    {
        item.kind = HELP_USAGE_OPTIONS;
        help_usage_add(context, &item);
    }

    /* Only an option that takes a value can be required. */
    item.kind = HELP_USAGE_OPTION;
    count = argx_command_option_count(command);
    for (index = 0; index < count; ++index)
    {
        item.option = argx_command_option_at(command, index);
        if (argx_option_is_required(item.option))
        {
            help_usage_add(context, &item);
        }
    }

    item.kind = HELP_USAGE_POSITIONAL;
    count = argx_command_positional_count(command);
    for (index = 0; index < count; ++index)
    {
        item.positional = argx_command_positional_at(command, index);
        help_usage_add(context, &item);
    }

    if (argx_command_subcommand_count(command) != 0)
    {
        item.kind = HELP_USAGE_COMMAND;
        help_usage_add(context, &item);
    }

    argx_out_newline(out);
}

argx_error_t argx_help_write_usage(const argx_command_t *command,
                                   const argx_help_options_t *options,
                                   argx_help_write_fn_t write_fn, void *user_data)
{
    help_context_t context;

    assert(command != NULL);
    assert(argx_command_is_validated(command));
    assert(write_fn != NULL);

    help_context_init(&context, command, options, write_fn, user_data);
    help_write_usage_line(&context);

    return help_context_result(&context);
}

/* ---- The lists ----------------------------------------------------------- */

/* The kinds of list items. */
typedef enum help_heading_kind
{
    HELP_HEADING_SUBCOMMAND, /* an item of "Commands:" */
    HELP_HEADING_POSITIONAL, /* an item of "Arguments:" */
    HELP_HEADING_OPTION      /* an item of "Options:" */
} help_heading_kind_t;

/*
 * The heading of one list item: what stands to the left of its
 * description.
 */
typedef struct help_heading
{
    help_heading_kind_t kind;
    const argx_command_t *subcommand;    /* for HELP_HEADING_SUBCOMMAND */
    const argx_positional_t *positional; /* for HELP_HEADING_POSITIONAL */
    const argx_option_t *option;         /* for HELP_HEADING_OPTION */
} help_heading_t;

/*
 * Writes heading to out. This is the one place that knows a heading's
 * text, for both output and measuring. A subcommand's heading is its
 * name.
 */
static void help_heading_write(argx_out_t *out, const help_heading_t *heading)
{
    switch (heading->kind)
    {
    case HELP_HEADING_SUBCOMMAND:
        argx_help_item_string(out, argx_command_name(heading->subcommand));
        break;
    case HELP_HEADING_POSITIONAL:
        argx_help_item_positional_heading(out, heading->positional);
        break;
    case HELP_HEADING_OPTION:
        argx_help_item_option_heading(out, heading->option);
        break;
    }
}

/*
 * Returns heading's width: the number of all its bytes. The heading is
 * written once to an output that only counts; see help_counter_init().
 */
static size_t help_heading_width(const help_heading_t *heading)
{
    size_t width;
    argx_out_t counter;

    help_counter_init(&counter, &width);
    help_heading_write(&counter, heading);

    return width;
}

/* Returns the heading of command's subcommand at index. */
static help_heading_t help_subcommand_heading(const argx_command_t *command, size_t index)
{
    help_heading_t heading;

    heading.kind = HELP_HEADING_SUBCOMMAND;
    heading.subcommand = argx_command_subcommand_at(command, index);
    heading.positional = NULL;
    heading.option = NULL;

    return heading;
}

/* Returns the heading of command's positional at index. */
static help_heading_t help_positional_heading(const argx_command_t *command, size_t index)
{
    help_heading_t heading;

    heading.kind = HELP_HEADING_POSITIONAL;
    heading.subcommand = NULL;
    heading.positional = argx_command_positional_at(command, index);
    heading.option = NULL;

    return heading;
}

/* Returns the heading of command's option at index. */
static help_heading_t help_option_heading(const argx_command_t *command, size_t index)
{
    help_heading_t heading;

    heading.kind = HELP_HEADING_OPTION;
    heading.subcommand = NULL;
    heading.positional = NULL;
    heading.option = argx_command_option_at(command, index);

    return heading;
}

/*
 * Returns "indent + width + 2": the first column at which a description
 * can start on the line of a heading of that width.
 *
 * A width is the length of strings in memory and the indent is at most
 * ARGX_HELP_SETTING_MAX, so the sum is not expected to overflow a size_t;
 * help_add() keeps it defined if it ever does.
 */
static size_t help_heading_reach(const help_context_t *context, size_t width)
{
    return help_add(help_add(context->indent, width), 2);
}

/*
 * Returns the description column for the help of context's command:
 * "indent + widest heading width + 2", but no more than the limit. The
 * widest heading is searched for across all the lists of the help, so the
 * descriptions of every list start at the same column.
 *
 * The limit is the maximum description column setting or, when the width
 * is not 0 and half of it (rounded down) is less, that half. It is never
 * less than "indent + 2".
 */
static size_t help_description_column(const help_context_t *context)
{
    const argx_command_t *command = context->command;
    help_heading_t heading;
    size_t widest = 0;
    size_t limit;
    size_t lowest;
    size_t column;
    size_t width;
    size_t count;
    size_t index;

    count = argx_command_subcommand_count(command);
    for (index = 0; index < count; ++index)
    {
        heading = help_subcommand_heading(command, index);
        width = help_heading_width(&heading);
        if (width > widest)
        {
            widest = width;
        }
    }

    count = argx_command_positional_count(command);
    for (index = 0; index < count; ++index)
    {
        heading = help_positional_heading(command, index);
        width = help_heading_width(&heading);
        if (width > widest)
        {
            widest = width;
        }
    }

    count = argx_command_option_count(command);
    for (index = 0; index < count; ++index)
    {
        heading = help_option_heading(command, index);
        width = help_heading_width(&heading);
        if (width > widest)
        {
            widest = width;
        }
    }

    column = help_heading_reach(context, widest);

    limit = context->max_column;
    if ((context->width != 0) && ((context->width / 2) < limit))
    {
        limit = context->width / 2;
    }

    /* The indent is at most ARGX_HELP_SETTING_MAX: the sum cannot wrap. */
    lowest = context->indent + 2;
    if (limit < lowest)
    {
        limit = lowest;
    }

    return (column > limit) ? limit : column;
}

/*
 * Returns whether length bytes of text hold nothing to show: no byte
 * other than blanks and newlines. A description for which this is true
 * counts as not set. This is the one place that decides it, for both the
 * command description and the item descriptions.
 */
static bool help_is_blank(const char *text, size_t length)
{
    size_t index;

    for (index = 0; index < length; ++index)
    {
        if ((text[index] != ' ') && (text[index] != '\n'))
        {
            return false;
        }
    }

    return true;
}

/*
 * Returns the length of the part of text that a description writes: text
 * up to and including its last byte that is neither a blank nor a
 * newline. The run of blanks and newlines after that byte, the trailing
 * run, is not part of it. Its blanks are ignored, and the number of its
 * newlines is stored in *newlines. So "foo\n  " gives the same result as
 * "foo\n", and "foo\n \n" the same as "foo\n\n".
 *
 * Returns 0, and stores 0, when text is NULL or holds only blanks and
 * newlines: 0 means there is no description to write.
 */
static size_t help_whole_length(const char *text, size_t *newlines)
{
    size_t length;

    *newlines = 0;
    if (text == NULL)
    {
        return 0;
    }

    length = strlen(text);
    if (help_is_blank(text, length))
    {
        return 0;
    }

    while ((text[length - 1] == ' ') || (text[length - 1] == '\n'))
    {
        if (text[length - 1] == '\n')
        {
            ++*newlines;
        }
        --length;
    }

    return length;
}

/*
 * Returns the length of text before its first newline: the length of a
 * subcommand's description in a list. Returns 0 when text is NULL or that
 * part holds only blanks, whatever follows the newline: 0 means there is
 * no description to write.
 */
static size_t help_first_line_length(const char *text)
{
    const char *end;
    size_t length;

    if (text == NULL)
    {
        return 0;
    }

    end = strchr(text, '\n');
    length = (end != NULL) ? (size_t)(end - text) : strlen(text);

    return help_is_blank(text, length) ? 0 : length;
}

/*
 * Writes the description of context's command, when it has one that holds
 * more than blanks and newlines: an empty line, then the text up to its
 * trailing run, from the start of the line, broken at the width.
 */
static void help_write_description(help_context_t *context)
{
    argx_out_t *out = &context->out;
    const char *text = argx_command_description(context->command);
    size_t newlines;
    size_t length = help_whole_length(text, &newlines);

    if (length == 0)
    {
        return;
    }

    argx_out_newline(out);
    argx_wrap_write(out, text, length, 0, context->width);
    argx_out_newline(out);
}

/* Starts a list: writes an empty line and its title line. */
static void help_write_title(help_context_t *context, const char *title)
{
    argx_out_t *out = &context->out;

    argx_out_newline(out);
    argx_out_string(out, title);
    argx_out_newline(out);
}

/*
 * Writes one list item: the indent, heading, and length bytes of text,
 * its description, from the description column onward, broken at the
 * width. When length is 0, the line holds only the heading.
 *
 * The description starts on the heading's line when the heading ends at
 * least 2 columns left of the description column, that is, when "indent +
 * heading width + 2" does not exceed the column. Otherwise the line ends
 * after the heading, and the description starts on the next line, at the
 * description column.
 *
 * The decision uses the heading width (the number of all its bytes), not
 * the output column after the heading: the two differ for a heading with
 * a newline inside a name.
 */
static void help_write_item(help_context_t *context, const help_heading_t *heading,
                            const char *text, size_t length)
{
    argx_out_t *out = &context->out;

    argx_out_spaces(out, context->indent);
    help_heading_write(out, heading);
    if (length != 0)
    {
        if (help_heading_reach(context, help_heading_width(heading)) > context->column)
        {
            argx_out_newline(out);
        }

        /*
         * argx_wrap_write() needs the output to be at or left of the
         * description column. On the heading's line the output is at
         * "indent + heading width" at most (less after a newline inside
         * a name), which is at least 2 left of the column. On a new line
         * it is at 0.
         */
        assert(argx_out_column(out) <= context->column);
        argx_wrap_write(out, text, length, context->column, context->width);
    }
    argx_out_newline(out);
}

/*
 * Starts an item's cell text. Empties it, keeping its memory for the next
 * item, and appends description when the item has one that holds more
 * than blanks and newlines: the part up to its trailing run, then the
 * newlines of that run without its blanks (see help_whole_length()). A
 * description that ends with a newline keeps it, so an annotation then
 * starts on the next line.
 *
 * Returns ARGX_ERROR_OUT_OF_MEMORY when an allocation fails.
 */
static argx_error_t help_cell_start(help_context_t *context, const char *description)
{
    size_t newlines;
    size_t length = help_whole_length(description, &newlines);
    argx_error_t error;

    context->text.count = 0;
    if (length == 0)
    {
        return ARGX_ERROR_NONE;
    }

    error = argx_help_item_text_append(&context->text, description, length);
    while ((error == ARGX_ERROR_NONE) && (newlines != 0))
    {
        error = argx_help_item_text_append(&context->text, "\n", 1);
        --newlines;
    }

    return error;
}

/*
 * Writes one list item with the cell text as its description; see
 * help_write_item(). Newlines at the end of the cell text are not
 * written. An empty cell text gives a line with only the heading.
 */
static void help_write_cell_item(help_context_t *context, const help_heading_t *heading)
{
    size_t length = context->text.count;
    const char *text = (length != 0) ? argx_vec_at(&context->text, 0) : NULL;

    help_write_item(context, heading, text, length);
}

/*
 * Writes the subcommand list of context's command, when it has any: the
 * command's own subcommands, in the order they were added, each with the
 * first line of its description.
 */
static void help_write_commands(help_context_t *context)
{
    const argx_command_t *command = context->command;
    size_t count = argx_command_subcommand_count(command);
    size_t index;

    if (count == 0)
    {
        return;
    }

    help_write_title(context, "Commands:");
    for (index = 0; (index < count) && !argx_out_failed(&context->out); ++index)
    {
        help_heading_t heading = help_subcommand_heading(command, index);
        const char *text = argx_command_description(heading.subcommand);

        help_write_item(context, &heading, text, help_first_line_length(text));
    }
}

/*
 * Writes the positional list of context's command, when it has any: in
 * the order they were added, each with its whole description followed by
 * its annotations.
 *
 * Returns ARGX_ERROR_OUT_OF_MEMORY when an allocation for the cell text
 * fails; the list ends before the item it was for.
 */
static argx_error_t help_write_arguments(help_context_t *context)
{
    const argx_command_t *command = context->command;
    size_t count = argx_command_positional_count(command);
    size_t index;

    if (count == 0)
    {
        return ARGX_ERROR_NONE;
    }

    help_write_title(context, "Arguments:");
    for (index = 0; (index < count) && !argx_out_failed(&context->out); ++index)
    {
        help_heading_t heading = help_positional_heading(command, index);
        argx_error_t error =
            help_cell_start(context, argx_positional_description(heading.positional));

        if (error == ARGX_ERROR_NONE)
        {
            error = argx_help_item_positional_notes(&context->text, heading.positional);
        }

        if (error != ARGX_ERROR_NONE)
        {
            return error;
        }

        help_write_cell_item(context, &heading);
    }

    return ARGX_ERROR_NONE;
}

/*
 * Writes the option list of context's command, when it has any: the
 * command's own options, not those of its parent or its subcommands, in
 * the order they were added, each with its whole description followed by
 * its annotations. Nothing is listed that the schema does not have.
 *
 * Returns ARGX_ERROR_OUT_OF_MEMORY when an allocation for the cell text
 * fails; the list ends before the item it was for.
 */
static argx_error_t help_write_options(help_context_t *context)
{
    const argx_command_t *command = context->command;
    size_t count = argx_command_option_count(command);
    size_t index;

    if (count == 0)
    {
        return ARGX_ERROR_NONE;
    }

    help_write_title(context, "Options:");
    for (index = 0; (index < count) && !argx_out_failed(&context->out); ++index)
    {
        help_heading_t heading = help_option_heading(command, index);
        argx_error_t error = help_cell_start(context, argx_option_description(heading.option));

        if (error == ARGX_ERROR_NONE)
        {
            error = argx_help_item_option_notes(&context->text, heading.option);
        }

        if (error != ARGX_ERROR_NONE)
        {
            return error;
        }

        help_write_cell_item(context, &heading);
    }

    return ARGX_ERROR_NONE;
}

argx_error_t argx_help_write(const argx_command_t *command,
                             const argx_help_options_t *options,
                             argx_help_write_fn_t write_fn, void *user_data)
{
    help_context_t context;
    argx_error_t error;

    assert(command != NULL);
    assert(argx_command_is_validated(command));
    assert(write_fn != NULL);

    help_context_init(&context, command, options, write_fn, user_data);

    help_write_usage_line(&context);
    help_write_description(&context);

    /* One column for all the lists, computed from all their headings. */
    context.column = help_description_column(&context);
    help_write_commands(&context);
    error = help_write_arguments(&context);
    if (error == ARGX_ERROR_NONE)
    {
        error = help_write_options(&context);
    }

    /*
     * The cell text is the only memory a generation allocates. An
     * allocation and the write function never both fail: the first
     * failure ends the lists.
     */
    argx_vec_destroy(&context.text);
    if (error != ARGX_ERROR_NONE)
    {
        return error;
    }

    return help_context_result(&context);
}

/* ---- The buffer ---------------------------------------------------------- */

/*
 * The caller's array that a text is put into, and how much of the text
 * has arrived. It lives on the public function's stack for the duration
 * of one call.
 */
typedef struct help_buffer
{
    char *buffer;  /* the array; may be NULL when size is 0 */
    size_t size;   /* the size of the array in bytes */
    size_t length; /* the text bytes so far, including those that did not fit */
} help_buffer_t;

/*
 * The write function of the buffer functions. Copies as much of the part
 * as fits into the array, ahead of the place for the '\0', and counts all
 * of it. It never reports a failure, so the generation runs to the end and
 * the count becomes the length of the whole text. The count stops at the
 * greatest size_t value instead of wrapping around.
 */
static bool help_buffer_write(void *user_data, const char *text, size_t length)
{
    help_buffer_t *target = user_data;

    /* The text may fill the bytes before the index size - 1. */
    if ((target->size != 0) && (target->length < (target->size - 1)))
    {
        size_t room = target->size - 1 - target->length;

        memcpy(target->buffer + target->length, text, (length < room) ? length : room);
    }

    target->length = help_add(target->length, length);

    return true;
}

/*
 * Ends a generation into target that returned error. After a failure the
 * text counts as empty. When the array has at least one byte, it gets its
 * '\0' after the part of the text it holds. When length is not NULL,
 * *length gets the length of the whole text. Returns error.
 */
static argx_error_t help_buffer_finish(help_buffer_t *target, argx_error_t error,
                                       size_t *length)
{
    if (error != ARGX_ERROR_NONE)
    {
        target->length = 0;
    }

    if (target->size != 0)
    {
        target->buffer[(target->length < (target->size - 1)) ? target->length : (target->size - 1)] = '\0';
    }

    if (length != NULL)
    {
        *length = target->length;
    }

    return error;
}

argx_error_t argx_help_format(const argx_command_t *command,
                              const argx_help_options_t *options, char *buffer, size_t size,
                              size_t *length)
{
    help_buffer_t target;

    assert((buffer != NULL) || (size == 0));

    target.buffer = buffer;
    target.size = size;
    target.length = 0;

    return help_buffer_finish(
        &target, argx_help_write(command, options, help_buffer_write, &target), length);
}

argx_error_t argx_help_format_usage(const argx_command_t *command,
                                    const argx_help_options_t *options, char *buffer,
                                    size_t size, size_t *length)
{
    help_buffer_t target;

    assert((buffer != NULL) || (size == 0));

    target.buffer = buffer;
    target.size = size;
    target.length = 0;

    return help_buffer_finish(
        &target, argx_help_write_usage(command, options, help_buffer_write, &target), length);
}
