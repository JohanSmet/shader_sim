// spirv_sim_ext.h - Johan Smet - BSD-3-Clause (see LICENSE)
//
// SPIR-V simulator extenstion instructions

#ifndef JS_SHADER_SIM_SPIRV_SIM_EXT_H
#define JS_SHADER_SIM_SPIRV_SIM_EXT_H

// forward declarations
struct SPIRV_opcode;
struct SPIRV_simulator;

// types
typedef void (*SPIRV_SIM_EXTINST_FUNC)(struct SPIRV_simulator *sim, struct SPIRV_opcode *op);

// functions
void spirv_sim_extension_GLSL_std_450(struct SPIRV_simulator *sim, struct SPIRV_opcode *op);


#ifdef SPIRV_SIM_EXT_INTERNAL

#define EXTINST_OPCODE(op)      (op)->optional[3]
#define EXTINST_PARAM(op,idx)   (op)->optional[4 + (idx)]

#define EXTINST_REGISTER(reg, idx)    \
    SimRegister *reg = spirv_sim_register_by_id(sim,idx);    \
    assert(reg != NULL);

#define EXTINST_BEGIN(kind) \
    static inline void spirv_sim_extinst_##kind(SPIRV_simulator *sim, SPIRV_opcode *op) { \
        assert(sim);        \
        assert(op);

#define EXTINST_RES_1OP(kind)   \
    EXTINST_BEGIN(kind)         \
        /* assign a register to keep the data */    \
        EXTINST_REGISTER(res_reg, op->optional[1]); \
                                                    \
        /* retrieve register used for operand */    \
        EXTINST_REGISTER(op_reg, EXTINST_PARAM(op, 0));

#define EXTINST_END     }

#endif // SPIRV_SIM_EXT_INTERNAL


#endif // JS_SHADER_SIM_SPIRV_SIM_EXT_H
