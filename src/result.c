/*
 * CLine - the result of a parse: its creation and destruction, the copy
 * of the arguments, the selected commands, the values, the parse error and
 * the functions that read it.
 *
 * This is the only source that reads and writes the fields of a result.
 */
#include "parse_internal.h"

#include "alloc_internal.h"
#include "message.h"
#include "utf8.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

/* Returns the frame of the selected command at index. */
static cline_result_frame_t *frame_at(const cline_result_t *result, size_t index)
{
    return (cline_result_frame_t *)cline_vec_at(&result->frames, index);
}

/* ---- The copy of the arguments ------------------------------------------ */

/*
 * Copies the count strings of argv into one allocation, the array of the
 * pointers first and the strings after it, and writes it to *out. The bytes
 * are copied as they are. With a count of 0, nothing is allocated and *out
 * is NULL.
 *
 * Returns CLINE_ERROR_OUT_OF_MEMORY when the allocation fails or the size
 * of the copy does not fit in size_t; *out is NULL then.
 */
static cline_error_t copy_args(size_t count, char *const *argv, char ***out)
{
    size_t size;
    char **args;
    char *next;
    size_t i;

    *out = NULL;
    if (count == 0)
    {
        return CLINE_ERROR_NONE;
    }

    if (count > (SIZE_MAX / sizeof(char *)))
    {
        return CLINE_ERROR_OUT_OF_MEMORY;
    }
    size = count * sizeof(char *);
    for (i = 0; i < count; ++i)
    {
        size_t length;

        assert(argv[i] != NULL);

        length = strlen(argv[i]);
        if (length > (SIZE_MAX - size))
        {
            return CLINE_ERROR_OUT_OF_MEMORY;
        }
        size += length;
        if (size == SIZE_MAX)
        {
            return CLINE_ERROR_OUT_OF_MEMORY;
        }
        size += 1;
    }

    args = (char **)cline_malloc(size);
    if (args == NULL)
    {
        return CLINE_ERROR_OUT_OF_MEMORY;
    }

    next = (char *)args + (count * sizeof(char *));
    for (i = 0; i < count; ++i)
    {
        size_t length = strlen(argv[i]) + 1;

        memcpy(next, argv[i], length);
        args[i] = next;
        next += length;
    }

    *out = args;
    return CLINE_ERROR_NONE;
}

/*
 * The same for the count wide strings of argv, which are converted to
 * UTF-8 ("utf8.h"): each unit that cannot be converted becomes U+FFFD.
 * *first_invalid is set to the index of the first string after argv[0]
 * that has such a unit, or to 0 when there is none; a unit of argv[0] that
 * cannot be converted is replaced and not reported.
 *
 * The length of one string in UTF-8 is not checked against size_t. A unit
 * gives four bytes at most where it has four itself, so the count is not
 * more than the size of the wide string, an object that exists. Where a
 * unit has two bytes it gives three at most, which is more than it has:
 * there the count stays in range because an object has PTRDIFF_MAX bytes
 * at most, so a string has SIZE_MAX / 4 units at most, and three times
 * that is less than SIZE_MAX. The sum over the strings is checked as in
 * copy_args().
 */
static cline_error_t copy_args_wide(size_t count, wchar_t *const *argv, char ***out, size_t *first_invalid)
{
    size_t size;
    char **args;
    char *next;
    size_t i;

    *out = NULL;
    *first_invalid = 0;
    if (count == 0)
    {
        return CLINE_ERROR_NONE;
    }

    if (count > (SIZE_MAX / sizeof(char *)))
    {
        return CLINE_ERROR_OUT_OF_MEMORY;
    }
    size = count * sizeof(char *);
    for (i = 0; i < count; ++i)
    {
        size_t length;
        bool valid;

        assert(argv[i] != NULL);

        length = cline_utf8_length(argv[i], &valid);
        if (!valid && (i != 0) && (*first_invalid == 0))
        {
            *first_invalid = i;
        }
        if (length > (SIZE_MAX - size))
        {
            return CLINE_ERROR_OUT_OF_MEMORY;
        }
        size += length;
        if (size == SIZE_MAX)
        {
            return CLINE_ERROR_OUT_OF_MEMORY;
        }
        size += 1;
    }

    args = (char **)cline_malloc(size);
    if (args == NULL)
    {
        return CLINE_ERROR_OUT_OF_MEMORY;
    }

    next = (char *)args + (count * sizeof(char *));
    for (i = 0; i < count; ++i)
    {
        args[i] = next;
        next += cline_utf8_write(argv[i], next) + 1;
    }

    *out = args;
    return CLINE_ERROR_NONE;
}

