// spirv_simulator.c - Johan Smet - BSD-3-Clause (see LICENSE)

#include "spirv_simulator.h"
#include "spirv_binary.h"
#include "spirv_sim_ext.h"
#include "spirv/spirv_names.h"
#include "dyn_array.h"

#include <assert.h>
#include <stdlib.h>
#include <math.h>

static SimPointer *new_sim_pointer(SPIRV_simulator *sim, Type *type, uint32_t pointer) {
    SimPointer *result = (SimPointer *) mem_arena_allocate(&sim->current_frame->memory, sizeof(SimPointer));
    *result = (SimPointer){
        .type = type,
        .pointer = pointer
    };
    return result;
}

static inline SimRegister *spirv_sim_assign_register(SPIRV_simulator *sim, uint32_t id, Type *type) {
    assert(sim);
    assert(sim->current_frame);

    SimRegister *reg = mem_arena_allocate(&sim->current_frame->memory, sizeof(SimRegister));
    reg->vec = mem_arena_allocate(&sim->current_frame->memory, type->element_size * type->count);
    reg->id = id;
    reg->type = type;
    map_int_ptr_put(&sim->current_frame->regs, id, reg);

    return reg;
}

static inline SimRegister *spirv_sim_clone_register(SPIRV_simulator *sim, SPIRV_stackframe *frame, uint32_t id, SimRegister *src) {

    assert(sim);
    assert(frame);
    assert(src);

    SimRegister *reg = mem_arena_allocate(&frame->memory, sizeof(SimRegister));
    reg->vec = mem_arena_allocate(&frame->memory, src->type->element_size * src->type->count);
    memcpy(reg->vec, src->vec, src->type->element_size * src->type->count);
    reg->id = id;
    reg->type = src->type;

    map_int_ptr_put(&frame->regs, id, reg);
    return reg;
}

static inline uint64_t var_data_key(VariableKind kind, VariableAccess *access) {
   return (uint64_t) kind << 48 | (uint64_t) access->kind << 32 | (uint32_t) access->index;
}

static void spirv_add_interface_pointers(SPIRV_simulator *sim, Variable *var_desc, uint32_t pointer) {

    // interface pointer to the entire type
    if (var_desc->access.kind != VarAccessNone) {
        map_int_ptr_put(
            &sim->intf_pointers,
            var_data_key(var_desc->kind, &var_desc->access),
            new_sim_pointer(sim, var_desc->type->base_type, pointer)
        );
    }
    
    // interface pointer to members
    Type *aggregate_type = NULL;
    if (var_desc->type->kind == TypeStructure) {
        aggregate_type = var_desc->type;
    } else if (var_desc->type->kind == TypePointer && var_desc->type->base_type->kind == TypeStructure) {
        aggregate_type = var_desc->type->base_type;
    }

    if (aggregate_type) {
        size_t offset = 0;

        for (uint32_t member = 0; member < arr_len(var_desc->member_access); ++member) {
            Type *member_type = aggregate_type->structure.members[member]; 

            if (var_desc->member_access[member].kind != VarAccessNone) {
                map_int_ptr_put(
                    &sim->intf_pointers,
                    var_data_key(var_desc->kind, &var_desc->member_access[member]),
                    new_sim_pointer(sim, member_type, pointer + offset));
            }

            offset += member_type->element_size * member_type->count;
       }
    }
}

static void stackframe_init(SPIRV_stackframe *frame) {
    memset(frame, 0, sizeof(SPIRV_stackframe));
    mem_arena_init(&frame->memory, 256 * 16, ARENA_DEFAULT_ALIGN);
}

static SPIRV_stackframe *stackframe_new(SPIRV_simulator *sim, SPIRV_function *func) {
    arr_reserve(sim->func_frames, 1);
    sim->current_frame = sim->func_frames + (arr_len(sim->func_frames) - 2);
    SPIRV_stackframe *new_frame = sim->func_frames + (arr_len(sim->func_frames) - 1);
    stackframe_init(new_frame);
    new_frame->func = func;
    return new_frame;
}

static void stackframe_free(SPIRV_stackframe *frame) {
    map_free(&frame->regs);
    mem_arena_free(&frame->memory);
}

static uint32_t allocate_variable(SPIRV_simulator *sim, Variable *var) {

    /* allocate memory */
    size_t total_size = var->array_elements * var->type->base_type->element_size * var->type->base_type->count;
    size_t alloc_size = ALIGN_UP(total_size, 8u);

    arr_reserve(sim->memory, alloc_size);
    uint32_t mem_ptr = sim->memory_free_start;
    sim->memory_free_start += alloc_size;
        
    /* store pointer in a register */
    SimRegister *reg = spirv_sim_assign_register(sim, var->id, var->type);
    reg->uvec[0] = mem_ptr;

    return mem_ptr;
}

