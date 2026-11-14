/*
 * CLine - parsing: the two entries, and the scan of the arguments.
 *
 * The arguments are read once each, from the first after the name of the
 * program to the last, classified by their shape ("lex.h") and matched
 * with the schema, which is read through <cline/schema.h> only. Everything
 * that is read is recorded through the functions of "parse_internal.h":
 * this source never reads or writes the fields of a result, and it reads
 * the arguments from the copy that the result holds, so that every string
 * the result keeps a pointer to is in that copy.
 *
 * The state of the scan is an automatic variable of the scan. Nothing is
 * kept between two parses.
 */
#include <cline/parse.h>

#include "lex.h"
#include "parse_internal.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* The state of a scan. */
typedef struct scan
{
    const cline_command_t *command; /* The current command: the last selected command. */
    size_t index;                    /* The index of the argument that is being read. */
    size_t count;                    /* The number of arguments, the name of the program included. */
    bool options_ended;              /* Whether the terminator of the options was read. */
    /*
     * The index of the positional of command that takes the next
     * argument that is not an option.
     */
    size_t positional;
    bool has_digit_short;            /* Whether command has an option whose short name is a digit. */
} scan_t;

/* Makes command the current command of scan. */
static void enter_command(scan_t *scan, const cline_command_t *command)
{
    char digit;

    scan->command = command;
    scan->positional = 0;
    scan->has_digit_short = false;
    for (digit = '0'; digit <= '9'; digit++)
    {
        if (cline_command_find_short(command, digit) != NULL)
        {
            scan->has_digit_short = true;
        }
    }
}

/*
 * Records a parse error at where, in the current command of scan. value
 * and option may be NULL: an error of an argument that is not an option
 * has no option. Returns what cline_result_fail() returns.
 */
static cline_error_t fail_at(cline_result_t *result, const scan_t *scan, cline_parse_error_kind_t kind,
                              const cline_result_where_t *where, const char *value,
                              const cline_option_t *option)
{
    cline_result_failure_t failure;

    failure.kind = kind;
    failure.where = *where;
    failure.value = value;
    failure.command = scan->command;
    failure.option = option;
    failure.positional = NULL;
    return cline_result_fail(result, &failure);
}

/*
 * Checks the argument that scan is at before it is read: a wide argument
 * with a sequence that could not be converted to UTF-8 is a parse error,
 * whatever it would be read as. The argument and the value of the error
 * are the argument as the result holds it, with U+FFFD for each unit that
 * could not be converted; the error has no option and no letter.
 *
 * The result remembers the first such argument only. That is enough: the
 * scan stops there, so no later one is ever read.
 */
static cline_error_t check_encoding(cline_result_t *result, const scan_t *scan)
{
    cline_result_where_t where;

    /* The scan starts after the name of the program, and 0 is "none". */
    if (scan->index != cline_result_first_invalid_arg(result))
    {
        return CLINE_ERROR_NONE;
    }

    where.source = CLINE_SOURCE_COMMAND_LINE;
    where.index = scan->index;
    where.short_name = '\0';
    where.name_prefix = "";
    where.name = NULL;
    where.name_length = 0;
    return fail_at(result, scan, CLINE_PARSE_ERROR_INVALID_ENCODING, &where,
                   cline_result_arg(result, scan->index), NULL);
}

/*
 * Records that option, an option of the current command of scan, was given
 * by the argument that scan is at. where tells how it was written. on is
 * the state that a negatable flag was given with. value is the value that
 * was written with the option, in the same argument, or NULL when there is
 * none.
 *
 * An option that takes a value without one takes the next argument,
 * whatever it looks like, and scan moves on to it; where->index is then
 * set to that argument, the one that holds the value. The argument is
 * checked before it is taken (check_encoding()).
 */
static cline_error_t record_option(cline_result_t *result, scan_t *scan, const cline_option_t *option,
                                    bool on, const char *value, cline_result_where_t *where)
{
    const cline_option_kind_t kind = cline_option_kind(option);
    cline_error_t error;

    if (kind == CLINE_OPTION_VALUE)
    {
        if (value == NULL)
        {
            if ((scan->index + 1) >= scan->count)
            {
                return fail_at(result, scan, CLINE_PARSE_ERROR_MISSING_VALUE, where, NULL, option);
            }
            scan->index++;
            error = check_encoding(result, scan);
            if (error != CLINE_ERROR_NONE)
            {
                return error;
            }
            where->index = scan->index;
            value = cline_result_arg(result, scan->index);
        }
        return cline_result_record_option_value(result, option, value, where);
    }

    if (value != NULL)
    {
        return fail_at(result, scan, CLINE_PARSE_ERROR_UNEXPECTED_VALUE, where, value, option);
    }
    switch (kind)
    {
    case CLINE_OPTION_FLAG:
        cline_result_record_flag(result, option);
        break;
    case CLINE_OPTION_COUNT:
        cline_result_record_count(result, option);
        break;
    case CLINE_OPTION_NEGATABLE:
        cline_result_record_state(result, option, on);
        break;
    case CLINE_OPTION_VALUE:
    default:
        /*
         * An option that takes a value was recorded above. A kind that a
         * later version adds is passed over, on purpose, until the scan
         * learns what giving it means: nothing is recorded for it.
         */
        break;
    }
    return CLINE_ERROR_NONE;
}

