/*
 * CLArgs - the help: the public functions that generate the text, and the
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
 * The state of one generation. It lives on the stack of the public
 * function, for the one call.
 */
typedef struct help_context
{
    const clargs_command_t *command; /* the command the text is about */
    size_t width;                    /* the width in columns; 0 means no limit */
    size_t indent;                   /* the blanks before each item of a list */
    size_t max_column;               /* the last column at which the descriptions may start */
    size_t column;                   /* the description column; set before the lists are written */
    clargs_out_t out;                /* where the text goes */
    /*
     * the cell text of one item, as bytes; used again for the
     * next item, freed before the generation returns
     */
    clargs_vec_t text;
} help_context_t;

/*
 * Starts a generation for command: takes the settings from options, or
 * their default values when options is NULL, and starts the output. The
 * cell text is empty; nothing is allocated.
 */
static void help_context_init(help_context_t *context, const clargs_command_t *command,
                              const clargs_help_options_t *options, clargs_help_write_fn_t write_fn,
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
        context->width = CLARGS_HELP_DEFAULT_WIDTH;
        context->indent = CLARGS_HELP_DEFAULT_INDENT;
        context->max_column = CLARGS_HELP_DEFAULT_MAX_COLUMN;
    }
    context->column = 0;
    clargs_out_init(&context->out, write_fn, user_data);
    clargs_vec_init(&context->text, 1);
}

/*
 * The result of a generation in which no allocation failed: whether the
 * write function has reported a failure.
 */
static clargs_error_t help_context_result(const help_context_t *context)
{
    return clargs_out_failed(&context->out) ? CLARGS_ERROR_WRITE_FAILED : CLARGS_ERROR_NONE;
}

/*
 * Writes the path of command: the names of the commands from the root down
 * to command, separated by one blank each. Nothing is allocated: for each
 * level, from the root down, the parents are walked again from command.
 */
static void help_write_path(clargs_out_t *out, const clargs_command_t *command)
{
    const clargs_command_t *ancestor;
    size_t depth = 0;
    size_t level;

    for (ancestor = clargs_command_parent(command); ancestor != NULL;
         ancestor = clargs_command_parent(ancestor))
    {
        ++depth;
    }

    for (level = depth + 1; level-- > 0;)
    {
        size_t step;

        ancestor = command;
        for (step = 0; step < level; ++step)
        {
            ancestor = clargs_command_parent(ancestor);
        }

        if (level != depth)
        {
            clargs_out_spaces(out, 1);
        }
        clargs_help_item_string(out, clargs_command_name(ancestor));
    }
}

/*
 * Returns whether command has an option that is not required: one that
 * takes no value, or one with a value that need not be given.
 */
static bool help_has_optional_option(const clargs_command_t *command)
{
    size_t count = clargs_command_option_count(command);
    size_t index;

    for (index = 0; index < count; ++index)
    {
        if (!clargs_option_is_required(clargs_command_option_at(command, index)))
        {
            return true;
        }
    }
    return false;
}

/*
 * The blanks that start each line of the usage line after the first: the
 * width of "Usage: ".
 */
#define HELP_USAGE_INDENT ((size_t)7)

/* The kinds of the items of the usage line that follow the path. */
typedef enum help_usage_kind
{
    HELP_USAGE_OPTIONS,    /* "[OPTIONS]" */
    HELP_USAGE_OPTION,     /* a required option */
    HELP_USAGE_POSITIONAL, /* a positional */
    HELP_USAGE_COMMAND     /* "[COMMAND]" */
} help_usage_kind_t;

/* One item of the usage line: a text that is never divided. */
typedef struct help_usage_item
{
    help_usage_kind_t kind;
    const clargs_option_t *option;         /* for HELP_USAGE_OPTION */
    const clargs_positional_t *positional; /* for HELP_USAGE_POSITIONAL */
} help_usage_item_t;

/*
 * Writes item to out: the one place that knows the text of an item, for
 * the output and for the measuring.
 */
