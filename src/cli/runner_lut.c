// runner_lut.c - Johan Smet - BSD-3-Clause (see LICENSE)

#include "runner_lut.h"
#include "hash_map.h"

#include "spirv/spirv.h"

#include <assert.h>

static HashMap map_builtins;    // string -> int32_t
static HashMap map_datatypes;   // string -> int32_t
static HashMap map_cmp_ops;     // string -> int32_t

#define ZERO_OFFSET 10000

static void runner_lut_init_builtins(void) {
    map_str_int_put(&map_builtins, "Position", SpvBuiltInPosition + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "PointSize", SpvBuiltInPointSize + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "ClipDistance", SpvBuiltInClipDistance + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "CullDistance", SpvBuiltInCullDistance + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "VertexId", SpvBuiltInVertexId + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "InstanceId", SpvBuiltInInstanceId + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "PrimitiveId", SpvBuiltInPrimitiveId + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "InvocationId", SpvBuiltInInvocationId + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "Layer", SpvBuiltInLayer + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "ViewportIndex", SpvBuiltInViewportIndex + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "TessLevelOuter", SpvBuiltInTessLevelOuter + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "TessLevelInner", SpvBuiltInTessLevelInner + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "TessCoord", SpvBuiltInTessCoord + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "PatchVertices", SpvBuiltInPatchVertices + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "FragCoord", SpvBuiltInFragCoord + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "PointCoord", SpvBuiltInPointCoord + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "FrontFacing", SpvBuiltInFrontFacing + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "SampleId", SpvBuiltInSampleId + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "SamplePosition", SpvBuiltInSamplePosition + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "SampleMask", SpvBuiltInSampleMask + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "FragDepth", SpvBuiltInFragDepth + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "HelperInvocation", SpvBuiltInHelperInvocation + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "NumWorkgroups", SpvBuiltInNumWorkgroups + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "WorkgroupSize", SpvBuiltInWorkgroupSize + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "WorkgroupId", SpvBuiltInWorkgroupId + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "LocalInvocationId", SpvBuiltInLocalInvocationId + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "GlobalInvocationId", SpvBuiltInGlobalInvocationId + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "LocalInvocationIndex", SpvBuiltInLocalInvocationIndex + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "WorkDim", SpvBuiltInWorkDim + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "GlobalSize", SpvBuiltInGlobalSize + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "EnqueuedWorkgroupSize", SpvBuiltInEnqueuedWorkgroupSize + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "GlobalOffset", SpvBuiltInGlobalOffset + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "GlobalLinearId", SpvBuiltInGlobalLinearId + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "SubgroupSize", SpvBuiltInSubgroupSize + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "SubgroupMaxSize", SpvBuiltInSubgroupMaxSize + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "NumSubgroups", SpvBuiltInNumSubgroups + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "NumEnqueuedSubgroups", SpvBuiltInNumEnqueuedSubgroups + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "SubgroupId", SpvBuiltInSubgroupId + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "SubgroupLocalInvocationId", SpvBuiltInSubgroupLocalInvocationId + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "VertexIndex", SpvBuiltInVertexIndex + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "InstanceIndex", SpvBuiltInInstanceIndex + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "SubgroupEqMaskKHR", SpvBuiltInSubgroupEqMaskKHR + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "SubgroupGeMaskKHR", SpvBuiltInSubgroupGeMaskKHR + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "SubgroupGtMaskKHR", SpvBuiltInSubgroupGtMaskKHR + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "SubgroupLeMaskKHR", SpvBuiltInSubgroupLeMaskKHR + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "SubgroupLtMaskKHR", SpvBuiltInSubgroupLtMaskKHR + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "BaseVertex", SpvBuiltInBaseVertex + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "BaseInstance", SpvBuiltInBaseInstance + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "DrawIndex", SpvBuiltInDrawIndex + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "DeviceIndex", SpvBuiltInDeviceIndex + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "ViewIndex", SpvBuiltInViewIndex + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "BaryCoordNoPerspAMD", SpvBuiltInBaryCoordNoPerspAMD + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "BaryCoordNoPerspCentroidAMD", SpvBuiltInBaryCoordNoPerspCentroidAMD + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "BaryCoordNoPerspSampleAMD", SpvBuiltInBaryCoordNoPerspSampleAMD + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "BaryCoordSmoothAMD", SpvBuiltInBaryCoordSmoothAMD + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "BaryCoordSmoothCentroidAMD", SpvBuiltInBaryCoordSmoothCentroidAMD + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "BaryCoordSmoothSampleAMD", SpvBuiltInBaryCoordSmoothSampleAMD + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "BaryCoordPullModelAMD", SpvBuiltInBaryCoordPullModelAMD + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "FragStencilRefEXT", SpvBuiltInFragStencilRefEXT + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "ViewportMaskNV", SpvBuiltInViewportMaskNV + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "SecondaryPositionNV", SpvBuiltInSecondaryPositionNV + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "SecondaryViewportMaskNV", SpvBuiltInSecondaryViewportMaskNV + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "PositionPerViewNV", SpvBuiltInPositionPerViewNV + ZERO_OFFSET);
    map_str_int_put(&map_builtins, "ViewportMaskPerViewNV", SpvBuiltInViewportMaskPerViewNV + ZERO_OFFSET);
}