/*
 * Reads arg, the argument that scan is at, as a long option of the current
 * command.
 */
static cline_error_t scan_long(cline_result_t *result, scan_t *scan, const char *arg)
{
    const cline_option_t *option;
    cline_result_where_t where;
    const char *value = NULL;
    bool negated = false;

    where.source = CLINE_SOURCE_COMMAND_LINE;
    where.index = scan->index;
    where.short_name = '\0';
    where.name_prefix = "--";
    cline_lex_split_long(arg, &where.name, &where.name_length, &value);

    option = cline_command_find_long(scan->command, where.name, where.name_length, &negated);
    if (option == NULL)
    {
        return fail_at(result, scan, CLINE_PARSE_ERROR_UNKNOWN_OPTION, &where, NULL, NULL);
    }
    return record_option(result, scan, option, !negated, value, &where);
}

/*
 * Reads arg, the argument that scan is at, as a bundle of short options of
 * the current command: each character after the '-' is a short name, from
 * the first to the last.
 *
 * An option that takes a value ends the bundle: the characters after its
 * letter, whatever they are, are its value, and when it is the last letter
 * it takes the next argument (record_option()). The options before a letter
 * that fails stay recorded.
 */
static cline_error_t scan_bundle(cline_result_t *result, scan_t *scan, const char *arg)
{
    const char *letter;

    for (letter = arg + 1; *letter != '\0'; letter++)
    {
        const cline_option_t *option = cline_command_find_short(scan->command, *letter);
        cline_result_where_t where;
        cline_error_t error;

        /*
         * The argument of an error is the whole bundle; the name of its
         * message is the letter.
         */
        where.source = CLINE_SOURCE_COMMAND_LINE;
        where.index = scan->index;
        where.short_name = *letter;
        where.name_prefix = "-";
        where.name = letter;
        where.name_length = 1;

        if (option == NULL)
        {
            return fail_at(result, scan, CLINE_PARSE_ERROR_UNKNOWN_OPTION, &where, NULL, NULL);
        }
        if (cline_option_kind(option) == CLINE_OPTION_VALUE)
        {
            return record_option(result, scan, option, true, (letter[1] != '\0') ? (letter + 1) : NULL, &where);
        }
        error = record_option(result, scan, option, true, NULL, &where);
        if (error != CLINE_ERROR_NONE)
        {
            return error;
        }
    }
    return CLINE_ERROR_NONE;
}

/*
 * Reads arg, the argument that scan is at, as an argument that is not an
 * option.
 *
 * For a command that has subcommands, arg is the name of one of them,
 * compared as a whole and with its case: that subcommand is added to the
 * selected commands of result and becomes the current command of scan, so
 * that the arguments that follow are read with its options, its
 * positionals from the first, and its subcommands, to any depth. A name
 * that is no subcommand of the current command is a parse error. After the
 * terminator of the options, arg is not a name: such a command has no
 * positional, and arg is one argument too many.
 *
 * For a command that has no subcommand, arg is a value of the positional
 * of the current command that the cursor of scan is at. A positional that
 * takes one argument at most moves the cursor on; one that takes any
 * number keeps it, and so takes every argument that follows. With the
 * cursor after the last positional, also for a command that has none, arg
 * is one argument too many.
 */
static cline_error_t scan_plain(cline_result_t *result, scan_t *scan, const char *arg)
{
    const cline_positional_t *positional;
    cline_result_where_t where;

    where.source = CLINE_SOURCE_COMMAND_LINE;
    where.index = scan->index;
    where.short_name = '\0';
    where.name_prefix = "";
    where.name = NULL;
    where.name_length = 0;

    if (cline_command_subcommand_count(scan->command) != 0)
    {
        const cline_command_t *subcommand;
        cline_error_t error;

        if (scan->options_ended)
        {
            return fail_at(result, scan, CLINE_PARSE_ERROR_UNEXPECTED_POSITIONAL, &where, arg, NULL);
        }
        subcommand = cline_command_find_subcommand(scan->command, arg);
        if (subcommand == NULL)
        {
            return fail_at(result, scan, CLINE_PARSE_ERROR_UNKNOWN_SUBCOMMAND, &where, arg, NULL);
        }
        error = cline_result_push_command(result, subcommand);
        if (error != CLINE_ERROR_NONE)
        {
            return error;
        }
        enter_command(scan, subcommand);
        return CLINE_ERROR_NONE;
    }

    if (scan->positional >= cline_command_positional_count(scan->command))
    {
        return fail_at(result, scan, CLINE_PARSE_ERROR_UNEXPECTED_POSITIONAL, &where, arg, NULL);
    }

    positional = cline_command_positional_at(scan->command, scan->positional);
    switch (cline_positional_arity(positional))
    {
    case CLINE_ARITY_ZERO_OR_MORE:
    case CLINE_ARITY_ONE_OR_MORE:
        break;
    case CLINE_ARITY_REQUIRED:
    case CLINE_ARITY_OPTIONAL:
    default:
        scan->positional++;
        break;
    }

    /*
     * The name of the message of a value that the value type does not
     * take is the name of the positional.
     */
    where.name = cline_positional_name(positional);
    where.name_length = strlen(where.name);
    return cline_result_record_positional_value(result, positional, arg, &where);
}