static void help_usage_item_write(clargs_out_t *out, const help_usage_item_t *item)
{
    switch (item->kind)
    {
    case HELP_USAGE_OPTIONS:
        clargs_out_string(out, "[OPTIONS]");
        break;
    case HELP_USAGE_OPTION:
        clargs_help_item_usage_option(out, item->option);
        break;
    case HELP_USAGE_POSITIONAL:
        clargs_help_item_positional_heading(out, item->positional);
        break;
    case HELP_USAGE_COMMAND:
        clargs_out_string(out, "[COMMAND]");
        break;
    }
}

/*
 * Returns a + b, or the greatest value of a size_t when the sum would be
 * greater than it.
 */
static size_t help_add(size_t a, size_t b)
{
    return (b > ((size_t)-1 - a)) ? (size_t)-1 : (a + b);
}

/*
 * A write function that keeps nothing: it adds length to the size_t that
 * user_data points to. The sum stops at the greatest value of a size_t
 * instead of wrapping around.
 */
static bool help_count_write(void *user_data, const char *text, size_t length)
{
    size_t *total = user_data;

    (void)text;
    *total = help_add(*total, length);
    return true;
}

/*
 * Starts counter as an output that measures a width: *width is set to 0,
 * and every byte that is then written to counter adds 1 to it. Nothing is
 * kept and nothing is allocated.
 *
 * The width of an item of the usage line or of a heading is the number of
 * all its bytes, also of those before a newline inside a name and of the
 * newline itself. The column of an output would not do, as it starts again
 * at 0 after a newline. Inside an item or a heading no blank is
 * remembered, so every byte reaches the count.
 */
static void help_counter_init(clargs_out_t *counter, size_t *width)
{
    *width = 0;
    clargs_out_init(counter, help_count_write, width);
}

/*
 * Returns the width of item: the number of all its bytes. The item is
 * written once to an output that only counts; see help_counter_init().
 */
static size_t help_usage_item_width(const help_usage_item_t *item)
{
    size_t width;
    clargs_out_t counter;

    help_counter_init(&counter, &width);
    help_usage_item_write(&counter, item);
    return width;
}

/*
 * Separates item, the next item of the usage line, from the one before it:
 * with one blank, or, when the width is not 0 and "the current column + 1 +
 * the width of the item" is greater than it, with a newline and the indent
 * of the usage line. An item that is wider than the width is so written as
 * it is, on a line of its own.
 *
 * The current column is the one of the output. After an item with a
 * newline inside a name it counts from that newline: it is the length of
 * the last line of what was written, not the width of the item before.
 */
static void help_usage_separate(help_context_t *context, const help_usage_item_t *item)
{
    clargs_out_t *out = &context->out;

    if (context->width != 0)
    {
        size_t item_width = help_usage_item_width(item);

        /*
         * "column + 1 + item_width > width", written without a sum that
         * could wrap around: the subtraction is done only when item_width
         * is less than the width.
         */
        if ((item_width >= context->width) ||
            (clargs_out_column(out) >= (context->width - item_width)))
        {
            clargs_out_newline(out);
            clargs_out_spaces(out, HELP_USAGE_INDENT);
            return;
        }
    }

    clargs_out_spaces(out, 1);
}

/*
 * Writes item, an item of the usage line after the first, behind what
 * separates it from the item before.
 */
static void help_usage_add(help_context_t *context, const help_usage_item_t *item)
{
    help_usage_separate(context, item);
    help_usage_item_write(&context->out, item);
}

/*
 * Writes the usage line of the command of context, with the newline that
 * ends it. The items, in their order: "Usage: " and the path of the
 * command, "[OPTIONS]", the required options, the positionals and
 * "[COMMAND]". The line is broken between two items where it would go
 * beyond the width; no item is divided.
 */
static void help_write_usage_line(help_context_t *context)
{
    const clargs_command_t *command = context->command;
    clargs_out_t *out = &context->out;
    help_usage_item_t item;
    size_t count;
    size_t index;

    /*
     * The first item: nothing separates it from anything, and it is not
     * measured. The blank is part of it: it is written even when the name
     * of the root starts with a newline.
     */
    clargs_out_text(out, "Usage: ", 7);
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
    count = clargs_command_option_count(command);
    for (index = 0; index < count; ++index)
    {
        item.option = clargs_command_option_at(command, index);
        if (clargs_option_is_required(item.option))
        {
            help_usage_add(context, &item);
        }
    }

    item.kind = HELP_USAGE_POSITIONAL;
    count = clargs_command_positional_count(command);
    for (index = 0; index < count; ++index)
    {
        item.positional = clargs_command_positional_at(command, index);
        help_usage_add(context, &item);
    }

    if (clargs_command_subcommand_count(command) != 0)
    {
        item.kind = HELP_USAGE_COMMAND;
        help_usage_add(context, &item);
    }

    clargs_out_newline(out);
}

