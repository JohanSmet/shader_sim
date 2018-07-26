//  test_spirv_sim_arithmetic.c - Johan Smet - BSD-3-Clause (see LICENSE)

#include "munit/munit.h"
#include "types.h"
#include "dyn_array.h"

#include "spirv_binary.h"
#include "spirv_module.h"
#include "spirv_simulator.h"
#include "spirv/spirv.h"

#include <math.h>

#define S(a,b,c,d)  ((uint32_t) (d) << 24 | (c) << 16 | (b) << 8 | (a))
#define ID(i) i
#define SPIRV_OP(bin,op,...)  spirv_bin_opcode_add(bin, op, (uint32_t[]){__VA_ARGS__}, sizeof((uint32_t[]) {__VA_ARGS__})/sizeof(uint32_t))

#define TEST_TYPE_FLOAT32   1
#define TEST_TYPE_INT32     2
#define TEST_TYPE_UINT32    4

static inline float fremf(float v1, float v2) {
    /* The math.h remainder function rounds the quotient to the nearest integer value which means
       that two positive numbers can have a negative remainder (e.g. 3.5 / 1). This doesn't match
       the behavior described by the SPIR-V specification wich takes the sign from either the first
       or the second operand. This functions takes the sign from the second operand */
    return (v1 - (v2 * floorf(v1/v2)));
}

static inline void spirv_common_header(SPIRV_binary *bin) {
    SPIRV_OP(bin, SpvOpCapability,  SpvCapabilityShader);
    SPIRV_OP(bin, SpvOpExtInstImport, ID(1), S('G','L','S','L'), S('.','s','t','d'), S('.','4','5','0'), S(0,0,0,0));
    SPIRV_OP(bin, SpvOpMemoryModel, SpvAddressingModelLogical, SpvMemoryModelGLSL450);
    SPIRV_OP(bin, SpvOpEntryPoint, SpvExecutionModelVertex, 4, S('m','a','i','n'), S(0,0,0,0));
}

static inline void spirv_common_types(SPIRV_binary *bin, uint32_t type_flags) {
    SPIRV_OP(bin, SpvOpTypeVoid, ID(2));
    SPIRV_OP(bin, SpvOpTypeFunction, ID(3), ID(2));
    
    if (type_flags & TEST_TYPE_FLOAT32) {
        SPIRV_OP(bin, SpvOpTypeFloat, ID(10), 32);
        SPIRV_OP(bin, SpvOpTypeVector, ID(11), ID(10), 4);
        SPIRV_OP(bin, SpvOpTypeMatrix, ID(12), ID(11), 4);
        SPIRV_OP(bin, SpvOpTypePointer, ID(13), SpvStorageClassInput, ID(11));
    }
    
    if (type_flags & TEST_TYPE_INT32) {
        SPIRV_OP(bin, SpvOpTypeInt, ID(20), 32, 1);
        SPIRV_OP(bin, SpvOpTypeVector, ID(21), ID(20), 4);
        SPIRV_OP(bin, SpvOpTypeMatrix, ID(22), ID(21), 4);
        SPIRV_OP(bin, SpvOpTypePointer, ID(23), SpvStorageClassInput, ID(21));
    }
    
    if (type_flags & TEST_TYPE_INT32) {
        SPIRV_OP(bin, SpvOpTypeInt, ID(30), 32, 0);
        SPIRV_OP(bin, SpvOpTypeVector, ID(31), ID(30), 4);
        SPIRV_OP(bin, SpvOpTypeMatrix, ID(32), ID(31), 4);
        SPIRV_OP(bin, SpvOpTypePointer, ID(33), SpvStorageClassInput, ID(31));
    }
}

static inline void spirv_common_function_header(SPIRV_binary *bin) {
    SPIRV_OP(bin, SpvOpFunction, ID(2), ID(4), SpvFunctionControlMaskNone, ID(3));
    SPIRV_OP(bin, SpvOpLabel, ID(5));
}

static inline void spirv_common_function_footer(SPIRV_binary *bin) {
    SPIRV_OP(bin, SpvOpReturn);
    SPIRV_OP(bin, SpvOpFunctionEnd);
}

#define ASSERT_REGISTER_VEC4(sim, id, op, e0, e1, e2, e3) { \
    SimRegister *reg = spirv_sim_register_by_id(sim, id);   \
    munit_assert_not_null(reg);                             \
    munit_assert_float(reg->vec[0], op, e0);       \
    munit_assert_float(reg->vec[1], op, e1);       \
    munit_assert_float(reg->vec[2], op, e2);       \
    munit_assert_float(reg->vec[3], op, e3);       \
}

