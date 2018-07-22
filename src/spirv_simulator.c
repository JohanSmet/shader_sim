// spirv_simulator.c - Johan Smet - BSD-3-Clause (see LICENSE)

#include "spirv_simulator.h"
#include "spirv_binary.h"
#include "spirv/spirv_names.h"
#include "dyn_array.h"

#include <assert.h>
#include <stdlib.h>
#include <math.h>

static SimVarMemory *new_sim_var_memory(SPIRV_simulator *sim, Variable *var, size_t size) {
    SimVarMemory *result = (SimVarMemory *) malloc(sizeof(SimVarMemory) + size);
    result->var_desc = var;
    result->size = size;
    return result;
}

static SimPointer *new_sim_pointer(SPIRV_simulator *sim, Type *type, uint8_t *pointer) {
    SimPointer *result = (SimPointer *) malloc(sizeof(SimPointer));
    *result = (SimPointer){
        .type = type,
        .memory = pointer
    };
    return result;
}

/*static inline Variable *spirv_sim_retrieve_var_desc(SPIRV_simulator *sim, uint32_t id) {
    assert(sim);
    
    Variable *result = map_int_ptr_get(&sim->module->variables, id);
    return result;
} */

/*static inline VariableData *spirv_sim_var_data(SPIRV_simulator *sim, Variable *var) {
    assert(sim);
    assert(var);
    
    return spirv_sim_retrieve_var(sim, var->kind, var->if_type, var->if_index);
}*/

