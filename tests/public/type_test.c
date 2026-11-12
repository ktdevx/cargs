/*
 * Tests of the public value type API: <clargs/type.h>.
 *
 * The boundaries of the conversion of numbers are tested with the internal
 * functions (tests/internal/number_test.c). Here, the built-in value types
 * are used the way a consumer uses them: through clargs_type_convert().
 */
#include <clargs/clargs.h>

#include <naos/naos.h>

#include <stddef.h>
#include <stdint.h>

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
            NAOS_CHECK_TRUE(0);
            break;
        }

        clargs_type_release(types[index], &value);
    }
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

NAOS_MAIN(
    NAOS_ADD_SUITE(builtin),
    NAOS_ADD_SUITE(convert)
)
