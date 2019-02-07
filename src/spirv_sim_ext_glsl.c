#include "spirv_simulator.h"
#include "spirv_binary.h"
#include "spirv/spirv.h"
#include "spirv/GLSL.std.450.h"
#include "dyn_array.h"

#include <assert.h>
#include <math.h>

#define SPIRV_SIM_EXT_INTERNAL
#include "spirv_sim_ext.h"


EXTINST_RES_1OP(GLSLstd450Normalize) {
/* Result is the vector in the same direction as x but with a length of 1. */
    assert(res_reg->type == op_reg->type);
    // assert(spirv_sim_type_is_float(op_reg->type));

    float len = 0.0f;
    for (uint32_t i = 0; i < op_reg->type->count; ++i) {
        len += op_reg->vec[i] * op_reg->vec[i];
    }
    len = sqrtf(len);

    for (uint32_t i = 0; i < op_reg->type->count; ++i) {
        res_reg->vec[i] = op_reg->vec[i] / len;
    }

} EXTINST_END


void spirv_sim_extension_GLSL_std_450(SPIRV_simulator *sim, SPIRV_opcode *op) {
    assert(sim);
    assert(op);
    assert(op->op.kind == SpvOpExtInst);

#define OP(kind)                             \
    case kind:                               \
        spirv_sim_extinst_##kind(sim, op);   \
        break;
#define OP_DEFAULT(kind)

    switch (EXTINST_OPCODE(op)) {

        OP_DEFAULT(GLSLstd450Round)
        OP_DEFAULT(GLSLstd450RoundEven)
        OP_DEFAULT(GLSLstd450Trunc)
        OP_DEFAULT(GLSLstd450FAbs)
        OP_DEFAULT(GLSLstd450SAbs)
        OP_DEFAULT(GLSLstd450FSign)
        OP_DEFAULT(GLSLstd450SSign)
        OP_DEFAULT(GLSLstd450Floor)
        OP_DEFAULT(GLSLstd450Ceil)
        OP_DEFAULT(GLSLstd450Fract)

        OP_DEFAULT(GLSLstd450Radians)
        OP_DEFAULT(GLSLstd450Degrees)
        OP_DEFAULT(GLSLstd450Sin)
        OP_DEFAULT(GLSLstd450Cos)
        OP_DEFAULT(GLSLstd450Tan)
        OP_DEFAULT(GLSLstd450Asin)
        OP_DEFAULT(GLSLstd450Acos)
        OP_DEFAULT(GLSLstd450Atan)
        OP_DEFAULT(GLSLstd450Sinh)
        OP_DEFAULT(GLSLstd450Cosh)
        OP_DEFAULT(GLSLstd450Tanh)
        OP_DEFAULT(GLSLstd450Asinh)
        OP_DEFAULT(GLSLstd450Acosh)
        OP_DEFAULT(GLSLstd450Atanh)
        OP_DEFAULT(GLSLstd450Atan2)

        OP_DEFAULT(GLSLstd450Pow)
        OP_DEFAULT(GLSLstd450Exp)
        OP_DEFAULT(GLSLstd450Log)
        OP_DEFAULT(GLSLstd450Exp2)
        OP_DEFAULT(GLSLstd450Log2)
        OP_DEFAULT(GLSLstd450Sqrt)
        OP_DEFAULT(GLSLstd450InverseSqrt)

        OP_DEFAULT(GLSLstd450Determinant)
        OP_DEFAULT(GLSLstd450MatrixInverse)

        OP_DEFAULT(GLSLstd450Modf)
        OP_DEFAULT(GLSLstd450ModfStruct)
        OP_DEFAULT(GLSLstd450FMin)
        OP_DEFAULT(GLSLstd450UMin)
        OP_DEFAULT(GLSLstd450SMin)
        OP_DEFAULT(GLSLstd450FMax)
        OP_DEFAULT(GLSLstd450UMax)
        OP_DEFAULT(GLSLstd450SMax)
        OP_DEFAULT(GLSLstd450FClamp)
        OP_DEFAULT(GLSLstd450UClamp)
        OP_DEFAULT(GLSLstd450SClamp)
        OP_DEFAULT(GLSLstd450FMix)
        OP_DEFAULT(GLSLstd450IMix)
        OP_DEFAULT(GLSLstd450Step)
        OP_DEFAULT(GLSLstd450SmoothStep)

        OP_DEFAULT(GLSLstd450Fma)
        OP_DEFAULT(GLSLstd450Frexp)
        OP_DEFAULT(GLSLstd450FrexpStruct)
        OP_DEFAULT(GLSLstd450Ldexp)

        OP_DEFAULT(GLSLstd450PackSnorm4x8)
        OP_DEFAULT(GLSLstd450PackUnorm4x8)
        OP_DEFAULT(GLSLstd450PackSnorm2x16)
        OP_DEFAULT(GLSLstd450PackUnorm2x16)
        OP_DEFAULT(GLSLstd450PackHalf2x16)
        OP_DEFAULT(GLSLstd450PackDouble2x32)
        OP_DEFAULT(GLSLstd450UnpackSnorm2x16)
        OP_DEFAULT(GLSLstd450UnpackUnorm2x16)
        OP_DEFAULT(GLSLstd450UnpackHalf2x16)
        OP_DEFAULT(GLSLstd450UnpackSnorm4x8)
        OP_DEFAULT(GLSLstd450UnpackUnorm4x8)
        OP_DEFAULT(GLSLstd450UnpackDouble2x32)

        OP_DEFAULT(GLSLstd450Length)
        OP_DEFAULT(GLSLstd450Distance)
        OP_DEFAULT(GLSLstd450Cross)
        OP(GLSLstd450Normalize)
        OP_DEFAULT(GLSLstd450FaceForward)
        OP_DEFAULT(GLSLstd450Reflect)
        OP_DEFAULT(GLSLstd450Refract)

        OP_DEFAULT(GLSLstd450FindILsb)
        OP_DEFAULT(GLSLstd450FindSMsb)
        OP_DEFAULT(GLSLstd450FindUMsb)

        OP_DEFAULT(GLSLstd450InterpolateAtCentroid)
        OP_DEFAULT(GLSLstd450InterpolateAtSample)
        OP_DEFAULT(GLSLstd450InterpolateAtOffset)

        OP_DEFAULT(GLSLstd450NMin)
        OP_DEFAULT(GLSLstd450NMax)
        OP_DEFAULT(GLSLstd450NClamp)

        default:
            arr_printf(sim->error_msg, "Unsupported GLSL.std.450 extension [%d]", EXTINST_OPCODE(op));
    }
}
