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

static inline int32_t smod(int32_t v1, int32_t v2) {
    return (v1 - (v2 * truncf((float) v1 / v2)));
}

static inline int32_t srem(int32_t v1, int32_t v2) {
    return (v1 - (v2 * floorf((float) v1 / v2)));
}

static inline uint32_t urem(uint32_t v1, uint32_t v2) {
    return (v1 - (v2 * floorf((float) v1 / v2)));
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
        SPIRV_OP(bin, SpvOpTypePointer, ID(13), SpvStorageClassInput, ID(10));
        SPIRV_OP(bin, SpvOpTypePointer, ID(14), SpvStorageClassInput, ID(11));
    }
    
    if (type_flags & TEST_TYPE_INT32) {
        SPIRV_OP(bin, SpvOpTypeInt, ID(20), 32, 1);
        SPIRV_OP(bin, SpvOpTypeVector, ID(21), ID(20), 4);
        SPIRV_OP(bin, SpvOpTypeMatrix, ID(22), ID(21), 4);
        SPIRV_OP(bin, SpvOpTypePointer, ID(23), SpvStorageClassInput, ID(20));
        SPIRV_OP(bin, SpvOpTypePointer, ID(24), SpvStorageClassInput, ID(21));
    }
    
    if (type_flags & TEST_TYPE_UINT32) {
        SPIRV_OP(bin, SpvOpTypeInt, ID(30), 32, 0);
        SPIRV_OP(bin, SpvOpTypeVector, ID(31), ID(30), 4);
        SPIRV_OP(bin, SpvOpTypeMatrix, ID(32), ID(31), 4);
        SPIRV_OP(bin, SpvOpTypePointer, ID(33), SpvStorageClassInput, ID(30));
        SPIRV_OP(bin, SpvOpTypePointer, ID(34), SpvStorageClassInput, ID(31));
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

#define ASSERT_REGISTER_FLOAT(sim, id, op, e0) { \
    SimRegister *reg = spirv_sim_register_by_id(sim, id);   \
    munit_assert_not_null(reg);                             \
    munit_assert_float(reg->vec[0], op, (e0));              \
}

#define ASSERT_REGISTER_VEC4(sim, id, op, e0, e1, e2, e3) { \
    SimRegister *reg = spirv_sim_register_by_id(sim, id);   \
    munit_assert_not_null(reg);                             \
    munit_assert_float(reg->vec[0], op, (e0));       \
    munit_assert_float(reg->vec[1], op, (e1));       \
    munit_assert_float(reg->vec[2], op, (e2));       \
    munit_assert_float(reg->vec[3], op, (e3));       \
}

#define ASSERT_REGISTER_SVEC4(sim, id, op, e0, e1, e2, e3) { \
    SimRegister *reg = spirv_sim_register_by_id(sim, id);   \
    munit_assert_not_null(reg);                             \
    munit_assert_int32(reg->svec[0], op, (e0));       \
    munit_assert_int32(reg->svec[1], op, (e1));       \
    munit_assert_int32(reg->svec[2], op, (e2));       \
    munit_assert_int32(reg->svec[3], op, (e3));       \
}

#define ASSERT_REGISTER_UVEC4(sim, id, op, e0, e1, e2, e3) { \
    SimRegister *reg = spirv_sim_register_by_id(sim, id);   \
    munit_assert_not_null(reg);                             \
    munit_assert_uint32(reg->svec[0], op, (e0));       \
    munit_assert_uint32(reg->svec[1], op, (e1));       \
    munit_assert_uint32(reg->svec[2], op, (e2));       \
    munit_assert_uint32(reg->svec[3], op, (e3));       \
    }

