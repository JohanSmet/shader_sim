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

static inline uint32_t spirv_sim_assign_register(SPIRV_simulator *sim, uint32_t id, Type *type) {
    assert(sim);
    uint32_t reg_idx = sim->reg_free_start++;
    sim->temp_regs[reg_idx].vec = mem_arena_allocate(&sim->reg_data, type->element_size * type->count);
    sim->temp_regs[reg_idx].id = id;
    sim->temp_regs[reg_idx].type = type;
    map_int_int_put(&sim->assigned_regs, id, reg_idx);
    
    return reg_idx;
}

static inline bool spirv_sim_type_is_integer(Type *type) {
    return type->kind == TypeInteger ||
           type->kind == TypeVectorInteger ||
           type->kind == TypeMatrixInteger;
}

static inline bool spirv_sim_type_is_signed_integer(Type *type) {
    return spirv_sim_type_is_integer(type ) && type->is_signed;
}

static inline bool spirv_sim_type_is_unsigned_integer(Type *type) {
    return spirv_sim_type_is_integer(type ) && !type->is_signed;
}

static inline bool spirv_sim_type_is_float(Type *type) {
    return type->kind == TypeFloat ||
           type->kind == TypeVectorFloat ||
           type->kind == TypeMatrixFloat;
}

static inline bool spirv_sim_type_is_scalar(Type *type) {
   return (type->kind == TypeInteger || type->kind == TypeFloat || type->kind == TypeBool) &&
          type->count == 1;
}

static inline bool spirv_sim_type_is_vector(Type *type) {
    return type->kind == TypeVectorInteger || type->kind == TypeVectorFloat;
}