MunitResult test_arithmetic_float32(const MunitParameter params[], void* user_data_or_fixture) {
    
    /* prepare binary */
    SPIRV_binary spirv_bin;
    spirv_bin_init(&spirv_bin, 1, 0);
    
    spirv_common_header(&spirv_bin);
    SPIRV_OP(&spirv_bin, SpvOpDecorate, ID(40), SpvDecorationLocation, 0);
    SPIRV_OP(&spirv_bin, SpvOpDecorate, ID(41), SpvDecorationLocation, 1);
    spirv_common_types(&spirv_bin, TEST_TYPE_FLOAT32);
    SPIRV_OP(&spirv_bin, SpvOpVariable, ID(13), ID(40), SpvStorageClassInput);
    SPIRV_OP(&spirv_bin, SpvOpVariable, ID(13), ID(41), SpvStorageClassInput);
    spirv_common_function_header(&spirv_bin);
    SPIRV_OP(&spirv_bin, SpvOpLoad, ID(11), ID(60), ID(40));
    SPIRV_OP(&spirv_bin, SpvOpLoad, ID(11), ID(61), ID(41));
    SPIRV_OP(&spirv_bin, SpvOpFNegate, ID(11), ID(62), ID(60));
    SPIRV_OP(&spirv_bin, SpvOpFAdd, ID(11), ID(63), ID(61), ID(60));
    SPIRV_OP(&spirv_bin, SpvOpFSub, ID(11), ID(64), ID(61), ID(60));
    SPIRV_OP(&spirv_bin, SpvOpFMul, ID(11), ID(65), ID(61), ID(60));
    SPIRV_OP(&spirv_bin, SpvOpFDiv, ID(11), ID(66), ID(61), ID(60));
    SPIRV_OP(&spirv_bin, SpvOpFRem, ID(11), ID(67), ID(61), ID(60));
    SPIRV_OP(&spirv_bin, SpvOpFRem, ID(11), ID(68), ID(61), ID(62));
    SPIRV_OP(&spirv_bin, SpvOpFMod, ID(11), ID(69), ID(61), ID(60));
    SPIRV_OP(&spirv_bin, SpvOpFMod, ID(11), ID(70), ID(61), ID(62));
    spirv_common_function_footer(&spirv_bin);
    spirv_bin.header.bound_ids = 71;
    spirv_bin_finalize(&spirv_bin);
    
    /* prepare simulator */
    SPIRV_module spirv_module;
    spirv_module_load(&spirv_module, &spirv_bin);
    
    SPIRV_simulator spirv_sim;
    spirv_sim_init(&spirv_sim, &spirv_module);
    float data1[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float data2[4] = {3.5f, 6.6f, 8.0f, 11.0f};
    spirv_sim_variable_associate_data(&spirv_sim, VarKindInput, VarInterfaceLocation, 0, (uint8_t *) data1, sizeof(data1));
    spirv_sim_variable_associate_data(&spirv_sim, VarKindInput, VarInterfaceLocation, 1, (uint8_t *) data2, sizeof(data2));

    /* run simulator */
    while (!spirv_sim.finished && !spirv_sim.error_msg) {
        spirv_sim_step(&spirv_sim);
    }
    
    /* check registers */
    ASSERT_REGISTER_VEC4(&spirv_sim, ID(62), ==,                        /* OpFNegate */
        -data1[0], -data1[1], -data1[2], -data1[3]);
    ASSERT_REGISTER_VEC4(&spirv_sim, ID(63), ==,                        /* OpFAdd */
        data2[0] + data1[0], data2[1] + data1[1], data2[2] + data1[2], data2[3] + data1[3]);
    ASSERT_REGISTER_VEC4(&spirv_sim, ID(64), ==,                        /* OpFSub */
        data2[0] - data1[0], data2[1] - data1[1], data2[2] - data1[2], data2[3] - data1[3]);
    ASSERT_REGISTER_VEC4(&spirv_sim, ID(65), ==,                        /* OpFMul */
        data2[0] * data1[0], data2[1] * data1[1], data2[2] * data1[2], data2[3] * data1[3]);
    ASSERT_REGISTER_VEC4(&spirv_sim, ID(66), ==,                        /* OpFDiv */
        data2[0] / data1[0], data2[1] / data1[1], data2[2] / data1[2], data2[3] / data1[3]);
    ASSERT_REGISTER_VEC4(&spirv_sim, ID(67), ==,                        /* OpFRem */
        fmodf(data2[0], data1[0]), fmodf(data2[1], data1[1]),
        fmodf(data2[2], data1[2]), fmodf(data2[3], data1[3]));
    ASSERT_REGISTER_VEC4(&spirv_sim, ID(68), ==,                        /* OpFRem */
        fmodf(data2[0], -data1[0]), fmodf(data2[1], -data1[1]),
        fmodf(data2[2], -data1[2]), fmodf(data2[3], -data1[3]));
    ASSERT_REGISTER_VEC4(&spirv_sim, ID(69), ==,                        /* OpFMod */
        fremf(data2[0], data1[0]), fremf(data2[1], data1[1]),
        fremf(data2[2], data1[2]), fremf(data2[3], data1[3]));
    ASSERT_REGISTER_VEC4(&spirv_sim, ID(70), ==,                        /* OpFMod */
        fremf(data2[0], -data1[0]), fremf(data2[1], -data1[1]),
        fremf(data2[2], -data1[2]), fremf(data2[3], -data1[3]));

    return MUNIT_OK;
}

MunitTest spirv_sim_tests[] = {
    { "/arithmetic_float32", test_arithmetic_float32, NULL, NULL,  MUNIT_TEST_OPTION_NONE, NULL },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};