static inline uint32_t spirv_sim_assign_register(SPIRV_simulator *sim, uint32_t id, Type *type) {
    assert(sim);
    uint32_t reg_idx = sim->reg_free_start++;
    sim->temp_regs[reg_idx].vec = mem_arena_allocate(&sim->reg_data, type->element_size * type->count);
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

static inline uint64_t var_data_key(VariableKind kind, VariableInterface if_type, uint32_t if_index) {
   return (uint64_t) kind << 48 | (uint64_t) if_type << 32 | if_index;
}

static void spirv_add_interface_pointers(SPIRV_simulator *sim, SimVarMemory *mem) {

    // interface pointer to the entire type
    if (mem->var_desc->if_type != VarInterfaceNone) {
        map_int_ptr_put(
            &sim->intf_pointers,
            var_data_key(mem->var_desc->kind, mem->var_desc->if_type, mem->var_desc->if_index),
            new_sim_pointer(sim, mem->var_desc->type, mem->memory)
        );
    }
    
    // interface pointer to members
    size_t offset = 0;
    
    for (Variable **iter = mem->var_desc->members; iter != arr_end(mem->var_desc->members); ++iter) {
        map_int_ptr_put(
            &sim->intf_pointers,
            var_data_key((*iter)->kind, (*iter)->if_type, (*iter)->if_index),
            new_sim_pointer(sim, mem->var_desc->type, mem->memory + offset)
        );
        offset += (*iter)->type->element_size * (*iter)->type->count;
    }
}

void spirv_sim_init(SPIRV_simulator *sim, SPIRV_module *module) {
    assert(sim);
    assert(module);

    *sim = (SPIRV_simulator) {
        .module = module
    };

    mem_arena_init(&sim->reg_data, 256 * 16, ARENA_DEFAULT_ALIGN);
    spirv_sim_select_entry_point(sim, 0);
   
    /* setup access to constants */
    for (int iter = map_begin(&module->constants); iter != map_end(&module->constants); iter = map_next(&module->constants, iter)) {
        uint32_t id = map_key_int(&module->constants, iter);
        Constant *constant = map_val(&module->constants, iter);
        
        int32_t reg = spirv_sim_assign_register(sim, id, constant->type);
        if (constant->type->count == 1) {
            memcpy(sim->temp_regs[reg].vec, &constant->value.as_int, constant->type->element_size);
        } else {
            memcpy(sim->temp_regs[reg].vec, constant->value.as_int_array, constant->type->element_size * constant->type->count);
        }
    }
    
    /* allocate memory for variables */
    for (int iter = map_begin(&module->variables); iter != map_end(&module->variables); iter = map_next(&module->variables, iter)) {
        uint32_t id = map_key_int(&module->variables, iter);
        Variable *var = map_val(&module->variables, iter);
        
        size_t total_size = var->array_elements * var->type->element_size * var->type->count;
        SimVarMemory *mem = new_sim_var_memory(sim, var, total_size);
        
        map_int_ptr_put(&sim->var_memory, id, mem);
        map_int_ptr_put(&sim->mem_pointers, id, new_sim_pointer(sim, var->type, mem->memory));
        spirv_add_interface_pointers(sim, mem);
    }
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

    SimPointer *ptr = map_int_ptr_get(&sim->intf_pointers, var_data_key(kind, if_type, if_index));
    assert(ptr);
    assert(data_size <= (ptr->type->element_size * ptr->type->count));
    
    memcpy(ptr->memory, data, data_size);
}

void spirv_sim_select_entry_point(SPIRV_simulator *sim, uint32_t index) {
    assert(sim);
    assert(index < arr_len(sim->module->entry_points));

    SPIRV_function *func = sim->module->entry_points[index].function;
    spirv_bin_opcode_jump_to(sim->module->spirv_bin, func->fst_opcode);
}

SimPointer *spirv_sim_retrieve_intf_pointer(SPIRV_simulator *sim, VariableKind kind, VariableInterface if_type, int32_t if_index) {
    assert(sim);
    
    SimPointer *result = map_int_ptr_get(&sim->intf_pointers, var_data_key(kind, if_type, if_index));
    return result;
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

static inline uint32_t sign_extend(uint32_t data, uint32_t width) {
    return (uint32_t) ((int32_t) (data << (32 - width)) >> (32 - width));
}

static inline uint32_t reverse_bits(uint32_t data) {
    data = ((data >> 1) & 0x55555555) | ((data << 1) & 0xaaaaaaaa);
    data = ((data >> 2) & 0x33333333) | ((data << 2) & 0xcccccccc);
    data = ((data >> 4) & 0x0f0f0f0f) | ((data << 4) & 0xf0f0f0f0);
    data = ((data >> 8) & 0x00ff00ff) | ((data << 8) & 0xff00ff00);
    data = ((data >> 16) & 0x0000ffff) | ((data << 16) & 0xffff0000);
    return data;
}

static inline uint32_t count_set_bits(uint32_t data) {
    uint32_t result = 0;
    
    while (data) {
        data &= (data-1);   /* unset right-most bit */
        result++;
    }
    
    return result;
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
    SimPointer *ptr = map_int_ptr_get(&sim->mem_pointers, pointer_id);

    // validate type
    Type *res_type = spirv_module_type_by_id(sim->module, result_type);

    if (res_type != ptr->type) {
        arr_printf(sim->error_msg, "Type mismatch in SpvOpLoad");
        return;
    }

    // assign a register to keep the data
    uint32_t reg = spirv_sim_assign_register(sim, result_id, res_type);

    // copy data
    size_t var_size = res_type->count * res_type->element_size;
    memcpy(sim->temp_regs[reg].vec, ptr->memory, var_size);

OP_FUNC_END

OP_FUNC_BEGIN(SpvOpStore)
    uint32_t pointer_id = op->optional[0];
    uint32_t object_id = op->optional[1];

    // retrieve pointer to output buffer
    SimPointer *ptr = map_int_ptr_get(&sim->mem_pointers, pointer_id);

    // find the register that has the object to store
    uint32_t reg = map_int_int_get(&sim->assigned_regs, object_id);

    // validate type
    Type *res_type = sim->temp_regs[reg].type;

    if (res_type != ptr->type) {
        arr_printf(sim->error_msg, "Type mismatch in SpvOpStore");
        return;
    }

    // copy data
    size_t var_size = res_type->count * res_type->element_size;
    memcpy(ptr->memory, sim->temp_regs[reg].vec, var_size);

OP_FUNC_END

OP_FUNC_BEGIN(SpvOpAccessChain) {
    uint32_t result_type = op->optional[0];
    uint32_t result_id = op->optional[1];
    uint32_t pointer_id = op->optional[2];
   
    SimVarMemory *mem = map_int_ptr_get(&sim->var_memory, pointer_id);
    Type *cur_type = mem->var_desc->type;
    uint8_t *member_ptr = mem->memory;
    
    for (uint32_t idx = 3; idx < op->op.length - 1; ++idx) {
        uint32_t field_offset = spirv_module_constant_by_id(sim->module, op->optional[idx])->value.as_int;
        
        switch (cur_type->kind) {
            case TypeStructure:
                for (int32_t i = 0; i < field_offset; ++i) {
                    member_ptr += cur_type->structure.members[i]->element_size * cur_type->structure.members[i]->count;
                }
                cur_type = cur_type->structure.members[field_offset];
                break;
                
            case TypeArray:
            case TypeVectorFloat:
            case TypeVectorInteger:
            case TypeMatrixFloat:
            case TypeMatrixInteger:
                member_ptr += cur_type->element_size * field_offset;
                cur_type = cur_type->base_type;
                break;
                
            default:
                assert(0 && "Unsupported type in SvpOpAccessChain");
        }
    }
    
    Type *pointer_type = spirv_module_type_by_id(sim->module, result_type);
    map_int_ptr_put(&sim->mem_pointers, result_id, new_sim_pointer(sim, pointer_type->base_type, member_ptr));

} OP_FUNC_END

/*
 * conversion instructions
 */

OP_FUNC_RES_1OP(SpvOpConvertFToU) {
    /* Convert (value preserving) from floating point to unsigned integer, with round toward 0.0. */
    assert(spirv_sim_type_is_float(sim->temp_regs[op_reg].type->kind));
    assert(spirv_sim_type_is_integer(res_type->kind));
    assert(!res_type->is_signed);
   
    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].uvec[i] = (uint32_t) sim->temp_regs[op_reg].vec[i];
    }

} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpConvertFToS) {
    /* Convert (value preserving) from floating point to signed integer, with round toward 0.0. */
    assert(spirv_sim_type_is_float(sim->temp_regs[op_reg].type->kind));
    assert(spirv_sim_type_is_integer(res_type->kind));
    assert(res_type->is_signed);
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].svec[i] = (int32_t) sim->temp_regs[op_reg].vec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpConvertSToF) {
    /* Convert (value preserving) from signed integer to floating point. */
    assert(spirv_sim_type_is_integer(sim->temp_regs[op_reg].type->kind));
    assert(sim->temp_regs[op_reg].type->is_signed);
    assert(spirv_sim_type_is_float(res_type->kind));
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].vec[i] = (float) sim->temp_regs[op_reg].svec[i];
    }
    
} OP_FUNC_END


