// test_hash_map.c - Johan Smet - BSD-3-Clause (see LICENSE)

#include "munit/munit.h"
#include "hash_map.h"

MunitResult test_pointer(const MunitParameter params[], void* user_data_or_fixture) {
    char *key1 = strdup("key");
    char *val1 = strdup("val");
    char *key2 = strdup("another_key");
    char *val2 = strdup("and another val");
    char *key3 = strdup("third");
    char *val3 = strdup("moar");

    HashMap map = {0};
    map_ptr_ptr_put(&map, key1, val1);
    map_ptr_ptr_put(&map, key2, val2);
    map_ptr_ptr_put(&map, key3, val3);
    munit_assert_int(map_len(&map), ==, 3);

    munit_assert_string_equal(map_ptr_ptr_get(&map, key1), val1);
    munit_assert_string_equal(map_ptr_ptr_get(&map, key2), val2);
    munit_assert_string_equal(map_ptr_ptr_get(&map, key3), val3);

    return MUNIT_OK;
}

MunitResult test_integer(const MunitParameter params[], void* user_data_or_fixture) {
    HashMap map = {0};

    map_int_int_put(&map, 2, 2);
    munit_assert_int(map_len(&map), ==, 1);
    munit_assert_int((int64_t) map_int_int_get(&map, 2), ==, 2);
    munit_assert_int(map_int_int_get(&map, 3), ==, 0);

    map_int_int_put(&map, 2, 3);
    munit_assert_int((int64_t) map_int_int_get(&map, 2), ==, 3);

    for (uint64_t idx = 1; idx < 1024; ++idx) {
        map_int_int_put(&map, idx, idx * idx);
    }

    for (uint64_t idx = 1; idx < 1024; ++idx) {
        munit_assert_int((int64_t) map_int_int_get(&map, idx), ==, idx * idx);
    }

    return MUNIT_OK;
}

MunitResult test_string(const MunitParameter params[], void* user_data_or_fixture) {
    HashMap map = {0};

    map_str_int_put(&map, "first", 1);
    map_str_int_put(&map, "second", 2);
    munit_assert_int(map_len(&map), ==, 2);

    munit_assert_int(map_str_int_get(&map, "first"), ==, 1);
    munit_assert_int(map_str_int_get(&map, "second"), ==, 2);

    map_str_int_put(&map, "first", 10);
    munit_assert_int(map_len(&map), ==, 2);
    munit_assert_int(map_str_int_get(&map, "first"), ==, 10);

    return MUNIT_OK;
}

MunitTest hash_map_tests[] = {
    { "/pointer", test_pointer, NULL, NULL,  MUNIT_TEST_OPTION_NONE, NULL },
    { "/integer", test_integer, NULL, NULL,  MUNIT_TEST_OPTION_NONE, NULL },
    { "/string", test_string, NULL, NULL,  MUNIT_TEST_OPTION_NONE, NULL },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};
