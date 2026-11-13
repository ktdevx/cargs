/*
 * CLArgs - the result of a parse: its creation and destruction, the copy
 * of the arguments, the selected commands, the values and the functions
 * that read it.
 *
 * This is the only source that reads and writes the fields of a result.
 */
#include "parse_internal.h"

#include "alloc_internal.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

/* Returns the frame of the selected command at index. */
static clargs_result_frame_t *frame_at(const clargs_result_t *result, size_t index)
{
    return (clargs_result_frame_t *)clargs_vec_at(&result->frames, index);
}

/* ---- The copy of the arguments ------------------------------------------ */

/*
 * Copies the count strings of argv into one allocation, the array of the
 * pointers first and the strings after it, and writes it to *out. The bytes
 * are copied as they are. With a count of 0, nothing is allocated and *out
 * is NULL.
 *
 * Returns CLARGS_ERROR_OUT_OF_MEMORY when the allocation fails or the size
 * of the copy does not fit in size_t; *out is NULL then.
 */
static clargs_error_t copy_args(size_t count, char *const *argv, char ***out)
{
    size_t size;
    char **args;
    char *next;
    size_t i;

    *out = NULL;
    if (count == 0) {
        return CLARGS_ERROR_NONE;
    }

    if (count > SIZE_MAX / sizeof(char *)) {
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }
    size = count * sizeof(char *);
    for (i = 0; i < count; ++i) {
        size_t length;

        assert(argv[i] != NULL);

        length = strlen(argv[i]);
        if (length > SIZE_MAX - size) {
            return CLARGS_ERROR_OUT_OF_MEMORY;
        }
        size += length;
        if (size == SIZE_MAX) {
            return CLARGS_ERROR_OUT_OF_MEMORY;
        }
        size += 1;
    }

    args = (char **)clargs_malloc(size);
    if (args == NULL) {
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }

    next = (char *)args + count * sizeof(char *);
    for (i = 0; i < count; ++i) {
        size_t length = strlen(argv[i]) + 1;

        memcpy(next, argv[i], length);
        args[i] = next;
        next += length;
    }

    *out = args;
    return CLARGS_ERROR_NONE;
}

/* ---- Creation and destruction ------------------------------------------- */

/*
 * Allocates a result of root with no argument, no selected command and no
 * parse error, and writes it to *out. Returns CLARGS_ERROR_OUT_OF_MEMORY
 * when the allocation fails; *out is NULL then.
 */
static clargs_error_t new_result(const clargs_command_t *root, clargs_result_t **out)
{
    clargs_result_t *result = (clargs_result_t *)clargs_malloc(sizeof(*result));

    *out = result;
    if (result == NULL) {
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }

    result->root = root;
    result->args = NULL;
    result->arg_count = 0;
    result->first_invalid_arg = 0;
    clargs_vec_init(&result->frames, sizeof(clargs_result_frame_t));
    result->error = CLARGS_ERROR_NONE;
    result->error_kind = CLARGS_PARSE_ERROR_NONE;
    result->error_source = CLARGS_SOURCE_NONE;
    result->error_index = 0;
    result->error_argument = NULL;
    result->error_value = NULL;
    result->error_command = NULL;
    result->error_option = NULL;
    result->error_positional = NULL;
    result->error_short_name = '\0';
    result->error_message = NULL;
    return CLARGS_ERROR_NONE;
}

clargs_error_t clargs_result_create(const clargs_command_t *root, int argc, char *const *argv,
                                    clargs_result_t **out)
{
    clargs_result_t *result;
    clargs_error_t error;

    assert(root != NULL);
    assert(out != NULL);
    assert(clargs_command_parent(root) == NULL);
    assert(argc >= 0);
    assert(argc == 0 || argv != NULL);

    *out = NULL;

    error = new_result(root, &result);
    if (error != CLARGS_ERROR_NONE) {
        return error;
    }

    /* From here on the result is destroyed as a whole when a step fails. */
    error = copy_args((size_t)argc, argv, &result->args);
    if (error == CLARGS_ERROR_NONE) {
        result->arg_count = (size_t)argc;
        error = clargs_result_push_command(result, root);
    }
    if (error != CLARGS_ERROR_NONE) {
        clargs_result_destroy(result);
        return error;
    }

    *out = result;
    return CLARGS_ERROR_NONE;
}

