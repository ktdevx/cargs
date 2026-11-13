/*
 * CLArgs - the help: the public functions that generate the text, and the
 * order of its parts.
 */
#include "help_internal.h"
#include "help_item.h"

#include <assert.h>
#include <stddef.h>

/* The state of one generation. It lives on the stack of the public
 * function, for the one call. */
typedef struct help_context {
    const clargs_command_t *command; /* the command the text is about */
    size_t width;                    /* the width in columns; 0 means no limit */
    size_t indent;                   /* the blanks before each item of a list */
    size_t max_column;               /* the last column at which the descriptions may start */
    clargs_out_t out;                /* where the text goes */
} help_context_t;

/* Starts a generation for command: takes the settings from options, or
 * their default values when options is NULL, and starts the output. */
static void help_context_init(help_context_t *context, const clargs_command_t *command,
                              const clargs_help_options_t *options, clargs_help_write_fn_t write_fn,
                              void *user_data)
{
    context->command = command;
    if (options != NULL) {
        context->width = options->width;
        context->indent = options->indent;
        context->max_column = options->max_column;
    } else {
        context->width = CLARGS_HELP_DEFAULT_WIDTH;
        context->indent = CLARGS_HELP_DEFAULT_INDENT;
        context->max_column = CLARGS_HELP_DEFAULT_MAX_COLUMN;
    }
    clargs_out_init(&context->out, write_fn, user_data);
}

/* The result of a generation that allocated nothing: whether the write
 * function has reported a failure. */
static clargs_error_t help_context_result(const help_context_t *context)
{
    return clargs_out_failed(&context->out) ? CLARGS_ERROR_WRITE_FAILED : CLARGS_ERROR_NONE;
}

/* Writes the path of command: the names of the commands from the root down
 * to command, separated by one blank each. Nothing is allocated: for each
 * level, from the root down, the parents are walked again from command. */
static void help_write_path(clargs_out_t *out, const clargs_command_t *command)
{
    const clargs_command_t *ancestor;
    size_t depth = 0;
    size_t level;

    for (ancestor = clargs_command_parent(command); ancestor != NULL;
         ancestor = clargs_command_parent(ancestor)) {
        ++depth;
    }

    for (level = depth + 1; level-- > 0;) {
        size_t step;

        ancestor = command;
        for (step = 0; step < level; ++step) {
            ancestor = clargs_command_parent(ancestor);
        }

        if (level != depth) {
            clargs_out_spaces(out, 1);
        }
        clargs_help_item_string(out, clargs_command_name(ancestor));
    }
}

/* Returns whether command has an option that is not required: one that
 * takes no value, or one with a value that need not be given. */
static bool help_has_optional_option(const clargs_command_t *command)
{
    size_t count = clargs_command_option_count(command);
    size_t index;

    for (index = 0; index < count; ++index) {
        if (!clargs_option_is_required(clargs_command_option_at(command, index))) {
            return true;
        }
    }
    return false;
}

/* Separates the next item of the usage line from the one before it. */
static void help_usage_separate(help_context_t *context)
{
    clargs_out_spaces(&context->out, 1);
}

/*
 * Writes the usage line of the command of context, with the newline that
 * ends it. The items, in their order: "Usage: " and the path of the
 * command, "[OPTIONS]", the required options, the positionals and
 * "[COMMAND]".
 */
static void help_write_usage_line(help_context_t *context)
{
    const clargs_command_t *command = context->command;
    clargs_out_t *out = &context->out;
    size_t count;
    size_t index;

    /* The blank is part of the first item: it is written even when the
     * name of the root starts with a newline. */
    clargs_out_text(out, "Usage: ", 7);
    help_write_path(out, command);

    if (help_has_optional_option(command)) {
        help_usage_separate(context);
        clargs_out_string(out, "[OPTIONS]");
    }

    /* Only an option that takes a value can be required. */
    count = clargs_command_option_count(command);
    for (index = 0; index < count; ++index) {
        const clargs_option_t *option = clargs_command_option_at(command, index);

        if (clargs_option_is_required(option)) {
            help_usage_separate(context);
            clargs_help_item_usage_option(out, option);
        }
    }

    count = clargs_command_positional_count(command);
    for (index = 0; index < count; ++index) {
        help_usage_separate(context);
        clargs_help_item_positional_heading(out, clargs_command_positional_at(command, index));
    }

    if (clargs_command_subcommand_count(command) != 0) {
        help_usage_separate(context);
        clargs_out_string(out, "[COMMAND]");
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
