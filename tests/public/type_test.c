/*
 * Tests of the public value type API: <clargs/type.h>.
 *
 * The boundaries of the conversion of numbers are tested with the internal
 * functions (tests/internal/number_test.c). Here, the built-in value types
 * are used the way a consumer uses them: through clargs_type_convert().
 *
 * The tests that create a value type use the test allocator
 * ("../support/test_allocator.h"). Its state and the allocator that is set
 * are shared by all the tests of this executable. Such a test starts with
 * an install, destroys what it created, sets the default allocator back
 * before it ends, and ends with check_clean().
 */
#include <clargs/clargs.h>

#include "../support/test_allocator.h"

#include <naos/naos.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* The end of every test that allocates: the test allocator has no live
 * block and saw no violation of the allocator contract. */
static void check_clean(void)
{
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
}

/* Converts text with a choice value type. On success, *index is the index
 * of the choice that matched, and the value has been released. */
static clargs_error_t convert_choice(const clargs_type_t *type, const char *text, size_t *index)
{
    clargs_error_t error = clargs_type_convert(type, text, index);

    if (error == CLARGS_ERROR_NONE) {
        clargs_type_release(type, index);
    }
    return error;
}

/* A built-in value type as the pointer to a modifiable object that
 * clargs_type_destroy() takes. The conversion goes through a union, so
 * that no cast drops the const. */
static clargs_type_t *as_destroyable(const clargs_type_t *type)
{
    union {
        const clargs_type_t *as_const;
        clargs_type_t *as_modifiable;
    } pointer;

    pointer.as_const = type;
    return pointer.as_modifiable;
}

/* Converts text with the integer value type. On success, *value is the
 * integer, and the value has been released. */
static clargs_error_t convert_int(const char *text, int64_t *value)
{
    clargs_error_t error = clargs_type_convert(clargs_type_int(), text, value);

    if (error == CLARGS_ERROR_NONE) {
        clargs_type_release(clargs_type_int(), value);
    }
    return error;
}

/* Converts text with the floating-point value type. On success, *value is
 * the number, and the value has been released. */
static clargs_error_t convert_float(const char *text, double *value)
{
    clargs_error_t error = clargs_type_convert(clargs_type_float(), text, value);

    if (error == CLARGS_ERROR_NONE) {
        clargs_type_release(clargs_type_float(), value);
    }
    return error;
}

NAOS_TEST(string_has_its_name_kind_and_value_size)
{
    const clargs_type_t *type = clargs_type_string();

    NAOS_REQUIRE_TRUE(type != NULL);
    NAOS_CHECK_STR_EQ(clargs_type_name(type), "string");
    NAOS_CHECK_INT_EQ(clargs_type_kind(type), CLARGS_TYPE_STRING);
    NAOS_CHECK_UINT_EQ(clargs_type_value_size(type), sizeof(const char *));
}

NAOS_TEST(int_has_its_name_kind_and_value_size)
{
    const clargs_type_t *type = clargs_type_int();

    NAOS_REQUIRE_TRUE(type != NULL);
    NAOS_CHECK_STR_EQ(clargs_type_name(type), "integer");
    NAOS_CHECK_INT_EQ(clargs_type_kind(type), CLARGS_TYPE_INT);
    NAOS_CHECK_UINT_EQ(clargs_type_value_size(type), sizeof(int64_t));
}

NAOS_TEST(float_has_its_name_kind_and_value_size)
{
    const clargs_type_t *type = clargs_type_float();

    NAOS_REQUIRE_TRUE(type != NULL);
    NAOS_CHECK_STR_EQ(clargs_type_name(type), "float");
    NAOS_CHECK_INT_EQ(clargs_type_kind(type), CLARGS_TYPE_FLOAT);
    NAOS_CHECK_UINT_EQ(clargs_type_value_size(type), sizeof(double));
}

/* A built-in value type is one fixed object: every call returns it. */
NAOS_TEST(each_function_returns_the_same_pointer_every_time)
{
    NAOS_CHECK_TRUE(clargs_type_string() == clargs_type_string());
    NAOS_CHECK_TRUE(clargs_type_int() == clargs_type_int());
    NAOS_CHECK_TRUE(clargs_type_float() == clargs_type_float());
}