MunitResult test_arithmetic_float32(const MunitParameter params[], void* user_data_or_fixture) {
    
    /* prepare binary */
    SPIRV_binary spirv_bin;
    spirv_bin_init(&spirv_bin, 1, 0);
    
    spirv_common_header(&spirv_bin);
    SPIRV_OP(&spirv_bin, SpvOpDecorate, ID(40), SpvDecorationLocation, 0);
    SPIRV_OP(&spirv_bin, SpvOpDecorate, ID(41), SpvDecorationLocation, 1);
    spirv_common_types(&spirv_bin, TEST_TYPE_FLOAT32);
    SPIRV_OP(&spirv_bin, SpvOpVariable, ID(14), ID(40), SpvStorageClassInput);
    SPIRV_OP(&spirv_bin, SpvOpVariable, ID(14), ID(41), SpvStorageClassInput);
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
    spirv_sim_variable_associate_data(
        &spirv_sim, 
        VarKindInput, 
        (VariableAccess) {VarAccessLocation, 0}, 
        (uint8_t *) data1, sizeof(data1));
    spirv_sim_variable_associate_data(
        &spirv_sim, 
        VarKindInput, 
        (VariableAccess) {VarAccessLocation, 1}, 
        (uint8_t *) data2, sizeof(data2));

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

MunitResult test_arithmetic_int32(const MunitParameter params[], void* user_data_or_fixture) {
    
    /* prepare binary */
    SPIRV_binary spirv_bin;
    spirv_bin_init(&spirv_bin, 1, 0);
    
    spirv_common_header(&spirv_bin);
    SPIRV_OP(&spirv_bin, SpvOpDecorate, ID(40), SpvDecorationLocation, 0);
    SPIRV_OP(&spirv_bin, SpvOpDecorate, ID(41), SpvDecorationLocation, 1);
    spirv_common_types(&spirv_bin, TEST_TYPE_INT32);
    SPIRV_OP(&spirv_bin, SpvOpVariable, ID(24), ID(40), SpvStorageClassInput);
    SPIRV_OP(&spirv_bin, SpvOpVariable, ID(24), ID(41), SpvStorageClassInput);
    spirv_common_function_header(&spirv_bin);
    SPIRV_OP(&spirv_bin, SpvOpLoad, ID(21), ID(60), ID(40));
    SPIRV_OP(&spirv_bin, SpvOpLoad, ID(21), ID(61), ID(41));
    SPIRV_OP(&spirv_bin, SpvOpSNegate, ID(21), ID(62), ID(60));
    SPIRV_OP(&spirv_bin, SpvOpIAdd, ID(21), ID(63), ID(61), ID(60));
    SPIRV_OP(&spirv_bin, SpvOpISub, ID(21), ID(64), ID(61), ID(60));
    SPIRV_OP(&spirv_bin, SpvOpIMul, ID(21), ID(65), ID(61), ID(60));
    SPIRV_OP(&spirv_bin, SpvOpSDiv, ID(21), ID(66), ID(61), ID(60));
    SPIRV_OP(&spirv_bin, SpvOpSRem, ID(21), ID(67), ID(61), ID(60));
    SPIRV_OP(&spirv_bin, SpvOpSRem, ID(21), ID(68), ID(61), ID(62));
    SPIRV_OP(&spirv_bin, SpvOpSMod, ID(21), ID(69), ID(61), ID(60));
    SPIRV_OP(&spirv_bin, SpvOpSMod, ID(21), ID(70), ID(61), ID(62));
    spirv_common_function_footer(&spirv_bin);
    spirv_bin.header.bound_ids = 71;
    spirv_bin_finalize(&spirv_bin);
    
    /* prepare simulator */
    SPIRV_module spirv_module;
    spirv_module_load(&spirv_module, &spirv_bin);
    
    SPIRV_simulator spirv_sim;
    spirv_sim_init(&spirv_sim, &spirv_module);
    int32_t data1[4] = {1, 2, 3, 4};
    int32_t data2[4] = {3, 6, 8, 11};
    spirv_sim_variable_associate_data(
        &spirv_sim, 
        VarKindInput, 
        (VariableAccess) {VarAccessLocation, 0},
        (uint8_t *) data1, sizeof(data1));
    spirv_sim_variable_associate_data(
        &spirv_sim, 
        VarKindInput, 
        (VariableAccess) {VarAccessLocation, 1}, 
        (uint8_t *) data2, sizeof(data2));
    
    /* run simulator */
    while (!spirv_sim.finished && !spirv_sim.error_msg) {
        spirv_sim_step(&spirv_sim);
    }
    
    /* check registers */
    ASSERT_REGISTER_SVEC4(&spirv_sim, ID(62), ==,                        /* OpSNegate */
                         -data1[0], -data1[1], -data1[2], -data1[3]);
    ASSERT_REGISTER_SVEC4(&spirv_sim, ID(63), ==,                        /* OpIAdd */
                         data2[0] + data1[0], data2[1] + data1[1], data2[2] + data1[2], data2[3] + data1[3]);
    ASSERT_REGISTER_SVEC4(&spirv_sim, ID(64), ==,                        /* OpISub */
                         data2[0] - data1[0], data2[1] - data1[1], data2[2] - data1[2], data2[3] - data1[3]);
    ASSERT_REGISTER_SVEC4(&spirv_sim, ID(65), ==,                        /* OpIMul */
                         data2[0] * data1[0], data2[1] * data1[1], data2[2] * data1[2], data2[3] * data1[3]);
    ASSERT_REGISTER_SVEC4(&spirv_sim, ID(66), ==,                        /* OpSDiv */
                         data2[0] / data1[0], data2[1] / data1[1], data2[2] / data1[2], data2[3] / data1[3]);
    ASSERT_REGISTER_SVEC4(&spirv_sim, ID(67), ==,                        /* OpSRem */
                         smod(data2[0], data1[0]), smod(data2[1], data1[1]),
                         smod(data2[2], data1[2]), smod(data2[3], data1[3]));
    ASSERT_REGISTER_SVEC4(&spirv_sim, ID(68), ==,                        /* OpSRem */
                         smod(data2[0], -data1[0]), smod(data2[1], -data1[1]),
                         smod(data2[2], -data1[2]), smod(data2[3], -data1[3]));
    ASSERT_REGISTER_SVEC4(&spirv_sim, ID(69), ==,                        /* OpSMod */
                         srem(data2[0], data1[0]), srem(data2[1], data1[1]),
                         srem(data2[2], data1[2]), srem(data2[3], data1[3]));
    ASSERT_REGISTER_SVEC4(&spirv_sim, ID(70), ==,                        /* OpSMod */
                         srem(data2[0], -data1[0]), srem(data2[1], -data1[1]),
                         srem(data2[2], -data1[2]), srem(data2[3], -data1[3]));

    return MUNIT_OK;
}

MunitResult test_arithmetic_uint32(const MunitParameter params[], void* user_data_or_fixture) {
    
    /* prepare binary */
    SPIRV_binary spirv_bin;
    spirv_bin_init(&spirv_bin, 1, 0);
    
    spirv_common_header(&spirv_bin);
    SPIRV_OP(&spirv_bin, SpvOpDecorate, ID(40), SpvDecorationLocation, 0);
    SPIRV_OP(&spirv_bin, SpvOpDecorate, ID(41), SpvDecorationLocation, 1);
    spirv_common_types(&spirv_bin, TEST_TYPE_UINT32);
    SPIRV_OP(&spirv_bin, SpvOpVariable, ID(34), ID(40), SpvStorageClassInput);
    SPIRV_OP(&spirv_bin, SpvOpVariable, ID(34), ID(41), SpvStorageClassInput);
    spirv_common_function_header(&spirv_bin);
    SPIRV_OP(&spirv_bin, SpvOpLoad, ID(31), ID(60), ID(40));
    SPIRV_OP(&spirv_bin, SpvOpLoad, ID(31), ID(61), ID(41));
    SPIRV_OP(&spirv_bin, SpvOpIAdd, ID(31), ID(62), ID(61), ID(60));
    SPIRV_OP(&spirv_bin, SpvOpISub, ID(31), ID(63), ID(61), ID(60));
    SPIRV_OP(&spirv_bin, SpvOpIMul, ID(31), ID(64), ID(61), ID(60));
    SPIRV_OP(&spirv_bin, SpvOpUDiv, ID(31), ID(65), ID(61), ID(60));
    SPIRV_OP(&spirv_bin, SpvOpUMod, ID(31), ID(66), ID(61), ID(60));
    spirv_common_function_footer(&spirv_bin);
    spirv_bin.header.bound_ids = 67;
    spirv_bin_finalize(&spirv_bin);
    
    /* prepare simulator */
    SPIRV_module spirv_module;
    spirv_module_load(&spirv_module, &spirv_bin);
    
    SPIRV_simulator spirv_sim;
    spirv_sim_init(&spirv_sim, &spirv_module);
    uint32_t data1[4] = {1, 2, 3, 4};
    uint32_t data2[4] = {3, 6, 8, 11};
    spirv_sim_variable_associate_data(
        &spirv_sim, 
        VarKindInput, 
        (VariableAccess) {VarAccessLocation, 0},
        (uint8_t *) data1, sizeof(data1));
    spirv_sim_variable_associate_data(
        &spirv_sim, 
        VarKindInput, 
        (VariableAccess) {VarAccessLocation, 1},
        (uint8_t *) data2, sizeof(data2));
    
    /* run simulator */
    while (!spirv_sim.finished && !spirv_sim.error_msg) {
        spirv_sim_step(&spirv_sim);
    }
    
    /* check registers */
    ASSERT_REGISTER_UVEC4(&spirv_sim, ID(62), ==,                        /* OpIAdd */
                          data2[0] + data1[0], data2[1] + data1[1], data2[2] + data1[2], data2[3] + data1[3]);
    ASSERT_REGISTER_UVEC4(&spirv_sim, ID(63), ==,                        /* OpISub */
                          data2[0] - data1[0], data2[1] - data1[1], data2[2] - data1[2], data2[3] - data1[3]);
    ASSERT_REGISTER_UVEC4(&spirv_sim, ID(64), ==,                        /* OpIMul */
                          data2[0] * data1[0], data2[1] * data1[1], data2[2] * data1[2], data2[3] * data1[3]);
    ASSERT_REGISTER_UVEC4(&spirv_sim, ID(65), ==,                        /* OpUDiv */
                          data2[0] / data1[0], data2[1] / data1[1], data2[2] / data1[2], data2[3] / data1[3]);
    ASSERT_REGISTER_UVEC4(&spirv_sim, ID(66), ==,                        /* OpUMod */
                          urem(data2[0], data1[0]), urem(data2[1], data1[1]),
                          urem(data2[2], data1[2]), urem(data2[3], data1[3]));

    return MUNIT_OK;
}

MunitResult test_composite_float32(const MunitParameter params[], void* user_data_or_fixture) {
 
#pragma pack(push, 0)
    typedef struct CompositeStruct {
        float f0;
        float f1;
        float v0[4];
        float v1[4];
    } CompositeStruct;
#pragma pack(pop)
    
    /* prepare binary */
    SPIRV_binary spirv_bin;
    spirv_bin_init(&spirv_bin, 1, 0);
    
    spirv_common_header(&spirv_bin);
    SPIRV_OP(&spirv_bin, SpvOpDecorate, ID(40), SpvDecorationLocation, 0);
    SPIRV_OP(&spirv_bin, SpvOpDecorate, ID(41), SpvDecorationLocation, 1);
    SPIRV_OP(&spirv_bin, SpvOpDecorate, ID(42), SpvDecorationLocation, 2);
    spirv_common_types(&spirv_bin, TEST_TYPE_FLOAT32 | TEST_TYPE_UINT32);
    SPIRV_OP(&spirv_bin, SpvOpVariable, ID(14), ID(40), SpvStorageClassInput);
    SPIRV_OP(&spirv_bin, SpvOpVariable, ID(14), ID(41), SpvStorageClassInput);
    SPIRV_OP(&spirv_bin, SpvOpVariable, ID(13), ID(42), SpvStorageClassInput);
    SPIRV_OP(&spirv_bin, SpvOpConstant, ID(30), ID(43), 0);
    SPIRV_OP(&spirv_bin, SpvOpConstant, ID(30), ID(44), 1);
    SPIRV_OP(&spirv_bin, SpvOpConstant, ID(30), ID(45), 2);
    SPIRV_OP(&spirv_bin, SpvOpConstant, ID(30), ID(46), 3);
    SPIRV_OP(&spirv_bin, SpvOpTypeStruct, ID(50), ID(10), ID(10), ID(11), ID(11));
    SPIRV_OP(&spirv_bin, SpvOpTypeArray, ID(51), ID(11), ID(45));
    SPIRV_OP(&spirv_bin, SpvOpTypeVector, ID(52), ID(10), 6);
    spirv_common_function_header(&spirv_bin);
    SPIRV_OP(&spirv_bin, SpvOpLoad, ID(11), ID(60), ID(40));
    SPIRV_OP(&spirv_bin, SpvOpLoad, ID(11), ID(61), ID(41));
    SPIRV_OP(&spirv_bin, SpvOpLoad, ID(10), ID(62), ID(42));
    SPIRV_OP(&spirv_bin, SpvOpVectorExtractDynamic, ID(10), ID(63), ID(60), ID(45));
    SPIRV_OP(&spirv_bin, SpvOpVectorInsertDynamic, ID(11), ID(64), ID(60), ID(62), ID(44));
    SPIRV_OP(&spirv_bin, SpvOpVectorShuffle, ID(11), ID(65), ID(60), ID(61), 4, 1, 6, 3);
    SPIRV_OP(&spirv_bin, SpvOpCompositeConstruct, ID(50), ID(66), ID(62), ID(63), ID(60), ID(61));
    SPIRV_OP(&spirv_bin, SpvOpCompositeConstruct, ID(51), ID(67), ID(60), ID(61));
    SPIRV_OP(&spirv_bin, SpvOpCompositeConstruct, ID(52), ID(68), ID(62), ID(60), ID(62));
    SPIRV_OP(&spirv_bin, SpvOpCompositeConstruct, ID(12), ID(69), ID(60), ID(61), ID(64), ID(65));
    SPIRV_OP(&spirv_bin, SpvOpCompositeExtract, ID(10), ID(70), ID(66), 2, 3);
    SPIRV_OP(&spirv_bin, SpvOpCompositeInsert, ID(50), ID(71), ID(62), ID(66), 3, 2);
    SPIRV_OP(&spirv_bin, SpvOpCopyObject, ID(50), ID(72), ID(71));
    SPIRV_OP(&spirv_bin, SpvOpTranspose, ID(12), ID(73), ID(69));
    spirv_common_function_footer(&spirv_bin);
    spirv_bin.header.bound_ids = 74;
    spirv_bin_finalize(&spirv_bin);
    
    /* prepare simulator */
    SPIRV_module spirv_module;
    spirv_module_load(&spirv_module, &spirv_bin);
    
    SPIRV_simulator spirv_sim;
    spirv_sim_init(&spirv_sim, &spirv_module);
    float data1[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float data2[4] = {3.5f, 6.6f, 8.0f, 11.0f};
    float data3 = 5.0f;
    spirv_sim_variable_associate_data(
        &spirv_sim, 
        VarKindInput, 
        (VariableAccess) {VarAccessLocation, 0},
        (uint8_t *) data1, sizeof(data1));
    spirv_sim_variable_associate_data(
        &spirv_sim, 
        VarKindInput, 
        (VariableAccess) {VarAccessLocation, 1},
        (uint8_t *) data2, sizeof(data2));
    spirv_sim_variable_associate_data(
        &spirv_sim, 
        VarKindInput, 
        (VariableAccess) {VarAccessLocation, 2},
        (uint8_t *) &data3, sizeof(data3));
    
    /* run simulator */
    while (!spirv_sim.finished && !spirv_sim.error_msg) {
        munit_assert_null(spirv_sim.error_msg);
        spirv_sim_step(&spirv_sim);
    }
    
    /* check registers */
    ASSERT_REGISTER_FLOAT(&spirv_sim, ID(63), ==, data1[2]);            /* OpVectorExtractDynamic */
    ASSERT_REGISTER_VEC4(&spirv_sim, ID(64), ==,                        /* OpVectorInsertDynamic */
                         data1[0], data3, data1[2], data1[3]);
    ASSERT_REGISTER_VEC4(&spirv_sim, ID(65), ==,                        /* OpVectorShuffle */
                         data2[0], data1[1], data2[2], data1[3]);
    
    /* OpCompositeConstruct - struct */
    SimRegister *c1_reg = &spirv_sim.temp_regs[map_int_int_get(&spirv_sim.assigned_regs, ID(66))];
    CompositeStruct c1_expected = (CompositeStruct) {
        .f0 = data3,
        .f1 = data1[2],
        .v0 = {data1[0], data1[1], data1[2], data1[3]},
        .v1 = {data2[0], data2[1], data2[2], data2[3]},
    };
    munit_assert_memory_equal(sizeof(CompositeStruct), c1_reg->raw, &c1_expected);
    
    /* OpCompositeConstruct - array */
    SimRegister *c2_reg = &spirv_sim.temp_regs[map_int_int_get(&spirv_sim.assigned_regs, ID(67))];
    float c2_expected[2][4] = {
        {data1[0], data1[1], data1[2], data1[3]},
        {data2[0], data2[1], data2[2], data2[3]}
    };
    munit_assert_memory_equal(sizeof(float) * 8, c2_reg->raw, c2_expected);
    
    /* OpCompositeConstruct - vector */
    SimRegister *c3_reg = &spirv_sim.temp_regs[map_int_int_get(&spirv_sim.assigned_regs, ID(68))];
    float c3_expected[6] = {data3, data1[0], data1[1], data1[2], data1[3], data3};
    munit_assert_memory_equal(sizeof(float) * 6, c3_reg->raw, c3_expected);
    
    /* OpCompositeConstruct - matrix */
    SimRegister *c4_reg = &spirv_sim.temp_regs[map_int_int_get(&spirv_sim.assigned_regs, ID(69))];
    float c4_expected[4][4] = {
        {data1[0], data1[1], data1[2], data1[3]},
        {data2[0], data2[1], data2[2], data2[3]},
        {data1[0], data3, data1[2], data1[3]},
        {data2[0], data1[1], data2[2], data1[3]}
    };
    munit_assert_memory_equal(sizeof(float) * 16, c4_reg->raw, c4_expected);
    
    /* OpCompositeExtract */
    ASSERT_REGISTER_FLOAT(&spirv_sim, ID(70), ==, c1_expected.v0[3]);
    
    /* OpCompositeInsert */
    SimRegister *i1_reg = &spirv_sim.temp_regs[map_int_int_get(&spirv_sim.assigned_regs, ID(71))];
    CompositeStruct i1_expected = (CompositeStruct) {
        .f0 = data3,
        .f1 = data1[2],
        .v0 = {data1[0], data1[1], data1[2], data1[3]},
        .v1 = {data2[0], data2[1], data3, data2[3]},
    };
    munit_assert_memory_equal(sizeof(CompositeStruct), i1_reg->raw, &i1_expected);
    
    /* OpCopyObject */
    SimRegister *o1_reg = &spirv_sim.temp_regs[map_int_int_get(&spirv_sim.assigned_regs, ID(72))];
    munit_assert_memory_equal(sizeof(CompositeStruct), o1_reg->raw, i1_reg->raw);
    
    /* OpTranspose */
    SimRegister *t1_reg = &spirv_sim.temp_regs[map_int_int_get(&spirv_sim.assigned_regs, ID(73))];
    float t1_expected[4][4] = {
        {data1[0], data2[0], data1[0], data2[0]},
        {data1[1], data2[1], data3, data1[1]},
        {data1[2], data2[2], data1[2], data2[2]},
        {data1[3], data2[3], data1[3], data1[3]}
    };
    munit_assert_memory_equal(sizeof(float) * 16, t1_reg->raw, t1_expected);
    
    return MUNIT_OK;
}

MunitResult test_conversion(const MunitParameter params[], void* user_data_or_fixture) {
 
    /* prepare binary */
    SPIRV_binary spirv_bin;
    spirv_bin_init(&spirv_bin, 1, 0);
    
    spirv_common_header(&spirv_bin);
    SPIRV_OP(&spirv_bin, SpvOpDecorate, ID(40), SpvDecorationLocation, 0);
    SPIRV_OP(&spirv_bin, SpvOpDecorate, ID(41), SpvDecorationLocation, 1);
    SPIRV_OP(&spirv_bin, SpvOpDecorate, ID(42), SpvDecorationLocation, 2);
    spirv_common_types(&spirv_bin, TEST_TYPE_FLOAT32 | TEST_TYPE_INT32 | TEST_TYPE_UINT32);
    SPIRV_OP(&spirv_bin, SpvOpVariable, ID(14), ID(40), SpvStorageClassInput);  /* float */
    SPIRV_OP(&spirv_bin, SpvOpVariable, ID(24), ID(41), SpvStorageClassInput);  /* signed int */
    SPIRV_OP(&spirv_bin, SpvOpVariable, ID(34), ID(42), SpvStorageClassInput);  /* unsigned int */
    spirv_common_function_header(&spirv_bin);
    SPIRV_OP(&spirv_bin, SpvOpLoad, ID(11), ID(60), ID(40));
    SPIRV_OP(&spirv_bin, SpvOpLoad, ID(21), ID(61), ID(41));
    SPIRV_OP(&spirv_bin, SpvOpLoad, ID(31), ID(62), ID(42));
    SPIRV_OP(&spirv_bin, SpvOpConvertFToU, ID(31), ID(63), ID(60));
    SPIRV_OP(&spirv_bin, SpvOpConvertFToS, ID(21), ID(64), ID(60));
    SPIRV_OP(&spirv_bin, SpvOpConvertSToF, ID(11), ID(65), ID(61));
    SPIRV_OP(&spirv_bin, SpvOpConvertUToF, ID(11), ID(66), ID(62));
    /* OpUConvert / OpSConvert / OpFConvert are hard to test at the moment because we only support 32-bit types. */
    SPIRV_OP(&spirv_bin, SpvOpSatConvertSToU, ID(31), ID(67), ID(61));
    SPIRV_OP(&spirv_bin, SpvOpSatConvertUToS, ID(21), ID(68), ID(62));
    SPIRV_OP(&spirv_bin, SpvOpConvertPtrToU, ID(30), ID(69), ID(40));
    SPIRV_OP(&spirv_bin, SpvOpConvertUToPtr, ID(14), ID(70), ID(69));
    SPIRV_OP(&spirv_bin, SpvOpLoad, ID(11), ID(71), ID(70));
    spirv_common_function_footer(&spirv_bin);
    spirv_bin.header.bound_ids = 74;
    spirv_bin_finalize(&spirv_bin);
    
    /* prepare simulator */
    SPIRV_module spirv_module;
    spirv_module_load(&spirv_module, &spirv_bin);
    
    SPIRV_simulator spirv_sim;
    spirv_sim_init(&spirv_sim, &spirv_module);
    float data_f[4] = {1.3f, -2.0f, 3.7f, 4.0f};
    int32_t data_s[4] = {1, 2, -3, 4};
    uint32_t data_u[4] = {1, 2, 3, UINT32_MAX};
    spirv_sim_variable_associate_data(
        &spirv_sim, 
        VarKindInput, 
        (VariableAccess) {VarAccessLocation, 0},
        (uint8_t *) data_f, sizeof(data_f));
    spirv_sim_variable_associate_data(
        &spirv_sim,
        VarKindInput, 
        (VariableAccess) {VarAccessLocation, 1},
        (uint8_t *) data_s, sizeof(data_s));
    spirv_sim_variable_associate_data(
        &spirv_sim, 
        VarKindInput, 
        (VariableAccess) {VarAccessLocation, 2},
        (uint8_t *) data_u, sizeof(data_u));
    
    /* run simulator */
    while (!spirv_sim.finished && !spirv_sim.error_msg) {
        munit_assert_null(spirv_sim.error_msg);
        spirv_sim_step(&spirv_sim);
    }
    
    /* check registers */
    ASSERT_REGISTER_UVEC4(&spirv_sim, ID(63), ==, 1, 0, 3, 4);                      /* OpConvertFToU */
    ASSERT_REGISTER_SVEC4(&spirv_sim, ID(64), ==, 1, -2, 3, 4);
    ASSERT_REGISTER_VEC4(&spirv_sim, ID(65), ==, 1.0f, 2.0f, -3.0f, 4.0f);
    ASSERT_REGISTER_VEC4(&spirv_sim, ID(66), ==, 1.0f, 2.0f, 3.0f, (float) UINT32_MAX);
    ASSERT_REGISTER_UVEC4(&spirv_sim, ID(67), ==, 1, 2, 0, 4);
    ASSERT_REGISTER_SVEC4(&spirv_sim, ID(68), ==, 1, 2, 3, INT32_MAX);
    ASSERT_REGISTER_VEC4(&spirv_sim, ID(71), ==, data_f[0], data_f[1], data_f[2], data_f[3]);

    return MUNIT_OK;
}

MunitResult test_aggregate(const MunitParameter params[], void* user_data_or_fixture) {

#pragma pack(push, 0)
    typedef struct AggregateIn {
        float f0;
        float f1;
        float v0[4];
    } AggregateIn;

    typedef struct AggregateOut {
        float v0[4];
        float f1;
        float f0;
    } AggregateOut;
#pragma pack(pop)
 
    /* prepare binary */
    SPIRV_binary spirv_bin;
    spirv_bin_init(&spirv_bin, 1, 0);

    spirv_common_header(&spirv_bin);
    SPIRV_OP(&spirv_bin, SpvOpName, ID(40), S('d', 'a', 't', 'a'), S('_', 'i', 'n', 0));
    SPIRV_OP(&spirv_bin, SpvOpMemberName, ID(40), 0, S('f', '0', 0, 0));
    SPIRV_OP(&spirv_bin, SpvOpMemberName, ID(40), 1, S('f', '1', 0, 0));
    SPIRV_OP(&spirv_bin, SpvOpMemberName, ID(40), 2, S('v', '0', 0, 0));
    SPIRV_OP(&spirv_bin, SpvOpName, ID(45), S('d', 'a', 't', 'a'), S('_', 'o', 'u', 't'), S(0,0,0,0));
    SPIRV_OP(&spirv_bin, SpvOpMemberName, ID(45), 0, S('v', '0', 0, 0));
    SPIRV_OP(&spirv_bin, SpvOpMemberName, ID(45), 1, S('f', '1', 0, 0));
    SPIRV_OP(&spirv_bin, SpvOpMemberName, ID(45), 2, S('f', '0', 0, 0));
    SPIRV_OP(&spirv_bin, SpvOpMemberDecorate, ID(40), 0, SpvDecorationLocation, 0);
    SPIRV_OP(&spirv_bin, SpvOpMemberDecorate, ID(40), 1, SpvDecorationLocation, 1);
    SPIRV_OP(&spirv_bin, SpvOpMemberDecorate, ID(40), 2, SpvDecorationLocation, 2);
    SPIRV_OP(&spirv_bin, SpvOpMemberDecorate, ID(45), 0, SpvDecorationLocation, 0);
    SPIRV_OP(&spirv_bin, SpvOpMemberDecorate, ID(45), 1, SpvDecorationLocation, 1);
    SPIRV_OP(&spirv_bin, SpvOpMemberDecorate, ID(45), 2, SpvDecorationLocation, 2);
    spirv_common_types(&spirv_bin, TEST_TYPE_FLOAT32 | TEST_TYPE_UINT32);
    SPIRV_OP(&spirv_bin, SpvOpTypeStruct, ID(40), ID(10), ID(10), ID(11));
    SPIRV_OP(&spirv_bin, SpvOpTypePointer, ID(41), SpvStorageClassInput, ID(40));
    SPIRV_OP(&spirv_bin, SpvOpTypeStruct, ID(45), ID(11), ID(10), ID(10));
    SPIRV_OP(&spirv_bin, SpvOpTypePointer, ID(46), SpvStorageClassOutput, ID(45));
    SPIRV_OP(&spirv_bin, SpvOpVariable, ID(41), ID(50), SpvStorageClassInput);
    SPIRV_OP(&spirv_bin, SpvOpVariable, ID(46), ID(51), SpvStorageClassOutput);
    SPIRV_OP(&spirv_bin, SpvOpConstant, ID(30), ID(60), 0);
    SPIRV_OP(&spirv_bin, SpvOpConstant, ID(30), ID(61), 1);
    SPIRV_OP(&spirv_bin, SpvOpConstant, ID(30), ID(62), 2);
    spirv_common_function_header(&spirv_bin);
    SPIRV_OP(&spirv_bin, SpvOpAccessChain, ID(13), ID(70), ID(50), ID(60));
    SPIRV_OP(&spirv_bin, SpvOpAccessChain, ID(13), ID(71), ID(50), ID(61));
    SPIRV_OP(&spirv_bin, SpvOpAccessChain, ID(14), ID(72), ID(50), ID(62));
    SPIRV_OP(&spirv_bin, SpvOpLoad, ID(10), ID(75), ID(70));
    SPIRV_OP(&spirv_bin, SpvOpLoad, ID(10), ID(76), ID(71));
    SPIRV_OP(&spirv_bin, SpvOpLoad, ID(11), ID(77), ID(72));
    SPIRV_OP(&spirv_bin, SpvOpAccessChain, ID(14), ID(80), ID(51), ID(60));
    SPIRV_OP(&spirv_bin, SpvOpAccessChain, ID(13), ID(81), ID(51), ID(61));
    SPIRV_OP(&spirv_bin, SpvOpAccessChain, ID(13), ID(82), ID(51), ID(62));
    SPIRV_OP(&spirv_bin, SpvOpStore, ID(80), ID(77));
    SPIRV_OP(&spirv_bin, SpvOpStore, ID(81), ID(76));
    SPIRV_OP(&spirv_bin, SpvOpStore, ID(82), ID(75));
    spirv_common_function_footer(&spirv_bin);
    spirv_bin.header.bound_ids = 71;
    spirv_bin_finalize(&spirv_bin);
    
    /* prepare simulator */
    SPIRV_module spirv_module;
    spirv_module_load(&spirv_module, &spirv_bin);
    
    SPIRV_simulator spirv_sim;
    spirv_sim_init(&spirv_sim, &spirv_module);

    AggregateIn data_in = {1.0f, 2.0f, {3.5f, 6.6f, 8.0f, 11.0f}};
    spirv_sim_variable_associate_data(
        &spirv_sim, VarKindInput, (VariableAccess) {VarAccessLocation, 0},
        (uint8_t *) &data_in.f0, sizeof(data_in.f0)
    );
    spirv_sim_variable_associate_data(
        &spirv_sim, VarKindInput, (VariableAccess) {VarAccessLocation, 1},
        (uint8_t *) &data_in.f1, sizeof(data_in.f1)
    );
    spirv_sim_variable_associate_data(
        &spirv_sim, VarKindInput, (VariableAccess) {VarAccessLocation, 2},
        (uint8_t *) &data_in.v0, sizeof(data_in.v0)
    );

    SimPointer *ptr = spirv_sim_retrieve_intf_pointer(
        &spirv_sim, VarKindOutput,
        (VariableAccess) {VarAccessLocation, 0}
    );
    AggregateOut *data_out = ((AggregateOut *) (spirv_sim.memory + ptr->pointer));

    /* run simulator */
    while (!spirv_sim.finished && !spirv_sim.error_msg) {
        munit_assert_null(spirv_sim.error_msg);
        spirv_sim_step(&spirv_sim);
    }

    /* check registers */
    ASSERT_REGISTER_FLOAT(&spirv_sim, ID(75), ==, data_in.f0);
    ASSERT_REGISTER_FLOAT(&spirv_sim, ID(76), ==, data_in.f1);
    ASSERT_REGISTER_VEC4(&spirv_sim, ID(77), ==, data_in.v0[0], data_in.v0[1], data_in.v0[2], data_in.v0[3]);

    munit_assert_memory_equal(sizeof(data_in.v0), data_out->v0, data_in.v0);
    munit_assert_float(data_out->f0, ==, data_in.f0);
    munit_assert_float(data_out->f1, ==, data_in.f1);

    return MUNIT_OK;
}
    

MunitTest spirv_sim_tests[] = {
    { "/arithmetic_float32", test_arithmetic_float32, NULL, NULL,  MUNIT_TEST_OPTION_NONE, NULL },
    { "/arithmetic_int32", test_arithmetic_int32, NULL, NULL,  MUNIT_TEST_OPTION_NONE, NULL },
    { "/arithmetic_uint32", test_arithmetic_uint32, NULL, NULL,  MUNIT_TEST_OPTION_NONE, NULL },
    { "/composite_float32", test_composite_float32, NULL, NULL,  MUNIT_TEST_OPTION_NONE, NULL },
    { "/conversion", test_conversion, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    { "/aggregate", test_aggregate, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};