OP_FUNC_RES_1OP(SpvOpConvertUToF) {
    /* Convert (value preserving) from unsigned integer to floating point. */
    assert(spirv_sim_type_is_integer(sim->temp_regs[op_reg].type->kind));
    assert(!sim->temp_regs[op_reg].type->is_signed);
    assert(spirv_sim_type_is_float(res_type->kind));
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].vec[i] = (float) sim->temp_regs[op_reg].uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpUConvert) {
    /* Convert (value preserving) unsigned width. This is either a truncate or a zero extend. */
    assert(spirv_sim_type_is_integer(sim->temp_regs[op_reg].type->kind));
    assert(!sim->temp_regs[op_reg].type->is_signed);
    assert(spirv_sim_type_is_integer(res_type->kind));
    assert(!res_type->is_signed);

    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].uvec[i] = sim->temp_regs[op_reg].uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpSConvert) {
    /* Convert (value preserving) signed width. This is either a truncate or a sign extend. */
    assert(spirv_sim_type_is_integer(sim->temp_regs[op_reg].type->kind));
    assert(sim->temp_regs[op_reg].type->is_signed);
    assert(spirv_sim_type_is_integer(res_type->kind));
    assert(res_type->is_signed);
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].svec[i] = sim->temp_regs[op_reg].svec[i];
    }
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpFConvert) {
    /* Convert (value preserving) floating-point width. */
    assert(spirv_sim_type_is_float(sim->temp_regs[op_reg].type->kind));
    assert(spirv_sim_type_is_float(res_type->kind));

    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].vec[i] = sim->temp_regs[op_reg].vec[i];
    }
} OP_FUNC_END

