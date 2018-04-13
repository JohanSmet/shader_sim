cmake_minimum_required(VERSION 3.1)

project(shader_sim)

# force C99 for all targets
set(CMAKE_C_STANDARD 99)

#
# MAIN executable
#
set(APP_TARGET shader_sim)

set(SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/src/dyn_array.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/main.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/spirv_binary.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/spirv_text.c"
)

set(HEADERS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/dyn_array.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/hash_map.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/spirv_binary.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/spirv_text.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/types.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/spirv/GLSL.std.450.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/spirv/spirv.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/spirv/spirv_names.h"
)

add_executable(${APP_TARGET} ${SOURCES} ${HEADERS})