static inline bool spirv_sim_type_is_matrix(Type *type) {
    return type->kind == TypeMatrixInteger || type->kind == TypeMatrixFloat;
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

SimRegister *spirv_sim_register_by_id(SPIRV_simulator *sim, uint32_t id) {
    assert(sim);
    return &sim->temp_regs[map_int_int_get(&sim->assigned_regs, id)];
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
        if (spirv_sim_type_is_float(reg->type)) {
            used += snprintf(out_str + used, out_max - used, " %.4f", reg->vec[i]);
        } else if (spirv_sim_type_is_integer(reg->type)) {
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

uint32_t aggregate_indices_offset(Type *type, uint32_t num_indices, uint32_t *indices) {
    
    uint32_t offset = 0;
    Type *cur_type = type;
    
    for (uint32_t idx = 0; idx < num_indices; ++idx) {
        if (cur_type->kind == TypeStructure) {
            for (int32_t i = 0; i < indices[idx]; ++i) {
                offset += cur_type->structure.members[i]->element_size * cur_type->structure.members[i]->count;
            }
            cur_type = cur_type->structure.members[indices[idx]];
        } else if (cur_type->kind == TypeArray ||
                   spirv_sim_type_is_vector(cur_type) ||
                   spirv_sim_type_is_matrix(cur_type)) {
            offset += cur_type->element_size * indices[idx];
            cur_type = cur_type->base_type;
        } else {
            assert(0 && "Unsupported type in aggregate hierarchy");
        }
    }
    
    return offset;
}

#define OP_REGISTER(reg, id) \
    SimRegister *reg = &sim->temp_regs[map_int_int_get(&sim->assigned_regs, op->optional[id])];    \
    assert(reg != NULL);

#define OP_REGISTER_ASSIGN(reg, type, result_id) \
    uint32_t reg##_idx = spirv_sim_assign_register(sim, result_id, type);  \
    SimRegister *res_reg = &sim->temp_regs[reg##_idx];

#define OP_FUNC_BEGIN(kind) \
    static inline void spirv_sim_op_##kind(SPIRV_simulator *sim, SPIRV_opcode *op) {    \
        assert(sim);    \
        assert(op);

#define OP_FUNC_RES_1OP(kind) \
    OP_FUNC_BEGIN(kind)       \
        uint32_t result_type = op->optional[0];     \
        uint32_t result_id = op->optional[1];       \
                                                    \
        /* assign a register to keep the data */    \
        Type *res_type = spirv_module_type_by_id(sim->module, result_type);     \
        uint32_t res_idx = spirv_sim_assign_register(sim, result_id, res_type); \
        SimRegister *res_reg = &sim->temp_regs[res_idx];                        \
                                                                                \
        /* retrieve register used for operand */                                \
        OP_REGISTER(op_reg, 2);


#define OP_FUNC_RES_2OP(kind) \
    OP_FUNC_BEGIN(kind)       \
        uint32_t result_type = op->optional[0];     \
        uint32_t result_id = op->optional[1];       \
                                                    \
        /* assign new register for the result */    \
        Type *res_type = spirv_module_type_by_id(sim->module, result_type);     \
        uint32_t res_idx = spirv_sim_assign_register(sim, result_id, res_type); \
        SimRegister *res_reg = &sim->temp_regs[res_idx];                        \
                                                                                \
        /* retrieve register used for operand */                                \
        OP_REGISTER(op1_reg, 2);                                                \
        OP_REGISTER(op2_reg, 3);


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
    assert(spirv_sim_type_is_float(op_reg->type));
    assert(spirv_sim_type_is_signed_integer(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = (uint32_t) op_reg->vec[i];
    }

} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpConvertFToS) {
    /* Convert (value preserving) from floating point to signed integer, with round toward 0.0. */
    assert(spirv_sim_type_is_float(op_reg->type));
    assert(spirv_sim_type_is_signed_integer(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->svec[i] = (int32_t) op_reg->vec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpConvertSToF) {
    /* Convert (value preserving) from signed integer to floating point. */
    assert(spirv_sim_type_is_signed_integer(op_reg->type));
    assert(spirv_sim_type_is_float(res_type));
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->vec[i] = (float) op_reg->svec[i];
    }
    
} OP_FUNC_END


OP_FUNC_RES_1OP(SpvOpConvertUToF) {
    /* Convert (value preserving) from unsigned integer to floating point. */
    assert(spirv_sim_type_is_unsigned_integer(op_reg->type));
    assert(spirv_sim_type_is_float(res_type));
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->vec[i] = (float) op_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpUConvert) {
    /* Convert (value preserving) unsigned width. This is either a truncate or a zero extend. */
    assert(spirv_sim_type_is_unsigned_integer(op_reg->type));
    assert(spirv_sim_type_is_unsigned_integer(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpSConvert) {
    /* Convert (value preserving) signed width. This is either a truncate or a sign extend. */
    assert(spirv_sim_type_is_signed_integer(op_reg->type));
    assert(spirv_sim_type_is_signed_integer(res_type));
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->svec[i] = op_reg->svec[i];
    }
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpFConvert) {
    /* Convert (value preserving) floating-point width. */
    assert(spirv_sim_type_is_float(op_reg->type));
    assert(spirv_sim_type_is_float(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->vec[i] = op_reg->vec[i];
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
 * composite instructions
 */

OP_FUNC_RES_2OP(SpvOpVectorExtractDynamic) {
/* Extract a single, dynamically selected, component of a vector. */
    
    assert(spirv_sim_type_is_scalar(res_type));
    assert(spirv_sim_type_is_vector(op1_reg->type));
    assert(op1_reg->type->base_type == res_type);
    assert(op2_reg->type->kind == TypeInteger);
    
    res_reg->uvec[0] = op1_reg->uvec[op2_reg->uvec[0]];

} OP_FUNC_END

OP_FUNC_BEGIN(SpvOpVectorInsertDynamic) {
/* Make a copy of a vector, with a single, variably selected, component modified. */
    
    Type *res_type = spirv_module_type_by_id(sim->module, op->optional[0]);
    OP_REGISTER_ASSIGN(res_reg, res_type, op->optional[1]);
    OP_REGISTER(vector, 2);
    OP_REGISTER(component, 3);
    OP_REGISTER(index, 4);
    
    assert(spirv_sim_type_is_vector(res_type));
    assert(vector->type == res_type);
    assert(component->type == res_type->base_type);
    assert(index->type->kind == TypeInteger);
    
    memcpy(res_reg->uvec, vector->uvec, res_reg->type->element_size * res_reg->type->count);
    res_reg->uvec[index->uvec[0]] = component->uvec[0];

} OP_FUNC_END

OP_FUNC_BEGIN(SpvOpVectorShuffle) {
/* Select arbitrary components from two vectors to make a new vector. */
    
    Type *res_type = spirv_module_type_by_id(sim->module, op->optional[0]);
    OP_REGISTER_ASSIGN(res_reg, res_type, op->optional[1]);
    OP_REGISTER(vector_1, 2);
    OP_REGISTER(vector_2, 3);
    uint32_t num_components = op->op.length - 5;
    uint32_t *components = &op->optional[4];
    
    assert(spirv_sim_type_is_vector(res_type));
    assert(res_type->count == num_components);
    assert(spirv_sim_type_is_vector(vector_1->type));
    assert(vector_1->type->base_type == res_type->base_type);
    assert(spirv_sim_type_is_vector(vector_2->type));
    assert(vector_2->type->base_type == res_type->base_type);
    
    for (uint32_t c = 0; c < num_components; ++c) {
        if (components[c] == 0xFFFFFFFF) {
            /* no source, undefined */
            continue;
        } else if (components[c] >= vector_1->type->count) {
            res_reg->uvec[c] = vector_2->uvec[components[c] - vector_1->type->count];
        } else {
            res_reg->uvec[c] = vector_1->uvec[components[c]];
        }
    }

} OP_FUNC_END

OP_FUNC_BEGIN(SpvOpCompositeConstruct) {
/* Construct a new composite object from a set of constituent objects that will fully form it. */
    
    Type *res_type = spirv_module_type_by_id(sim->module, op->optional[0]);
    uint32_t result_id = op->optional[1];
    uint32_t num_constituents = op->op.length - 3;
    uint32_t *constituents = &op->optional[2];
    
    OP_REGISTER_ASSIGN(res_reg, res_type, result_id);
    
    if (res_type->kind == TypeStructure) {
        assert(arr_len(res_type->structure.members) == num_constituents);
        
        for (uint32_t c = 0, offset = 0; c < num_constituents; ++c) {
            SimRegister *c_reg = &sim->temp_regs[map_int_int_get(&sim->assigned_regs, constituents[c])];
            assert(res_type->structure.members[c] == c_reg->type);
            
            memcpy(res_reg->raw + offset, c_reg->raw, c_reg->type->count * c_reg->type->element_size);
            offset += c_reg->type->count * c_reg->type->element_size;
        }
    } else if (res_type->kind == TypeArray) {
        assert(res_type->count == num_constituents);
       
        for (uint32_t c = 0, offset = 0; c < num_constituents; ++c) {
            SimRegister *c_reg = &sim->temp_regs[map_int_int_get(&sim->assigned_regs, constituents[c])];
            assert(res_type->base_type == c_reg->type);
            
            memcpy(res_reg->raw + offset, c_reg->raw, c_reg->type->element_size * c_reg->type->count);
            offset += c_reg->type->element_size * c_reg->type->count;
        }
    } else if (spirv_sim_type_is_vector(res_type)) {
        /* for constructing a vector a contiguous subset of the scalars consumed can be represented by a vector operand */
        
        uint32_t res_idx = 0;
        
        for (uint32_t c = 0; c < num_constituents; ++c) {
            SimRegister *c_reg = &sim->temp_regs[map_int_int_get(&sim->assigned_regs, constituents[c])];
            assert(c_reg->type == res_type->base_type || c_reg->type->base_type == res_type->base_type);
            
            for (uint32_t c_idx = 0; c_idx < c_reg->type->count; ++c_idx) {
                res_reg->uvec[res_idx++] = c_reg->uvec[c_idx];
            }
        }
        
        assert(res_idx == res_type->count);
        
    } else if (spirv_sim_type_is_matrix(res_type)) {
        assert(res_type->matrix.num_cols == num_constituents);
        
        for (uint32_t c = 0, offset = 0; c < num_constituents; ++c) {
            SimRegister *c_reg = &sim->temp_regs[map_int_int_get(&sim->assigned_regs, constituents[c])];
            assert(res_type->base_type == c_reg->type);
            
            memcpy(res_reg->raw + offset, c_reg->raw, c_reg->type->count * c_reg->type->element_size);
            offset += c_reg->type->count * c_reg->type->element_size;
        }
    } else {
        assert(0 && "Unsupported type in SpvOpCompositeConstruct");
        return;
    }
    
} OP_FUNC_END

OP_FUNC_BEGIN(SpvOpCompositeExtract) {
/* Extract a part of a composite object. */

    Type *res_type = spirv_module_type_by_id(sim->module, op->optional[0]);
    OP_REGISTER_ASSIGN(res_reg, res_type, op->optional[1]);
    OP_REGISTER(composite, 2);
    
    uint32_t offset = aggregate_indices_offset(composite->type, op->op.length - 4, &op->optional[3]);
    memcpy(res_reg->raw, composite->raw + offset, res_reg->type->count * res_reg->type->element_size);

} OP_FUNC_END

OP_FUNC_BEGIN(SpvOpCompositeInsert) {
/* Make a copy of a composite object, while modifying one part of it. */
    
    Type *res_type = spirv_module_type_by_id(sim->module, op->optional[0]);
    OP_REGISTER_ASSIGN(res_reg, res_type, op->optional[1]);
    OP_REGISTER(object, 2);
    OP_REGISTER(composite, 3);
    
    assert(res_type == composite->type);
    
    uint32_t offset = aggregate_indices_offset(composite->type, op->op.length - 5, &op->optional[4]);
    memcpy(res_reg->raw, composite->raw, res_reg->type->count * res_reg->type->element_size);
    memcpy(res_reg->raw + offset, object->raw, object->type->count * object->type->element_size);
    
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpCopyObject) {
/* Make a copy of Operand. There are no dereferences involved. */
    
    assert(res_reg->type == op_reg->type);
    memcpy(res_reg->raw, op_reg->raw, res_reg->type->count * res_reg->type->element_size);
    
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpTranspose) {
/* Transpose a matrix. */
    
    assert(spirv_sim_type_is_matrix(res_reg->type));
    assert(spirv_sim_type_is_matrix(op_reg->type));
    assert(res_reg->type->base_type == op_reg->type->base_type);
    assert(res_reg->type->matrix.num_cols == op_reg->type->matrix.num_rows);
    assert(res_reg->type->matrix.num_rows == op_reg->type->matrix.num_cols);
    
    for (uint32_t s_row = 0; s_row < op_reg->type->matrix.num_rows; ++s_row) {
        for (uint32_t s_col = 0; s_col < op_reg->type->matrix.num_rows; ++s_col) {
            res_reg->uvec[s_col * op_reg->type->matrix.num_rows + s_row] =
                op_reg->uvec[s_row * op_reg->type->matrix.num_cols + s_col];
        }
    }
    
} OP_FUNC_END

/*
 * arithmetic instructions
 */

OP_FUNC_RES_1OP(SpvOpSNegate)

    assert(spirv_sim_type_is_integer(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->svec[i] = -op_reg->svec[i];
    }

OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpFNegate)

    assert(spirv_sim_type_is_float(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->vec[i] = -op_reg->vec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpIAdd)

    assert(spirv_sim_type_is_integer(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->svec[i] = op1_reg->svec[i] + op2_reg->svec[i];
}

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFAdd)

    assert(spirv_sim_type_is_float(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->vec[i] = op1_reg->vec[i] + op2_reg->vec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpISub)

    assert(spirv_sim_type_is_integer(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->svec[i] = op1_reg->svec[i] - op2_reg->svec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFSub)

    assert(spirv_sim_type_is_float(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->vec[i] = op1_reg->vec[i] - op2_reg->vec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpIMul)

    assert(spirv_sim_type_is_integer(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->svec[i] = op1_reg->svec[i] * op2_reg->svec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFMul)

    assert(spirv_sim_type_is_float(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->vec[i] = op1_reg->vec[i] * op2_reg->vec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpUDiv)

    assert(spirv_sim_type_is_unsigned_integer(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] / op2_reg->uvec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpSDiv)

    assert(spirv_sim_type_is_signed_integer(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->svec[i] = op1_reg->svec[i] / op2_reg->svec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFDiv)

    assert(spirv_sim_type_is_float(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->vec[i] = op1_reg->vec[i] / op2_reg->vec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpUMod)

    assert(spirv_sim_type_is_unsigned_integer(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] % op2_reg->uvec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpSRem)
/* Signed remainder operation of Operand 1 divided by Operand 2. The sign of a non-0 result comes from Operand 1. */

    assert(spirv_sim_type_is_signed_integer(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        int32_t v1 = op1_reg->svec[i];
        int32_t v2 = op2_reg->svec[i];
        res_reg->uvec[i] = (v1 - (v2 * (int)(v1/v2)));
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpSMod)
/* Signed modulo operation of Operand 1 modulo Operand 2. The sign of a non-0 result comes from Operand 2. */

    assert(spirv_sim_type_is_signed_integer(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        int32_t v1 = op1_reg->svec[i];
        int32_t v2 = op2_reg->svec[i];
        res_reg->uvec[i] = (v1 - (v2 * (int)floorf((float) v1/v2)));
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFRem)
/* Floating-point remainder operation of Operand 1 divided by Operand 2. The sign of a non-0 result comes from Operand 1. */

    assert(spirv_sim_type_is_float(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        float v1 = op1_reg->vec[i];
        float v2 = op2_reg->vec[i];
        res_reg->vec[i] = (v1 - (v2 * truncf(v1/v2)));
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFMod)
/* Floating-point modulo operation of Operand 1 divided by Operand 2. The sign of a non-0 result comes from Operand 2. */

    assert(spirv_sim_type_is_float(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        float v1 = op1_reg->vec[i];
        float v2 = op2_reg->vec[i];
        res_reg->vec[i] = (v1 - (v2 * floorf(v1/v2)));
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpVectorTimesScalar)
/* Scale a floating-point vector. */

    assert(spirv_sim_type_is_float(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->vec[i] = op1_reg->vec[i] * op2_reg->vec[0];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpMatrixTimesScalar)
/* Scale a floating-point matrix. */

    assert(spirv_sim_type_is_float(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->vec[i] = op1_reg->vec[i] * op2_reg->vec[0];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpVectorTimesMatrix)
/* Linear-algebraic Vector X Matrix. */

    int32_t num_rows = op2_reg->type->matrix.num_rows;
    int32_t num_cols = op2_reg->type->matrix.num_cols;
    float *v = op1_reg->vec;
    float *m = op2_reg->vec;
    float *res = res_reg->vec;

    for (int32_t col = 0; col < num_cols; ++col) {
        res[col] = 0;
        for (int32_t row = 0; row < num_rows; ++row) {
            res[col] += v[row] * m[col * num_rows + row];
        }
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpMatrixTimesVector) {
/* Linear-algebraic Matrix X Vector. */
    
    int32_t num_rows = op1_reg->type->matrix.num_rows;
    int32_t num_cols = op1_reg->type->matrix.num_cols;
    float *m = op1_reg->vec;
    float *v = op2_reg->vec;
    float *res = res_reg->vec;
    
    for (int32_t row = 0; row < num_rows; ++row) {
        res[row] = 0;
        for (int32_t col = 0; col < num_cols; ++col) {
            res[row] += m[col * num_rows + row] * v[col];
        }
    }

} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpMatrixTimesMatrix) {
/* Linear-algebraic multiply of LeftMatrix X RightMatrix. */
    
    int32_t num_rows1 = op1_reg->type->matrix.num_rows;
    int32_t num_cols1 = op1_reg->type->matrix.num_cols;
    int32_t num_cols2 = op2_reg->type->matrix.num_cols;
    float *m1 = op1_reg->vec;
    float *m2 = op2_reg->vec;
    float *res = res_reg->vec;
    
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
    
    int32_t num_rows = op1_reg->type->count;
    int32_t num_cols = op2_reg->type->count;
    float *v1 = op1_reg->vec;
    float *v2 = op2_reg->vec;
    float *res = res_reg->vec;
    
    for (int32_t row = 0; row < num_rows; ++row) {
        for (int32_t col = 0; col < num_cols; ++col) {
            res[row * num_cols + col] = v1[row] * v2[col];
        }
    }

} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpDot) {
/* Dot product of Vector 1 and Vector 2. */
    
    int32_t n = op1_reg->type->count;
    float *v1 = op1_reg->vec;
    float *v2 = op2_reg->vec;
    float *res = res_reg->vec;
    
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
        res_reg->uvec[i] = op1_reg->uvec[i] >> op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpShiftRightArithmetic) {
    /* Shift the bits in Base right by the number of bits specified in Shift. The most-significant bits will be filled with the sign bit from Base. */
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->svec[i] = op1_reg->svec[i] >> op2_reg->uvec[i];
    }
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpShiftLeftLogical) {
    /* Shift the bits in Base left by the number of bits specified in Shift. The least-significant bits will be zero filled. */
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] << op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpBitwiseOr) {
    
    assert(spirv_sim_type_is_integer(res_type));
    assert(spirv_sim_type_is_integer(op1_reg->type));
    assert(spirv_sim_type_is_integer(op2_reg->type));
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] | op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpBitwiseXor) {
    
    assert(spirv_sim_type_is_integer(res_type));
    assert(spirv_sim_type_is_integer(op1_reg->type));
    assert(spirv_sim_type_is_integer(op2_reg->type));
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] ^ op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpBitwiseAnd) {
    
    assert(spirv_sim_type_is_integer(res_type));
    assert(spirv_sim_type_is_integer(op1_reg->type));
    assert(spirv_sim_type_is_integer(op2_reg->type));
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] & op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpNot) {
    
    assert(spirv_sim_type_is_integer(res_type));
    assert(spirv_sim_type_is_integer(op_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = ~ op_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_BEGIN(SpvOpBitFieldInsert) {
    
    Type *res_type = spirv_module_type_by_id(sim->module, op->optional[0]);
    OP_REGISTER_ASSIGN(res_reg, res_type, op->optional[1]);
    OP_REGISTER(base_reg, 2);
    OP_REGISTER(insert_reg, 3);
    OP_REGISTER(offset_reg, 4);
    OP_REGISTER(count_reg, 5);

    assert(spirv_sim_type_is_integer(offset_reg->type));
    assert(spirv_sim_type_is_integer(count_reg->type));
    
    uint32_t base_mask = ((1 << count_reg->uvec[0]) - 1) << offset_reg->uvec[0];
    uint32_t insert_mask = ~base_mask;

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = (base_reg->uvec[i] & base_mask) | (insert_reg->uvec[i] & insert_mask);
    }

} OP_FUNC_END

OP_FUNC_BEGIN(SpvOpBitFieldSExtract) {
    
    Type *res_type = spirv_module_type_by_id(sim->module, op->optional[0]);
    OP_REGISTER_ASSIGN(res_reg, res_type, op->optional[1]);
    OP_REGISTER(base_reg, 2);
    OP_REGISTER(offset_reg, 3);
    OP_REGISTER(count_reg, 4);

    uint32_t mask = ((1 << count_reg->uvec[0]) - 1) << offset_reg->uvec[0];
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->svec[i] = sign_extend((base_reg->uvec[i] & mask) >> offset_reg->uvec[0], count_reg->uvec[0]);
    }

} OP_FUNC_END

OP_FUNC_BEGIN(SpvOpBitFieldUExtract) {
    
    Type *res_type = spirv_module_type_by_id(sim->module, op->optional[0]);
    OP_REGISTER_ASSIGN(res_reg, res_type, op->optional[1]);
    OP_REGISTER(base_reg, 2);
    OP_REGISTER(offset_reg, 3);
    OP_REGISTER(count_reg, 4);

    uint32_t mask = ((1 << count_reg->uvec[0]) - 1) << offset_reg->uvec[0];
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = (base_reg->uvec[i] & mask) >> offset_reg->uvec[0];
    }
    
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpBitReverse) {
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = reverse_bits(op_reg->uvec[i]);
    }
    
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpBitCount) {
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = count_set_bits(op_reg->uvec[i]);
    }
    
} OP_FUNC_END

/*
 * Relational and Logical Instructions
 */

OP_FUNC_RES_1OP(SpvOpAny) {
/* Result is true if any component of Vector is true, otherwise result is false. */
    
    assert(res_type->kind == TypeBool);
    assert(res_type->count == 1);
    assert(op_reg->type->kind == TypeBool);
    
    res_reg->uvec[0] = false;
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[0] |= op_reg->uvec[i];
    }

} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpAll) {
/* Result is true if all components of Vector are true, otherwise result is false. */
    
    assert(res_type->kind == TypeBool);
    assert(res_type->count == 1);
    assert(op_reg->type->kind == TypeBool);
    
    res_reg->uvec[0] = true;
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[0] &= op_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpIsNan) {
/* Result is true if x is an IEEE NaN, otherwise result is false. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_float(op_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = isnan(op_reg->vec[i]);
    }

} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpIsInf) {
/* Result is true if x is an IEEE Inf, otherwise result is false. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_float(op_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = isinf(op_reg->vec[i]);
    }
    
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpIsFinite) {
/* Result is true if x is an IEEE finite number, otherwise result is false. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_float(op_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = isfinite(op_reg->vec[i]);
    }
    
} OP_FUNC_END
    
OP_FUNC_RES_1OP(SpvOpIsNormal) {
/* Result is true if x is an IEEE normal number, otherwise result is false. */

    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_float(op_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = isnormal(op_reg->vec[i]);
    }
    
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpSignBitSet) {
/* Result is true if x has its sign bit set, otherwise result is false. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_float(op_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = signbit(op_reg->vec[i]);
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpLessOrGreater) {
/* Result is true if x < y or x > y, where IEEE comparisons are used, otherwise result is false. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_float(op1_reg->type));
    assert(spirv_sim_type_is_float(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = islessgreater(op1_reg->vec[i], op2_reg->vec[i]);
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpOrdered) {
/* Result is true if both x == x and y == y are true, where IEEE comparison is used, otherwise result is false. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_float(op1_reg->type));
    assert(spirv_sim_type_is_float(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = !isunordered(op1_reg->vec[i], op2_reg->vec[i]);
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpUnordered) {
/* Result is true if either x or y is an IEEE NaN, otherwise result is false. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_float(op1_reg->type));
    assert(spirv_sim_type_is_float(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = isunordered(op1_reg->vec[i], op2_reg->vec[i]);
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpLogicalEqual) {
/* Result is true if Operand 1 and Operand 2 have the same value. Result is false if Operand 1 and Operand 2 have different values. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_float(op1_reg->type));
    assert(spirv_sim_type_is_float(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] == op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpLogicalNotEqual) {
/* Result is true if Operand 1 and Operand 2 have different values. Result is false if Operand 1 and Operand 2 have the same value. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_float(op1_reg->type));
    assert(spirv_sim_type_is_float(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] != op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpLogicalOr) {
/* Result is true if either Operand 1 or Operand 2 is true. Result is false if both Operand 1 and Operand 2 are false. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_float(op1_reg->type));
    assert(spirv_sim_type_is_float(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] || op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpLogicalAnd) {
/* Result is true if both Operand 1 and Operand 2 are true. Result is false if either Operand 1 or Operand 2 are false. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_float(op1_reg->type));
    assert(spirv_sim_type_is_float(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] && op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpLogicalNot) {
/* Result is true if Operand is false. Result is false if Operand is true. */
    
    assert(res_type->kind == TypeBool);
    assert(op_reg->type == res_type);

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = !op_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_BEGIN(SpvOpSelect) {
/* Select components from two objects. */
    
    Type *res_type = spirv_module_type_by_id(sim->module, op->optional[0]);
    OP_REGISTER_ASSIGN(res_reg, res_type, op->optional[1]);
    OP_REGISTER(cond_reg, 2);
    OP_REGISTER(obj1_reg, 3);
    OP_REGISTER(obj2_reg, 4);

    assert(obj1_reg->type == res_reg->type);
    assert(obj2_reg->type == res_reg->type);

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = (cond_reg->uvec[i]) ? obj1_reg->uvec[i] : obj2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpIEqual) {
/* Integer comparison for equality. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_integer(op1_reg->type));
    assert(spirv_sim_type_is_integer(op2_reg->type));
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] == op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpINotEqual) {
/* Integer comparison for inequality. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_integer(op1_reg->type));
    assert(spirv_sim_type_is_integer(op2_reg->type));
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] != op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpUGreaterThan) {
/* Unsigned-integer comparison if Operand 1 is greater than Operand 2. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_unsigned_integer(op1_reg->type));
    assert(spirv_sim_type_is_unsigned_integer(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] > op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpSGreaterThan) {
/* Signed-integer comparison if Operand 1 is greater than Operand 2. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_signed_integer(op1_reg->type));
    assert(spirv_sim_type_is_signed_integer(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->svec[i] = op1_reg->svec[i] > op2_reg->svec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpUGreaterThanEqual) {
/* Unsigned-integer comparison if Operand 1 is greater than or equal to Operand 2. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_unsigned_integer(op1_reg->type));
    assert(spirv_sim_type_is_unsigned_integer(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] >= op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpSGreaterThanEqual) {
/* Signed-integer comparison if Operand 1 is greater than or equal to Operand 2. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_signed_integer(op1_reg->type));
    assert(spirv_sim_type_is_signed_integer(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->svec[i] = op1_reg->svec[i] >= op2_reg->svec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpULessThan) {
/* Unsigned-integer comparison if Operand 1 is less than Operand 2. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_unsigned_integer(op1_reg->type));
    assert(spirv_sim_type_is_unsigned_integer(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] < op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpSLessThan) {
/* Signed-integer comparison if Operand 1 is less than Operand 2. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_signed_integer(op1_reg->type));
    assert(spirv_sim_type_is_signed_integer(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->svec[i] = op1_reg->svec[i] < op2_reg->svec[i];
    }

} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpULessThanEqual) {
/* Unsigned-integer comparison if Operand 1 is less than or equal to Operand 2. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_unsigned_integer(op1_reg->type));
    assert(spirv_sim_type_is_unsigned_integer(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] <= op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpSLessThanEqual) {
/* Signed-integer comparison if Operand 1 is less than or equal to Operand 2. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_signed_integer(op1_reg->type));
    assert(spirv_sim_type_is_signed_integer(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->svec[i] = op1_reg->svec[i] <= op2_reg->svec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFOrdEqual) {
/* Floating-point comparison for being ordered and equal. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_float(op1_reg->type));
    assert(op1_reg->type == op2_reg->type);
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] =
            !isunordered(op1_reg->vec[i], op2_reg->vec[i]) &&
            (op1_reg->vec[i] == op2_reg->vec[i]);
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFUnordEqual) {
/* Floating-point comparison for being unordered or equal. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_float(op1_reg->type));
    assert(op1_reg->type == op2_reg->type);
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] =
        isunordered(op1_reg->vec[i], op2_reg->vec[i]) ||
        (op1_reg->vec[i] == op2_reg->vec[i]);
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFOrdNotEqual) {
/* Floating-point comparison for being ordered and not equal. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_float(op1_reg->type));
    assert(op1_reg->type == op2_reg->type);
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] =
        !isunordered(op1_reg->vec[i], op2_reg->vec[i]) &&
        (op1_reg->vec[i] != op2_reg->vec[i]);
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFUnordNotEqual) {
/* Floating-point comparison for being unordered or not equal. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_float(op1_reg->type));
    assert(op1_reg->type == op2_reg->type);
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] =
        isunordered(op1_reg->vec[i], op2_reg->vec[i]) ||
        (op1_reg->vec[i] != op2_reg->vec[i]);
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFOrdLessThan) {
/* Floating-point comparison if operands are ordered and Operand 1 is less than Operand 2. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_float(op1_reg->type));
    assert(op1_reg->type == op2_reg->type);
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] =
        !isunordered(op1_reg->vec[i], op2_reg->vec[i]) &&
        (op1_reg->vec[i] < op2_reg->vec[i]);
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFUnordLessThan) {
/* Floating-point comparison if operands are unordered or Operand 1 is less than Operand 2. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_float(op1_reg->type));
    assert(op1_reg->type == op2_reg->type);
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] =
        isunordered(op1_reg->vec[i], op2_reg->vec[i]) ||
        (op1_reg->vec[i] < op2_reg->vec[i]);
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFOrdGreaterThan) {
/* Floating-point comparison if operands are ordered and Operand 1 is greater than Operand 2. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_float(op1_reg->type));
    assert(op1_reg->type == op2_reg->type);
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] =
        !isunordered(op1_reg->vec[i], op2_reg->vec[i]) &&
        (op1_reg->vec[i] > op2_reg->vec[i]);
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFUnordGreaterThan) {
/* Floating-point comparison if operands are unordered or Operand 1 is greater than Operand 2. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_float(op1_reg->type));
    assert(op1_reg->type == op2_reg->type);
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] =
        isunordered(op1_reg->vec[i], op2_reg->vec[i]) ||
        (op1_reg->vec[i] > op2_reg->vec[i]);
    }
    
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFOrdLessThanEqual) {
/* Floating-point comparison if operands are ordered and Operand 1 is less than or equal to Operand 2. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_float(op1_reg->type));
    assert(op1_reg->type == op2_reg->type);
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] =
        !isunordered(op1_reg->vec[i], op2_reg->vec[i]) &&
        (op1_reg->vec[i] <= op2_reg->vec[i]);
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFUnordLessThanEqual) {
/* Floating-point comparison if operands are unordered or Operand 1 is less than or equal to Operand 2. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_float(op1_reg->type));
    assert(op1_reg->type == op2_reg->type);
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] =
        isunordered(op1_reg->vec[i], op2_reg->vec[i]) ||
        (op1_reg->vec[i] <= op2_reg->vec[i]);
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFOrdGreaterThanEqual) {
/* Floating-point comparison if operands are ordered and Operand 1 is greater than or equal to Operand 2. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_float(op1_reg->type));
    assert(op1_reg->type == op2_reg->type);
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] =
        !isunordered(op1_reg->vec[i], op2_reg->vec[i]) &&
        (op1_reg->vec[i] >= op2_reg->vec[i]);
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFUnordGreaterThanEqual) {
/* Floating-point comparison if operands are unordered or Operand 1 is greater than or equal to Operand 2. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_sim_type_is_float(op1_reg->type));
    assert(op1_reg->type == op2_reg->type);
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] =
        isunordered(op1_reg->vec[i], op2_reg->vec[i]) ||
        (op1_reg->vec[i] >= op2_reg->vec[i]);
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
            
        // composite instructions
        OP(SpvOpVectorExtractDynamic)
        OP(SpvOpVectorInsertDynamic)
        OP(SpvOpVectorShuffle)
        OP(SpvOpCompositeConstruct)
        OP(SpvOpCompositeExtract)
        OP(SpvOpCompositeInsert)
        OP(SpvOpCopyObject)
        OP(SpvOpTranspose)

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
            
        // relational and logical instructions
        OP(SpvOpAny)
        OP(SpvOpAll)
        OP(SpvOpIsNan)
        OP(SpvOpIsInf)
        OP(SpvOpIsFinite)
        OP(SpvOpIsNormal)
        OP(SpvOpSignBitSet)
        OP(SpvOpLessOrGreater)
        OP(SpvOpOrdered)
        OP(SpvOpUnordered)
        OP(SpvOpLogicalEqual)
        OP(SpvOpLogicalNotEqual)
        OP(SpvOpLogicalOr)
        OP(SpvOpLogicalAnd)
        OP(SpvOpLogicalNot)
        OP(SpvOpSelect)
        OP(SpvOpIEqual)
        OP(SpvOpINotEqual)
        OP(SpvOpUGreaterThan)
        OP(SpvOpSGreaterThan)
        OP(SpvOpUGreaterThanEqual)
        OP(SpvOpSGreaterThanEqual)
        OP(SpvOpULessThan)
        OP(SpvOpSLessThan)
        OP(SpvOpULessThanEqual)
        OP(SpvOpSLessThanEqual)
        OP(SpvOpFOrdEqual)
        OP(SpvOpFUnordEqual)
        OP(SpvOpFOrdNotEqual)
        OP(SpvOpFUnordNotEqual)
        OP(SpvOpFOrdLessThan)
        OP(SpvOpFUnordLessThan)
        OP(SpvOpFOrdGreaterThan)
        OP(SpvOpFUnordGreaterThan)
        OP(SpvOpFOrdLessThanEqual)
        OP(SpvOpFUnordLessThanEqual)
        OP(SpvOpFOrdGreaterThanEqual)
        OP(SpvOpFUnordGreaterThanEqual)

        OP(SpvOpReturn)

        default:
            arr_printf(sim->error_msg, "Unsupported opcode [%s]", spirv_op_name(op->op.kind));
    }

#undef OP_IGNORE
#undef OP
#undef OP_DEFAULT

    spirv_bin_opcode_next(sim->module->spirv_bin);
}
