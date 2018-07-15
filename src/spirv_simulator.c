// spirv_simulator.c - Johan Smet - BSD-3-Clause (see LICENSE)

#include "spirv_simulator.h"
#include "spirv_binary.h"
#include "spirv/spirv_names.h"
#include "dyn_array.h"

#include <assert.h>
#include <stdlib.h>
#include <math.h>

static VariableData *new_variable_data(uint8_t *data, size_t size) {
    VariableData *result = (VariableData *) malloc(sizeof(VariableData));
    result->buffer = data;
    result->size = size;
    return result;
}

void spirv_sim_init(SPIRV_simulator *sim, SPIRV_module *module) {
    assert(sim);
    assert(module);

    *sim = (SPIRV_simulator) {
        .module = module
    };

    spirv_sim_select_entry_point(sim, 0);
}

void spirv_sim_variable_associate_data(
    SPIRV_simulator *sim, 
    VariableKind kind,
    VariableInterface if_type, 
    uint32_t if_index,
    uint8_t *data,
    size_t data_size
) {
    assert(sim);
    assert(data);

    uint64_t key = (uint64_t) kind << 48 | (uint64_t) if_type << 32 | if_index;
    map_int_ptr_put(&sim->variable_data, key, new_variable_data(data, data_size));
}

void spirv_sim_select_entry_point(SPIRV_simulator *sim, uint32_t index) {
    assert(sim);
    assert(index < arr_len(sim->module->entry_points));

    SPIRV_function *func = sim->module->entry_points[index].function;
    spirv_bin_opcode_jump_to(sim->module->spirv_bin, func->fst_opcode);
}

static inline Variable *spriv_sim_retrieve_var_desc(SPIRV_simulator *sim, uint32_t id) {
    assert(sim);

    Variable *result = map_int_ptr_get(&sim->module->variables, id);
    return result;
}

VariableData *spirv_sim_retrieve_var(SPIRV_simulator *sim, VariableKind kind, VariableInterface if_type, int32_t if_index) {
    assert(sim);

    uint64_t key = (uint64_t) kind << 48 | (uint64_t) if_type << 32 | if_index;
    VariableData *result = map_int_ptr_get(&sim->variable_data, key);
    return result;
}

static inline VariableData *spirv_sim_var_data(SPIRV_simulator *sim, Variable *var) {
    assert(sim);
    assert(var);

    return spirv_sim_retrieve_var(sim, var->kind, var->if_type, var->if_index);
}

static inline uint32_t spirv_sim_assign_register(SPIRV_simulator *sim, uint32_t id, Type *type) {
    assert(sim);
    uint32_t reg_idx = sim->reg_free_start++;
    sim->temp_regs[reg_idx].id = id;
    sim->temp_regs[reg_idx].type = type;
    map_int_int_put(&sim->assigned_regs, id, reg_idx);
    
    return reg_idx;
}

static inline bool spirv_sim_type_is_integer(TypeKind kind) {
    return kind == TypeInteger ||
           kind == TypeVectorInteger ||
           kind == TypeMatrixInteger;
}

static inline bool spirv_sim_type_is_float(TypeKind kind) {
    return kind == TypeFloat ||
           kind == TypeVectorFloat ||
           kind == TypeMatrixFloat;
}

size_t spirv_register_to_string(SPIRV_simulator *sim, uint32_t reg_idx, char *out_str, size_t out_max) {

    SimRegister *reg = &sim->temp_regs[reg_idx];
    size_t used = snprintf(out_str, out_max, "reg %2d (%%%d):", reg_idx, reg->id);
    
    for (uint32_t i = 0; i < reg->type->count; ++i) {
        if (spirv_sim_type_is_float(reg->type->kind)) {
            used += snprintf(out_str + used, out_max - used, " %.4f", reg->vec[i]);
        } else if (spirv_sim_type_is_integer(reg->type->kind)) {
            if (reg->type->is_signed) {
                used += snprintf(out_str + used, out_max - used, " %d", reg->svec[i]);
            } else {
                used += snprintf(out_str + used, out_max - used, " %d", reg->uvec[i]);
            }
        }
    }
    
    return used;
}