/* Cleans up the value of type in the area value, and frees the area. This
 * is the one place that releases a value. */
static void destroy_value(const clargs_type_t *type, void *value)
{
    clargs_type_release(type, value);
    clargs_free(value);
}

/* Cleans up every value that the slots of frame hold, once, and frees the
 * slots. */
static void destroy_frame(clargs_result_frame_t *frame)
{
    size_t i;
    size_t k;

    for (i = 0; i < frame->option_count; ++i) {
        if (frame->options[i].value != NULL) {
            destroy_value(clargs_option_type(clargs_command_option_at(frame->command, i)),
                          frame->options[i].value);
        }
    }
    for (i = 0; i < frame->positional_count; ++i) {
        clargs_vec_t *values = &frame->positionals[i].values;
        const clargs_type_t *type =
            clargs_positional_type(clargs_command_positional_at(frame->command, i));

        for (k = 0; k < values->count; ++k) {
            destroy_value(type, *(void **)clargs_vec_at(values, k));
        }
        clargs_vec_destroy(values);
    }
    clargs_free(frame->positionals);
    clargs_free(frame->options);
}

void clargs_result_destroy(clargs_result_t *result)
{
    size_t i;

    if (result == NULL) {
        return;
    }

    for (i = 0; i < result->frames.count; ++i) {
        destroy_frame(frame_at(result, i));
    }
    clargs_vec_destroy(&result->frames);
    clargs_free(result->args);
    clargs_free(result);
}

/* ---- The arguments ------------------------------------------------------ */

const char *clargs_result_arg(const clargs_result_t *result, size_t index)
{
    assert(result != NULL);
    assert(index < result->arg_count);

    return result->args[index];
}

size_t clargs_result_first_invalid_arg(const clargs_result_t *result)
{
    assert(result != NULL);

    return result->first_invalid_arg;
}

/* ---- The selected commands ---------------------------------------------- */

/* Puts slot in the state of option when it is not given. */
static void init_option_slot(clargs_option_slot_t *slot, const clargs_option_t *option)
{
    slot->source = CLARGS_SOURCE_NONE;
    slot->on = false;
    slot->count = 0;
    slot->value = NULL;

    switch (clargs_option_kind(option)) {
    case CLARGS_OPTION_FLAG:
    case CLARGS_OPTION_COUNT:
        slot->source = CLARGS_SOURCE_DEFAULT;
        break;
    case CLARGS_OPTION_NEGATABLE:
        slot->source = CLARGS_SOURCE_DEFAULT;
        slot->on = clargs_option_initial(option);
        break;
    case CLARGS_OPTION_VALUE:
    default:
        break;
    }
}

/*
 * Fills frame for command: allocates and initializes one slot for each of
 * its options and positionals. An array of no slots is not allocated.
 * Returns CLARGS_ERROR_OUT_OF_MEMORY when an allocation fails; nothing is
 * left allocated then.
 */
static clargs_error_t init_frame(clargs_result_frame_t *frame, const clargs_command_t *command)
{
    size_t i;

    frame->command = command;
    frame->options = NULL;
    frame->option_count = clargs_command_option_count(command);
    frame->positionals = NULL;
    frame->positional_count = clargs_command_positional_count(command);

    if (frame->option_count > 0) {
        frame->options =
            (clargs_option_slot_t *)clargs_calloc(frame->option_count, sizeof(*frame->options));
        if (frame->options == NULL) {
            return CLARGS_ERROR_OUT_OF_MEMORY;
        }
        for (i = 0; i < frame->option_count; ++i) {
            init_option_slot(&frame->options[i], clargs_command_option_at(command, i));
        }
    }

    if (frame->positional_count > 0) {
        frame->positionals = (clargs_positional_slot_t *)clargs_calloc(frame->positional_count,
                                                                       sizeof(*frame->positionals));
        if (frame->positionals == NULL) {
            clargs_free(frame->options);
            return CLARGS_ERROR_OUT_OF_MEMORY;
        }
        for (i = 0; i < frame->positional_count; ++i) {
            frame->positionals[i].source = CLARGS_SOURCE_NONE;
            clargs_vec_init(&frame->positionals[i].values, sizeof(void *));
        }
    }

    return CLARGS_ERROR_NONE;
}