/* ---- Creation and destruction ------------------------------------------- */

/*
 * Allocates a result of root with no argument, no selected command and no
 * parse error, and writes it to *out. Returns CLINE_ERROR_OUT_OF_MEMORY
 * when the allocation fails; *out is NULL then.
 */
static cline_error_t new_result(const cline_command_t *root, cline_result_t **out)
{
    cline_result_t *result = (cline_result_t *)cline_malloc(sizeof(*result));

    *out = result;
    if (result == NULL)
    {
        return CLINE_ERROR_OUT_OF_MEMORY;
    }

    result->root = root;
    result->args = NULL;
    result->arg_count = 0;
    result->first_invalid_arg = 0;
    cline_vec_init(&result->frames, sizeof(cline_result_frame_t));
    result->error = CLINE_ERROR_NONE;
    result->error_kind = CLINE_PARSE_ERROR_NONE;
    result->error_source = CLINE_SOURCE_NONE;
    result->error_index = 0;
    result->error_argument = NULL;
    result->error_value = NULL;
    result->error_command = NULL;
    result->error_option = NULL;
    result->error_positional = NULL;
    result->error_short_name = '\0';
    result->error_message = NULL;
    return CLINE_ERROR_NONE;
}

/*
 * Ends the creation of result, whose copy of count arguments was made with
 * the outcome error: selects root and writes result to *out. When the copy
 * failed, or root cannot be selected, result is destroyed as a whole and
 * *out stays NULL. The two functions that create a result differ in the
 * copy of the arguments only.
 */
static cline_error_t finish_result(cline_result_t *result, cline_error_t error, size_t count,
                                    cline_result_t **out)
{
    if (error == CLINE_ERROR_NONE)
    {
        result->arg_count = count;
        error = cline_result_push_command(result, result->root);
    }
    if (error != CLINE_ERROR_NONE)
    {
        cline_result_destroy(result);
        return error;
    }

    *out = result;
    return CLINE_ERROR_NONE;
}

cline_error_t cline_result_create(const cline_command_t *root, int argc, char *const *argv,
                                    cline_result_t **out)
{
    cline_result_t *result;
    cline_error_t error;

    assert(root != NULL);
    assert(out != NULL);
    assert(cline_command_parent(root) == NULL);
    assert(argc >= 0);
    assert((argc == 0) || (argv != NULL));

    *out = NULL;

    error = new_result(root, &result);
    if (error != CLINE_ERROR_NONE)
    {
        return error;
    }
    error = copy_args((size_t)argc, argv, &result->args);
    return finish_result(result, error, (size_t)argc, out);
}

cline_error_t cline_result_create_wide(const cline_command_t *root, int argc, wchar_t *const *argv,
                                         cline_result_t **out)
{
    cline_result_t *result;
    cline_error_t error;

    assert(root != NULL);
    assert(out != NULL);
    assert(cline_command_parent(root) == NULL);
    assert(argc >= 0);
    assert((argc == 0) || (argv != NULL));

    *out = NULL;

    error = new_result(root, &result);
    if (error != CLINE_ERROR_NONE)
    {
        return error;
    }
    error = copy_args_wide((size_t)argc, argv, &result->args, &result->first_invalid_arg);
    return finish_result(result, error, (size_t)argc, out);
}

/*
 * Cleans up the value of type in the area value, and frees the area. This
 * is the one place that releases a value.
 */
static void destroy_value(const cline_type_t *type, void *value)
{
    cline_type_release(type, value);
    cline_free(value);
}

/*
 * Cleans up every value that the slots of frame hold, once, and frees the
 * slots.
 */
static void destroy_frame(cline_result_frame_t *frame)
{
    size_t i;
    size_t k;

    for (i = 0; i < frame->option_count; ++i)
    {
        if (frame->options[i].value != NULL)
        {
            destroy_value(cline_option_type(cline_command_option_at(frame->command, i)),
                          frame->options[i].value);
        }
    }
    for (i = 0; i < frame->positional_count; ++i)
    {
        cline_vec_t *values = &frame->positionals[i].values;
        const cline_type_t *type =
            cline_positional_type(cline_command_positional_at(frame->command, i));

        for (k = 0; k < values->count; ++k)
        {
            destroy_value(type, *(void **)cline_vec_at(values, k));
        }
        cline_vec_destroy(values);
    }
    cline_free(frame->positionals);
    cline_free(frame->options);
}