NAOS_TEST(the_three_types_are_different_objects)
{
    NAOS_CHECK_TRUE(clargs_type_string() != clargs_type_int());
    NAOS_CHECK_TRUE(clargs_type_string() != clargs_type_float());
    NAOS_CHECK_TRUE(clargs_type_int() != clargs_type_float());
}

/* The enumerators have the values of their order, starting at 0. */
NAOS_TEST(kinds_have_the_values_of_their_order)
{
    NAOS_CHECK_INT_EQ(CLARGS_TYPE_STRING, 0);
    NAOS_CHECK_INT_EQ(CLARGS_TYPE_INT, 1);
    NAOS_CHECK_INT_EQ(CLARGS_TYPE_FLOAT, 2);
    NAOS_CHECK_INT_EQ(CLARGS_TYPE_CHOICE, 3);
    NAOS_CHECK_INT_EQ(CLARGS_TYPE_CUSTOM, 4);
}

/* The value of a string is the pointer to the input itself, not a copy. */
NAOS_TEST(string_conversion_writes_the_pointer_to_the_input)
{
    static const char *const inputs[] = {
        "text", "", " ", " padded ", "12", "-x", "caf\xc3\xa9", "a\tb\n"
    };
    size_t index;

    for (index = 0; index < sizeof(inputs) / sizeof(inputs[0]); ++index) {
        const char *value = NULL;
        clargs_error_t error = clargs_type_convert(clargs_type_string(), inputs[index], &value);

        NAOS_CHECK_INT_EQ(error, CLARGS_ERROR_NONE);
        NAOS_CHECK_TRUE(value == inputs[index]);
        clargs_type_release(clargs_type_string(), &value);
        /* The release of a built-in value does nothing. */
        NAOS_CHECK_TRUE(value == inputs[index]);
    }
}

