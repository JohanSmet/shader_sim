//  test_basic_ops.c - Johan Smet - BSD-3-Clause (see LICENSE)

#include "munit/munit.h"
#include "types.h"

MunitResult test_min_max(const MunitParameter params[], void* user_data_or_fixture) {
    
    munit_assert_int(MIN(5, 6), ==, 5);
    munit_assert_int(MIN(6, 5), ==, 5);
    munit_assert_int(MIN(5, 5), ==, 5);
    
    munit_assert_int(MAX(5, 6), ==, 6);
    munit_assert_int(MAX(6, 5), ==, 6);
    munit_assert_int(MAX(5, 5), ==, 5);
    
    return MUNIT_OK;
}

MunitResult test_pow2(const MunitParameter params[], void* user_data_or_fixture) {
    
    munit_assert(!IS_POW2(0));
    munit_assert(IS_POW2(1));
    munit_assert(IS_POW2(2));
    munit_assert(!IS_POW2(3));
    munit_assert(IS_POW2(4));

    return MUNIT_OK;
}

MunitResult test_align(const MunitParameter params[], void* user_data_or_fixture) {
    
    munit_assert_int(ALIGN_DOWN(18, 8), ==, 16);
    munit_assert_int(ALIGN_DOWN(16, 8), ==, 16);
    munit_assert_int(ALIGN_DOWN(33, 8), ==, 32);
    munit_assert_int(ALIGN_DOWN(31, 8), ==, 24);
    
    munit_assert_int(ALIGN_UP(18, 8), ==, 24);
    munit_assert_int(ALIGN_UP(16, 8), ==, 16);
    munit_assert_int(ALIGN_UP(33, 8), ==, 40);
    munit_assert_int(ALIGN_UP(31, 8), ==, 32);
    
    munit_assert_ptr(PTR_ALIGN_DOWN((void *) 18, 8), ==, (void *) 16);
    munit_assert_ptr(PTR_ALIGN_UP((void *) 18, 8), ==, (void *) 24);
    
    void *ptr = (void *) 0x0000000102802200;
    munit_assert_ptr(PTR_ALIGN_DOWN(ptr, 8), ==, ptr);


    return MUNIT_OK;
}

MunitTest basic_ops_tests[] = {
    { "/min_max", test_min_max, NULL, NULL,  MUNIT_TEST_OPTION_NONE, NULL },
    { "/pow_2", test_pow2, NULL, NULL,  MUNIT_TEST_OPTION_NONE, NULL },
    { "/align", test_align, NULL, NULL,  MUNIT_TEST_OPTION_NONE, NULL },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};