void cline_result_destroy(cline_result_t *result)
{
    size_t i;

    if (result == NULL)
    {
        return;
    }

    for (i = 0; i < result->frames.count; ++i)
    {
        destroy_frame(frame_at(result, i));
    }
    cline_vec_destroy(&result->frames);
    cline_free(result->error_message);
    cline_free(result->args);
    cline_free(result);
}

/* ---- The arguments ------------------------------------------------------ */

const char *cline_result_arg(const cline_result_t *result, size_t index)
{
    assert(result != NULL);
    assert(index < result->arg_count);

    return result->args[index];
}

size_t cline_result_first_invalid_arg(const cline_result_t *result)
{
    assert(result != NULL);

    return result->first_invalid_arg;
}

/* ---- The selected commands ---------------------------------------------- */

/* Puts slot in the state of option when it is not given. */
static void init_option_slot(cline_option_slot_t *slot, const cline_option_t *option)
{
    slot->source = CLINE_SOURCE_NONE;
    slot->on = false;
    slot->count = 0;
    slot->value = NULL;

    switch (cline_option_kind(option))
    {
    case CLINE_OPTION_FLAG:
    case CLINE_OPTION_COUNT:
        slot->source = CLINE_SOURCE_DEFAULT;
        break;
    case CLINE_OPTION_NEGATABLE:
        slot->source = CLINE_SOURCE_DEFAULT;
        slot->on = cline_option_initial(option);
        break;
    case CLINE_OPTION_VALUE:
    default:
        break;
    }
}

/*
 * Fills frame for command: allocates and initializes one slot for each of
 * its options and positionals. An array of no slots is not allocated.
 * Returns CLINE_ERROR_OUT_OF_MEMORY when an allocation fails; nothing is
 * left allocated then.
 */
static cline_error_t init_frame(cline_result_frame_t *frame, const cline_command_t *command)
{
    size_t i;

    frame->command = command;
    frame->options = NULL;
    frame->option_count = cline_command_option_count(command);
    frame->positionals = NULL;
    frame->positional_count = cline_command_positional_count(command);

    if (frame->option_count > 0)
    {
        frame->options =
            (cline_option_slot_t *)cline_calloc(frame->option_count, sizeof(*frame->options));
        if (frame->options == NULL)
        {
            return CLINE_ERROR_OUT_OF_MEMORY;
        }
        for (i = 0; i < frame->option_count; ++i)
        {
            init_option_slot(&frame->options[i], cline_command_option_at(command, i));
        }
    }

    if (frame->positional_count > 0)
    {
        frame->positionals = (cline_positional_slot_t *)cline_calloc(frame->positional_count,
                                                                       sizeof(*frame->positionals));
        if (frame->positionals == NULL)
        {
            cline_free(frame->options);
            return CLINE_ERROR_OUT_OF_MEMORY;
        }
        for (i = 0; i < frame->positional_count; ++i)
        {
            frame->positionals[i].source = CLINE_SOURCE_NONE;
            cline_vec_init(&frame->positionals[i].values, sizeof(void *));
        }
    }

    return CLINE_ERROR_NONE;
}

cline_error_t cline_result_push_command(cline_result_t *result, const cline_command_t *command)
{
    cline_result_frame_t frame;
    cline_error_t error;

    assert(result != NULL);
    assert(command != NULL);
    assert((result->frames.count > 0) || (command == result->root));
    assert((result->frames.count == 0) ||
           (cline_command_parent(command) == frame_at(result, result->frames.count - 1)->command));

    error = init_frame(&frame, command);
    if (error != CLINE_ERROR_NONE)
    {
        return error;
    }

    error = cline_vec_push(&result->frames, &frame);
    if (error != CLINE_ERROR_NONE)
    {
        destroy_frame(&frame);
        return error;
    }
    return CLINE_ERROR_NONE;
}

/* ---- The slots of the options ------------------------------------------- */

/*
 * Returns the root of the schema of command: the command that has no
 * parent, reached from command through the parents.
 */
static const cline_command_t *root_of(const cline_command_t *command)
{
    const cline_command_t *parent = cline_command_parent(command);

    while (parent != NULL)
    {
        command = parent;
        parent = cline_command_parent(command);
    }
    return command;
}

