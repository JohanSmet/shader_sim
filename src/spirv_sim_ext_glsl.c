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

 /* 
  * trigonometric functions 
  */

EXTINST_RES_1OP(GLSLstd450Radians) {
/* Converts degrees to radians */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < op_reg->type->count; ++i) {
        res_reg->vec[i] = (op_reg->vec[i] * PI_F) / 180.0f;
    }

} EXTINST_END

EXTINST_RES_1OP(GLSLstd450Degrees) {
/* Converts radians to degrees */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < op_reg->type->count; ++i) {
        res_reg->vec[i] = (op_reg->vec[i] * 180.0f) /  PI_F;
    }
    
} EXTINST_END

EXTINST_RES_1OP(GLSLstd450Sin) {
/* The standard trigonometric sine of x radians. */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < op_reg->type->count; ++i) {
        res_reg->vec[i] = sinf(op_reg->vec[i]);
    }

} EXTINST_END

EXTINST_RES_1OP(GLSLstd450Cos) {
/* The standard trigonometric cosine of x radians. */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < op_reg->type->count; ++i) {
        res_reg->vec[i] = cosf(op_reg->vec[i]);
    }
    
} EXTINST_END

EXTINST_RES_1OP(GLSLstd450Tan) {
/* The standard trigonometric tangent of x radians. */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < op_reg->type->count; ++i) {
        res_reg->vec[i] = tanf(op_reg->vec[i]);
    }
    
} EXTINST_END

EXTINST_RES_1OP(GLSLstd450Asin) {
/* Arc sine. Result is an angle, in radians, whose sine is x. */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < op_reg->type->count; ++i) {
        res_reg->vec[i] = asinf(op_reg->vec[i]);
    }
    
} EXTINST_END

EXTINST_RES_1OP(GLSLstd450Acos) {
/* Arc cosine. Result is an angle, in radians, whose cosine is x.     */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < op_reg->type->count; ++i) {
        res_reg->vec[i] = acosf(op_reg->vec[i]);
    }
    
} EXTINST_END

EXTINST_RES_1OP(GLSLstd450Atan) {
/* Arc tangent. Result is an angle, in radians, whose tangent is y_over_x. */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < op_reg->type->count; ++i) {
        res_reg->vec[i] = atanf(op_reg->vec[i]);
    }
    
} EXTINST_END

EXTINST_RES_1OP(GLSLstd450Sinh) {
/* Hyperbolic sine of x radians. */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < op_reg->type->count; ++i) {
        res_reg->vec[i] = sinhf(op_reg->vec[i]);
    }
    
} EXTINST_END

EXTINST_RES_1OP(GLSLstd450Cosh) {
/* Hyperbolic cosine of x radians. */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < op_reg->type->count; ++i) {
        res_reg->vec[i] = coshf(op_reg->vec[i]);
    }
    
} EXTINST_END

EXTINST_RES_1OP(GLSLstd450Tanh) {
/* Hyperbolic tangent of x radians. */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < op_reg->type->count; ++i) {
        res_reg->vec[i] = tanhf(op_reg->vec[i]);
    }
    
} EXTINST_END

EXTINST_RES_1OP(GLSLstd450Asinh) {
/* Arc hyperbolic sine; result is the inverse of sinh. */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < op_reg->type->count; ++i) {
        res_reg->vec[i] = asinhf(op_reg->vec[i]);
    }
    
} EXTINST_END

EXTINST_RES_1OP(GLSLstd450Acosh) {
/* Arc hyperbolic cosine; Result is the non-negative inverse of cosh. */    
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < op_reg->type->count; ++i) {
        res_reg->vec[i] = acoshf(op_reg->vec[i]);
    }
    
} EXTINST_END

EXTINST_RES_1OP(GLSLstd450Atanh) {
/* Arc hyperbolic tangent; result is the inverse of tanh. Result is undefined if abs x ≥ 1. */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < op_reg->type->count; ++i) {
        res_reg->vec[i] = atanhf(op_reg->vec[i]);
    }
    
} EXTINST_END

EXTINST_RES_2OP(GLSLstd450Atan2) {
/* Arc tangent. Result is an angle, in radians, whose tangent is y / x. */
    assert(spirv_type_is_float(op1_reg->type));
    assert(op2_reg->type == op1_reg->type);
    assert(res_reg->type == op1_reg->type);

    for (uint32_t i = 0; i < res_reg->type->count; ++i) {
        res_reg->vec[i] = atan2f(op1_reg->vec[i], op2_reg->vec[i]);
    }

} EXTINST_END

/* 
 * exponential/power functions 
 */

EXTINST_RES_2OP(GLSLstd450Pow) {
/* Result is x raised to the y power */
    assert(spirv_type_is_float(op1_reg->type));
    assert(op2_reg->type == op1_reg->type);
    assert(res_reg->type == op1_reg->type);

    for (uint32_t i = 0; i < res_reg->type->count; ++i) {
        res_reg->vec[i] = powf(op1_reg->vec[i], op2_reg->vec[i]);
    }

} EXTINST_END

EXTINST_RES_1OP(GLSLstd450Exp) {
/* Result is the natural exponentiation of x */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < res_reg->type->count; ++i) {
        res_reg->vec[i] = expf(op_reg->vec[i]);
    }

} EXTINST_END

EXTINST_RES_1OP(GLSLstd450Log) {
/* Result is the natural logarithm of x */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < res_reg->type->count; ++i) {
        res_reg->vec[i] = logf(op_reg->vec[i]);
    }
    
} EXTINST_END

EXTINST_RES_1OP(GLSLstd450Exp2) {
/* Result is 2 raised to the x power */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < res_reg->type->count; ++i) {
        res_reg->vec[i] = exp2f(op_reg->vec[i]);
    }
    
} EXTINST_END

EXTINST_RES_1OP(GLSLstd450Log2) {
/* Result is the base-2 logarithm of x */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < res_reg->type->count; ++i) {
        res_reg->vec[i] = log2f(op_reg->vec[i]);
    }
    
} EXTINST_END

EXTINST_RES_1OP(GLSLstd450Sqrt) {
/* Result is the square root of x */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < res_reg->type->count; ++i) {
        res_reg->vec[i] = sqrtf(op_reg->vec[i]);
    }
    
} EXTINST_END

EXTINST_RES_1OP(GLSLstd450InverseSqrt) {
/* Result is the reciprocal of sqrt x */
    assert(spirv_type_is_float(op_reg->type));
    assert(res_reg->type == op_reg->type);

    for (uint32_t i = 0; i < res_reg->type->count; ++i) {
        res_reg->vec[i] = 1.0f / sqrtf(op_reg->vec[i]);
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

        /* trigonometric functions */
        OP(GLSLstd450Radians)
        OP(GLSLstd450Degrees)
        OP(GLSLstd450Sin)
        OP(GLSLstd450Cos)
        OP(GLSLstd450Tan)
        OP(GLSLstd450Asin)
        OP(GLSLstd450Acos)
        OP(GLSLstd450Atan)
        OP(GLSLstd450Sinh)
        OP(GLSLstd450Cosh)
        OP(GLSLstd450Tanh)
        OP(GLSLstd450Asinh)
        OP(GLSLstd450Acosh)
        OP(GLSLstd450Atanh)
        OP(GLSLstd450Atan2)

        /* exponential/power functions */
        OP(GLSLstd450Pow)
        OP(GLSLstd450Exp)
        OP(GLSLstd450Log)
        OP(GLSLstd450Exp2)
        OP(GLSLstd450Log2)
        OP(GLSLstd450Sqrt)
        OP(GLSLstd450InverseSqrt)

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
