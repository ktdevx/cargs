/*
 * Tests of the public error API: <clargs/error.h>.
 *
 * The table below is generated from CLARGS_ERROR_LIST, so these tests cover
 * a new error code without being changed.
 */
#include <clargs/clargs.h>

#include <naos/naos.h>

#include <stddef.h>

/* What clargs_error_message() returns for a value that is not in the list. */
#define UNKNOWN_MESSAGE "unknown error code"

typedef struct error_entry {
    clargs_error_t error;
    const char *name;
    const char *message;
} error_entry_t;

static const error_entry_t error_entries[] = {
#define ERROR_ENTRY(name, message) { name, #name, message },
    CLARGS_ERROR_LIST(ERROR_ENTRY)
#undef ERROR_ENTRY
};

static const size_t error_entry_count = sizeof(error_entries) / sizeof(error_entries[0]);

/* Every code of the list has the message that the list gives it. */
NAOS_TEST(every_code_has_the_message_of_the_list)
{
    size_t i;

    NAOS_REQUIRE_UINT_GT(error_entry_count, 0);
    for (i = 0; i < error_entry_count; ++i) {
        const char *message = clargs_error_message(error_entries[i].error);

        NAOS_REQUIRE_TRUE(message != NULL);
        NAOS_CHECK_STR_EQ(message, error_entries[i].message);
    }
}

/* No message is empty. */
NAOS_TEST(every_message_is_not_empty)
{
    size_t i;

    NAOS_REQUIRE_UINT_GT(error_entry_count, 0);
    for (i = 0; i < error_entry_count; ++i) {
        const char *message = clargs_error_message(error_entries[i].error);

        NAOS_REQUIRE_TRUE(message != NULL);
        NAOS_CHECK_STR_NE(message, "");
    }
}

/* No code of the list shares the message of the unknown values. */
NAOS_TEST(every_message_differs_from_the_unknown_message)
{
    size_t i;

    NAOS_REQUIRE_UINT_GT(error_entry_count, 0);
    for (i = 0; i < error_entry_count; ++i) {
        const char *message = clargs_error_message(error_entries[i].error);

        NAOS_REQUIRE_TRUE(message != NULL);
        NAOS_CHECK_STR_NE(message, UNKNOWN_MESSAGE);
    }
}

/* No two codes of the list share a message. */
NAOS_TEST(every_message_is_unique)
{
    size_t i;
    size_t j;

    NAOS_REQUIRE_UINT_GT(error_entry_count, 0);
    for (i = 0; i < error_entry_count; ++i) {
        const char *message = clargs_error_message(error_entries[i].error);

        NAOS_REQUIRE_TRUE(message != NULL);
        for (j = i + 1; j < error_entry_count; ++j) {
            const char *other = clargs_error_message(error_entries[j].error);

            NAOS_REQUIRE_TRUE(other != NULL);
            NAOS_CHECK_STR_NE(message, other);
        }
    }
}

/* The values follow the order of the list, starting at 0. */
NAOS_TEST(values_follow_the_order_of_the_list)
{
    size_t i;

    NAOS_REQUIRE_UINT_GT(error_entry_count, 0);
    for (i = 0; i < error_entry_count; ++i) {
        NAOS_CHECK_UINT_EQ((size_t)error_entries[i].error, i);
    }
}

/* The values that are already public have not moved. */
NAOS_TEST(first_values_are_fixed)
{
    NAOS_CHECK_INT_EQ(CLARGS_ERROR_NONE, 0);
    NAOS_CHECK_INT_EQ(CLARGS_ERROR_OUT_OF_MEMORY, 1);
}

/* The first two codes have the messages they were published with. */
NAOS_TEST(first_messages_are_fixed)
{
    NAOS_CHECK_STR_EQ(clargs_error_message(CLARGS_ERROR_NONE), "success");
    NAOS_CHECK_STR_EQ(clargs_error_message(CLARGS_ERROR_OUT_OF_MEMORY), "out of memory");
}

/* A value that is not in the list gets the unknown message, never NULL. */
NAOS_TEST(unknown_value_has_the_unknown_message)
{
    const char *negative = clargs_error_message((clargs_error_t)-1);
    const char *large = clargs_error_message((clargs_error_t)9999);
    const char *next = clargs_error_message((clargs_error_t)error_entry_count);

    NAOS_REQUIRE_TRUE(negative != NULL);
    NAOS_REQUIRE_TRUE(large != NULL);
    NAOS_REQUIRE_TRUE(next != NULL);
    NAOS_CHECK_STR_EQ(negative, UNKNOWN_MESSAGE);
    NAOS_CHECK_STR_EQ(large, UNKNOWN_MESSAGE);
    NAOS_CHECK_STR_EQ(next, UNKNOWN_MESSAGE);
}

/* The message is the same string on every call: nothing to free. */
NAOS_TEST(message_is_the_same_on_every_call)
{
    size_t i;

    NAOS_REQUIRE_UINT_GT(error_entry_count, 0);
    for (i = 0; i < error_entry_count; ++i) {
        NAOS_CHECK_PTR_EQ(clargs_error_message(error_entries[i].error),
                          clargs_error_message(error_entries[i].error));
    }
    NAOS_CHECK_PTR_EQ(clargs_error_message((clargs_error_t)9999),
                      clargs_error_message((clargs_error_t)9999));
}

NAOS_SUITE(error,
    NAOS_ADD_TEST(every_code_has_the_message_of_the_list),
    NAOS_ADD_TEST(every_message_is_not_empty),
    NAOS_ADD_TEST(every_message_differs_from_the_unknown_message),
    NAOS_ADD_TEST(every_message_is_unique),
    NAOS_ADD_TEST(values_follow_the_order_of_the_list),
    NAOS_ADD_TEST(first_values_are_fixed),
    NAOS_ADD_TEST(first_messages_are_fixed),
    NAOS_ADD_TEST(unknown_value_has_the_unknown_message),
    NAOS_ADD_TEST(message_is_the_same_on_every_call)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(error)
)