/*
 * Returns the slot of option in result, or NULL when the command of option
 * is not a selected command of result.
 *
 * The frame is that of the command of option, and the slot has the index
 * that option has in that command. A command of another schema is never
 * selected, so its options have no slot.
 */
static cline_option_slot_t *find_option_slot(const cline_result_t *result,
                                              const cline_option_t *option)
{
    const cline_command_t *command = cline_option_command(option);
    size_t i;
    size_t k;

    if (root_of(command) != result->root)
    {
        return NULL;
    }

    for (i = 0; i < result->frames.count; ++i)
    {
        cline_result_frame_t *frame = frame_at(result, i);

        if (frame->command != command)
        {
            continue;
        }
        for (k = 0; k < frame->option_count; ++k)
        {
            if (cline_command_option_at(command, k) == option)
            {
                return &frame->options[k];
            }
        }
        return NULL;
    }
    return NULL;
}

void cline_result_record_flag(cline_result_t *result, const cline_option_t *option)
{
    cline_option_slot_t *slot;

    assert(result != NULL);
    assert(option != NULL);
    assert(cline_option_kind(option) == CLINE_OPTION_FLAG);

    slot = find_option_slot(result, option);
    assert(slot != NULL);

    slot->on = true;
    slot->source = CLINE_SOURCE_COMMAND_LINE;
}

void cline_result_record_state(cline_result_t *result, const cline_option_t *option, bool on)
{
    cline_option_slot_t *slot;

    assert(result != NULL);
    assert(option != NULL);
    assert(cline_option_kind(option) == CLINE_OPTION_NEGATABLE);

    slot = find_option_slot(result, option);
    assert(slot != NULL);

    slot->on = on;
    slot->source = CLINE_SOURCE_COMMAND_LINE;
}

void cline_result_record_count(cline_result_t *result, const cline_option_t *option)
{
    cline_option_slot_t *slot;

    assert(result != NULL);
    assert(option != NULL);
    assert(cline_option_kind(option) == CLINE_OPTION_COUNT);

    slot = find_option_slot(result, option);
    assert(slot != NULL);

    slot->count += 1;
    slot->source = CLINE_SOURCE_COMMAND_LINE;
}

bool cline_result_flag(const cline_result_t *result, const cline_option_t *option)
{
    const cline_option_slot_t *slot;

    assert(result != NULL);
    assert(option != NULL);
    assert(root_of(cline_option_command(option)) == result->root);
    assert((cline_option_kind(option) == CLINE_OPTION_FLAG) ||
           (cline_option_kind(option) == CLINE_OPTION_NEGATABLE));

    slot = find_option_slot(result, option);
    if (slot == NULL)
    {
        /*
         * Not given: false for a flag, the initial state for a negatable
         * flag.
         */
        return cline_option_initial(option);
    }
    return slot->on;
}

size_t cline_result_count(const cline_result_t *result, const cline_option_t *option)
{
    const cline_option_slot_t *slot;

    assert(result != NULL);
    assert(option != NULL);
    assert(root_of(cline_option_command(option)) == result->root);
    assert(cline_option_kind(option) == CLINE_OPTION_COUNT);

    slot = find_option_slot(result, option);
    if (slot == NULL)
    {
        return 0;
    }
    return slot->count;
}

cline_source_t cline_result_option_source(const cline_result_t *result,
                                            const cline_option_t *option)
{
    const cline_option_slot_t *slot;

    assert(result != NULL);
    assert(option != NULL);
    assert(root_of(cline_option_command(option)) == result->root);

    slot = find_option_slot(result, option);
    if (slot == NULL)
    {
        return CLINE_SOURCE_NONE;
    }
    return slot->source;
}

/* ---- The values --------------------------------------------------------- */

/*
 * Returns the slot of positional in result, or NULL when the command of
 * positional is not a selected command of result. The slot is found as
 * that of an option is; see find_option_slot().
 */
static cline_positional_slot_t *find_positional_slot(const cline_result_t *result,
                                                      const cline_positional_t *positional)
{
    const cline_command_t *command = cline_positional_command(positional);
    size_t i;
    size_t k;

    if (root_of(command) != result->root)
    {
        return NULL;
    }

    for (i = 0; i < result->frames.count; ++i)
    {
        cline_result_frame_t *frame = frame_at(result, i);

        if (frame->command != command)
        {
            continue;
        }
        for (k = 0; k < frame->positional_count; ++k)
        {
            if (cline_command_positional_at(command, k) == positional)
            {
                return &frame->positionals[k];
            }
        }
        return NULL;
    }
    return NULL;
}