/*
 * Reads the arguments of result, which has count of them and root as its
 * only selected command, from the first after the name of the program.
 * The current command is root at first and then the subcommand that was
 * named last: an option is looked up in the current command only, never in
 * its parent or its subcommands. Each argument is checked before it is
 * read (check_encoding()). Stops at the first error.
 *
 * Returns CLINE_ERROR_NONE when all the arguments were read,
 * CLINE_ERROR_INVALID_COMMAND_LINE when a parse error was recorded, and
 * CLINE_ERROR_OUT_OF_MEMORY when an allocation failed.
 */
static cline_error_t scan_arguments(cline_result_t *result, const cline_command_t *root, size_t count)
{
    scan_t scan;

    enter_command(&scan, root);
    scan.count = count;
    scan.options_ended = false;

    for (scan.index = 1; scan.index < scan.count; scan.index++)
    {
        const char *arg = cline_result_arg(result, scan.index);
        cline_error_t error = check_encoding(result, &scan);

        if (error != CLINE_ERROR_NONE)
        {
            return error;
        }
        if (scan.options_ended)
        {
            /*
             * After the terminator, an argument is not classified: it is
             * not an option, whatever it looks like, a second "--"
             * included. The terminator stays read after a subcommand was
             * selected.
             */
            error = scan_plain(result, &scan, arg);
        }
        else
        {
            switch (cline_lex_classify(arg))
            {
            case CLINE_LEX_LONG:
                error = scan_long(result, &scan, arg);
                break;
            case CLINE_LEX_SHORT:
                /*
                 * An argument with the form of a negative number is a
                 * bundle only for a command that has a digit as a short
                 * name. For any other command it is not an option.
                 */
                if (scan.has_digit_short || !cline_lex_is_negative_number(arg))
                {
                    error = scan_bundle(result, &scan, arg);
                }
                else
                {
                    error = scan_plain(result, &scan, arg);
                }
                break;
            case CLINE_LEX_TERMINATOR:
                /* The terminator itself is not a value. */
                scan.options_ended = true;
                break;
            case CLINE_LEX_PLAIN:
            default:
                error = scan_plain(result, &scan, arg);
                break;
            }
        }
        if (error != CLINE_ERROR_NONE)
        {
            return error;
        }
    }
    return CLINE_ERROR_NONE;
}

/*
 * Parses the arguments of result, a new result of root with count
 * arguments, and writes result to *out: scans the arguments and, when the
 * scan ends without a parse error, resolves the values that were not given
 * ("parse_internal.h"). A result that cannot be completed for a lack of
 * memory is destroyed, and NULL is written to *out.
 */
static cline_error_t parse_result(cline_result_t *result, const cline_command_t *root, size_t count,
                                   cline_result_t **out)
{
    cline_error_t error = scan_arguments(result, root, count);

    if (error == CLINE_ERROR_NONE)
    {
        error = cline_resolve(result);
    }

    if (error == CLINE_ERROR_OUT_OF_MEMORY)
    {
        cline_result_destroy(result);
        *out = NULL;
        return error;
    }
    *out = result;
    return error;
}

cline_error_t cline_parse(const cline_command_t *root, int argc, char *const *argv,
                            cline_result_t **out)
{
    cline_result_t *result = NULL;
    cline_error_t error;

    /* That no argument is NULL is checked by cline_result_create(). */
    assert(root != NULL);
    assert(out != NULL);
    assert(cline_command_parent(root) == NULL);
    assert(cline_command_is_validated(root));
    assert(argc >= 0);
    assert((argc == 0) || (argv != NULL));

    error = cline_result_create(root, argc, argv, &result);
    if (error != CLINE_ERROR_NONE)
    {
        *out = NULL;
        return error;
    }
    return parse_result(result, root, (size_t)argc, out);
}

cline_error_t cline_parse_wide(const cline_command_t *root, int argc, wchar_t *const *argv,
                                 cline_result_t **out)
{
    cline_result_t *result = NULL;
    cline_error_t error;

    /* That no argument is NULL is checked by cline_result_create_wide(). */
    assert(root != NULL);
    assert(out != NULL);
    assert(cline_command_parent(root) == NULL);
    assert(cline_command_is_validated(root));
    assert(argc >= 0);
    assert((argc == 0) || (argv != NULL));

    /*
     * The arguments are converted to UTF-8 here. From here on the two
     * entries are the same.
     */
    error = cline_result_create_wide(root, argc, argv, &result);
    if (error != CLINE_ERROR_NONE)
    {
        *out = NULL;
        return error;
    }
    return parse_result(result, root, (size_t)argc, out);
}