clargs_error_t clargs_help_write_usage(const clargs_command_t *command,
                                       const clargs_help_options_t *options,
                                       clargs_help_write_fn_t write_fn, void *user_data)
{
    help_context_t context;

    assert(command != NULL);
    assert(clargs_command_is_validated(command));
    assert(write_fn != NULL);

    help_context_init(&context, command, options, write_fn, user_data);
    help_write_usage_line(&context);

    return help_context_result(&context);
}

/* ---- The lists ----------------------------------------------------------- */

/* The kinds of the items of the lists. */
typedef enum help_heading_kind
{
    HELP_HEADING_SUBCOMMAND, /* an item of "Commands:" */
    HELP_HEADING_POSITIONAL, /* an item of "Arguments:" */
    HELP_HEADING_OPTION      /* an item of "Options:" */
} help_heading_kind_t;

/*
 * The heading of one item of a list: what stands on the left of its
 * description.
 */
typedef struct help_heading
{
    help_heading_kind_t kind;
    const clargs_command_t *subcommand;    /* for HELP_HEADING_SUBCOMMAND */
    const clargs_positional_t *positional; /* for HELP_HEADING_POSITIONAL */
    const clargs_option_t *option;         /* for HELP_HEADING_OPTION */
} help_heading_t;

/*
 * Writes heading to out: the one place that knows the text of a heading,
 * for the output and for the measuring. The heading of a subcommand is its
 * name.
 */
static void help_heading_write(clargs_out_t *out, const help_heading_t *heading)
{
    switch (heading->kind)
    {
    case HELP_HEADING_SUBCOMMAND:
        clargs_help_item_string(out, clargs_command_name(heading->subcommand));
        break;
    case HELP_HEADING_POSITIONAL:
        clargs_help_item_positional_heading(out, heading->positional);
        break;
    case HELP_HEADING_OPTION:
        clargs_help_item_option_heading(out, heading->option);
        break;
    }
}

/*
 * Returns the width of heading: the number of all its bytes. The heading
 * is written once to an output that only counts; see help_counter_init().
 */
static size_t help_heading_width(const help_heading_t *heading)
{
    size_t width;
    clargs_out_t counter;

    help_counter_init(&counter, &width);
    help_heading_write(&counter, heading);
    return width;
}

/* Returns the heading of the subcommand of command at index. */
static help_heading_t help_subcommand_heading(const clargs_command_t *command, size_t index)
{
    help_heading_t heading;

    heading.kind = HELP_HEADING_SUBCOMMAND;
    heading.subcommand = clargs_command_subcommand_at(command, index);
    heading.positional = NULL;
    heading.option = NULL;
    return heading;
}

/* Returns the heading of the positional of command at index. */
static help_heading_t help_positional_heading(const clargs_command_t *command, size_t index)
{
    help_heading_t heading;

    heading.kind = HELP_HEADING_POSITIONAL;
    heading.subcommand = NULL;
    heading.positional = clargs_command_positional_at(command, index);
    heading.option = NULL;
    return heading;
}

/* Returns the heading of the option of command at index. */
static help_heading_t help_option_heading(const clargs_command_t *command, size_t index)
{
    help_heading_t heading;

    heading.kind = HELP_HEADING_OPTION;
    heading.subcommand = NULL;
    heading.positional = NULL;
    heading.option = clargs_command_option_at(command, index);
    return heading;
}

/*
 * Returns "the indent + width + 2": the first column at which a
 * description can start on the line of a heading of that width.
 *
 * A width is the length of strings in memory and the indent is at most
 * CLARGS_HELP_SETTING_MAX, so the sum is not expected to go beyond a
 * size_t; help_add() keeps it defined if it ever does.
 */