/*
 * Allocates the area of a value of type, converts text into it and writes
 * the area to *out. text is passed to the value type as it is. This is the
 * one place that converts a value.
 *
 * Returns CLINE_ERROR_NONE on success; the caller then owns the area and
 * the value in it (destroy_value()). Returns CLINE_ERROR_OUT_OF_MEMORY
 * when the allocation of the area fails, and what cline_type_convert()
 * returned when the conversion fails; the area is freed, without a cleanup
 * of the value, and *out is NULL then.
 */
static cline_error_t convert_value(const cline_type_t *type, const char *text, void **out)
{
    void *value = cline_malloc(cline_type_value_size(type));
    cline_error_t error;

    *out = NULL;
    if (value == NULL)
    {
        return CLINE_ERROR_OUT_OF_MEMORY;
    }

    error = cline_type_convert(type, text, value);
    if (error != CLINE_ERROR_NONE)
    {
        cline_free(value);
        return error;
    }

    *out = value;
    return CLINE_ERROR_NONE;
}

/* ---- The parse error ---------------------------------------------------- */

/*
 * Records the parse error failure in result: builds its message and, when
 * that succeeded, writes the fields of the error. This is the one place
 * that makes a result one of a parse error.
 *
 * The message is built first, so a lack of memory leaves result as it was:
 * no part of the error is recorded, and result still reports
 * CLINE_ERROR_NONE.
 *
 * Returns CLINE_ERROR_INVALID_COMMAND_LINE when the error was recorded
 * and CLINE_ERROR_OUT_OF_MEMORY when the allocation of the message fails.
 */
static cline_error_t record_error(cline_result_t *result, const cline_result_failure_t *failure)
{
    const cline_result_where_t *where = &failure->where;
    cline_message_parts_t parts;
    cline_error_t error;
    char *message;

    parts.kind = failure->kind;
    parts.source = where->source;
    parts.for_positional = failure->positional != NULL;
    parts.name_prefix = (where->name_prefix != NULL) ? where->name_prefix : "";
    parts.name = where->name;
    parts.name_length = where->name_length;
    parts.value = failure->value;
    parts.type_name = NULL;
    if (failure->kind == CLINE_PARSE_ERROR_INVALID_VALUE)
    {
        /*
         * The value type is that of the option or the positional that the
         * value is for.
         */
        parts.type_name = cline_type_name((failure->positional != NULL) ?
                                           cline_positional_type(failure->positional) :
                                           cline_option_type(failure->option));
    }

    error = cline_message_build(&parts, &message);
    if (error != CLINE_ERROR_NONE)
    {
        return error;
    }

    result->error = CLINE_ERROR_INVALID_COMMAND_LINE;
    result->error_kind = failure->kind;
    result->error_source = where->source;
    result->error_index = where->index;
    /* Only an error on the command line is in an argument. */
    result->error_argument =
        (where->source == CLINE_SOURCE_COMMAND_LINE) ? cline_result_arg(result, where->index) : NULL;
    result->error_value = failure->value;
    result->error_command = failure->command;
    result->error_option = failure->option;
    result->error_positional = failure->positional;
    result->error_short_name = where->short_name;
    result->error_message = message;
    return CLINE_ERROR_INVALID_COMMAND_LINE;
}

cline_error_t cline_result_fail(cline_result_t *result, const cline_result_failure_t *failure)
{
    assert(result != NULL);
    assert(failure != NULL);
    assert(failure->kind != CLINE_PARSE_ERROR_NONE);
    assert(failure->command != NULL);
    assert(result->error == CLINE_ERROR_NONE);
    assert((failure->where.source != CLINE_SOURCE_COMMAND_LINE) || (failure->where.index < result->arg_count));
    assert((failure->kind != CLINE_PARSE_ERROR_INVALID_VALUE) || (failure->option != NULL) ||
           (failure->positional != NULL));

    return record_error(result, failure);
}

/*
 * What a record of a value returns when convert_value() failed with error
 * for text, at where: the lack of memory as it is and, for every other
 * failure, the parse error of a value that its value type does not take,
 * which is recorded in result as cline_result_fail() does. option or
 * positional is the one that the value is for; the other is NULL.
 */