NAOS_TEST(string_conversion_succeeds_for_the_empty_string)
{
    const char *input = "";
    const char *value = NULL;

    NAOS_CHECK_INT_EQ(clargs_type_convert(clargs_type_string(), input, &value), CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(value == input);
    NAOS_CHECK_STR_EQ(value, "");
    clargs_type_release(clargs_type_string(), &value);
}

NAOS_TEST(int_conversion_succeeds)
{
    int64_t value = 99;

    NAOS_CHECK_INT_EQ(convert_int("0", &value), CLARGS_ERROR_NONE);
    NAOS_CHECK_TRUE(value == 0);
    NAOS_CHECK_INT_EQ(convert_int("42", &value), CLARGS_ERROR_NONE);
    NAOS_CHECK_TRUE(value == 42);
    NAOS_CHECK_INT_EQ(convert_int("+42", &value), CLARGS_ERROR_NONE);
    NAOS_CHECK_TRUE(value == 42);
    NAOS_CHECK_INT_EQ(convert_int("-42", &value), CLARGS_ERROR_NONE);
    NAOS_CHECK_TRUE(value == -42);
    NAOS_CHECK_INT_EQ(convert_int("007", &value), CLARGS_ERROR_NONE);
    NAOS_CHECK_TRUE(value == 7);
    NAOS_CHECK_INT_EQ(convert_int("9223372036854775807", &value), CLARGS_ERROR_NONE);
    NAOS_CHECK_TRUE(value == INT64_MAX);
    NAOS_CHECK_INT_EQ(convert_int("-9223372036854775808", &value), CLARGS_ERROR_NONE);
    NAOS_CHECK_TRUE(value == INT64_MIN);
}

NAOS_TEST(int_conversion_fails)
{
    static const char *const inputs[] = {
        "", "+", "-", "abc", "12a", " 12", "12 ", "1.0", "1e3", "0x10", "--1",
        "9223372036854775808", "-9223372036854775809"
    };
    size_t index;

    for (index = 0; index < sizeof(inputs) / sizeof(inputs[0]); ++index) {
        int64_t value = 0;

        NAOS_CHECK_INT_EQ(clargs_type_convert(clargs_type_int(), inputs[index], &value),
                          CLARGS_ERROR_INVALID_VALUE);
    }
}

NAOS_TEST(float_conversion_succeeds)
{
    double value = 99.0;

    NAOS_CHECK_INT_EQ(convert_float("0", &value), CLARGS_ERROR_NONE);
    NAOS_CHECK_TRUE(value == 0.0);
    NAOS_CHECK_INT_EQ(convert_float("1.5", &value), CLARGS_ERROR_NONE);
    NAOS_CHECK_TRUE(value == 1.5);
    NAOS_CHECK_INT_EQ(convert_float("-2.25", &value), CLARGS_ERROR_NONE);
    NAOS_CHECK_TRUE(value == -2.25);
    NAOS_CHECK_INT_EQ(convert_float("+.5", &value), CLARGS_ERROR_NONE);
    NAOS_CHECK_TRUE(value == 0.5);
    NAOS_CHECK_INT_EQ(convert_float("3.", &value), CLARGS_ERROR_NONE);
    NAOS_CHECK_TRUE(value == 3.0);
    NAOS_CHECK_INT_EQ(convert_float("25e-2", &value), CLARGS_ERROR_NONE);
    NAOS_CHECK_TRUE(value == 0.25);
    NAOS_CHECK_INT_EQ(convert_float("1E3", &value), CLARGS_ERROR_NONE);
    NAOS_CHECK_TRUE(value == 1000.0);
    NAOS_CHECK_INT_EQ(convert_float("-1.5e-3", &value), CLARGS_ERROR_NONE);
    NAOS_CHECK_DOUBLE_NEAR(value, -0.0015, 1e-15);
}

NAOS_TEST(float_conversion_fails)
{
    static const char *const inputs[] = {
        "", ".", "+", "abc", "1.5x", " 1.5", "1.5 ", "1,5", "e5", "1e", "0x1p3",
        "inf", "-inf", "infinity", "nan", "NaN", "1e400", "-1e400"
    };
    size_t index;

    for (index = 0; index < sizeof(inputs) / sizeof(inputs[0]); ++index) {
        double value = 0.0;

        NAOS_CHECK_INT_EQ(clargs_type_convert(clargs_type_float(), inputs[index], &value),
                          CLARGS_ERROR_INVALID_VALUE);
    }
}

/* The caller of a conversion needs no knowledge of the kind: an area of
 * clargs_type_value_size() bytes that is aligned for any type is enough for
 * every value type. */
NAOS_TEST(conversion_is_requested_the_same_way_for_every_type)
{
    typedef union any_value {
        const char *as_string;
        int64_t as_int;
        double as_float;
    } any_value_t;
    const char *input = "12";
    const clargs_type_t *types[3];
    size_t index;

    types[0] = clargs_type_string();
    types[1] = clargs_type_int();
    types[2] = clargs_type_float();

    for (index = 0; index < 3; ++index) {
        any_value_t value;

        value.as_int = 0;
        NAOS_REQUIRE_TRUE(clargs_type_value_size(types[index]) <= sizeof(value));
        NAOS_REQUIRE_TRUE(clargs_type_convert(types[index], input, &value) == CLARGS_ERROR_NONE);

        switch (clargs_type_kind(types[index])) {
        case CLARGS_TYPE_STRING:
            NAOS_CHECK_TRUE(value.as_string == input);
            break;
        case CLARGS_TYPE_INT:
            NAOS_CHECK_TRUE(value.as_int == 12);
            break;
        case CLARGS_TYPE_FLOAT:
            NAOS_CHECK_TRUE(value.as_float == 12.0);
            break;
        default:
            NAOS_FAIL("a built-in value type has an unexpected kind");
            break;
        }

        clargs_type_release(types[index], &value);
    }
}

/* ---- Choices ---------------------------------------------------------- */

NAOS_TEST(choice_has_its_name_kind_and_value_size)
{
    static const char *const choices[] = { "red", "green", "blue" };
    clargs_type_t *type = NULL;

    clargs_test_allocator_install();

    NAOS_REQUIRE_TRUE(clargs_type_create_choice(choices, 3, &type) == CLARGS_ERROR_NONE);
    NAOS_REQUIRE_TRUE(type != NULL);
    NAOS_CHECK_STR_EQ(clargs_type_name(type), "choice");
    NAOS_CHECK_INT_EQ(clargs_type_kind(type), CLARGS_TYPE_CHOICE);
    NAOS_CHECK_UINT_EQ(clargs_type_value_size(type), sizeof(size_t));

    clargs_type_destroy(type);
    clargs_test_allocator_uninstall();
    check_clean();
}

/* The value of a choice is the index of the choice that matched. */
NAOS_TEST(choice_conversion_writes_the_index_of_the_match)
{
    static const char *const choices[] = { "red", "green", "blue" };
    clargs_type_t *type = NULL;
    size_t index = 99;

    clargs_test_allocator_install();
    NAOS_REQUIRE_TRUE(clargs_type_create_choice(choices, 3, &type) == CLARGS_ERROR_NONE);

    NAOS_CHECK_INT_EQ(convert_choice(type, "red", &index), CLARGS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(index, 0);
    NAOS_CHECK_INT_EQ(convert_choice(type, "green", &index), CLARGS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(index, 1);
    NAOS_CHECK_INT_EQ(convert_choice(type, "blue", &index), CLARGS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(index, 2);

    clargs_type_destroy(type);
    clargs_test_allocator_uninstall();
    check_clean();
}

/* Only an input that is exactly one of the choices is taken. A conversion
 * allocates nothing. */
NAOS_TEST(choice_conversion_fails_without_an_exact_match)
{
    static const char *const choices[] = { "red", "green", "blue" };
    static const char *const inputs[] = {
        "Red", "RED", "gReen", "blue ", " blue", "re", "redd", "", "yellow", "0"
    };
    clargs_type_t *type = NULL;
    size_t requests;
    size_t position;

    clargs_test_allocator_install();
    NAOS_REQUIRE_TRUE(clargs_type_create_choice(choices, 3, &type) == CLARGS_ERROR_NONE);
    requests = clargs_test_allocator_requests();

    for (position = 0; position < sizeof(inputs) / sizeof(inputs[0]); ++position) {
        size_t index = 0;

        NAOS_CHECK_INT_EQ(clargs_type_convert(type, inputs[position], &index),
                          CLARGS_ERROR_INVALID_VALUE);
    }
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), requests);

    clargs_type_destroy(type);
    clargs_test_allocator_uninstall();
    check_clean();
}

/* Choices that differ only in case are different choices. */
NAOS_TEST(choices_are_case_sensitive)
{
    static const char *const choices[] = { "on", "On", "ON" };
    clargs_type_t *type = NULL;
    size_t index = 99;

    clargs_test_allocator_install();
    NAOS_REQUIRE_TRUE(clargs_type_create_choice(choices, 3, &type) == CLARGS_ERROR_NONE);

    NAOS_CHECK_INT_EQ(convert_choice(type, "ON", &index), CLARGS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(index, 2);
    NAOS_CHECK_INT_EQ(convert_choice(type, "On", &index), CLARGS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(index, 1);
    NAOS_CHECK_INT_EQ(clargs_type_convert(type, "oN", &index), CLARGS_ERROR_INVALID_VALUE);

    clargs_type_destroy(type);
    clargs_test_allocator_uninstall();
    check_clean();
}

/* The empty string can be a choice; then, and only then, the empty input
 * is taken. */
NAOS_TEST(the_empty_string_can_be_a_choice)
{
    static const char *const choices[] = { "auto", "" };
    clargs_type_t *type = NULL;
    size_t index = 99;

    clargs_test_allocator_install();
    NAOS_REQUIRE_TRUE(clargs_type_create_choice(choices, 2, &type) == CLARGS_ERROR_NONE);

    NAOS_CHECK_UINT_EQ(clargs_type_choice_count(type), 2);
    NAOS_CHECK_STR_EQ(clargs_type_choice_at(type, 1), "");
    NAOS_CHECK_INT_EQ(convert_choice(type, "", &index), CLARGS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(index, 1);

    clargs_type_destroy(type);
    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(a_single_choice_is_enough)
{
    static const char *const choices[] = { "only" };
    clargs_type_t *type = NULL;
    size_t index = 99;

    clargs_test_allocator_install();
    NAOS_REQUIRE_TRUE(clargs_type_create_choice(choices, 1, &type) == CLARGS_ERROR_NONE);

    NAOS_CHECK_UINT_EQ(clargs_type_choice_count(type), 1);
    NAOS_CHECK_INT_EQ(convert_choice(type, "only", &index), CLARGS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(index, 0);

    clargs_type_destroy(type);
    clargs_test_allocator_uninstall();
    check_clean();
}

/* The choices are read in the order in which they were given. */
NAOS_TEST(choices_are_read_in_the_order_of_the_definition)
{
    static const char *const choices[] = { "zeta", "alpha", "", "Mid", "beta" };
    const size_t count = sizeof(choices) / sizeof(choices[0]);
    clargs_type_t *type = NULL;
    size_t position;

    clargs_test_allocator_install();
    NAOS_REQUIRE_TRUE(clargs_type_create_choice(choices, count, &type) == CLARGS_ERROR_NONE);

    NAOS_REQUIRE_TRUE(clargs_type_choice_count(type) == count);
    for (position = 0; position < count; ++position) {
        NAOS_CHECK_STR_EQ(clargs_type_choice_at(type, position), choices[position]);
    }

    clargs_type_destroy(type);
    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(a_type_that_is_not_a_choice_has_no_choices)
{
    NAOS_CHECK_UINT_EQ(clargs_type_choice_count(clargs_type_string()), 0);
    NAOS_CHECK_UINT_EQ(clargs_type_choice_count(clargs_type_int()), 0);
    NAOS_CHECK_UINT_EQ(clargs_type_choice_count(clargs_type_float()), 0);
}

/* The value type keeps copies: neither the array nor the strings that were
 * passed are used after the creation. */
NAOS_TEST(choices_are_copied)
{
    char first[] = "red";
    char second[] = "green";
    const char *choices[2];
    clargs_type_t *type = NULL;
    size_t index = 99;

    choices[0] = first;
    choices[1] = second;

    clargs_test_allocator_install();
    NAOS_REQUIRE_TRUE(clargs_type_create_choice(choices, 2, &type) == CLARGS_ERROR_NONE);

    /* Change the strings and the array. */
    memcpy(first, "RED", sizeof(first));
    second[0] = '\0';
    choices[0] = "other";
    choices[1] = NULL;

    NAOS_REQUIRE_TRUE(clargs_type_choice_count(type) == 2);
    NAOS_CHECK_TRUE(clargs_type_choice_at(type, 0) != first);
    NAOS_CHECK_TRUE(clargs_type_choice_at(type, 1) != second);
    NAOS_CHECK_STR_EQ(clargs_type_choice_at(type, 0), "red");
    NAOS_CHECK_STR_EQ(clargs_type_choice_at(type, 1), "green");
    NAOS_CHECK_INT_EQ(convert_choice(type, "red", &index), CLARGS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(index, 0);
    NAOS_CHECK_INT_EQ(convert_choice(type, "green", &index), CLARGS_ERROR_NONE);
    NAOS_CHECK_UINT_EQ(index, 1);
    NAOS_CHECK_INT_EQ(clargs_type_convert(type, "RED", &index), CLARGS_ERROR_INVALID_VALUE);
    NAOS_CHECK_INT_EQ(clargs_type_convert(type, "other", &index), CLARGS_ERROR_INVALID_VALUE);

    clargs_type_destroy(type);
    clargs_test_allocator_uninstall();
    check_clean();
}

/* An empty list creates nothing. The array is not read then, so it may be
 * NULL. */
NAOS_TEST(an_empty_list_is_an_invalid_definition)
{
    static const char *const choices[] = { "red" };
    clargs_type_t *type = as_destroyable(clargs_type_string());

    clargs_test_allocator_install();

    NAOS_CHECK_INT_EQ(clargs_type_create_choice(choices, 0, &type), CLARGS_ERROR_INVALID_DEFINITION);
    NAOS_CHECK_TRUE(type == NULL);

    type = as_destroyable(clargs_type_string());
    NAOS_CHECK_INT_EQ(clargs_type_create_choice(NULL, 0, &type), CLARGS_ERROR_INVALID_DEFINITION);
    NAOS_CHECK_TRUE(type == NULL);

    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 0);
    clargs_test_allocator_uninstall();
    check_clean();
}

/* A string that is in the list twice creates nothing, wherever the two
 * are. */
NAOS_TEST(a_duplicate_is_an_invalid_definition)
{
    static const char *const adjacent[] = { "red", "red" };
    static const char *const apart[] = { "red", "green", "blue", "red" };
    static const char *const last_two[] = { "red", "green", "blue", "blue" };
    static const char *const empty_twice[] = { "", "red", "" };
    clargs_type_t *type = as_destroyable(clargs_type_string());

    clargs_test_allocator_install();

    NAOS_CHECK_INT_EQ(clargs_type_create_choice(adjacent, 2, &type), CLARGS_ERROR_INVALID_DEFINITION);
    NAOS_CHECK_TRUE(type == NULL);

    type = as_destroyable(clargs_type_string());
    NAOS_CHECK_INT_EQ(clargs_type_create_choice(apart, 4, &type), CLARGS_ERROR_INVALID_DEFINITION);
    NAOS_CHECK_TRUE(type == NULL);

    type = as_destroyable(clargs_type_string());
    NAOS_CHECK_INT_EQ(clargs_type_create_choice(last_two, 4, &type), CLARGS_ERROR_INVALID_DEFINITION);
    NAOS_CHECK_TRUE(type == NULL);

    type = as_destroyable(clargs_type_string());
    NAOS_CHECK_INT_EQ(clargs_type_create_choice(empty_twice, 3, &type), CLARGS_ERROR_INVALID_DEFINITION);
    NAOS_CHECK_TRUE(type == NULL);

    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);

    /* Two strings that differ only in case are no duplicate. */
    {
        static const char *const different_case[] = { "red", "Red" };

        NAOS_CHECK_INT_EQ(clargs_type_create_choice(different_case, 2, &type), CLARGS_ERROR_NONE);
        NAOS_CHECK_TRUE(type != NULL);
        clargs_type_destroy(type);
    }

    clargs_test_allocator_uninstall();
    check_clean();
}

/* When an allocation of the creation fails, whichever it is, nothing stays
 * allocated, the output is NULL, and the error is the allocation failure. */
NAOS_TEST(choice_creation_frees_everything_when_an_allocation_fails)
{
    static const char *const choices[] = { "red", "green", "blue" };
    clargs_type_t *type = NULL;
    size_t allocation_points;
    size_t n;

    clargs_test_allocator_install();

    /* How many requests a creation takes. */
    NAOS_REQUIRE_TRUE(clargs_type_create_choice(choices, 3, &type) == CLARGS_ERROR_NONE);
    allocation_points = clargs_test_allocator_requests();
    clargs_type_destroy(type);
    NAOS_REQUIRE_TRUE(allocation_points > 0);
    NAOS_REQUIRE_TRUE(clargs_test_allocator_live() == 0);

    for (n = 1; n <= allocation_points; ++n) {
        clargs_test_allocator_reset();
        clargs_test_allocator_fail_at(n);

        type = as_destroyable(clargs_type_string());
        NAOS_CHECK_INT_EQ(clargs_type_create_choice(choices, 3, &type), CLARGS_ERROR_OUT_OF_MEMORY);
        NAOS_CHECK_TRUE(type == NULL);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), n);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);
        NAOS_CHECK_UINT_EQ(clargs_test_allocator_violations(), 0);
    }

    /* The creation works again afterwards. */
    clargs_test_allocator_reset();
    NAOS_CHECK_INT_EQ(clargs_type_create_choice(choices, 3, &type), CLARGS_ERROR_NONE);
    clargs_type_destroy(type);

    clargs_test_allocator_uninstall();
    check_clean();
}

/* ---- Destroy ---------------------------------------------------------- */

/* The destruction frees every block of the creation. */
NAOS_TEST(destroy_frees_everything)
{
    static const char *const choices[] = { "red", "", "blue" };
    clargs_type_t *type = NULL;

    clargs_test_allocator_install();

    NAOS_REQUIRE_TRUE(clargs_type_create_choice(choices, 3, &type) == CLARGS_ERROR_NONE);
    NAOS_CHECK_TRUE(clargs_test_allocator_live() > 0);
    clargs_type_destroy(type);
    NAOS_CHECK_UINT_EQ(clargs_test_allocator_live(), 0);

    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_TEST(destroy_does_nothing_for_null)
{
    clargs_test_allocator_install();

    clargs_type_destroy(NULL);

    NAOS_CHECK_UINT_EQ(clargs_test_allocator_requests(), 0);
    clargs_test_allocator_uninstall();
    check_clean();
}

/* A built-in value type is not freed and stays as it is. A free of it
 * would be seen by the test allocator as a live count that wraps around. */
NAOS_TEST(destroy_does_nothing_for_the_builtin_types)
{
    int64_t value = 0;

    clargs_test_allocator_install();

    clargs_type_destroy(as_destroyable(clargs_type_string()));
    clargs_type_destroy(as_destroyable(clargs_type_int()));
    clargs_type_destroy(as_destroyable(clargs_type_float()));
    clargs_type_destroy(as_destroyable(clargs_type_int()));

    NAOS_CHECK_STR_EQ(clargs_type_name(clargs_type_string()), "string");
    NAOS_CHECK_STR_EQ(clargs_type_name(clargs_type_int()), "integer");
    NAOS_CHECK_STR_EQ(clargs_type_name(clargs_type_float()), "float");
    NAOS_CHECK_INT_EQ(convert_int("42", &value), CLARGS_ERROR_NONE);
    NAOS_CHECK_TRUE(value == 42);

    clargs_test_allocator_uninstall();
    check_clean();
}

NAOS_SUITE(builtin,
    NAOS_ADD_TEST(string_has_its_name_kind_and_value_size),
    NAOS_ADD_TEST(int_has_its_name_kind_and_value_size),
    NAOS_ADD_TEST(float_has_its_name_kind_and_value_size),
    NAOS_ADD_TEST(each_function_returns_the_same_pointer_every_time),
    NAOS_ADD_TEST(the_three_types_are_different_objects),
    NAOS_ADD_TEST(kinds_have_the_values_of_their_order)
)

NAOS_SUITE(convert,
    NAOS_ADD_TEST(string_conversion_writes_the_pointer_to_the_input),
    NAOS_ADD_TEST(string_conversion_succeeds_for_the_empty_string),
    NAOS_ADD_TEST(int_conversion_succeeds),
    NAOS_ADD_TEST(int_conversion_fails),
    NAOS_ADD_TEST(float_conversion_succeeds),
    NAOS_ADD_TEST(float_conversion_fails),
    NAOS_ADD_TEST(conversion_is_requested_the_same_way_for_every_type)
)

NAOS_SUITE(choice,
    NAOS_ADD_TEST(choice_has_its_name_kind_and_value_size),
    NAOS_ADD_TEST(choice_conversion_writes_the_index_of_the_match),
    NAOS_ADD_TEST(choice_conversion_fails_without_an_exact_match),
    NAOS_ADD_TEST(choices_are_case_sensitive),
    NAOS_ADD_TEST(the_empty_string_can_be_a_choice),
    NAOS_ADD_TEST(a_single_choice_is_enough),
    NAOS_ADD_TEST(choices_are_read_in_the_order_of_the_definition),
    NAOS_ADD_TEST(a_type_that_is_not_a_choice_has_no_choices),
    NAOS_ADD_TEST(choices_are_copied),
    NAOS_ADD_TEST(an_empty_list_is_an_invalid_definition),
    NAOS_ADD_TEST(a_duplicate_is_an_invalid_definition),
    NAOS_ADD_TEST(choice_creation_frees_everything_when_an_allocation_fails)
)

NAOS_SUITE(destroy,
    NAOS_ADD_TEST(destroy_frees_everything),
    NAOS_ADD_TEST(destroy_does_nothing_for_null),
    NAOS_ADD_TEST(destroy_does_nothing_for_the_builtin_types)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(builtin),
    NAOS_ADD_SUITE(convert),
    NAOS_ADD_SUITE(choice),
    NAOS_ADD_SUITE(destroy)
)