/*OP_FUNC_RES_1OP(SpvOpQuantizeToF16)
OP_FUNC_RES_1OP(SpvOpConvertPtrToU)
OP_FUNC_RES_1OP(SpvOpSatConvertSToU)
OP_FUNC_RES_1OP(SpvOpSatConvertUToS)
OP_FUNC_RES_1OP(SpvOpConvertUToPtr)
OP_FUNC_RES_1OP(SpvOpPtrCastToGeneric)
OP_FUNC_RES_1OP(SpvOpGenericCastToPtr)
OP_FUNC_RES_1OP(SpvOpGenericCastToPtrExplicit)
OP_FUNC_RES_1OP(SpvOpBitcast)*/

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

OP_FUNC_RES_2OP(SpvOpVectorTimesMatrix)
/* Linear-algebraic Vector X Matrix. */

    int32_t num_rows = sim->temp_regs[op2_reg].type->matrix.num_rows;
    int32_t num_cols = sim->temp_regs[op2_reg].type->matrix.num_cols;
    float *v = sim->temp_regs[op1_reg].vec;
    float *m = sim->temp_regs[op2_reg].vec;
    float *res = sim->temp_regs[res_reg].vec;

    for (int32_t col = 0; col < num_cols; ++col) {
        res[col] = 0;
        for (int32_t row = 0; row < num_rows; ++row) {
            res[col] += v[row] * m[col * num_rows + row];
        }
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpMatrixTimesVector) {
/* Linear-algebraic Matrix X Vector. */
    
    int32_t num_rows = sim->temp_regs[op1_reg].type->matrix.num_rows;
    int32_t num_cols = sim->temp_regs[op1_reg].type->matrix.num_cols;
    float *m = sim->temp_regs[op1_reg].vec;
    float *v = sim->temp_regs[op2_reg].vec;
    float *res = sim->temp_regs[res_reg].vec;
    
    for (int32_t row = 0; row < num_rows; ++row) {
        res[row] = 0;
        for (int32_t col = 0; col < num_cols; ++col) {
            res[row] += m[col * num_rows + row] * v[col];
        }
    }

} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpMatrixTimesMatrix) {
/* Linear-algebraic multiply of LeftMatrix X RightMatrix. */
    
    int32_t num_rows1 = sim->temp_regs[op1_reg].type->matrix.num_rows;
    int32_t num_cols1 = sim->temp_regs[op1_reg].type->matrix.num_cols;
    int32_t num_cols2 = sim->temp_regs[op2_reg].type->matrix.num_cols;
    float *m1 = sim->temp_regs[op1_reg].vec;
    float *m2 = sim->temp_regs[op2_reg].vec;
    float *res = sim->temp_regs[res_reg].vec;
    
    for (int32_t i = 0; i < num_rows1; ++i) {
        for (int32_t j = 0; j < num_cols2; ++j) {
            res[i * num_cols2 + j] = 0;
            for (int32_t k = 0; k < num_cols1; ++k) {
                res[i * num_cols2 + j] += m1[i * num_cols1 + k] * m2[k * num_cols2 + j];
            }
        }
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpOuterProduct) {
/* Linear-algebraic outer product of Vector 1 and Vector 2. */
    
    int32_t num_rows = sim->temp_regs[op1_reg].type->count;
    int32_t num_cols = sim->temp_regs[op2_reg].type->count;
    float *v1 = sim->temp_regs[op1_reg].vec;
    float *v2 = sim->temp_regs[op2_reg].vec;
    float *res = sim->temp_regs[res_reg].vec;
    
    for (int32_t row = 0; row < num_rows; ++row) {
        for (int32_t col = 0; col < num_cols; ++col) {
            res[row * num_cols + col] = v1[row] * v2[col];
        }
    }

} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpDot) {
/* Dot product of Vector 1 and Vector 2. */
    
    int32_t n = sim->temp_regs[op1_reg].type->count;
    float *v1 = sim->temp_regs[op1_reg].vec;
    float *v2 = sim->temp_regs[op2_reg].vec;
    float *res = sim->temp_regs[res_reg].vec;
    
    *res = v1[0] * v2[0];
    for (int32_t i = 1; i < n; ++i) {
        *res += v1[i] * v2[i];
    }
    
} OP_FUNC_END