static cline_error_t conversion_failure(cline_result_t *result, cline_error_t error, const char *text,
                                         const cline_result_where_t *where,
                                         const cline_option_t *option,
                                         const cline_positional_t *positional)
{
    cline_result_failure_t failure;

    if (error == CLINE_ERROR_OUT_OF_MEMORY)
    {
        return CLINE_ERROR_OUT_OF_MEMORY;
    }

    failure.kind = CLINE_PARSE_ERROR_INVALID_VALUE;
    failure.where = *where;
    failure.value = text;
    failure.command =
        (positional != NULL) ? cline_positional_command(positional) : cline_option_command(option);
    failure.option = option;
    failure.positional = positional;
    return cline_result_fail(result, &failure);
}

cline_error_t cline_result_record_option_value(cline_result_t *result, const cline_option_t *option,
                                                 const char *text, const cline_result_where_t *where)
{
    const cline_type_t *type;
    cline_option_slot_t *slot;
    cline_error_t error;
    void *value;

    assert(result != NULL);
    assert(option != NULL);
    assert(text != NULL);
    assert(where != NULL);
    assert(cline_option_kind(option) == CLINE_OPTION_VALUE);

    slot = find_option_slot(result, option);
    assert(slot != NULL);

    /*
     * The new value is converted before the old one is touched: when the
     * conversion fails, the old value stays.
     */
    type = cline_option_type(option);
    error = convert_value(type, text, &value);
    if (error != CLINE_ERROR_NONE)
    {
        return conversion_failure(result, error, text, where, option, NULL);
    }

    if (slot->value != NULL)
    {
        destroy_value(type, slot->value);
    }
    slot->value = value;
    slot->source = where->source;
    return CLINE_ERROR_NONE;
}

cline_error_t cline_result_record_positional_value(cline_result_t *result,
                                                     const cline_positional_t *positional,
                                                     const char *text, const cline_result_where_t *where)
{
    const cline_type_t *type;
    cline_positional_slot_t *slot;
    cline_error_t error;
    void *value;

    assert(result != NULL);
    assert(positional != NULL);
    assert(text != NULL);
    assert(where != NULL);

    slot = find_positional_slot(result, positional);
    assert(slot != NULL);

    type = cline_positional_type(positional);
    error = convert_value(type, text, &value);
    if (error != CLINE_ERROR_NONE)
    {
        return conversion_failure(result, error, text, where, NULL, positional);
    }

    error = cline_vec_push(&slot->values, &value);
    if (error != CLINE_ERROR_NONE)
    {
        /* The value is not kept, so it is cleaned up here. */
        destroy_value(type, value);
        return error;
    }
    slot->source = where->source;
    return CLINE_ERROR_NONE;
}

/*
 * Takes back the default values in the slots of frame: see
 * cline_result_discard_defaults().
 */
static void discard_frame_defaults(cline_result_frame_t *frame)
{
    size_t i;
    size_t k;

    for (i = 0; i < frame->option_count; ++i)
    {
        cline_option_slot_t *slot = &frame->options[i];

        /*
         * A flag, a count and a negatable flag that were not given have
         * the source of a default value too, but never a value.
         */
        if ((slot->source != CLINE_SOURCE_DEFAULT) || (slot->value == NULL))
        {
            continue;
        }
        destroy_value(cline_option_type(cline_command_option_at(frame->command, i)), slot->value);
        slot->value = NULL;
        slot->source = CLINE_SOURCE_NONE;
    }

    for (i = 0; i < frame->positional_count; ++i)
    {
        cline_positional_slot_t *slot = &frame->positionals[i];
        const cline_type_t *type;

        if (slot->source != CLINE_SOURCE_DEFAULT)
        {
            continue;
        }
        type = cline_positional_type(cline_command_positional_at(frame->command, i));
        for (k = 0; k < slot->values.count; ++k)
        {
            destroy_value(type, *(void **)cline_vec_at(&slot->values, k));
        }
        /* The slot is as it was when the frame was made. */
        cline_vec_destroy(&slot->values);
        cline_vec_init(&slot->values, sizeof(void *));
        slot->source = CLINE_SOURCE_NONE;
    }
}

void cline_result_discard_defaults(cline_result_t *result)
{
    size_t i;

    assert(result != NULL);

    for (i = 0; i < result->frames.count; ++i)
    {
        discard_frame_defaults(frame_at(result, i));
    }
}

/*
 * Returns the area of the value of option in result, or NULL when option
 * has no value or its command is not a selected command of result.
 */