void runner_lut_init_datatypes(void) {
    map_str_int_put(&map_datatypes, "Void", TypeVoid + ZERO_OFFSET);
    map_str_int_put(&map_datatypes, "Bool", TypeBool + ZERO_OFFSET);
    map_str_int_put(&map_datatypes, "Integer", TypeInteger + ZERO_OFFSET);
    map_str_int_put(&map_datatypes, "Float", TypeFloat + ZERO_OFFSET);
    map_str_int_put(&map_datatypes, "VectorInteger", TypeVectorInteger + ZERO_OFFSET);
    map_str_int_put(&map_datatypes, "VectorFloat", TypeVectorFloat + ZERO_OFFSET);
    map_str_int_put(&map_datatypes, "MatrixInteger", TypeMatrixInteger + ZERO_OFFSET);
    map_str_int_put(&map_datatypes, "MatrixFloat", TypeMatrixFloat + ZERO_OFFSET);
    map_str_int_put(&map_datatypes, "Pointer", TypePointer + ZERO_OFFSET);
    map_str_int_put(&map_datatypes, "Function", TypeFunction + ZERO_OFFSET);
}

void runner_lut_init_cmp_op(void) {
    map_str_int_put(&map_cmp_ops, "==", CMP_EQ + ZERO_OFFSET);
    map_str_int_put(&map_cmp_ops, "!=", CMP_NEQ + ZERO_OFFSET);
    map_str_int_put(&map_cmp_ops, "<", CMP_LT + ZERO_OFFSET);
    map_str_int_put(&map_cmp_ops, ">", CMP_GT + ZERO_OFFSET);
    map_str_int_put(&map_cmp_ops, "<=", CMP_LTEQ + ZERO_OFFSET);
    map_str_int_put(&map_cmp_ops, ">=", CMP_GTEQ + ZERO_OFFSET);
}

void runner_lut_init(void) {

    if (map_len(&map_builtins) > 0) {
        return;
    }

    runner_lut_init_builtins();
    runner_lut_init_datatypes();
    runner_lut_init_cmp_op();
}

bool runner_lut_lookup_builtin(const char *key, int32_t *value) {
    assert(key);
    assert(value);

    int32_t result = map_str_int_get(&map_builtins, key);

    if (result >= ZERO_OFFSET) {
        *value = result - ZERO_OFFSET;
        return true;
    } else {
        return false;
    }
}

bool runner_lut_lookup_datatype(const char *key, TypeKind *value) {
    assert(key);
    assert(value);

    int32_t result = map_str_int_get(&map_datatypes, key);

    if (result >= ZERO_OFFSET) {
        *value = result - ZERO_OFFSET;
        return true;
    } else {
        return false;
    }
}

bool runner_lut_lookup_cmp_op(const char *key, RunnerCmpOp *value) {
    assert(key);
    assert(value);

    int32_t result = map_str_int_get(&map_cmp_ops, key);

    if (result >= ZERO_OFFSET) {
        *value = result - ZERO_OFFSET;
        return true;
    } else {
        return false;
    }
}