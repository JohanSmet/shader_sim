#include "spirv_simulator.h"
#include "spirv_binary.h"
#include "spirv/spirv.h"
#include "spirv/GLSL.std.450.h"
#include "dyn_array.h"

#include <assert.h>
#include <math.h>

#define SPIRV_SIM_EXT_INTERNAL
#include "spirv_sim_ext.h"

static inline float vec_length(float *vec, size_t n) {
    float len2 = 0.0f;

    for (uint32_t i = 0; i < n; ++i) {
        len2 += vec[i] * vec[i];
    }

    return sqrtf(len2);
}

/*
 * basic math functions
 */

EXTINST_RES_1OP(GLSLstd450Round) {
/* Result is the value equal to the nearest whole number to x.
   The fraction 0.5 will round in a direction chosen by the implementation, 
   presumably the direction that is fastest */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < op_reg->type->count; ++i) {
        res_reg->vec[i] = roundf(op_reg->vec[i]);
    }

} EXTINST_END

EXTINST_RES_1OP(GLSLstd450RoundEven) {
/* Result is the value equal to the nearest whole number to x. 
   A fractional part of 0.5 will round toward the nearest even whole number. */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < op_reg->type->count; ++i) {
        int integer = (int) op_reg->vec[i];
        float whole_part = (float) integer;
        float fract_part = op_reg->vec[i] - whole_part;

        if (fract_part < 0.5f || fract_part > 0.5f) {
            res_reg->vec[i] = roundf(op_reg->vec[i]);
        } else if ((integer % 2) == 0) {
            res_reg->vec[i] = whole_part;
        } else if (op_reg->vec[i] < 0.0f) {
            res_reg->vec[i] = whole_part - 1.0f;
        } else {
            res_reg->vec[i] = whole_part + 1.0f;
        }
    }

} EXTINST_END

EXTINST_RES_1OP(GLSLstd450Trunc) {
/* Result is the value equal to the nearest whole number to x whose absolute value 
   is not larger than the absolute value of x. */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < op_reg->type->count; ++i) {
        res_reg->vec[i] = truncf(op_reg->vec[i]);
    }

} EXTINST_END

EXTINST_RES_1OP(GLSLstd450FAbs) {
/* Result is x if x ≥ 0; otherwise result is -x. */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < op_reg->type->count; ++i) {
        res_reg->vec[i] = fabsf(op_reg->vec[i]);
    }
    
} EXTINST_END

EXTINST_RES_1OP(GLSLstd450SAbs) {
/* Result is x if x ≥ 0; otherwise result is -x, where x is interpreted as a signed integer. */
    assert(spirv_type_is_signed_integer(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < op_reg->type->count; ++i) {
        res_reg->svec[i] = abs(op_reg->svec[i]);
    }
    
} EXTINST_END

EXTINST_RES_1OP(GLSLstd450FSign) {
/* Result is 1.0 if x > 0, 0.0 if x = 0, or -1.0 if x < 0. */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < op_reg->type->count; ++i) {
        res_reg->vec[i] = (op_reg->vec[i] > 0.0f) ? 1.0f : 
                                (op_reg->vec[i] < 0.0f) ? -1.0f : 0.0f;
    }

} EXTINST_END

EXTINST_RES_1OP(GLSLstd450SSign) {
/* Result is 1 if x > 0, 0 if x = 0, or -1 if x < 0, where x is interpreted as a signed integer. */
    assert(spirv_type_is_signed_integer(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < op_reg->type->count; ++i) {
        res_reg->svec[i] = (op_reg->svec[i] > 0) ? 1 : 
                                (op_reg->svec[i] < 0) ? -1 : 0;
    }
    
} EXTINST_END

EXTINST_RES_1OP(GLSLstd450Floor) {
/* Result is the value equal to the nearest whole number that is less than or equal to x. */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < op_reg->type->count; ++i) {
        res_reg->vec[i] = floorf(op_reg->vec[i]);
    }
    
} EXTINST_END

EXTINST_RES_1OP(GLSLstd450Ceil) {
/* Result is the value equal to the nearest whole number that is greater than or equal to x. */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < op_reg->type->count; ++i) {
        res_reg->vec[i] = ceilf(op_reg->vec[i]);
    }
    
} EXTINST_END

EXTINST_RES_1OP(GLSLstd450Fract) {
/* Result is x - floor x. */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < op_reg->type->count; ++i) {
        res_reg->vec[i] = op_reg->vec[i] - floorf(op_reg->vec[i]);
    }

} EXTINST_END




EXTINST_RES_1OP(GLSLstd450Length) {
/* Result is the length of vector */ 
    assert(spirv_type_is_float(op_reg->type));
    assert(op_reg->type->base_type == res_reg->type);

    res_reg->vec[0] = vec_length(op_reg->vec, op_reg->type->count);

} EXTINST_END

EXTINST_RES_2OP(GLSLstd450Distance) {
/* Result is the distance between p0 and p1, i.e., length(p0 - p1). */
    assert(spirv_type_is_float(op1_reg->type));
    assert(op1_reg->type == op2_reg->type);
    assert(op1_reg->type->base_type == res_reg->type);

    float dist2 = 0.0f;

    for (uint32_t i = 0; i < op1_reg->type->count; ++i) {
        float diff = op1_reg->vec[i] - op2_reg->vec[i];
        dist2 += diff * diff;
    }

    res_reg->vec[0] = sqrtf(dist2);

} EXTINST_END

EXTINST_RES_1OP(GLSLstd450Normalize) {
/* Result is the vector in the same direction as x but with a length of 1. */
    assert(res_reg->type == op_reg->type);
    assert(spirv_type_is_float(op_reg->type));

    float len = vec_length(op_reg->vec, op_reg->type->count);

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

        /* basic math functions */
        OP(GLSLstd450Round)
        OP(GLSLstd450RoundEven)
        OP(GLSLstd450Trunc)
        OP(GLSLstd450FAbs)
        OP(GLSLstd450SAbs)
        OP(GLSLstd450FSign)
        OP(GLSLstd450SSign)
        OP(GLSLstd450Floor)
        OP(GLSLstd450Ceil)
        OP(GLSLstd450Fract)
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

        OP(GLSLstd450Length)
        OP(GLSLstd450Distance)
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