static void setup_function_call(SPIRV_simulator *sim, SPIRV_function *func, uint32_t result_id, uint32_t *param_ids, SPIRV_opcode *return_addr) {

    /* create new stack frame */
    SPIRV_stackframe *new_frame = stackframe_new(sim, func);

    /* when the function ends, return to the opcode right after the current opcode */
    new_frame->return_addr = return_addr;
    new_frame->return_id   = result_id;
    new_frame->heap_start  = sim->memory_free_start;

    /* push parameters */
    for (uint32_t idx = 0; idx < arr_len(func->func.parameter_ids); ++idx) {
        SimRegister *arg_reg = spirv_sim_register_by_id(sim, param_ids[idx]);
        spirv_sim_clone_register(sim, new_frame, func->func.parameter_ids[idx], arg_reg);
    }

    /* make stackframe of the new function current */
    sim->current_frame = new_frame;

    /* allocate local variables */
    for (uint32_t idx = 0; idx < arr_len(func->func.variable_ids); ++idx) {
        Variable *var = spirv_module_variable_by_id(sim->module, func->func.variable_ids[idx]);
        allocate_variable(sim, var);
    }
}

void spirv_sim_init(SPIRV_simulator *sim, SPIRV_module *module, uint32_t entrypoint) {
    assert(sim);
    assert(module);
    assert(entrypoint < arr_len(module->entry_points));

    *sim = (SPIRV_simulator) {
        .module = module
    };

    /* load imported extension instructions */
    for (int iter = map_begin(&module->extinst_sets); iter != map_end(&module->extinst_sets); iter = map_next(&module->extinst_sets, iter)) {
        uint32_t id = map_key_int(&module->extinst_sets, iter);
        const char *ext = map_val_str(&module->extinst_sets, iter);

        if (!strcmp(ext, "GLSL.std.450")) {
            map_int_ptr_put(&sim->extinst_funcs, id, &spirv_sim_extension_GLSL_std_450);
        } else {
            arr_printf(sim->error_msg, "Unsupported extension [%s]", ext);
        }
    }

    /* setup stackframe for globals */
    stackframe_init(&sim->global_frame);
    sim->current_frame = &sim->global_frame;

    /* setup access to constants */
    for (int iter = map_begin(&module->constants); iter != map_end(&module->constants); iter = map_next(&module->constants, iter)) {
        uint32_t id = (uint32_t) map_key_int(&module->constants, iter);
        Constant *constant = map_val(&module->constants, iter);
        
        SimRegister *reg = spirv_sim_assign_register(sim, id, constant->type);
        if (constant->type->count == 1) {
            memcpy(reg->vec, &constant->value.as_int, constant->type->element_size);
        } else {
            memcpy(reg->vec, constant->value.as_int_array, constant->type->element_size * constant->type->count);
        }
    }
    
    /* allocate memory for global / pipeline variables */
    for (int iter = map_begin(&module->variables); iter != map_end(&module->variables); iter = map_next(&module->variables, iter)) {
        uint32_t id = (int32_t) map_key_int(&module->variables, iter);
        Variable *var = map_val(&module->variables, iter);

        if (var->kind == VarKindFunction) {
            continue;
        }
        
        /* allocate memory */
        uint32_t mem_ptr = allocate_variable(sim, var);
        spirv_add_interface_pointers(sim, var, mem_ptr);
    }

    /* setup entrypoint */
    sim->entry_point = &sim->module->entry_points[entrypoint];
    SPIRV_function *func = sim->entry_point->function;
    setup_function_call(sim, sim->entry_point->function, 0, NULL, NULL);
    spirv_bin_opcode_jump_to(sim->module->spirv_bin, func->fst_opcode);
}

void spirv_sim_shutdown(SPIRV_simulator *sim) {
    assert(sim);

    /* extensions */
    map_free(&sim->extinst_funcs);

    /* heap */
    arr_free(sim->memory);

    /* interface pointers */
    map_free(&sim->intf_pointers);

    /* stackframes */
    stackframe_free(&sim->global_frame);
    for (SPIRV_stackframe *frame = sim->func_frames; frame != arr_end(sim->func_frames); ++frame) {
        stackframe_free(frame);
    }
    arr_free(sim->func_frames);

    /* error message */
    arr_free(sim->error_msg);
}

void spirv_sim_variable_associate_data(
    SPIRV_simulator *sim, 
    VariableKind kind,
    VariableAccess access, 
    uint8_t *data,
    size_t data_size
) {
    assert(sim);
    assert(data);

    SimPointer *ptr = map_int_ptr_get(&sim->intf_pointers, var_data_key(kind, &access));
    assert(ptr);
    assert(data_size <= (ptr->type->element_size * ptr->type->count));
    
    memcpy(sim->memory + ptr->pointer, data, data_size);
}

SimRegister *spirv_sim_register_by_id(SPIRV_simulator *sim, uint32_t id) {
    assert(sim);

    /* check the stack frame of the current function */
    if (sim->current_frame) {
        SimRegister *reg = map_int_ptr_get(&sim->current_frame->regs, id);
        if (reg != NULL) {
            return reg;
        }
    }

    /* if not found: check the global frame */
    return map_int_ptr_get(&sim->global_frame.regs, id);
}