static size_t help_heading_reach(const help_context_t *context, size_t width)
{
    return help_add(help_add(context->indent, width), 2);
}

/*
 * Returns the description column of the help of the command of context:
 * "the indent + the width of the widest heading + 2", but not more than
 * the limit. The widest heading is looked for in all the lists of the
 * help, so the descriptions of every list start at the same column.
 *
 * The limit is the maximum column of the settings or, when the width is
 * not 0 and half of it, rounded down, is less, that half. It is never less
 * than "the indent + 2".
 */
static size_t help_description_column(const help_context_t *context)
{
    const clargs_command_t *command = context->command;
    help_heading_t heading;
    size_t widest = 0;
    size_t limit;
    size_t lowest;
    size_t column;
    size_t width;
    size_t count;
    size_t index;

    count = clargs_command_subcommand_count(command);
    for (index = 0; index < count; ++index)
    {
        heading = help_subcommand_heading(command, index);
        width = help_heading_width(&heading);
        if (width > widest)
        {
            widest = width;
        }
    }

    count = clargs_command_positional_count(command);
    for (index = 0; index < count; ++index)
    {
        heading = help_positional_heading(command, index);
        width = help_heading_width(&heading);
        if (width > widest)
        {
            widest = width;
        }
    }

    count = clargs_command_option_count(command);
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

    /* The indent is at most CLARGS_HELP_SETTING_MAX: the sum cannot wrap. */
    lowest = context->indent + 2;
    if (limit < lowest)
    {
        limit = lowest;
    }

    return (column > limit) ? limit : column;
}

/*
 * Returns whether the length bytes of text hold nothing to show: no byte
 * other than blanks and newlines. A description of which that is true
 * counts as not set; this is the one place that decides it, for the
 * description of the command and for those of the items.
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
 * up to and with its last byte that is neither a blank nor a newline. The
 * run of blanks and newlines behind that byte, the trailing run, is not
 * part of it: its blanks are ignored, and the number of its newlines is
 * stored in *newlines. So "foo\n  " gives what "foo\n" gives, and
 * "foo\n \n" what "foo\n\n" gives.
 *
 * Returns 0, and stores 0, when text is NULL or holds nothing but blanks
 * and newlines: 0 means that there is no description to write.
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
 * Returns the length of the part of text before its first newline: the
 * length of the description of a subcommand in a list. Returns 0 when text
 * is NULL or that part holds nothing but blanks, whatever follows the
 * newline: 0 means that there is no description to write.
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
 * Writes the description of the command of context, when it has one that
 * holds more than blanks and newlines: an empty line, then the text up to
 * its trailing run, from the start of the line, broken at the width.
 */
static void help_write_description(help_context_t *context)
{
    clargs_out_t *out = &context->out;
    const char *text = clargs_command_description(context->command);
    size_t newlines;
    size_t length = help_whole_length(text, &newlines);

    if (length == 0)
    {
        return;
    }

    clargs_out_newline(out);
    clargs_wrap_write(out, text, length, 0, context->width);
    clargs_out_newline(out);
}

/* Starts a list: writes an empty line and the line of its title. */
static void help_write_title(help_context_t *context, const char *title)
{
    clargs_out_t *out = &context->out;

    clargs_out_newline(out);
    clargs_out_string(out, title);
    clargs_out_newline(out);
}

/*
 * Writes one item of a list: the indent, heading, and the length bytes of
 * text, its description, from the description column on, broken at the
 * width. With the length 0 the line holds the heading only.
 *
 * The description starts on the line of the heading when the heading ends
 * to the left of the description column by 2 columns or more: when "the
 * indent + the width of the heading + 2" is not greater than the column.
 * If not, the line ends behind the heading, and the description starts on
 * the next line, at the description column.
 *
 * What decides is the width of the heading, the number of all its bytes,
 * not the column of the output behind it: the two differ for a heading
 * with a newline inside a name.
 */