/*
 * bit instructions
 */

OP_FUNC_RES_2OP(SpvOpShiftRightLogical) {
    /* Shift the bits in Base right by the number of bits specified in Shift. The most-significant bits will be zero filled. */
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].uvec[i] = sim->temp_regs[op1_reg].uvec[i] >> sim->temp_regs[op2_reg].uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpShiftRightArithmetic) {
    /* Shift the bits in Base right by the number of bits specified in Shift. The most-significant bits will be filled with the sign bit from Base. */
    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].svec[i] = sim->temp_regs[op1_reg].svec[i] >> sim->temp_regs[op2_reg].uvec[i];
    }
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpShiftLeftLogical) {
    /* Shift the bits in Base left by the number of bits specified in Shift. The least-significant bits will be zero filled. */
    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].uvec[i] = sim->temp_regs[op1_reg].uvec[i] << sim->temp_regs[op2_reg].uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpBitwiseOr) {
    
    assert(spirv_sim_type_is_integer(res_type->kind));
    assert(spirv_sim_type_is_integer(sim->temp_regs[op1_reg].type->kind));
    assert(spirv_sim_type_is_integer(sim->temp_regs[op2_reg].type->kind));
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].uvec[i] = sim->temp_regs[op1_reg].uvec[i] | sim->temp_regs[op2_reg].uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpBitwiseXor) {
    
    assert(spirv_sim_type_is_integer(res_type->kind));
    assert(spirv_sim_type_is_integer(sim->temp_regs[op1_reg].type->kind));
    assert(spirv_sim_type_is_integer(sim->temp_regs[op2_reg].type->kind));
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].uvec[i] = sim->temp_regs[op1_reg].uvec[i] ^ sim->temp_regs[op2_reg].uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpBitwiseAnd) {
    
    assert(spirv_sim_type_is_integer(res_type->kind));
    assert(spirv_sim_type_is_integer(sim->temp_regs[op1_reg].type->kind));
    assert(spirv_sim_type_is_integer(sim->temp_regs[op2_reg].type->kind));
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].uvec[i] = sim->temp_regs[op1_reg].uvec[i] & sim->temp_regs[op2_reg].uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpNot) {
    
    assert(spirv_sim_type_is_integer(res_type->kind));
    assert(spirv_sim_type_is_integer(sim->temp_regs[op_reg].type->kind));

    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].uvec[i] = ~ sim->temp_regs[op_reg].uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_BEGIN(SpvOpBitFieldInsert) {
    
    Type *res_type = spirv_module_type_by_id(sim->module, op->optional[0]);
    SimRegister *res_reg = map_int_ptr_get(&sim->assigned_regs, op->optional[1]);
    SimRegister *base_reg = map_int_ptr_get(&sim->assigned_regs, op->optional[2]);
    SimRegister *insert_reg = map_int_ptr_get(&sim->assigned_regs, op->optional[3]);
    SimRegister *offset_reg = map_int_ptr_get(&sim->assigned_regs, op->optional[4]);
    SimRegister *count_reg = map_int_ptr_get(&sim->assigned_regs, op->optional[5]);

    assert(spirv_sim_type_is_integer(offset_reg->type->kind));
    assert(spirv_sim_type_is_integer(count_reg->type->kind));
    
    uint32_t base_mask = ((1 << count_reg->uvec[0]) - 1) << offset_reg->uvec[0];
    uint32_t insert_mask = ~base_mask;

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = (base_reg->uvec[i] & base_mask) | (insert_reg->uvec[i] & insert_mask);
    }

} OP_FUNC_END

OP_FUNC_BEGIN(SpvOpBitFieldSExtract) {
    
    Type *res_type = spirv_module_type_by_id(sim->module, op->optional[0]);
    SimRegister *res_reg = map_int_ptr_get(&sim->assigned_regs, op->optional[1]);
    SimRegister *base_reg = map_int_ptr_get(&sim->assigned_regs, op->optional[2]);
    SimRegister *offset_reg = map_int_ptr_get(&sim->assigned_regs, op->optional[4]);
    SimRegister *count_reg = map_int_ptr_get(&sim->assigned_regs, op->optional[5]);
    
    uint32_t mask = ((1 << count_reg->uvec[0]) - 1) << offset_reg->uvec[0];
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->svec[i] = sign_extend((base_reg->uvec[i] & mask) >> offset_reg->uvec[0], count_reg->uvec[0]);
    }

} OP_FUNC_END

