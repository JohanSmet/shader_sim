// lut.c - Johan Smet - BSD-3-Clause (see LICENSE)

#include "lut.h"
#include "hash_map.h"

#include "spirv/spirv.h"

#include <assert.h>

static HashMap map_builtins;    // int32_t -> string

static void lut_init_builtins(void) {
    map_int_str_put(&map_builtins, SpvBuiltInPosition, "Position");
    map_int_str_put(&map_builtins, SpvBuiltInPointSize, "PointSize");
    map_int_str_put(&map_builtins, SpvBuiltInClipDistance, "ClipDistance");
    map_int_str_put(&map_builtins, SpvBuiltInCullDistance, "CullDistance");
    map_int_str_put(&map_builtins, SpvBuiltInVertexId, "VertexId");
    map_int_str_put(&map_builtins, SpvBuiltInInstanceId, "InstanceId");
    map_int_str_put(&map_builtins, SpvBuiltInPrimitiveId, "PrimitiveId");
    map_int_str_put(&map_builtins, SpvBuiltInInvocationId, "InvocationId");
    map_int_str_put(&map_builtins, SpvBuiltInLayer, "Layer");
    map_int_str_put(&map_builtins, SpvBuiltInViewportIndex, "ViewportIndex");
    map_int_str_put(&map_builtins, SpvBuiltInTessLevelOuter, "TessLevelOuter");
    map_int_str_put(&map_builtins, SpvBuiltInTessLevelInner, "TessLevelInner");
    map_int_str_put(&map_builtins, SpvBuiltInTessCoord, "TessCoord");
    map_int_str_put(&map_builtins, SpvBuiltInPatchVertices, "PatchVertices");
    map_int_str_put(&map_builtins, SpvBuiltInFragCoord, "FragCoord");
    map_int_str_put(&map_builtins, SpvBuiltInPointCoord, "PointCoord");
    map_int_str_put(&map_builtins, SpvBuiltInFrontFacing, "FrontFacing");
    map_int_str_put(&map_builtins, SpvBuiltInSampleId, "SampleId");
    map_int_str_put(&map_builtins, SpvBuiltInSamplePosition, "SamplePosition");
    map_int_str_put(&map_builtins, SpvBuiltInSampleMask, "SampleMask");
    map_int_str_put(&map_builtins, SpvBuiltInFragDepth, "FragDepth");
    map_int_str_put(&map_builtins, SpvBuiltInHelperInvocation, "HelperInvocation");
    map_int_str_put(&map_builtins, SpvBuiltInNumWorkgroups, "NumWorkgroups");
    map_int_str_put(&map_builtins, SpvBuiltInWorkgroupSize, "WorkgroupSize");
    map_int_str_put(&map_builtins, SpvBuiltInWorkgroupId, "WorkgroupId");
    map_int_str_put(&map_builtins, SpvBuiltInLocalInvocationId, "LocalInvocationId");
    map_int_str_put(&map_builtins, SpvBuiltInGlobalInvocationId, "GlobalInvocationId");
    map_int_str_put(&map_builtins, SpvBuiltInLocalInvocationIndex, "LocalInvocationIndex");
    map_int_str_put(&map_builtins, SpvBuiltInWorkDim, "WorkDim");
    map_int_str_put(&map_builtins, SpvBuiltInGlobalSize, "GlobalSize");
    map_int_str_put(&map_builtins, SpvBuiltInEnqueuedWorkgroupSize, "EnqueuedWorkgroupSize");
    map_int_str_put(&map_builtins, SpvBuiltInGlobalOffset, "GlobalOffset");
    map_int_str_put(&map_builtins, SpvBuiltInGlobalLinearId, "GlobalLinearId");
    map_int_str_put(&map_builtins, SpvBuiltInSubgroupSize, "SubgroupSize");
    map_int_str_put(&map_builtins, SpvBuiltInSubgroupMaxSize, "SubgroupMaxSize");
    map_int_str_put(&map_builtins, SpvBuiltInNumSubgroups, "NumSubgroups");
    map_int_str_put(&map_builtins, SpvBuiltInNumEnqueuedSubgroups, "NumEnqueuedSubgroups");
    map_int_str_put(&map_builtins, SpvBuiltInSubgroupId, "SubgroupId");
    map_int_str_put(&map_builtins, SpvBuiltInSubgroupLocalInvocationId, "SubgroupLocalInvocationId");
    map_int_str_put(&map_builtins, SpvBuiltInVertexIndex, "VertexIndex");
    map_int_str_put(&map_builtins, SpvBuiltInInstanceIndex, "InstanceIndex");
    map_int_str_put(&map_builtins, SpvBuiltInSubgroupEqMaskKHR, "SubgroupEqMaskKHR");
    map_int_str_put(&map_builtins, SpvBuiltInSubgroupGeMaskKHR, "SubgroupGeMaskKHR");
    map_int_str_put(&map_builtins, SpvBuiltInSubgroupGtMaskKHR, "SubgroupGtMaskKHR");
    map_int_str_put(&map_builtins, SpvBuiltInSubgroupLeMaskKHR, "SubgroupLeMaskKHR");
    map_int_str_put(&map_builtins, SpvBuiltInSubgroupLtMaskKHR, "SubgroupLtMaskKHR");
    map_int_str_put(&map_builtins, SpvBuiltInBaseVertex, "BaseVertex");
    map_int_str_put(&map_builtins, SpvBuiltInBaseInstance, "BaseInstance");
    map_int_str_put(&map_builtins, SpvBuiltInDrawIndex, "DrawIndex");
    map_int_str_put(&map_builtins, SpvBuiltInDeviceIndex, "DeviceIndex");
    map_int_str_put(&map_builtins, SpvBuiltInViewIndex, "ViewIndex");
    map_int_str_put(&map_builtins, SpvBuiltInBaryCoordNoPerspAMD, "BaryCoordNoPerspAMD");
    map_int_str_put(&map_builtins, SpvBuiltInBaryCoordNoPerspCentroidAMD, "BaryCoordNoPerspCentroidAMD");
    map_int_str_put(&map_builtins, SpvBuiltInBaryCoordNoPerspSampleAMD, "BaryCoordNoPerspSampleAMD");
    map_int_str_put(&map_builtins, SpvBuiltInBaryCoordSmoothAMD, "BaryCoordSmoothAMD");
    map_int_str_put(&map_builtins, SpvBuiltInBaryCoordSmoothCentroidAMD, "BaryCoordSmoothCentroidAMD");
    map_int_str_put(&map_builtins, SpvBuiltInBaryCoordSmoothSampleAMD, "BaryCoordSmoothSampleAMD");
    map_int_str_put(&map_builtins, SpvBuiltInBaryCoordPullModelAMD, "BaryCoordPullModelAMD");
    map_int_str_put(&map_builtins, SpvBuiltInFragStencilRefEXT, "FragStencilRefEXT");
    map_int_str_put(&map_builtins, SpvBuiltInViewportMaskNV, "ViewportMaskNV");
    map_int_str_put(&map_builtins, SpvBuiltInSecondaryPositionNV, "SecondaryPositionNV");
    map_int_str_put(&map_builtins, SpvBuiltInSecondaryViewportMaskNV, "SecondaryViewportMaskNV");
    map_int_str_put(&map_builtins, SpvBuiltInPositionPerViewNV, "PositionPerViewNV");
    map_int_str_put(&map_builtins, SpvBuiltInViewportMaskPerViewNV, "ViewportMaskPerViewNV");
}