#define OP_FUNC_BEGIN(kind) \
    static inline void spirv_sim_op_##kind(SPIRV_simulator *sim, SPIRV_opcode *op) {    \
        assert(sim);    \
        assert(op);

#define OP_FUNC_RES_1OP(kind) \
    OP_FUNC_BEGIN(kind)       \
        uint32_t result_type = op->optional[0];     \
        uint32_t result_id = op->optional[1];       \
        uint32_t operand = op->optional[2];         \
                                                    \
        /* assign a register to keep the data */    \
        Type *res_type = spirv_module_type_by_id(sim->module, result_type);     \
        uint32_t res_reg = spirv_sim_assign_register(sim, result_id, res_type); \
                                                                                \
        /* retrieve register used for operand */                                \
        uint32_t op_reg = map_int_int_get(&sim->assigned_regs, operand);


#define OP_FUNC_RES_2OP(kind) \
    OP_FUNC_BEGIN(kind)       \
        uint32_t result_type = op->optional[0];     \
        uint32_t result_id = op->optional[1];       \
        uint32_t op1_id = op->optional[2];          \
        uint32_t op2_id = op->optional[3];          \
                                                    \
        /* assign new register for the result */    \
        Type *res_type = spirv_module_type_by_id(sim->module, result_type);     \
        uint32_t res_reg = spirv_sim_assign_register(sim, result_id, res_type); \
                                                                                \
        /* retrieve register used for operand */                                \
        uint32_t op1_reg = map_int_int_get(&sim->assigned_regs, op1_id);        \
        uint32_t op2_reg = map_int_int_get(&sim->assigned_regs, op2_id);


#define OP_FUNC_END   }

OP_FUNC_BEGIN(SpvOpLoad)
    uint32_t result_type = op->optional[0];
    uint32_t result_id = op->optional[1];
    uint32_t pointer_id = op->optional[2];

    // retrieve the data from memory
    Variable *var_desc = spriv_sim_retrieve_var_desc(sim, pointer_id);
    VariableData *data = spirv_sim_var_data(sim, var_desc);

    // validate type
    assert(var_desc->type->kind == TypePointer);
    Type *res_type = spirv_module_type_by_id(sim->module, result_type);

    if (res_type != var_desc->type->pointer.base_type) {
        arr_printf(sim->error_msg, "Type mismatch in SpvOpLoad");
        return;
    }

    // assign a register to keep the data
    uint32_t reg = spirv_sim_assign_register(sim, result_id, res_type);

    // copy data
    size_t var_size = res_type->count * res_type->size;
    memcpy(sim->temp_regs[reg].vec, data->buffer, var_size);

OP_FUNC_END

OP_FUNC_BEGIN(SpvOpStore)
    uint32_t pointer_id = op->optional[0];
    uint32_t object_id = op->optional[1];

    // retrieve pointer to output buffer
    Variable *var_desc = spriv_sim_retrieve_var_desc(sim, pointer_id);
    VariableData *data = spirv_sim_var_data(sim, var_desc);

    // find the register that has the object to store
    uint32_t reg = map_int_int_get(&sim->assigned_regs, object_id);

    // validate type
    assert(var_desc->type->kind == TypePointer);
    Type *res_type = sim->temp_regs[reg].type;

    if (res_type != var_desc->type->pointer.base_type) {
        arr_printf(sim->error_msg, "Type mismatch in SpvOpStore");
        return;
    }

    // copy data
    size_t var_size = res_type->count * res_type->size;
    memcpy(data->buffer, sim->temp_regs[reg].vec, var_size);

OP_FUNC_END

/*
 * arithmetic instructions
 */

OP_FUNC_RES_1OP(SpvOpSNegate)

    assert(spirv_sim_type_is_integer(res_type->kind));

    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].svec[i] = -sim->temp_regs[op_reg].svec[i];
    }

OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpFNegate)

    assert(spirv_sim_type_is_float(res_type->kind));

    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].vec[i] = -sim->temp_regs[op_reg].vec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpIAdd)

    assert(spirv_sim_type_is_integer(res_type->kind));

    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].svec[i] = sim->temp_regs[op1_reg].svec[i] + sim->temp_regs[op2_reg].svec[i];
}

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFAdd)

    assert(spirv_sim_type_is_float(res_type->kind));

    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].vec[i] = sim->temp_regs[op1_reg].vec[i] + sim->temp_regs[op2_reg].vec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpISub)

    assert(spirv_sim_type_is_integer(res_type->kind));

    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].svec[i] = sim->temp_regs[op1_reg].svec[i] - sim->temp_regs[op2_reg].svec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFSub)

    assert(spirv_sim_type_is_float(res_type->kind));

    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].vec[i] = sim->temp_regs[op1_reg].vec[i] - sim->temp_regs[op2_reg].vec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpIMul)

    assert(spirv_sim_type_is_integer(res_type->kind));

    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].svec[i] = sim->temp_regs[op1_reg].svec[i] * sim->temp_regs[op2_reg].svec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFMul)

    assert(spirv_sim_type_is_float(res_type->kind));

    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].vec[i] = sim->temp_regs[op1_reg].vec[i] * sim->temp_regs[op2_reg].vec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpUDiv)

    assert(spirv_sim_type_is_integer(res_type->kind));
    assert(!res_type->is_signed);

    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].uvec[i] = sim->temp_regs[op1_reg].uvec[i] / sim->temp_regs[op2_reg].uvec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpSDiv)

    assert(spirv_sim_type_is_integer(res_type->kind));
    assert(res_type->is_signed);

    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].svec[i] = sim->temp_regs[op1_reg].svec[i] / sim->temp_regs[op2_reg].svec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFDiv)

    assert(spirv_sim_type_is_float(res_type->kind));

    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].vec[i] = sim->temp_regs[op1_reg].vec[i] / sim->temp_regs[op2_reg].vec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpUMod)

    assert(spirv_sim_type_is_integer(res_type->kind));
    assert(!res_type->is_signed);

    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].uvec[i] = sim->temp_regs[op1_reg].uvec[i] / sim->temp_regs[op2_reg].uvec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpSRem)
/* Signed remainder operation of Operand 1 divided by Operand 2. The sign of a non-0 result comes from Operand 1. */

    assert(spirv_sim_type_is_integer(res_type->kind));
    assert(res_type->is_signed);

    for (int32_t i = 0; i < res_type->count; ++i) {
        int32_t v1 = sim->temp_regs[op1_reg].svec[i];
        int32_t v2 = sim->temp_regs[op2_reg].svec[i];
        sim->temp_regs[res_reg].uvec[i] = (v1 - (v2 * (int)(v1/v2)));
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpSMod)
/* Signed modulo operation of Operand 1 modulo Operand 2. The sign of a non-0 result comes from Operand 2. */

    assert(spirv_sim_type_is_integer(res_type->kind));
    assert(res_type->is_signed);

    for (int32_t i = 0; i < res_type->count; ++i) {
        int32_t v1 = sim->temp_regs[op1_reg].svec[i];
        int32_t v2 = sim->temp_regs[op2_reg].svec[i];
        sim->temp_regs[res_reg].uvec[i] = (v1 - (v2 * (int)floorf(v1/v2)));
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFRem)
/* Floating-point remainder operation of Operand 1 divided by Operand 2. The sign of a non-0 result comes from Operand 1. */

    assert(spirv_sim_type_is_float(res_type->kind));

    for (int32_t i = 0; i < res_type->count; ++i) {
        float v1 = sim->temp_regs[op1_reg].vec[i];
        float v2 = sim->temp_regs[op2_reg].vec[i];
        sim->temp_regs[res_reg].vec[i] = (v1 - (v2 * truncf(v1/v2)));
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFMod)
/* Floating-point modulo operation of Operand 1 divided by Operand 2. The sign of a non-0 result comes from Operand 2. */

    assert(spirv_sim_type_is_float(res_type->kind));

    for (int32_t i = 0; i < res_type->count; ++i) {
        float v1 = sim->temp_regs[op1_reg].vec[i];
        float v2 = sim->temp_regs[op2_reg].vec[i];
        sim->temp_regs[res_reg].vec[i] = (v1 - (v2 * floorf(v1/v2)));
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpVectorTimesScalar)
/* Scale a floating-point vector. */

    assert(spirv_sim_type_is_float(res_type->kind));

    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].vec[i] = sim->temp_regs[op1_reg].vec[i] * sim->temp_regs[op2_reg].vec[0];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpMatrixTimesScalar)