OP_FUNC_BEGIN(SpvOpBitFieldUExtract) {
    
    Type *res_type = spirv_module_type_by_id(sim->module, op->optional[0]);
    SimRegister *res_reg = map_int_ptr_get(&sim->assigned_regs, op->optional[1]);
    SimRegister *base_reg = map_int_ptr_get(&sim->assigned_regs, op->optional[2]);
    SimRegister *offset_reg = map_int_ptr_get(&sim->assigned_regs, op->optional[4]);
    SimRegister *count_reg = map_int_ptr_get(&sim->assigned_regs, op->optional[5]);
    
    uint32_t mask = ((1 << count_reg->uvec[0]) - 1) << offset_reg->uvec[0];
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = (base_reg->uvec[i] & mask) >> offset_reg->uvec[0];
    }
    
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpBitReverse) {
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].uvec[i] = reverse_bits(sim->temp_regs[op_reg].uvec[i]);
    }
    
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpBitCount) {
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        sim->temp_regs[res_reg].uvec[i] = count_set_bits(sim->temp_regs[op_reg].uvec[i]);
    }
    
} OP_FUNC_END

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
        OP(SpvOpAccessChain)
        OP_DEFAULT(SpvOpInBoundsAccessChain)
        OP_DEFAULT(SpvOpPtrAccessChain)
        OP_DEFAULT(SpvOpArrayLength)
        OP_DEFAULT(SpvOpGenericPtrMemSemantics)
        OP_DEFAULT(SpvOpInBoundsPtrAccessChain)
            
        // conversion instructions
        OP(SpvOpConvertFToU)
        OP(SpvOpConvertFToS)
        OP(SpvOpConvertSToF)
        OP(SpvOpConvertUToF)
        OP(SpvOpUConvert)
        OP(SpvOpSConvert)
        OP(SpvOpFConvert)
        OP_DEFAULT(SpvOpQuantizeToF16)
        OP_DEFAULT(SpvOpConvertPtrToU)
        OP_DEFAULT(SpvOpSatConvertSToU)
        OP_DEFAULT(SpvOpSatConvertUToS)
        OP_DEFAULT(SpvOpConvertUToPtr)
        OP_DEFAULT(SpvOpPtrCastToGeneric)
        OP_DEFAULT(SpvOpGenericCastToPtr)
        OP_DEFAULT(SpvOpGenericCastToPtrExplicit)
        OP_DEFAULT(SpvOpBitcast)

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
        OP(SpvOpVectorTimesMatrix)
        OP(SpvOpMatrixTimesVector)
        OP(SpvOpMatrixTimesMatrix)
        OP(SpvOpOuterProduct)
        OP(SpvOpDot)
        OP_DEFAULT(SpvOpIAddCarry)
        OP_DEFAULT(SpvOpISubBorrow)
        OP_DEFAULT(SpvOpUMulExtended)
        OP_DEFAULT(SpvOpSMulExtended)
            
        // bit instructions
        OP(SpvOpShiftRightLogical)
        OP(SpvOpShiftRightArithmetic)
        OP(SpvOpShiftLeftLogical)
        OP(SpvOpBitwiseOr)
        OP(SpvOpBitwiseXor)
        OP(SpvOpBitwiseAnd)
        OP(SpvOpNot)
        OP(SpvOpBitFieldInsert)
        OP(SpvOpBitFieldSExtract)
        OP(SpvOpBitFieldUExtract)
        OP(SpvOpBitReverse)
        OP(SpvOpBitCount)

        OP(SpvOpReturn)

        default:
            arr_printf(sim->error_msg, "Unsupported opcode [%s]", spirv_op_name(op->op.kind));
    }

#undef OP_IGNORE
#undef OP
#undef OP_DEFAULT

    spirv_bin_opcode_next(sim->module->spirv_bin);
}