clargs_error_t clargs_result_push_command(clargs_result_t *result, const clargs_command_t *command)
{
    clargs_result_frame_t frame;
    clargs_error_t error;

    assert(result != NULL);
    assert(command != NULL);
    assert(result->frames.count > 0 || command == result->root);
    assert(result->frames.count == 0
           || clargs_command_parent(command) == frame_at(result, result->frames.count - 1)->command);

    error = init_frame(&frame, command);
    if (error != CLARGS_ERROR_NONE) {
        return error;
    }

    error = clargs_vec_push(&result->frames, &frame);
    if (error != CLARGS_ERROR_NONE) {
        destroy_frame(&frame);
        return error;
    }
    return CLARGS_ERROR_NONE;
}

/* ---- The slots of the options ------------------------------------------- */

/* Returns the root of the schema of command: the command that has no
 * parent, reached from command through the parents. */
static const clargs_command_t *root_of(const clargs_command_t *command)
{
    const clargs_command_t *parent = clargs_command_parent(command);

    while (parent != NULL) {
        command = parent;
        parent = clargs_command_parent(command);
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
static clargs_option_slot_t *find_option_slot(const clargs_result_t *result,
                                              const clargs_option_t *option)
{
    const clargs_command_t *command = clargs_option_command(option);
    size_t i;
    size_t k;

    if (root_of(command) != result->root) {
        return NULL;
    }

    for (i = 0; i < result->frames.count; ++i) {
        clargs_result_frame_t *frame = frame_at(result, i);

        if (frame->command != command) {
            continue;
        }
        for (k = 0; k < frame->option_count; ++k) {
            if (clargs_command_option_at(command, k) == option) {
                return &frame->options[k];
            }
        }
        return NULL;
    }
    return NULL;
}

void clargs_result_record_flag(clargs_result_t *result, const clargs_option_t *option)
{
    clargs_option_slot_t *slot;

    assert(result != NULL);
    assert(option != NULL);
    assert(clargs_option_kind(option) == CLARGS_OPTION_FLAG);

    slot = find_option_slot(result, option);
    assert(slot != NULL);

    slot->on = true;
    slot->source = CLARGS_SOURCE_COMMAND_LINE;
}

void clargs_result_record_state(clargs_result_t *result, const clargs_option_t *option, bool on)
{
    clargs_option_slot_t *slot;

    assert(result != NULL);
    assert(option != NULL);
    assert(clargs_option_kind(option) == CLARGS_OPTION_NEGATABLE);

    slot = find_option_slot(result, option);
    assert(slot != NULL);

    slot->on = on;
    slot->source = CLARGS_SOURCE_COMMAND_LINE;
}

void clargs_result_record_count(clargs_result_t *result, const clargs_option_t *option)
{
    clargs_option_slot_t *slot;

    assert(result != NULL);
    assert(option != NULL);
    assert(clargs_option_kind(option) == CLARGS_OPTION_COUNT);

    slot = find_option_slot(result, option);
    assert(slot != NULL);

    slot->count += 1;
    slot->source = CLARGS_SOURCE_COMMAND_LINE;
}

bool clargs_result_flag(const clargs_result_t *result, const clargs_option_t *option)
{
    const clargs_option_slot_t *slot;

    assert(result != NULL);
    assert(option != NULL);
    assert(root_of(clargs_option_command(option)) == result->root);
    assert(clargs_option_kind(option) == CLARGS_OPTION_FLAG
           || clargs_option_kind(option) == CLARGS_OPTION_NEGATABLE);

    slot = find_option_slot(result, option);
    if (slot == NULL) {
        /* Not given: false for a flag, the initial state for a negatable
         * flag. */
        return clargs_option_initial(option);
    }
    return slot->on;
}

size_t clargs_result_count(const clargs_result_t *result, const clargs_option_t *option)
{
    const clargs_option_slot_t *slot;

    assert(result != NULL);
    assert(option != NULL);
    assert(root_of(clargs_option_command(option)) == result->root);
    assert(clargs_option_kind(option) == CLARGS_OPTION_COUNT);

    slot = find_option_slot(result, option);
    if (slot == NULL) {
        return 0;
    }
    return slot->count;
}

clargs_source_t clargs_result_option_source(const clargs_result_t *result,
                                            const clargs_option_t *option)
{
    const clargs_option_slot_t *slot;

    assert(result != NULL);
    assert(option != NULL);
    assert(root_of(clargs_option_command(option)) == result->root);

    slot = find_option_slot(result, option);
    if (slot == NULL) {
        return CLARGS_SOURCE_NONE;
    }
    return slot->source;
}

/* ---- The values --------------------------------------------------------- */

/*
 * Returns the slot of positional in result, or NULL when the command of
 * positional is not a selected command of result. The slot is found as
 * that of an option is; see find_option_slot().
 */
static clargs_positional_slot_t *find_positional_slot(const clargs_result_t *result,
                                                      const clargs_positional_t *positional)
{
    const clargs_command_t *command = clargs_positional_command(positional);
    size_t i;
    size_t k;

    if (root_of(command) != result->root) {
        return NULL;
    }

    for (i = 0; i < result->frames.count; ++i) {
        clargs_result_frame_t *frame = frame_at(result, i);

        if (frame->command != command) {
            continue;
        }
        for (k = 0; k < frame->positional_count; ++k) {
            if (clargs_command_positional_at(command, k) == positional) {
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
 * Returns CLARGS_ERROR_NONE on success; the caller then owns the area and
 * the value in it (destroy_value()). Returns CLARGS_ERROR_OUT_OF_MEMORY
 * when the allocation of the area fails, and what clargs_type_convert()
 * returned when the conversion fails; the area is freed, without a cleanup
 * of the value, and *out is NULL then.
 */
static clargs_error_t convert_value(const clargs_type_t *type, const char *text, void **out)
{
    void *value = clargs_malloc(clargs_type_value_size(type));
    clargs_error_t error;

    *out = NULL;
    if (value == NULL) {
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }

    error = clargs_type_convert(type, text, value);
    if (error != CLARGS_ERROR_NONE) {
        clargs_free(value);
        return error;
    }

    *out = value;
    return CLARGS_ERROR_NONE;
}

/* What a record of a value returns for the failure error of
 * convert_value(): the lack of memory as it is, and the error of a command
 * line that does not match the schema for a value that its value type does
 * not take. */
static clargs_error_t conversion_failure(clargs_error_t error)
{
    if (error == CLARGS_ERROR_OUT_OF_MEMORY) {
        return CLARGS_ERROR_OUT_OF_MEMORY;
    }
    return CLARGS_ERROR_INVALID_COMMAND_LINE;
}

clargs_error_t clargs_result_record_option_value(clargs_result_t *result, const clargs_option_t *option,
                                                 const char *text, const clargs_result_where_t *where)
{
    const clargs_type_t *type;
    clargs_option_slot_t *slot;
    clargs_error_t error;
    void *value;

    assert(result != NULL);
    assert(option != NULL);
    assert(text != NULL);
    assert(where != NULL);
    assert(clargs_option_kind(option) == CLARGS_OPTION_VALUE);

    slot = find_option_slot(result, option);
    assert(slot != NULL);

    /* The new value is converted before the old one is touched: when the
     * conversion fails, the old value stays. */
    type = clargs_option_type(option);
    error = convert_value(type, text, &value);
    if (error != CLARGS_ERROR_NONE) {
        return conversion_failure(error);
    }

    if (slot->value != NULL) {
        destroy_value(type, slot->value);
    }
    slot->value = value;
    slot->source = where->source;
    return CLARGS_ERROR_NONE;
}

clargs_error_t clargs_result_record_positional_value(clargs_result_t *result,
                                                     const clargs_positional_t *positional,
                                                     const char *text, const clargs_result_where_t *where)
{
    const clargs_type_t *type;
    clargs_positional_slot_t *slot;
    clargs_error_t error;
    void *value;

    assert(result != NULL);
    assert(positional != NULL);
    assert(text != NULL);
    assert(where != NULL);

    slot = find_positional_slot(result, positional);
    assert(slot != NULL);

    type = clargs_positional_type(positional);
    error = convert_value(type, text, &value);
    if (error != CLARGS_ERROR_NONE) {
        return conversion_failure(error);
    }

    error = clargs_vec_push(&slot->values, &value);
    if (error != CLARGS_ERROR_NONE) {
        /* The value is not kept, so it is cleaned up here. */
        destroy_value(type, value);
        return error;
    }
    slot->source = where->source;
    return CLARGS_ERROR_NONE;
}

/* Returns the area of the value of option in result, or NULL when option
 * has no value or its command is not a selected command of result. */
static const void *option_value(const clargs_result_t *result, const clargs_option_t *option)
{
    const clargs_option_slot_t *slot = find_option_slot(result, option);

    if (slot == NULL) {
        return NULL;
    }
    return slot->value;
}

bool clargs_result_option_string(const clargs_result_t *result, const clargs_option_t *option,
                                 const char **out)
{
    const void *value;

    assert(result != NULL);
    assert(option != NULL);
    assert(out != NULL);
    assert(root_of(clargs_option_command(option)) == result->root);
    assert(clargs_option_kind(option) == CLARGS_OPTION_VALUE);
    assert(clargs_type_kind(clargs_option_type(option)) == CLARGS_TYPE_STRING);

    value = option_value(result, option);
    if (value == NULL) {
        return false;
    }
    *out = *(const char *const *)value;
    return true;
}

bool clargs_result_option_int(const clargs_result_t *result, const clargs_option_t *option, int64_t *out)
{
    const void *value;

    assert(result != NULL);
    assert(option != NULL);
    assert(out != NULL);
    assert(root_of(clargs_option_command(option)) == result->root);
    assert(clargs_option_kind(option) == CLARGS_OPTION_VALUE);
    assert(clargs_type_kind(clargs_option_type(option)) == CLARGS_TYPE_INT);

    value = option_value(result, option);
    if (value == NULL) {
        return false;
    }
    *out = *(const int64_t *)value;
    return true;
}

bool clargs_result_option_float(const clargs_result_t *result, const clargs_option_t *option, double *out)
{
    const void *value;

    assert(result != NULL);
    assert(option != NULL);
    assert(out != NULL);
    assert(root_of(clargs_option_command(option)) == result->root);
    assert(clargs_option_kind(option) == CLARGS_OPTION_VALUE);
    assert(clargs_type_kind(clargs_option_type(option)) == CLARGS_TYPE_FLOAT);

    value = option_value(result, option);
    if (value == NULL) {
        return false;
    }
    *out = *(const double *)value;
    return true;
}

bool clargs_result_option_choice(const clargs_result_t *result, const clargs_option_t *option, size_t *out)
{
    const void *value;

    assert(result != NULL);
    assert(option != NULL);
    assert(out != NULL);
    assert(root_of(clargs_option_command(option)) == result->root);
    assert(clargs_option_kind(option) == CLARGS_OPTION_VALUE);
    assert(clargs_type_kind(clargs_option_type(option)) == CLARGS_TYPE_CHOICE);

    value = option_value(result, option);
    if (value == NULL) {
        return false;
    }
    *out = *(const size_t *)value;
    return true;
}

const void *clargs_result_option_value(const clargs_result_t *result, const clargs_option_t *option)
{
    assert(result != NULL);
    assert(option != NULL);
    assert(root_of(clargs_option_command(option)) == result->root);
    assert(clargs_option_kind(option) == CLARGS_OPTION_VALUE);

    return option_value(result, option);
}

size_t clargs_result_positional_count(const clargs_result_t *result, const clargs_positional_t *positional)
{
    const clargs_positional_slot_t *slot;

    assert(result != NULL);
    assert(positional != NULL);
    assert(root_of(clargs_positional_command(positional)) == result->root);

    slot = find_positional_slot(result, positional);
    if (slot == NULL) {
        return 0;
    }
    return slot->values.count;
}

clargs_source_t clargs_result_positional_source(const clargs_result_t *result,
                                                const clargs_positional_t *positional)
{
    const clargs_positional_slot_t *slot;

    assert(result != NULL);
    assert(positional != NULL);
    assert(root_of(clargs_positional_command(positional)) == result->root);

    slot = find_positional_slot(result, positional);
    if (slot == NULL) {
        return CLARGS_SOURCE_NONE;
    }
    return slot->source;
}

/* Returns the area of the value of positional at index in result, or NULL
 * when there is no value at index or the command of positional is not a
 * selected command of result. */
static const void *positional_value(const clargs_result_t *result, const clargs_positional_t *positional,
                                    size_t index)
{
    const clargs_positional_slot_t *slot = find_positional_slot(result, positional);

    if (slot == NULL || index >= slot->values.count) {
        return NULL;
    }
    return *(void *const *)clargs_vec_at(&slot->values, index);
}

bool clargs_result_positional_string(const clargs_result_t *result, const clargs_positional_t *positional,
                                     size_t index, const char **out)
{
    const void *value;

    assert(result != NULL);
    assert(positional != NULL);
    assert(out != NULL);
    assert(root_of(clargs_positional_command(positional)) == result->root);
    assert(clargs_type_kind(clargs_positional_type(positional)) == CLARGS_TYPE_STRING);

    value = positional_value(result, positional, index);
    if (value == NULL) {
        return false;
    }
    *out = *(const char *const *)value;
    return true;
}

bool clargs_result_positional_int(const clargs_result_t *result, const clargs_positional_t *positional,
                                  size_t index, int64_t *out)
{
    const void *value;

    assert(result != NULL);
    assert(positional != NULL);
    assert(out != NULL);
    assert(root_of(clargs_positional_command(positional)) == result->root);
    assert(clargs_type_kind(clargs_positional_type(positional)) == CLARGS_TYPE_INT);

    value = positional_value(result, positional, index);
    if (value == NULL) {
        return false;
    }
    *out = *(const int64_t *)value;
    return true;
}

bool clargs_result_positional_float(const clargs_result_t *result, const clargs_positional_t *positional,
                                    size_t index, double *out)
{
    const void *value;

    assert(result != NULL);
    assert(positional != NULL);
    assert(out != NULL);
    assert(root_of(clargs_positional_command(positional)) == result->root);
    assert(clargs_type_kind(clargs_positional_type(positional)) == CLARGS_TYPE_FLOAT);

    value = positional_value(result, positional, index);
    if (value == NULL) {
        return false;
    }
    *out = *(const double *)value;
    return true;
}

bool clargs_result_positional_choice(const clargs_result_t *result, const clargs_positional_t *positional,
                                     size_t index, size_t *out)
{
    const void *value;

    assert(result != NULL);
    assert(positional != NULL);
    assert(out != NULL);
    assert(root_of(clargs_positional_command(positional)) == result->root);
    assert(clargs_type_kind(clargs_positional_type(positional)) == CLARGS_TYPE_CHOICE);

    value = positional_value(result, positional, index);
    if (value == NULL) {
        return false;
    }
    *out = *(const size_t *)value;
    return true;
}

const void *clargs_result_positional_value(const clargs_result_t *result,
                                           const clargs_positional_t *positional, size_t index)
{
    assert(result != NULL);
    assert(positional != NULL);
    assert(root_of(clargs_positional_command(positional)) == result->root);

    return positional_value(result, positional, index);
}

/* ---- The outcome and the selected commands ------------------------------ */

clargs_error_t clargs_result_error(const clargs_result_t *result)
{
    assert(result != NULL);

    return result->error;
}

size_t clargs_result_command_count(const clargs_result_t *result)
{
    assert(result != NULL);

    return result->frames.count;
}

const clargs_command_t *clargs_result_command_at(const clargs_result_t *result, size_t index)
{
    assert(result != NULL);
    assert(index < result->frames.count);

    return frame_at(result, index)->command;
}

const clargs_command_t *clargs_result_command(const clargs_result_t *result)
{
    assert(result != NULL);
    assert(result->frames.count > 0);

    return frame_at(result, result->frames.count - 1)->command;
}