static void help_write_item(help_context_t *context, const help_heading_t *heading,
                            const char *text, size_t length)
{
    clargs_out_t *out = &context->out;

    clargs_out_spaces(out, context->indent);
    help_heading_write(out, heading);
    if (length != 0)
    {
        if (help_heading_reach(context, help_heading_width(heading)) > context->column)
        {
            clargs_out_newline(out);
        }

        /*
         * clargs_wrap_write() needs the output to stand at the description
         * column or to the left of it. On the line of the heading the
         * output stands at "the indent + the width of the heading" at
         * most, less after a newline inside a name, and that is to the
         * left of the column by 2 or more; on a new line it stands at 0.
         */
        assert(clargs_out_column(out) <= context->column);
        clargs_wrap_write(out, text, length, context->column, context->width);
    }
    clargs_out_newline(out);
}

/*
 * Starts the cell text of an item: makes it empty, its memory is kept for
 * the next item, and appends description when the item has one that holds
 * more than blanks and newlines: its part up to its trailing run, then the
 * newlines of that run, without its blanks (see help_whole_length()). A
 * description that ends with a newline keeps it: an annotation then starts
 * on the next line.
 *
 * Returns CLARGS_ERROR_OUT_OF_MEMORY when an allocation fails.
 */
static clargs_error_t help_cell_start(help_context_t *context, const char *description)
{
    size_t newlines;
    size_t length = help_whole_length(description, &newlines);
    clargs_error_t error;

    context->text.count = 0;
    if (length == 0)
    {
        return CLARGS_ERROR_NONE;
    }
    error = clargs_help_item_text_append(&context->text, description, length);
    while ((error == CLARGS_ERROR_NONE) && (newlines != 0))
    {
        error = clargs_help_item_text_append(&context->text, "\n", 1);
        --newlines;
    }
    return error;
}

/*
 * Writes one item of a list with the cell text as its description; see
 * help_write_item(). The newlines at the end of the cell text are not
 * written. An empty cell text gives a line with the heading only.
 */
static void help_write_cell_item(help_context_t *context, const help_heading_t *heading)
{
    size_t length = context->text.count;
    const char *text = (length != 0) ? clargs_vec_at(&context->text, 0) : NULL;

    help_write_item(context, heading, text, length);
}

/*
 * Writes the list of the subcommands of the command of context, when it
 * has one: the subcommands of the command itself, in the order in which
 * they were added, each with the first line of its description.
 */
static void help_write_commands(help_context_t *context)
{
    const clargs_command_t *command = context->command;
    size_t count = clargs_command_subcommand_count(command);
    size_t index;

    if (count == 0)
    {
        return;
    }

    help_write_title(context, "Commands:");
    for (index = 0; (index < count) && !clargs_out_failed(&context->out); ++index)
    {
        help_heading_t heading = help_subcommand_heading(command, index);
        const char *text = clargs_command_description(heading.subcommand);

        help_write_item(context, &heading, text, help_first_line_length(text));
    }
}

/*
 * Writes the list of the positionals of the command of context, when it
 * has one: in the order in which they were added, each with its whole
 * description and, behind it, its annotations.
 *
 * Returns CLARGS_ERROR_OUT_OF_MEMORY when an allocation for the cell text
 * fails; the list ends before the item that it was for.
 */
static clargs_error_t help_write_arguments(help_context_t *context)
{
    const clargs_command_t *command = context->command;
    size_t count = clargs_command_positional_count(command);
    size_t index;

    if (count == 0)
    {
        return CLARGS_ERROR_NONE;
    }

    help_write_title(context, "Arguments:");
    for (index = 0; (index < count) && !clargs_out_failed(&context->out); ++index)
    {
        help_heading_t heading = help_positional_heading(command, index);
        clargs_error_t error =
            help_cell_start(context, clargs_positional_description(heading.positional));

        if (error == CLARGS_ERROR_NONE)
        {
            error = clargs_help_item_positional_notes(&context->text, heading.positional);
        }
        if (error != CLARGS_ERROR_NONE)
        {
            return error;
        }
        help_write_cell_item(context, &heading);
    }
    return CLARGS_ERROR_NONE;
}

/*
 * Writes the list of the options of the command of context, when it has
 * one: the options of the command itself, not those of its parent or of
 * its subcommands, in the order in which they were added, each with its
 * whole description and, behind it, its annotations. Nothing is listed
 * that the schema does not have.
 *
 * Returns CLARGS_ERROR_OUT_OF_MEMORY when an allocation for the cell text
 * fails; the list ends before the item that it was for.
 */
