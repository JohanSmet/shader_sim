cmake_minimum_required(VERSION 3.1)

project(shader_sim)

# force C99 for all targets
set(CMAKE_C_STANDARD 99)

# platform detection
string(TOUPPER ${CMAKE_SYSTEM_NAME} PLATFORM_NAME)
string(CONCAT PLATFORM_DEF "PLATFORM_" ${PLATFORM_NAME})

#
# MAIN executable
#
set(APP_TARGET shader_sim)

set(SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/allocator.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/dyn_array.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/hash_map.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/main.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/spirv_binary.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/spirv_text.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/spirv_module.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/spirv_simulator.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/runner.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/runner_lut.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/utils.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/libs/cJSON/cJSON.c"
)

set(HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/allocator.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/dyn_array.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/hash_map.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/spirv_binary.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/spirv_text.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/spirv_module.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/spirv_simulator.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/types.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/spirv/GLSL.std.450.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/spirv/spirv.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/spirv/spirv_names.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/runner.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/runner_lut.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/utils.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/libs/cJSON/cJSON.h"
)

add_executable(${APP_TARGET} ${SOURCES} ${HEADERS})
target_include_directories(${APP_TARGET} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/libs")
target_compile_definitions(${APP_TARGET} PRIVATE ${PLATFORM_DEF})

#
# Unit tests
#

enable_testing()

set (TEST_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/tests/test_main.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/tests/test_dyn_array.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/tests/test_hash_map.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_basic_ops.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/dyn_array.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/hash_map.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/tests/munit/munit.c"
)

set (TEST_HEADERS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/dyn_array.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/hash_map.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/tests/munit/munit.h"
)

add_executable(test_runner ${TEST_SOURCES} ${TEST_HEADERS})
target_include_directories(test_runner PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
add_test(NAME unittests COMMAND test_runner)

add_custom_command(
     TARGET test_runner
     COMMENT "Run tests"
     POST_BUILD 
     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
     COMMAND ${CMAKE_CTEST_COMMAND} -C $<CONFIGURATION> --output-on-failures
)