SimPointer *spirv_sim_retrieve_intf_pointer(SPIRV_simulator *sim, VariableKind kind, VariableAccess access) {
    assert(sim);
    
    SimPointer *result = map_int_ptr_get(&sim->intf_pointers, var_data_key(kind, &access));
    return result;
}

void spirv_register_to_string(SPIRV_simulator *sim, SimRegister *reg, char **out_str) {
    assert(sim);
    assert(reg);

    arr_printf(*out_str, "reg %%%d:", reg->id);
    
    for (uint32_t i = 0; i < reg->type->count; ++i) {
        if (spirv_type_is_float(reg->type)) {
            arr_printf(*out_str, " %.4f", reg->vec[i]);
        } else if (spirv_type_is_integer(reg->type)) {
            if (reg->type->is_signed) {
                arr_printf(*out_str, " %d", reg->svec[i]);
            } else {
                arr_printf(*out_str, " %d", reg->uvec[i]);
            }
        } else if (reg->type->kind == TypePointer) {
            arr_printf(*out_str, " ptr(%x)", reg->uvec[i]);
        }
    }
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

static uint32_t aggregate_indices_offset(Type *type, uint32_t num_indices, uint32_t *indices) {
    
    uint32_t offset = 0;
    Type *cur_type = type;
    
    for (uint32_t idx = 0; idx < num_indices; ++idx) {
        if (cur_type->kind == TypeStructure) {
            for (int32_t i = 0; i < indices[idx]; ++i) {
                offset += cur_type->structure.members[i]->element_size * cur_type->structure.members[i]->count;
            }
            cur_type = cur_type->structure.members[indices[idx]];
        } else if (cur_type->kind == TypeArray ||
                   spirv_type_is_vector(cur_type) ||
                   spirv_type_is_matrix(cur_type)) {
            offset += cur_type->element_size * indices[idx];
            cur_type = cur_type->base_type;
        } else {
            assert(0 && "Unsupported type in aggregate hierarchy");
        }
    }
    
    return offset;
}

#define OP_REGISTER(reg, idx)                                            \
    SimRegister *reg = spirv_sim_register_by_id(sim, op->optional[idx]); \
    assert(reg != NULL);

#define OP_REGISTER_ASSIGN(reg, type, result_id) \
    SimRegister *res_reg = spirv_sim_assign_register(sim, result_id, type);  

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
        SimRegister *res_reg = spirv_sim_assign_register(sim, result_id, res_type); \
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
        SimRegister *res_reg = spirv_sim_assign_register(sim, result_id, res_type); \
                                                                                \
        /* retrieve register used for operand */                                \
        OP_REGISTER(op1_reg, 2);                                                \
        OP_REGISTER(op2_reg, 3);


#define OP_FUNC_END   }

OP_FUNC_BEGIN (SpvOpExtInst) {
    Type *res_type = spirv_module_type_by_id(sim->module, op->optional[0]);
    OP_REGISTER_ASSIGN(res_reg, res_type, op->optional[1]);
    uint32_t set_id = op->optional[2];

    SPIRV_SIM_EXTINST_FUNC extinst_func = (SPIRV_SIM_EXTINST_FUNC) map_int_ptr_get(&sim->extinst_funcs, set_id);
    assert(extinst_func);

    extinst_func(sim, op);

} OP_FUNC_END

OP_FUNC_BEGIN(SpvOpLoad)
    Type *res_type = spirv_module_type_by_id(sim->module, op->optional[0]);
    OP_REGISTER_ASSIGN(res_reg, res_type, op->optional[1]);
    OP_REGISTER(pointer, 2);

    // validate type
    if (res_type != pointer->type->base_type) {
        arr_printf(sim->error_msg, "Type mismatch in SpvOpLoad");
        return;
    }

    // copy data
    size_t var_size = res_type->count * res_type->element_size;
    memcpy(res_reg->raw, sim->memory + pointer->uvec[0], var_size);

OP_FUNC_END

OP_FUNC_BEGIN(SpvOpStore)
    OP_REGISTER(pointer, 0);
    OP_REGISTER(object, 1);

    // validate type
    if (object->type != pointer->type->base_type) {
        arr_printf(sim->error_msg, "Type mismatch in SpvOpStore");
        return;
    }

    // copy data
    size_t var_size = object->type->count * object->type->element_size;
    memcpy(sim->memory + pointer->uvec[0], object->raw, var_size);

OP_FUNC_END

OP_FUNC_BEGIN(SpvOpAccessChain) {
    Type *res_type = spirv_module_type_by_id(sim->module, op->optional[0]);
    OP_REGISTER_ASSIGN(res_reg, res_type, op->optional[1]);
    OP_REGISTER(base, 2);

    Type *cur_type = base->type->base_type;
    uint32_t pointer = base->uvec[0];
    
    for (uint32_t idx = 3; idx < op->op.length - 1; ++idx) {
        uint32_t field_offset = spirv_module_constant_by_id(sim->module, op->optional[idx])->value.as_uint;
        
        switch (cur_type->kind) {
            case TypeStructure:
                for (int32_t i = 0; i < field_offset; ++i) {
                    pointer += cur_type->structure.members[i]->element_size * cur_type->structure.members[i]->count;
                }
                cur_type = cur_type->structure.members[field_offset];
                break;
                
            case TypeArray:
            case TypeVectorFloat:
            case TypeVectorInteger:
            case TypeMatrixFloat:
            case TypeMatrixInteger:
                pointer += cur_type->element_size * field_offset;
                cur_type = cur_type->base_type;
                break;
                
            default:
                assert(0 && "Unsupported type in SvpOpAccessChain");
        }
    }
    
    res_reg->uvec[0] = pointer;

} OP_FUNC_END

/*
 * function instructions
 */

OP_FUNC_BEGIN(SpvOpFunctionCall) {

    // Type *res_type = spirv_module_type_by_id(sim->module, op->optional[0]);
    uint32_t res_id = op->optional[1];
    uint32_t func_id = op->optional[2];

    /* retrieve the function */
    SPIRV_function *func = spirv_module_function_by_id(sim->module, func_id);
    if (func == NULL) {
        arr_printf(sim->error_msg, "Unknown function with id [%%%d]", func_id);
        return;
    }

    assert(arr_len(func->func.parameter_ids) == op->op.length - 4);

    /* get pointer to the parameter ids */
    uint32_t *params = NULL;
    if (op->op.length > 4) {
        params = op->optional + 3;
    }

    /* setup the function call */
    setup_function_call(sim, func, res_id, params, spirv_bin_opcode_next(sim->module->spirv_bin));

    /* jump to the start of the function */
    sim->jump_to_op = func->fst_opcode;

} OP_FUNC_END

OP_FUNC_BEGIN(SpvOpReturn) {

    sim->jump_to_op = sim->current_frame->return_addr;

    /* simulation is done when we're returning from the entrypoint */
    sim->finished = arr_len(sim->func_frames) == 1;

    if (!sim->finished) {
        /* remove current stackframe */
        SPIRV_stackframe *old = &arr_pop(sim->func_frames);
        stackframe_free(old);

        /* free memory allocated for function variables */
        if (sim->memory) {
            arr_remove_back(sim->memory, sim->memory_free_start - old->heap_start);
            sim->memory_free_start = old->heap_start;
        }

        /* return to previous stackframe */
        sim->current_frame = sim->func_frames + (arr_len(sim->func_frames) - 1);
    }

} OP_FUNC_END


OP_FUNC_BEGIN(SpvOpReturnValue) {
    OP_REGISTER(value, 0);

    /* copy the return value to a register in the calling frame */
    if (arr_len(sim->func_frames) > 1) {
        SPIRV_stackframe *calling_frame = sim->func_frames + (arr_len(sim->func_frames) - 2);
        spirv_sim_clone_register(sim, calling_frame, sim->current_frame->return_id, value);
    }

    spirv_sim_op_SpvOpReturn(sim, op);

} OP_FUNC_END

/*
 * conversion instructions
 */

OP_FUNC_RES_1OP(SpvOpConvertFToU) {
    /* Convert (value preserving) from floating point to unsigned integer, with round toward 0.0. */
    assert(spirv_type_is_float(op_reg->type));
    assert(spirv_type_is_unsigned_integer(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = (uint32_t) CLAMP(op_reg->vec[i], 0, UINT32_MAX);
    }

} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpConvertFToS) {
    /* Convert (value preserving) from floating point to signed integer, with round toward 0.0. */
    assert(spirv_type_is_float(op_reg->type));
    assert(spirv_type_is_signed_integer(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->svec[i] = (int32_t) CLAMP(op_reg->vec[i], INT32_MIN, INT32_MAX);
    }
    
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpConvertSToF) {
    /* Convert (value preserving) from signed integer to floating point. */
    assert(spirv_type_is_signed_integer(op_reg->type));
    assert(spirv_type_is_float(res_type));
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->vec[i] = (float) op_reg->svec[i];
    }
    
} OP_FUNC_END


OP_FUNC_RES_1OP(SpvOpConvertUToF) {
    /* Convert (value preserving) from unsigned integer to floating point. */
    assert(spirv_type_is_unsigned_integer(op_reg->type));
    assert(spirv_type_is_float(res_type));
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->vec[i] = (float) op_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpUConvert) {
    /* Convert (value preserving) unsigned width. This is either a truncate or a zero extend. */
    assert(spirv_type_is_unsigned_integer(op_reg->type));
    assert(spirv_type_is_unsigned_integer(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpSConvert) {
    /* Convert (value preserving) signed width. This is either a truncate or a sign extend. */
    assert(spirv_type_is_signed_integer(op_reg->type));
    assert(spirv_type_is_signed_integer(res_type));
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->svec[i] = op_reg->svec[i];
    }
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpFConvert) {
    /* Convert (value preserving) floating-point width. */
    assert(spirv_type_is_float(op_reg->type));
    assert(spirv_type_is_float(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->vec[i] = op_reg->vec[i];
    }
} OP_FUNC_END

//OP_FUNC_RES_1OP(SpvOpQuantizeToF16)

OP_FUNC_RES_1OP(SpvOpConvertPtrToU) {
/* Convert a pointer to an unsigned integer type */
    assert(spirv_type_is_unsigned_integer(res_type));
    assert(op_reg->type->kind == TypePointer);

    res_reg->uvec[0] = op_reg->uvec[0];
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpSatConvertSToU) {
/* Convert a signed integer to unsigned integer. */
    assert(spirv_type_is_signed_integer(op_reg->type));
    assert(spirv_type_is_unsigned_integer(res_type));

    uint32_t max_uint = (uint32_t) (UINT64_C(1) << (res_type->element_size * 8)) - 1;

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = CLAMP(op_reg->svec[i], 0, max_uint);
    }

} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpSatConvertUToS) {
/* Convert an unsigned integer to signed integer.  */
    assert(spirv_type_is_unsigned_integer(op_reg->type));
    assert(spirv_type_is_signed_integer(res_type));

    int32_t max_sint = (1 << ((res_type->element_size * 8) - 1)) - 1;

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->svec[i] = MIN(op_reg->uvec[i], (uint32_t) max_sint);
    }

} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpConvertUToPtr) {
/* Convert an integer to pointer */

    assert(spirv_type_is_unsigned_integer(op_reg->type));
    assert(res_type->kind == TypePointer);

    res_reg->uvec[0] = op_reg->uvec[0];

} OP_FUNC_END