static clargs_error_t help_write_options(help_context_t *context)
{
    const clargs_command_t *command = context->command;
    size_t count = clargs_command_option_count(command);
    size_t index;

    if (count == 0)
    {
        return CLARGS_ERROR_NONE;
    }

    help_write_title(context, "Options:");
    for (index = 0; (index < count) && !clargs_out_failed(&context->out); ++index)
    {
        help_heading_t heading = help_option_heading(command, index);
        clargs_error_t error = help_cell_start(context, clargs_option_description(heading.option));

        if (error == CLARGS_ERROR_NONE)
        {
            error = clargs_help_item_option_notes(&context->text, heading.option);
        }
        if (error != CLARGS_ERROR_NONE)
        {
            return error;
        }
        help_write_cell_item(context, &heading);
    }
    return CLARGS_ERROR_NONE;
}

clargs_error_t clargs_help_write(const clargs_command_t *command,
                                 const clargs_help_options_t *options,
                                 clargs_help_write_fn_t write_fn, void *user_data)
{
    help_context_t context;
    clargs_error_t error;

    assert(command != NULL);
    assert(clargs_command_is_validated(command));
    assert(write_fn != NULL);

    help_context_init(&context, command, options, write_fn, user_data);

    help_write_usage_line(&context);
    help_write_description(&context);

    /* One column for all the lists, from the headings of all of them. */
    context.column = help_description_column(&context);
    help_write_commands(&context);
    error = help_write_arguments(&context);
    if (error == CLARGS_ERROR_NONE)
    {
        error = help_write_options(&context);
    }

    /*
     * The cell text is the only memory of a generation. An allocation and
     * the write function never both fail: the first failure ends the
     * lists.
     */
    clargs_vec_destroy(&context.text);
    if (error != CLARGS_ERROR_NONE)
    {
        return error;
    }
    return help_context_result(&context);
}

/* ---- The buffer ---------------------------------------------------------- */

/*
 * The array of the caller that a text is put into, and how much of the
 * text has come. It lives on the stack of the public function, for the one
 * call.
 */
typedef struct help_buffer
{
    char *buffer;  /* the array; may be NULL when size is 0 */
    size_t size;   /* the number of bytes of the array */
    size_t length; /* the bytes of the text so far, also those that did not fit */
} help_buffer_t;

/*
 * The write function of the buffer functions: copies what fits of the part
 * into the array, in front of the place of the '\0', and counts all of it.
 * It never reports a failure, so the generation runs to its end and the
 * count becomes the length of the whole text. The count stops at the
 * greatest value of a size_t instead of wrapping around.
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
 * Ends a generation into target, which returned error: after a failure the
 * text counts as empty. The array, when it has a byte, gets its '\0'
 * behind the part of the text that it holds, and *length, when length is
 * not NULL, the length of the whole text. Returns error.
 */
static clargs_error_t help_buffer_finish(help_buffer_t *target, clargs_error_t error,
                                         size_t *length)
{
    if (error != CLARGS_ERROR_NONE)
    {
        target->length = 0;
    }
    if (target->size != 0)
    {
        target->buffer[(target->length < (target->size - 1)) ? target->length : (target->size - 1)] =
            '\0';
    }
    if (length != NULL)
    {
        *length = target->length;
    }
    return error;
}

clargs_error_t clargs_help_format(const clargs_command_t *command,
                                  const clargs_help_options_t *options, char *buffer, size_t size,
                                  size_t *length)
{
    help_buffer_t target;

    assert((buffer != NULL) || (size == 0));

    target.buffer = buffer;
    target.size = size;
    target.length = 0;
    return help_buffer_finish(
        &target, clargs_help_write(command, options, help_buffer_write, &target), length);
}

clargs_error_t clargs_help_format_usage(const clargs_command_t *command,
                                        const clargs_help_options_t *options, char *buffer,
                                        size_t size, size_t *length)
{
    help_buffer_t target;

    assert((buffer != NULL) || (size == 0));

    target.buffer = buffer;
    target.size = size;
    target.length = 0;
    return help_buffer_finish(
        &target, clargs_help_write_usage(command, options, help_buffer_write, &target), length);
}