static const void *option_value(const cline_result_t *result, const cline_option_t *option)
{
    const cline_option_slot_t *slot = find_option_slot(result, option);

    if (slot == NULL)
    {
        return NULL;
    }
    return slot->value;
}

bool cline_result_option_string(const cline_result_t *result, const cline_option_t *option,
                                 const char **out)
{
    const void *value;

    assert(result != NULL);
    assert(option != NULL);
    assert(out != NULL);
    assert(root_of(cline_option_command(option)) == result->root);
    assert(cline_option_kind(option) == CLINE_OPTION_VALUE);
    assert(cline_type_kind(cline_option_type(option)) == CLINE_TYPE_STRING);

    value = option_value(result, option);
    if (value == NULL)
    {
        return false;
    }
    *out = *(const char *const *)value;
    return true;
}

bool cline_result_option_int(const cline_result_t *result, const cline_option_t *option, int64_t *out)
{
    const void *value;

    assert(result != NULL);
    assert(option != NULL);
    assert(out != NULL);
    assert(root_of(cline_option_command(option)) == result->root);
    assert(cline_option_kind(option) == CLINE_OPTION_VALUE);
    assert(cline_type_kind(cline_option_type(option)) == CLINE_TYPE_INT);

    value = option_value(result, option);
    if (value == NULL)
    {
        return false;
    }
    *out = *(const int64_t *)value;
    return true;
}

bool cline_result_option_float(const cline_result_t *result, const cline_option_t *option, double *out)
{
    const void *value;

    assert(result != NULL);
    assert(option != NULL);
    assert(out != NULL);
    assert(root_of(cline_option_command(option)) == result->root);
    assert(cline_option_kind(option) == CLINE_OPTION_VALUE);
    assert(cline_type_kind(cline_option_type(option)) == CLINE_TYPE_FLOAT);

    value = option_value(result, option);
    if (value == NULL)
    {
        return false;
    }
    *out = *(const double *)value;
    return true;
}

bool cline_result_option_choice(const cline_result_t *result, const cline_option_t *option, size_t *out)
{
    const void *value;

    assert(result != NULL);
    assert(option != NULL);
    assert(out != NULL);
    assert(root_of(cline_option_command(option)) == result->root);
    assert(cline_option_kind(option) == CLINE_OPTION_VALUE);
    assert(cline_type_kind(cline_option_type(option)) == CLINE_TYPE_CHOICE);

    value = option_value(result, option);
    if (value == NULL)
    {
        return false;
    }
    *out = *(const size_t *)value;
    return true;
}

const void *cline_result_option_value(const cline_result_t *result, const cline_option_t *option)
{
    assert(result != NULL);
    assert(option != NULL);
    assert(root_of(cline_option_command(option)) == result->root);
    assert(cline_option_kind(option) == CLINE_OPTION_VALUE);

    return option_value(result, option);
}

size_t cline_result_positional_count(const cline_result_t *result, const cline_positional_t *positional)
{
    const cline_positional_slot_t *slot;

    assert(result != NULL);
    assert(positional != NULL);
    assert(root_of(cline_positional_command(positional)) == result->root);

    slot = find_positional_slot(result, positional);
    if (slot == NULL)
    {
        return 0;
    }
    return slot->values.count;
}

cline_source_t cline_result_positional_source(const cline_result_t *result,
                                                const cline_positional_t *positional)
{
    const cline_positional_slot_t *slot;

    assert(result != NULL);
    assert(positional != NULL);
    assert(root_of(cline_positional_command(positional)) == result->root);

    slot = find_positional_slot(result, positional);
    if (slot == NULL)
    {
        return CLINE_SOURCE_NONE;
    }
    return slot->source;
}

/*
 * Returns the area of the value of positional at index in result, or NULL
 * when there is no value at index or the command of positional is not a
 * selected command of result.
 */
static const void *positional_value(const cline_result_t *result, const cline_positional_t *positional,
                                    size_t index)
{
    const cline_positional_slot_t *slot = find_positional_slot(result, positional);

    if ((slot == NULL) || (index >= slot->values.count))
    {
        return NULL;
    }
    return *(void *const *)cline_vec_at(&slot->values, index);
}