void lut_init(void) {

    if (map_len(&map_builtins) > 0) {
        return;
    }

    lut_init_builtins();
}

const char *lut_lookup_builtin(int32_t builtin) {

    const char *result = map_int_str_get(&map_builtins, builtin);

	if (!result) {
		result = "InvalidBuiltIn";
	}

	return result;
}

const char *lut_lookup_type_kind(TypeKind kind) {
	switch (kind) {
		case TypeVoid:
			return "Void";
		case TypeBool:
			return "Bool";
		case TypeInteger:
			return "Integer";
		case TypeFloat:
			return "Float";
		case TypeVectorInteger:
			return "Vector";
		case TypeVectorFloat:
			return "Vector";
		case TypeMatrixInteger:
			return "Matrix";
		case TypeMatrixFloat:
			return "Matrix";
		case TypePointer:
			return "Pointer";
		case TypeFunction:
			return "Function";
		case TypeArray:
			return "Array";
		case TypeStructure:
			return "Structure";
	}
}

const char *lut_lookup_variable_kind(VariableKind kind) {
	switch (kind) {
		case VarKindUniformConstant:
			return "UniformConstant";
		case VarKindInput:
			return "Input";
		case VarKindUniform:
			return "Uniform";
		case VarKindOutput:
			return "Output";
		case VarKindWorkgroup:
			return "Workgroup";
		case VarKindCrossWorkgroup:
			return "CrossWorkgroup";
		case VarKindPrivate:
			return "Private";
		case VarKindFunction:
			return "Function";
		case VarKindGeneric:
			return "Generic";
		case VarKindPushConstant:
			return "PushConstant";
		case VarKindAtomicCounter:
			return "AtomicCounter";
		case VarKindImage:
			return "Image";
		case VarKindStorageBuffer:
			return "StorageBuffer";
	}
}

const char *lut_lookup_variable_access(VariableAccessKind kind) {
	switch (kind) {
		case VarAccessNone:
			return "None";
		case VarAccessBuiltIn:
			return "BuiltIn";
		case VarAccessLocation:
			return "Location";
	}
}