/* Scale a floating-point matrix. */

    assert(spirv_sim_type_is_float(res_type->kind));

    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].vec[i] = sim->temp_regs[op1_reg].vec[i] * sim->temp_regs[op2_reg].vec[0];
    }

OP_FUNC_END

OP_FUNC_BEGIN(SpvOpReturn) 
    sim->finished = true;
OP_FUNC_END

#undef OP_FUNC_BEGIN
#undef OP_FUNC_RES_1OP
#undef OP_FUNC_RES_2OP
#undef OP_FUNC_END

void spirv_sim_step(SPIRV_simulator *sim) {
    assert(sim);

    if (sim->finished) {
        return;
    }

    SPIRV_opcode *op = spirv_bin_opcode_current(sim->module->spirv_bin);


#define OP_IGNORE(kind)                 \
    case kind:                          \
        break;     
#define OP(kind)                        \
    case kind:                          \
        spirv_sim_op_##kind(sim, op);   \
        break;
#define OP_DEFAULT(kind)

    switch (op->op.kind) {
        // miscellaneous instructions
        OP_IGNORE(SpvOpNop)
        OP_DEFAULT(SpvOpUndef)
        OP_DEFAULT(SpvOpSizeOf)
            
        // memory instructions
        OP_DEFAULT(SpvOpVariable)
        OP_DEFAULT(SpvOpImageTexelPointer)
        OP(SpvOpLoad)
        OP(SpvOpStore)
        OP_DEFAULT(SpvOpCopyMemory)
        OP_DEFAULT(SpvOpCopyMemorySized)
        OP_DEFAULT(SpvOpAccessChain)
        OP_DEFAULT(SpvOpInBoundsAccessChain)
        OP_DEFAULT(SpvOpPtrAccessChain)
        OP_DEFAULT(SpvOpArrayLength)
        OP_DEFAULT(SpvOpGenericPtrMemSemantics)
        OP_DEFAULT(SpvOpInBoundsPtrAccessChain)
            
        // arithmetic instructions
        OP(SpvOpSNegate)
        OP(SpvOpFNegate)
        OP(SpvOpIAdd)
        OP(SpvOpFAdd)
        OP(SpvOpISub)
        OP(SpvOpFSub)
        OP(SpvOpIMul)
        OP(SpvOpFMul)
        OP(SpvOpUDiv)
        OP(SpvOpSDiv)
        OP(SpvOpFDiv)
        OP(SpvOpUMod)
        OP(SpvOpSRem)
        OP(SpvOpSMod)
        OP(SpvOpFRem)
        OP(SpvOpFMod)
        OP(SpvOpVectorTimesScalar)
        OP(SpvOpMatrixTimesScalar)
        OP_DEFAULT(SpvOpVectorTimesMatrix)
        OP_DEFAULT(SpvOpMatrixTimesVector)
        OP_DEFAULT(SpvOpMatrixTimesMatrix)
        OP_DEFAULT(SpvOpOuterProduct)
        OP_DEFAULT(SpvOpDot)
        OP_DEFAULT(SpvOpIAddCarry)
        OP_DEFAULT(SpvOpISubBorrow)
        OP_DEFAULT(SpvOpUMulExtended)
        OP_DEFAULT(SpvOpSMulExtended)

        OP(SpvOpReturn)

        default:
            arr_printf(sim->error_msg, "Unsupported opcode [%s]", spirv_op_name(op->op.kind));
    }

#undef OP_IGNORE
#undef OP
#undef OP_DEFAULT

    spirv_bin_opcode_next(sim->module->spirv_bin);
}
