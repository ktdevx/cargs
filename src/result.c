/*
 * CLArgs - the result of a parse: its creation and destruction, the copy
 * of the arguments, the selected commands and the functions that read it.
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

/* Frees the slots of frame. */
static void destroy_frame(clargs_result_frame_t *frame)
{
    size_t i;

    for (i = 0; i < frame->positional_count; ++i) {
        clargs_vec_destroy(&frame->positionals[i].values);
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