bool cline_result_positional_string(const cline_result_t *result, const cline_positional_t *positional,
                                     size_t index, const char **out)
{
    const void *value;

    assert(result != NULL);
    assert(positional != NULL);
    assert(out != NULL);
    assert(root_of(cline_positional_command(positional)) == result->root);
    assert(cline_type_kind(cline_positional_type(positional)) == CLINE_TYPE_STRING);

    value = positional_value(result, positional, index);
    if (value == NULL)
    {
        return false;
    }
    *out = *(const char *const *)value;
    return true;
}

bool cline_result_positional_int(const cline_result_t *result, const cline_positional_t *positional,
                                  size_t index, int64_t *out)
{
    const void *value;

    assert(result != NULL);
    assert(positional != NULL);
    assert(out != NULL);
    assert(root_of(cline_positional_command(positional)) == result->root);
    assert(cline_type_kind(cline_positional_type(positional)) == CLINE_TYPE_INT);

    value = positional_value(result, positional, index);
    if (value == NULL)
    {
        return false;
    }
    *out = *(const int64_t *)value;
    return true;
}

bool cline_result_positional_float(const cline_result_t *result, const cline_positional_t *positional,
                                    size_t index, double *out)
{
    const void *value;

    assert(result != NULL);
    assert(positional != NULL);
    assert(out != NULL);
    assert(root_of(cline_positional_command(positional)) == result->root);
    assert(cline_type_kind(cline_positional_type(positional)) == CLINE_TYPE_FLOAT);

    value = positional_value(result, positional, index);
    if (value == NULL)
    {
        return false;
    }
    *out = *(const double *)value;
    return true;
}

bool cline_result_positional_choice(const cline_result_t *result, const cline_positional_t *positional,
                                     size_t index, size_t *out)
{
    const void *value;

    assert(result != NULL);
    assert(positional != NULL);
    assert(out != NULL);
    assert(root_of(cline_positional_command(positional)) == result->root);
    assert(cline_type_kind(cline_positional_type(positional)) == CLINE_TYPE_CHOICE);

    value = positional_value(result, positional, index);
    if (value == NULL)
    {
        return false;
    }
    *out = *(const size_t *)value;
    return true;
}

const void *cline_result_positional_value(const cline_result_t *result,
                                           const cline_positional_t *positional, size_t index)
{
    assert(result != NULL);
    assert(positional != NULL);
    assert(root_of(cline_positional_command(positional)) == result->root);

    return positional_value(result, positional, index);
}

/* ---- The outcome and the selected commands ------------------------------ */

cline_error_t cline_result_error(const cline_result_t *result)
{
    assert(result != NULL);

    return result->error;
}

size_t cline_result_command_count(const cline_result_t *result)
{
    assert(result != NULL);

    return result->frames.count;
}

const cline_command_t *cline_result_command_at(const cline_result_t *result, size_t index)
{
    assert(result != NULL);
    assert(index < result->frames.count);

    return frame_at(result, index)->command;
}

const cline_command_t *cline_result_command(const cline_result_t *result)
{
    assert(result != NULL);
    assert(result->frames.count > 0);

    return frame_at(result, result->frames.count - 1)->command;
}

/* ---- The information of the parse error --------------------------------- */

/*
 * The fields below have the values of "none" until record_error() writes
 * them, so a result without a parse error needs no case of its own.
 */

cline_parse_error_kind_t cline_result_error_kind(const cline_result_t *result)
{
    assert(result != NULL);

    return result->error_kind;
}

const char *cline_result_error_message(const cline_result_t *result)
{
    assert(result != NULL);

    return result->error_message;
}

const char *cline_result_error_argument(const cline_result_t *result)
{
    assert(result != NULL);

    return result->error_argument;
}

const char *cline_result_error_value(const cline_result_t *result)
{
    assert(result != NULL);

    return result->error_value;
}

cline_source_t cline_result_error_source(const cline_result_t *result)
{
    assert(result != NULL);

    return result->error_source;
}

size_t cline_result_error_index(const cline_result_t *result)
{
    assert(result != NULL);

    return result->error_index;
}

const cline_command_t *cline_result_error_command(const cline_result_t *result)
{
    assert(result != NULL);

    return result->error_command;
}

const cline_option_t *cline_result_error_option(const cline_result_t *result)
{
    assert(result != NULL);

    return result->error_option;
}

const cline_positional_t *cline_result_error_positional(const cline_result_t *result)
{
    assert(result != NULL);

    return result->error_positional;
}

char cline_result_error_short_name(const cline_result_t *result)
{
    assert(result != NULL);

    return result->error_short_name;
}
