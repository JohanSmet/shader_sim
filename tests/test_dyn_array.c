// test_dyn_array.c - Johan Smet - BSD-3-Clause (see LICENSE)

#include "munit/munit.h"
#include "dyn_array.h"

MunitResult test_basic(const MunitParameter params[], void* user_data_or_fixture) {
    int *test_array = NULL;

    munit_assert_int(arr_len(test_array), ==, 0);
    munit_assert_int(arr_cap(test_array), ==, 0);

    arr_push(test_array, 1);
    munit_assert_not_null(test_array);
    munit_assert_int(arr_len(test_array), ==, 1);
    munit_assert_int(arr_cap(test_array), >, 0);
    munit_assert_int(test_array[0], ==, 1);

    arr_free(test_array);
    munit_assert_null(test_array);
    munit_assert_int(arr_len(test_array), ==, 0);
    munit_assert_int(arr_cap(test_array), ==, 0);

    return MUNIT_OK;
}

MunitResult test_iteration(const MunitParameter params[], void* user_data_or_fixture) {
    int *test_array = NULL;
    int data[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    for (int idx = 0; idx < 10; ++idx) {
        arr_push_buf(test_array, data, 10);
        munit_assert_int(arr_len(test_array), ==, (idx + 1) * 10);
    }

    int expected = 0;
    for (int *it = test_array; it != arr_end(test_array); ++it) {
        munit_assert_int(*it, ==, expected);
        expected = (expected + 1) % 10;
    }

    arr_free(test_array);

    return MUNIT_OK;
}

MunitResult test_clear(const MunitParameter params[], void* user_data_or_fixture) {
    int *test_array = NULL;
    int data[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    arr_push_buf(test_array, data, 10);
    munit_assert_int(arr_len(test_array), ==, 10);
    size_t cap = arr_cap(test_array);

    arr_clear(test_array);

    munit_assert_not_null(test_array);
    munit_assert_int(arr_len(test_array), ==, 0);
    munit_assert_int(arr_cap(test_array), ==, cap);

    return MUNIT_OK;
}

MunitResult test_printf(const MunitParameter params[], void* user_data_or_fixture) {
    char *test_string = NULL;

    arr_printf(test_string, "%d", 10);
    munit_assert_string_equal(test_string, "10");
    arr_printf(test_string, " %.2f", 3.14);
    munit_assert_string_equal(test_string, "10 3.14");

    arr_clear(test_string);
    arr_printf(test_string, "Hello World");
    munit_assert_string_equal(test_string, "Hello World");

    return MUNIT_OK;
}

MunitResult test_reserve(const MunitParameter params[], void* user_data_or_fixture) {
    
    uint8_t *test_array = NULL;
    
    arr_reserve(test_array, 5);
    munit_assert_not_null(test_array);
    munit_assert_int(arr_len(test_array), ==, 5);
    munit_assert_int(arr_cap(test_array), >=, 5);
    
    return MUNIT_OK;
}

MunitResult test_pop(const MunitParameter params[], void* user_data_or_fixture) {
    int *test_array = NULL;

    arr_push(test_array, 0);
    arr_push(test_array, 1);
    arr_push(test_array, 2);
    arr_push(test_array, 3);

    munit_assert_int(arr_len(test_array), ==, 4);
    munit_assert_int(arr_pop(test_array), ==, 3);

    munit_assert_int(arr_len(test_array), ==, 3);
    munit_assert_int(arr_pop(test_array), ==, 2);

    munit_assert_int(arr_len(test_array), ==, 2);
    munit_assert_int(arr_pop(test_array), ==, 1);

    munit_assert_int(arr_len(test_array), ==, 1);
    munit_assert_int(arr_pop(test_array), ==, 0);

    munit_assert_int(arr_len(test_array), ==, 0);
}

MunitTest dyn_array_tests[] = {
    { "/basic", test_basic, NULL, NULL,  MUNIT_TEST_OPTION_NONE, NULL },
    { "/iteration", test_iteration, NULL, NULL,  MUNIT_TEST_OPTION_NONE, NULL },
    { "/clear", test_clear, NULL, NULL,  MUNIT_TEST_OPTION_NONE, NULL },
    { "/printf", test_printf, NULL, NULL,  MUNIT_TEST_OPTION_NONE, NULL },
    { "/reserve", test_reserve, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    { "/pop", test_pop, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};