/*
OP_FUNC_RES_1OP(SpvOpPtrCastToGeneric)
OP_FUNC_RES_1OP(SpvOpGenericCastToPtr)
OP_FUNC_RES_1OP(SpvOpGenericCastToPtrExplicit)
OP_FUNC_RES_1OP(SpvOpBitcast)
*/

/*
 * composite instructions
 */

OP_FUNC_RES_2OP(SpvOpVectorExtractDynamic) {
/* Extract a single, dynamically selected, component of a vector. */
    
    assert(spirv_type_is_scalar(res_type));
    assert(spirv_type_is_vector(op1_reg->type));
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
    
    assert(spirv_type_is_vector(res_type));
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
    
    assert(spirv_type_is_vector(res_type));
    assert(res_type->count == num_components);
    assert(spirv_type_is_vector(vector_1->type));
    assert(vector_1->type->base_type == res_type->base_type);
    assert(spirv_type_is_vector(vector_2->type));
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
            SimRegister *c_reg = spirv_sim_register_by_id(sim, constituents[c]);
            assert(res_type->structure.members[c] == c_reg->type);
            
            memcpy(res_reg->raw + offset, c_reg->raw, c_reg->type->count * c_reg->type->element_size);
            offset += c_reg->type->count * c_reg->type->element_size;
        }
    } else if (res_type->kind == TypeArray) {
        assert(res_type->count == num_constituents);
       
        for (uint32_t c = 0, offset = 0; c < num_constituents; ++c) {
            SimRegister *c_reg = spirv_sim_register_by_id(sim, constituents[c]);
            assert(res_type->base_type == c_reg->type);
            
            memcpy(res_reg->raw + offset, c_reg->raw, c_reg->type->element_size * c_reg->type->count);
            offset += c_reg->type->element_size * c_reg->type->count;
        }
    } else if (spirv_type_is_vector(res_type)) {
        /* for constructing a vector a contiguous subset of the scalars consumed can be represented by a vector operand */
        
        uint32_t res_idx = 0;
        
        for (uint32_t c = 0; c < num_constituents; ++c) {
            SimRegister *c_reg = spirv_sim_register_by_id(sim, constituents[c]);
            assert(c_reg->type == res_type->base_type || c_reg->type->base_type == res_type->base_type);
            
            for (uint32_t c_idx = 0; c_idx < c_reg->type->count; ++c_idx) {
                res_reg->uvec[res_idx++] = c_reg->uvec[c_idx];
            }
        }
        
        assert(res_idx == res_type->count);
        
    } else if (spirv_type_is_matrix(res_type)) {
        assert(res_type->matrix.num_cols == num_constituents);
        
        for (uint32_t c = 0, offset = 0; c < num_constituents; ++c) {
            SimRegister *c_reg = spirv_sim_register_by_id(sim, constituents[c]);
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
    
    assert(spirv_type_is_matrix(res_reg->type));
    assert(spirv_type_is_matrix(op_reg->type));
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

    assert(spirv_type_is_integer(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->svec[i] = -op_reg->svec[i];
    }

OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpFNegate)

    assert(spirv_type_is_float(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->vec[i] = -op_reg->vec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpIAdd)

    assert(spirv_type_is_integer(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->svec[i] = op1_reg->svec[i] + op2_reg->svec[i];
}

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFAdd)

    assert(spirv_type_is_float(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->vec[i] = op1_reg->vec[i] + op2_reg->vec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpISub)

    assert(spirv_type_is_integer(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->svec[i] = op1_reg->svec[i] - op2_reg->svec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFSub)

    assert(spirv_type_is_float(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->vec[i] = op1_reg->vec[i] - op2_reg->vec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpIMul)

    assert(spirv_type_is_integer(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->svec[i] = op1_reg->svec[i] * op2_reg->svec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFMul)

    assert(spirv_type_is_float(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->vec[i] = op1_reg->vec[i] * op2_reg->vec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpUDiv)

    assert(spirv_type_is_unsigned_integer(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] / op2_reg->uvec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpSDiv)

    assert(spirv_type_is_signed_integer(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->svec[i] = op1_reg->svec[i] / op2_reg->svec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFDiv)

    assert(spirv_type_is_float(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->vec[i] = op1_reg->vec[i] / op2_reg->vec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpUMod)

    assert(spirv_type_is_unsigned_integer(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] % op2_reg->uvec[i];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpSRem)
/* Signed remainder operation of Operand 1 divided by Operand 2. The sign of a non-0 result comes from Operand 1. */

    assert(spirv_type_is_signed_integer(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        int32_t v1 = op1_reg->svec[i];
        int32_t v2 = op2_reg->svec[i];
        res_reg->uvec[i] = (v1 - (v2 * (int)(v1/v2)));
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpSMod)
/* Signed modulo operation of Operand 1 modulo Operand 2. The sign of a non-0 result comes from Operand 2. */

    assert(spirv_type_is_signed_integer(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        int32_t v1 = op1_reg->svec[i];
        int32_t v2 = op2_reg->svec[i];
        res_reg->uvec[i] = (v1 - (v2 * (int)floorf((float) v1/v2)));
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFRem)
/* Floating-point remainder operation of Operand 1 divided by Operand 2. The sign of a non-0 result comes from Operand 1. */

    assert(spirv_type_is_float(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        float v1 = op1_reg->vec[i];
        float v2 = op2_reg->vec[i];
        res_reg->vec[i] = (v1 - (v2 * truncf(v1/v2)));
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFMod)
/* Floating-point modulo operation of Operand 1 divided by Operand 2. The sign of a non-0 result comes from Operand 2. */

    assert(spirv_type_is_float(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        float v1 = op1_reg->vec[i];
        float v2 = op2_reg->vec[i];
        res_reg->vec[i] = (v1 - (v2 * floorf(v1/v2)));
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpVectorTimesScalar)
/* Scale a floating-point vector. */

    assert(spirv_type_is_float(res_type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->vec[i] = op1_reg->vec[i] * op2_reg->vec[0];
    }

OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpMatrixTimesScalar)
/* Scale a floating-point matrix. */

    assert(spirv_type_is_float(res_type));

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
    
    assert(spirv_type_is_integer(res_type));
    assert(spirv_type_is_integer(op1_reg->type));
    assert(spirv_type_is_integer(op2_reg->type));
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] | op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpBitwiseXor) {
    
    assert(spirv_type_is_integer(res_type));
    assert(spirv_type_is_integer(op1_reg->type));
    assert(spirv_type_is_integer(op2_reg->type));
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] ^ op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpBitwiseAnd) {
    
    assert(spirv_type_is_integer(res_type));
    assert(spirv_type_is_integer(op1_reg->type));
    assert(spirv_type_is_integer(op2_reg->type));
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] & op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpNot) {
    
    assert(spirv_type_is_integer(res_type));
    assert(spirv_type_is_integer(op_reg->type));

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

    assert(spirv_type_is_integer(offset_reg->type));
    assert(spirv_type_is_integer(count_reg->type));
    
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
    assert(spirv_type_is_float(op_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = isnan(op_reg->vec[i]);
    }

} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpIsInf) {
/* Result is true if x is an IEEE Inf, otherwise result is false. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_type_is_float(op_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = isinf(op_reg->vec[i]);
    }
    
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpIsFinite) {
/* Result is true if x is an IEEE finite number, otherwise result is false. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_type_is_float(op_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = isfinite(op_reg->vec[i]);
    }
    
} OP_FUNC_END
    
OP_FUNC_RES_1OP(SpvOpIsNormal) {
/* Result is true if x is an IEEE normal number, otherwise result is false. */

    assert(res_type->kind == TypeBool);
    assert(spirv_type_is_float(op_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = isnormal(op_reg->vec[i]);
    }
    
} OP_FUNC_END

OP_FUNC_RES_1OP(SpvOpSignBitSet) {
/* Result is true if x has its sign bit set, otherwise result is false. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_type_is_float(op_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = signbit(op_reg->vec[i]);
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpLessOrGreater) {
/* Result is true if x < y or x > y, where IEEE comparisons are used, otherwise result is false. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_type_is_float(op1_reg->type));
    assert(spirv_type_is_float(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = islessgreater(op1_reg->vec[i], op2_reg->vec[i]);
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpOrdered) {
/* Result is true if both x == x and y == y are true, where IEEE comparison is used, otherwise result is false. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_type_is_float(op1_reg->type));
    assert(spirv_type_is_float(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = !isunordered(op1_reg->vec[i], op2_reg->vec[i]);
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpUnordered) {
/* Result is true if either x or y is an IEEE NaN, otherwise result is false. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_type_is_float(op1_reg->type));
    assert(spirv_type_is_float(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = isunordered(op1_reg->vec[i], op2_reg->vec[i]);
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpLogicalEqual) {
/* Result is true if Operand 1 and Operand 2 have the same value. Result is false if Operand 1 and Operand 2 have different values. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_type_is_float(op1_reg->type));
    assert(spirv_type_is_float(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] == op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpLogicalNotEqual) {
/* Result is true if Operand 1 and Operand 2 have different values. Result is false if Operand 1 and Operand 2 have the same value. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_type_is_float(op1_reg->type));
    assert(spirv_type_is_float(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] != op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpLogicalOr) {
/* Result is true if either Operand 1 or Operand 2 is true. Result is false if both Operand 1 and Operand 2 are false. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_type_is_float(op1_reg->type));
    assert(spirv_type_is_float(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] || op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpLogicalAnd) {
/* Result is true if both Operand 1 and Operand 2 are true. Result is false if either Operand 1 or Operand 2 are false. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_type_is_float(op1_reg->type));
    assert(spirv_type_is_float(op2_reg->type));

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
    assert(spirv_type_is_integer(op1_reg->type));
    assert(spirv_type_is_integer(op2_reg->type));
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] == op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpINotEqual) {
/* Integer comparison for inequality. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_type_is_integer(op1_reg->type));
    assert(spirv_type_is_integer(op2_reg->type));
    
    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] != op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpUGreaterThan) {
/* Unsigned-integer comparison if Operand 1 is greater than Operand 2. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_type_is_unsigned_integer(op1_reg->type));
    assert(spirv_type_is_unsigned_integer(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] > op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpSGreaterThan) {
/* Signed-integer comparison if Operand 1 is greater than Operand 2. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_type_is_signed_integer(op1_reg->type));
    assert(spirv_type_is_signed_integer(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->svec[i] = op1_reg->svec[i] > op2_reg->svec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpUGreaterThanEqual) {
/* Unsigned-integer comparison if Operand 1 is greater than or equal to Operand 2. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_type_is_unsigned_integer(op1_reg->type));
    assert(spirv_type_is_unsigned_integer(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] >= op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpSGreaterThanEqual) {
/* Signed-integer comparison if Operand 1 is greater than or equal to Operand 2. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_type_is_signed_integer(op1_reg->type));
    assert(spirv_type_is_signed_integer(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->svec[i] = op1_reg->svec[i] >= op2_reg->svec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpULessThan) {
/* Unsigned-integer comparison if Operand 1 is less than Operand 2. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_type_is_unsigned_integer(op1_reg->type));
    assert(spirv_type_is_unsigned_integer(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] < op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpSLessThan) {
/* Signed-integer comparison if Operand 1 is less than Operand 2. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_type_is_signed_integer(op1_reg->type));
    assert(spirv_type_is_signed_integer(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->svec[i] = op1_reg->svec[i] < op2_reg->svec[i];
    }

} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpULessThanEqual) {
/* Unsigned-integer comparison if Operand 1 is less than or equal to Operand 2. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_type_is_unsigned_integer(op1_reg->type));
    assert(spirv_type_is_unsigned_integer(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] = op1_reg->uvec[i] <= op2_reg->uvec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpSLessThanEqual) {
/* Signed-integer comparison if Operand 1 is less than or equal to Operand 2. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_type_is_signed_integer(op1_reg->type));
    assert(spirv_type_is_signed_integer(op2_reg->type));

    for (int32_t i = 0; i < res_type->count; ++i) {
        res_reg->svec[i] = op1_reg->svec[i] <= op2_reg->svec[i];
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFOrdEqual) {
/* Floating-point comparison for being ordered and equal. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_type_is_float(op1_reg->type));
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
    assert(spirv_type_is_float(op1_reg->type));
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
    assert(spirv_type_is_float(op1_reg->type));
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
    assert(spirv_type_is_float(op1_reg->type));
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
    assert(spirv_type_is_float(op1_reg->type));
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
    assert(spirv_type_is_float(op1_reg->type));
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
    assert(spirv_type_is_float(op1_reg->type));
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
    assert(spirv_type_is_float(op1_reg->type));
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
    assert(spirv_type_is_float(op1_reg->type));
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
    assert(spirv_type_is_float(op1_reg->type));
    assert(op1_reg->type == op2_reg->type);
    
    for (uint32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] =
        isunordered(op1_reg->vec[i], op2_reg->vec[i]) ||
        (op1_reg->vec[i] <= op2_reg->vec[i]);
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFOrdGreaterThanEqual) {
/* Floating-point comparison if operands are ordered and Operand 1 is greater than or equal to Operand 2. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_type_is_float(op1_reg->type));
    assert(op1_reg->type == op2_reg->type);
    
    for (uint32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] =
        !isunordered(op1_reg->vec[i], op2_reg->vec[i]) &&
        (op1_reg->vec[i] >= op2_reg->vec[i]);
    }
    
} OP_FUNC_END

OP_FUNC_RES_2OP(SpvOpFUnordGreaterThanEqual) {
/* Floating-point comparison if operands are unordered or Operand 1 is greater than or equal to Operand 2. */
    
    assert(res_type->kind == TypeBool);
    assert(spirv_type_is_float(op1_reg->type));
    assert(op1_reg->type == op2_reg->type);
    
    for (uint32_t i = 0; i < res_type->count; ++i) {
        res_reg->uvec[i] =
        isunordered(op1_reg->vec[i], op2_reg->vec[i]) ||
        (op1_reg->vec[i] >= op2_reg->vec[i]);
    }
    
} OP_FUNC_END

/*
 * control flow instructions
 */
    
OP_FUNC_BEGIN(SpvOpBranch) {
/* Unconditional branch to Target Label. */
    assert(sim);
    uint32_t label_id = op->optional[0];   

    sim->jump_to_op = spirv_module_opcode_by_label(sim->module, label_id);
    assert(sim->jump_to_op);

} OP_FUNC_END

OP_FUNC_BEGIN(SpvOpBranchConditional) {
/* If Condition is true, branch to True Label, otherwise branch to False Label. */
    assert(sim);
    OP_REGISTER(cond_reg, 0);
    uint32_t true_label = op->optional[1];
    uint32_t false_label = op->optional[2];

    sim->jump_to_op = spirv_module_opcode_by_label(
                            sim->module,
                            (cond_reg->svec[0]) ? true_label : false_label);
    assert(sim->jump_to_op);

} OP_FUNC_END

OP_FUNC_BEGIN(SpvOpSwitch) {
/* Multi-way branch to one of the operand label <id>. */
    assert(sim);
    OP_REGISTER(selector_reg, 0);
    uint32_t target = op->optional[1];  // default

    for(uint32_t idx = 2; idx < op->op.length - 1; idx + 2) {
        uint32_t case_literal = op->optional[idx];
        uint32_t case_label = op->optional[idx + 1];

        if (selector_reg->uvec[0] == case_literal) {
            target = case_label;
            break;
        }
    }

    sim->jump_to_op = spirv_module_opcode_by_label(sim->module, target);
    assert(sim->jump_to_op);

} OP_FUNC_END

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
    sim->jump_to_op = NULL;


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

        // extension instructions
        OP(SpvOpExtInst);
            
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

        // function instructions
        OP(SpvOpFunctionCall)
        OP(SpvOpReturn)
        OP(SpvOpReturnValue)
            
        // conversion instructions
        OP(SpvOpConvertFToU)
        OP(SpvOpConvertFToS)
        OP(SpvOpConvertSToF)
        OP(SpvOpConvertUToF)
        OP(SpvOpUConvert)
        OP(SpvOpSConvert)
        OP(SpvOpFConvert)
        OP_DEFAULT(SpvOpQuantizeToF16)
        OP(SpvOpConvertPtrToU)
        OP(SpvOpSatConvertSToU)
        OP(SpvOpSatConvertUToS)
        OP(SpvOpConvertUToPtr)
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

        // control-flow instructions
        OP_DEFAULT(SpvOpPhi)
        OP_IGNORE(SpvOpLoopMerge)
        OP_IGNORE(SpvOpSelectionMerge)
        OP_IGNORE(SpvOpLabel)
        OP(SpvOpBranch)
        OP(SpvOpBranchConditional)
        OP(SpvOpSwitch)
        OP_DEFAULT(SpvOpKill)
        OP_IGNORE(SpvOpUnreachable)
        OP_IGNORE(SpvOpLifetimeStart)
        OP_IGNORE(SpvOpLifetimeStop)

        default:
            arr_printf(sim->error_msg, "Unsupported opcode [%s]", spirv_op_name(op->op.kind));
    }

#undef OP_IGNORE
#undef OP
#undef OP_DEFAULT

    if (sim->jump_to_op != NULL) {
        spirv_bin_opcode_jump_to(sim->module->spirv_bin, sim->jump_to_op);
    } else {
        spirv_bin_opcode_next(sim->module->spirv_bin);
    }
}
