// test_hash_map.c - Johan Smet - BSD-3-Clause (see LICENSE)

#include "munit/munit.h"
#include "hash_map.h"

static MunitResult test_pointer(const MunitParameter params[], void* user_data_or_fixture) {
    char *key1 = "key";
    char *val1 = "val";
    char *key2 = "another_key";
    char *val2 = "and another val";
    char *key3 = "third";
    char *val3 = "moar";

    HashMap map = {0};
    map_ptr_ptr_put(&map, key1, val1);
    map_ptr_ptr_put(&map, key2, val2);
    map_ptr_ptr_put(&map, key3, val3);
    munit_assert_int(map_len(&map), ==, 3);

    munit_assert_string_equal(map_ptr_ptr_get(&map, key1), val1);
    munit_assert_string_equal(map_ptr_ptr_get(&map, key2), val2);
    munit_assert_string_equal(map_ptr_ptr_get(&map, key3), val3);

    map_free(&map);

    return MUNIT_OK;
}

static MunitResult test_integer(const MunitParameter params[], void* user_data_or_fixture) {
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

    map_free(&map);

    return MUNIT_OK;
}

static MunitResult test_string(const MunitParameter params[], void* user_data_or_fixture) {
    HashMap map = {0};

    map_str_int_put(&map, "first", 1);
    map_str_int_put(&map, "second", 2);
    munit_assert_int(map_len(&map), ==, 2);

    munit_assert_int(map_str_int_get(&map, "first"), ==, 1);
    munit_assert_int(map_str_int_get(&map, "second"), ==, 2);

    map_str_int_put(&map, "first", 10);
    munit_assert_int(map_len(&map), ==, 2);
    munit_assert_int(map_str_int_get(&map, "first"), ==, 10);

    map_free(&map);

    return MUNIT_OK;
}

static MunitResult test_iteration(const MunitParameter params[], void* user_data_or_fixture) {
    HashMap map = {0};

    map_str_int_put(&map, "one", 1);
    map_str_int_put(&map, "two", 2);
    map_str_int_put(&map, "three", 3);

    int check[3] = {0};

    for (int idx = map_begin(&map); idx != map_end(&map); idx = map_next(&map, idx)) {
        const char *key = map_key_str(&map, idx);
        int val = map_val_int(&map, idx);

        if (strcmp(key, "one") == 0) {
            ++check[0];
        } else if (strcmp(key, "two") == 0) {
            ++check[1];
        } else if (strcmp(key, "three") == 0) {
            ++check[2];
        }

        if (val >= 1 && val <= 3) {
            ++check[val-1];
        }
    }

    munit_assert_int(check[0], ==, 2);
    munit_assert_int(check[1], ==, 2);
    munit_assert_int(check[2], ==, 2);

    map_free(&map);

    return MUNIT_OK;
}

static MunitResult test_empty(const MunitParameter params[], void* user_data_or_fixture) {
    HashMap map = {0};

    munit_assert_int(map_len(&map), ==, 0);
    munit_assert_int(map_str_int_get(&map, "one"), ==, 0);

    return MUNIT_OK;
}

MunitTest hash_map_tests[] = {
    { "/pointer", test_pointer, NULL, NULL,  MUNIT_TEST_OPTION_NONE, NULL },
    { "/integer", test_integer, NULL, NULL,  MUNIT_TEST_OPTION_NONE, NULL },
    { "/string", test_string, NULL, NULL,  MUNIT_TEST_OPTION_NONE, NULL },
    { "/iteration", test_iteration, NULL, NULL,  MUNIT_TEST_OPTION_NONE, NULL },
    { "/empty", test_empty, NULL, NULL,  MUNIT_TEST_OPTION_NONE, NULL },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};
