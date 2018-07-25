#include "munit/munit.h"

extern MunitTest dyn_array_tests[];
extern MunitTest hash_map_tests[];
extern MunitTest basic_ops_tests[];
extern MunitTest spirv_sim_tests[];

static MunitSuite extern_suites[] = {
    { .prefix = "/dyn_array", 
      .tests = dyn_array_tests,
      .suites = NULL,
      .iterations = 1,
      .options = MUNIT_SUITE_OPTION_NONE
    },
    { .prefix = "/hash_map", 
      .tests = hash_map_tests,
      .suites = NULL,
      .iterations = 1,
      .options = MUNIT_SUITE_OPTION_NONE
    },
    { .prefix = "/basic_ops",
      .tests = basic_ops_tests,
      .suites = NULL,
      .iterations = 1,
      .options = MUNIT_SUITE_OPTION_NONE
    },
    { .prefix = "/spirv",
      .tests = spirv_sim_tests,
      .suites = NULL,
      .iterations = 1,
      .options = MUNIT_SUITE_OPTION_NONE
    },
    { NULL, NULL, NULL, 0, MUNIT_SUITE_OPTION_NONE}
};

static const MunitSuite suite = {
    "",
    NULL, /* tests */
    extern_suites,
    1, /* iterations */
    MUNIT_SUITE_OPTION_NONE /* options */
};

int main(int argc, char *argv[]) {
    return munit_suite_main_custom(&suite, NULL, argc, argv, NULL);
}
