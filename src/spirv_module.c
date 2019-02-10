// spirv_module.c - Johan Smet - BSD-3-Clause (see LICENSE)

#include "spirv_module.h"
#include "spirv_binary.h"
#include "spirv/spirv.h"

#include "dyn_array.h"

#include <assert.h>
#include <stdlib.h>

/* allocation / initialization functions */
static inline Type *new_type(SPIRV_module *module, uint32_t id, TypeKind kind) {
    Type *result = mem_arena_allocate(&module->allocator, sizeof(Type));
    *result = (Type) {
        .id = id,
        .kind = kind
    };
    return result;
}

static inline Constant *new_constant(SPIRV_module *module, Type *type) {
    Constant *result = mem_arena_allocate(&module->allocator, sizeof(Constant));
    *result = (Constant) {
        .type = type
    };
    return result;
}

static inline Variable *new_variable(SPIRV_module *module, uint32_t id, Type *type) {
    Variable *result = mem_arena_allocate(&module->allocator, sizeof(Variable));
    *result = (Variable) {
		.id = id,
        .type = type,
        .array_elements = 1,
        .initializer_kind = InitializerNone
    };
    return result;
}

static inline SPIRV_function *new_function(SPIRV_module *module, Type *type, uint32_t id) {
    SPIRV_function *result = mem_arena_allocate(&module->allocator, sizeof(SPIRV_function));
    *result = (SPIRV_function) {
        .func.id = id,
        .func.type = type
    };
    return result;
}

/* lookup functions */
static inline uint64_t id_member_to_key(uint32_t id, int32_t member) {
    return (uint64_t) id << 32 | (uint32_t) member;
}

Type *spirv_module_type_by_id(SPIRV_module *module, uint32_t id) {
    return map_int_ptr_get(&module->types, id);
}

const char *spirv_module_name_by_id(SPIRV_module *module, uint32_t id, int32_t member) {
    return map_int_str_get(&module->names, id_member_to_key(id, member));
}

Constant *spirv_module_constant_by_id(SPIRV_module *module, uint32_t id) {
    return map_int_ptr_get(&module->constants, id);
}

uint32_t spirv_module_variable_count(SPIRV_module *module) {
	return (uint32_t) map_len(&module->variables);
}

uint32_t spirv_module_variable_id(SPIRV_module *module, uint32_t index) {
	assert(index < map_len(&module->variables));

	uint32_t current = 0;

	for (int iter = map_begin(&module->variables); iter != map_end(&module->variables); iter = map_next(&module->variables, iter)) {
		if (current == index) {
			return (uint32_t) map_key_int(&module->variables, iter);
		} else {
			++current;
		}
	}

	return 0;
}

Variable *spirv_module_variable_by_id(SPIRV_module *module, uint32_t id) {
    return map_int_ptr_get(&module->variables, id);
}

bool spirv_module_variable_by_access(
    SPIRV_module *module, 
    VariableKind kind, 
    VariableAccess access,
    Variable **ret_var,
    int32_t *ret_member
) {
    assert(module != NULL);
    assert(ret_var != NULL);
    assert(ret_member != NULL);

    Variable **vars = map_int_ptr_get(&module->variables_sc, kind);
    
    if (!vars) {
        return false;
    }
    
    for (Variable **var = vars; var != arr_end(vars); ++var) {
        if (memcmp(&(*var)->access, &access, sizeof(VariableAccess)) == 0) {
            *ret_var = *var;
            *ret_member = -1;
            return true;
        }

        for (VariableAccess *mem = (*var)->member_access; mem != arr_end((*var)->member_access); ++mem) {
            if (memcmp(mem, &access, sizeof(VariableAccess)) == 0) {
                *ret_var = *var;
                *ret_member = (int32_t) (mem - (*var)->member_access);
                return true;
            }
        }
    }
    
    return false;
}

SPIRV_function *spirv_module_function_by_id(SPIRV_module *module, uint32_t id) {
    return map_int_ptr_get(&module->functions, id);
}

static SPIRV_opcode **decoration_ops_by_id(SPIRV_module *module, uint32_t target, int32_t member) {
    return map_int_ptr_get(&module->decorations, id_member_to_key(target, member));
}

static bool id_has_decoration (SPIRV_module *module, uint32_t target, int32_t member, SpvDecoration wanted) {
    
    SPIRV_opcode **decorations = decoration_ops_by_id(module, target, member);
    
    for (SPIRV_opcode **op = decorations; op != arr_end(decorations); ++op) {
        uint32_t dec_offset = ((*op)->op.kind == SpvOpDecorate) ? 1 : 2;
        uint32_t decoration = (*op)->optional[dec_offset];
        
        if (decoration == wanted) {
            return true;
        }
    }
    
    return false;
}

static void define_extinst_set(SPIRV_module *module, uint32_t id, const char *name) {
    map_int_str_put(&module->extinst_sets, id, name);
}

static void define_name(SPIRV_module *module, uint32_t id, const char *name, int member_index) {
    map_int_str_put(&module->names, id_member_to_key(id, member_index), name);
}

static void define_sc_variable(SPIRV_module *module, Variable *var) {
    Variable **kind_vars = map_int_ptr_get(&module->variables_sc, var->kind);
    arr_push(kind_vars, var);
    map_int_ptr_put(&module->variables_sc, var->kind, kind_vars);
}

static inline bool is_opcode_type(SPIRV_opcode *op) {
    return op->op.kind >= SpvOpTypeVoid && op->op.kind <= SpvOpTypeForwardPointer;
}

static void handle_opcode_type(SPIRV_module *module, SPIRV_opcode *op) {
    Type *type = NULL;
    uint32_t result_id = op->optional[0];

    switch (op->op.kind) {                                  // FIXME: support all types
        case SpvOpTypeVoid:
            type = new_type(module, result_id, TypeVoid);
            type->count = 1;
            break;
        case SpvOpTypeBool:
            type = new_type(module, result_id, TypeBool);
            type->count = 1;
            type->element_size = sizeof(bool);
            break;
        case SpvOpTypeInt:
            type = new_type(module, result_id, TypeInteger);
            type->count = 1;
            type->element_size = op->optional[1] / 8;
            type->is_signed = op->optional[2] == 1;
            break;
        case SpvOpTypeFloat:
            type = new_type(module, result_id, TypeFloat);
            type->count = 1;
            type->element_size = op->optional[1] / 8;
            break;
        case SpvOpTypeVector: {
            Type *base_type = spirv_module_type_by_id(module, op->optional[1]);
            if (base_type->kind == TypeInteger) {
                type = new_type(module, result_id, TypeVectorInteger);
                type->count = op->optional[2];
                type->element_size = base_type->element_size;
                type->is_signed = base_type->is_signed;
            } else if (base_type->kind == TypeFloat) {
                type = new_type(module, result_id, TypeVectorFloat);
                type->count = op->optional[2];
                type->element_size = base_type->element_size;
            }
            type->base_type = base_type;
            break;
        }
        case SpvOpTypeMatrix: {
            Type *col_type = spirv_module_type_by_id(module, op->optional[1]);
            if (col_type->kind == TypeVectorInteger) {
                type = new_type(module, result_id, TypeMatrixInteger);
                type->is_signed = col_type->is_signed;
            } else if (col_type->kind == TypeVectorFloat) {
                type = new_type(module, result_id, TypeMatrixFloat);
            }
            type->base_type = col_type;
            type->matrix.num_cols = op->optional[2];
            type->matrix.num_rows = col_type->count;
            type->count = type->matrix.num_cols * type->matrix.num_rows;
            type->element_size = col_type->element_size;
            
            if (id_has_decoration(module, result_id, 0, SpvDecorationColMajor)) {
                type->matrix.kind = MatrixColMajor;
            } else {
                type->matrix.kind = MatrixRowMajor;
            }
            
            break;
        }
        case SpvOpTypePointer: 
            type = new_type(module, result_id, TypePointer);
            type->base_type = spirv_module_type_by_id(module, op->optional[2]);
            type->element_size = sizeof(uint32_t);
            type->count = 1;
            break;

        case SpvOpTypeFunction:
            type = new_type(module, result_id, TypeFunction);
            type->function.return_type = spirv_module_type_by_id(module, op->optional[1]);
            for (int idx = 2; idx < op->op.length - 2; ++idx) {
                arr_push(type->function.parameter_types, spirv_module_type_by_id(module, op->optional[idx]));
            }
            break;
            
        case SpvOpTypeArray:
            type = new_type(module, result_id, TypeArray);
            type->base_type = spirv_module_type_by_id(module, op->optional[1]);
            type->count = spirv_module_constant_by_id(module, op->optional[2])->value.as_uint;
            type->element_size = type->base_type->element_size * type->base_type->count;
            break;
            
        case SpvOpTypeStruct:
            type = new_type(module, result_id, TypeStructure);
            type->count = 1;
            for (int idx = 1; idx < op->op.length - 1; ++idx) {
                Type *sub_type = spirv_module_type_by_id(module, op->optional[idx]);
                arr_push(type->structure.members, sub_type);
                type->element_size += sub_type->element_size * sub_type->count;
            }
            break;
    }

    if (type) {
        map_int_ptr_put(&module->types, op->optional[0], type);
    }
}

static inline bool is_opcode_constant(SPIRV_opcode *op) {
    return op->op.kind >= SpvOpConstantTrue && op->op.kind <= SpvOpSpecConstantOp;
}

static void handle_opcode_constant(SPIRV_module *module, SPIRV_opcode *op) {
    Constant *constant = NULL;
    uint32_t result_type = op->optional[0];
    uint32_t result_id = op->optional[1];

    switch (op->op.kind) {                                  // FIXME: support all types
        case SpvOpConstantTrue:
            constant = new_constant(module, spirv_module_type_by_id(module, result_type));
            constant->value.as_int = true;
            break;
        case SpvOpConstantFalse:
            constant = new_constant(module, spirv_module_type_by_id(module, result_type));
            constant->value.as_int = false;
            break;
        case SpvOpConstant:
            constant = new_constant(module, spirv_module_type_by_id(module, result_type));
            constant->value.as_int = (int32_t) op->optional[2];       // FIXME: wider types
            break;
        case SpvOpConstantComposite:
            constant = new_constant(module, spirv_module_type_by_id(module, result_type));
            size_t size = constant->type->count * constant->type->element_size;
            constant->value.as_int_array = mem_arena_allocate(&module->allocator, size);
            
            int8_t *dst = (int8_t *) constant->value.as_int_array;
            for (int32_t i = 2; i < op->op.length - 1; ++i) {
                Constant *src = map_int_ptr_get(&module->constants, op->optional[i]);
                size_t src_size = src->type->count * src->type->element_size;
                if (src->type->count == 1) {
                    memcpy(dst, &src->value.as_int, src_size);
                } else {
                    memcpy(dst, src->value.as_int_array, src_size);
                }
                dst += src_size;
            }
    }

    if (constant) {
        map_int_ptr_put(&module->constants, result_id, constant);
    }
}

static void variable_check_access_decorations(SPIRV_module *module, VariableAccess *access, uint32_t id, int32_t member_id) {

    SPIRV_opcode **decorations = decoration_ops_by_id(module, id, member_id);

    access->kind = VarAccessNone;
    access->index = -1;
    
    for (SPIRV_opcode **op = decorations; op != arr_end(decorations); ++op) {
        uint32_t dec_offset = ((*op)->op.kind == SpvOpDecorate) ? 1 : 2;
        uint32_t decoration = (*op)->optional[dec_offset];
        
        if (decoration == SpvDecorationBuiltIn) {
            access->kind = VarAccessBuiltIn;
            access->index = (int32_t) (*op)->optional[dec_offset+1];
        } else if (decoration == SpvDecorationLocation) {
            access->kind = VarAccessLocation;
            access->index = (int32_t) (*op)->optional[dec_offset+1];
        }
    }
}

static Variable *create_variable(SPIRV_module *module, uint32_t id, Type *type, uint32_t storage_class) {
    
    Variable *var = new_variable(module, id, type);
    var->kind = storage_class;
    
    // optional name that was defined earlier
    var->name = spirv_module_name_by_id(module, id, -1);

    // decorations
    variable_check_access_decorations(module, &var->access, id, -1);
    
    // aggregate types
    Type *aggregate_type = NULL;
    if (var->type->kind == TypeStructure) {
        aggregate_type = var->type;
    } else if (var->type->kind == TypePointer && var->type->base_type->kind == TypeStructure) {
        aggregate_type = var->type->base_type;
    }
    
    if (aggregate_type) {
        arr_reserve(var->member_access, arr_len(aggregate_type->structure.members));
        arr_reserve(var->member_name,  arr_len(aggregate_type->structure.members));

        for (int32_t i = 0; i < (int32_t) arr_len(aggregate_type->structure.members); ++i) {
            variable_check_access_decorations(module, &var->member_access[i], aggregate_type->id, i);
            var->member_name[i] = spirv_module_name_by_id(module, aggregate_type->id, i);
        }
    }
    
    define_sc_variable(module, var);

    return var;
}

static void handle_opcode_variable(SPIRV_module *module, SPIRV_opcode *op) {
    uint32_t var_type = op->optional[0];
    uint32_t var_id = op->optional[1];
    uint32_t storage_class = op->optional[2];
    
    Type *type = spirv_module_type_by_id(module, var_type);
    assert (type->kind == TypePointer);
    
    // create variable
    Variable *var = create_variable(
        module, var_id, 
        type,
        storage_class
    );
                                    
    // initializer - can be a constant or a (global) variable
    if (op->op.length == 5) {
        Constant *init_constant = spirv_module_constant_by_id(module, op->optional[3]);
        if (init_constant) {
            var->initializer_kind = InitializerConstant;
            var->initializer_constant = init_constant;
        } else {
            Variable *init_var = spirv_module_variable_by_id(module, op->optional[3]);
            if (init_var) {
                var->initializer_kind = InitializerVariable;
                var->initializer_variable = init_var;
            }
        }
    }

    // save to global id map
    map_int_ptr_put(&module->variables, var_id, var);
}

static void handle_opcode_function(SPIRV_module *module, SPIRV_opcode *op) {
    assert(module);
    assert(op);
    assert(op->op.kind == SpvOpFunction);
    assert(op->op.length == 5);

    // uint32_t result_type = op->optional[0];
    uint32_t func_id = op->optional[1];
    // uint32_t func_control = op->optional[2];
    uint32_t func_type = op->optional[3];

    SPIRV_function *func = new_function(module, spirv_module_type_by_id(module, func_type), func_id);

    // optional name that was defined earlier
    func->func.name = spirv_module_name_by_id(module, func_id, -1);

    // scan ahead to the first real instruction of the function
    func->fst_opcode = spirv_bin_opcode_next(module->spirv_bin);

    while (func->fst_opcode != NULL &&
           (func->fst_opcode->op.kind == SpvOpLabel ||
            func->fst_opcode->op.kind == SpvOpVariable ||
            func->fst_opcode->op.kind == SpvOpFunctionParameter)) {

        if (func->fst_opcode->op.kind == SpvOpVariable) {
            arr_push(func->func.variable_ids, func->fst_opcode->optional[1]);
        } else if (func->fst_opcode->op.kind == SpvOpFunctionParameter) {
            arr_push(func->func.parameter_ids, func->fst_opcode->optional[1]);
        }

        func->fst_opcode = spirv_bin_opcode_next(module->spirv_bin);
    }

    // scan ahead to the last real instruction of the function
    func->lst_opcode = func->fst_opcode;
    SPIRV_opcode *next = spirv_bin_opcode_next(module->spirv_bin);

    while (next && next->op.kind != SpvOpFunctionEnd) {
        func->lst_opcode = next;
        next = spirv_bin_opcode_next(module->spirv_bin);
    }

    spirv_bin_opcode_jump_to(module->spirv_bin, op);

    map_int_ptr_put(&module->functions, func_id, func);
}

static void handle_opcode_entrypoint(SPIRV_module *module, SPIRV_opcode *op) {
    assert(module);
    assert(op);
    assert(op->op.kind == SpvOpEntryPoint);
    assert(op->op.length >= 4);

    uint32_t execution_model = op->optional[0];
    uint32_t func_id = op->optional[1];

    EntryPoint ep = (EntryPoint) {
        .func_id = func_id,
        .kind = execution_model
    };
    arr_push(module->entry_points, ep);
}

static inline bool is_opcode_decoration(SPIRV_opcode *op) {
    return op->op.kind == SpvOpDecorate ||
           op->op.kind == SpvOpMemberDecorate;
}

static void handle_opcode_decoration(SPIRV_module *module, SPIRV_opcode *op) {
    uint32_t target = op->optional[0];
    int32_t member = (op->op.kind == SpvOpMemberDecorate) ? (int32_t) op->optional[1] : -1;
    uint64_t key = id_member_to_key(target, member);

    SPIRV_opcode **id_decs = map_int_ptr_get(&module->decorations, key);
    arr_push(id_decs, op);
    map_int_ptr_put(&module->decorations, key, id_decs);
}

void spirv_module_load(SPIRV_module *module, SPIRV_binary *binary) {
    assert(module);
    assert(binary);

    *module = (SPIRV_module) {
        .spirv_bin = binary
    };
    
    mem_arena_init(&module->allocator, ARENA_DEFAULT_SIZE, 4);

    for (SPIRV_opcode *op = spirv_bin_opcode_rewind(binary); op != spirv_bin_opcode_end(binary); op = spirv_bin_opcode_next(binary)) {
	
	    arr_push(module->opcode_array, op);

        if (op->op.kind == SpvOpExtInstImport) {
            define_extinst_set(module, op->optional[0], (const char *) &op->optional[1]);
        } else if (op->op.kind == SpvOpName) {
            define_name(module, 
                        op->optional[0], 
                        (const char *) &op->optional[1], -1);
        } else if (op->op.kind == SpvOpMemberName) {
            define_name(module, 
                        op->optional[0], 
                        (const char *) &op->optional[2],
                        (int32_t) op->optional[1]);
        } else if (is_opcode_type(op)) {
            handle_opcode_type(module, op);
        } else if (is_opcode_constant(op)) {
            handle_opcode_constant(module, op);
        } else if (op->op.kind == SpvOpVariable) {
            handle_opcode_variable(module, op);
        } else if (op->op.kind == SpvOpFunction) {
            handle_opcode_function(module, op);
        } else if (op->op.kind == SpvOpEntryPoint) {
            handle_opcode_entrypoint(module, op);
        } else if (is_opcode_decoration(op)) {
            handle_opcode_decoration(module, op);
        }
    }

    for (EntryPoint *ep = module->entry_points; ep != arr_end(module->entry_points); ++ep) {
        ep->function = spirv_module_function_by_id(module, ep->func_id);
    }
}

void spirv_module_free(SPIRV_module *module) {
    if (module) {
        for (int iter = map_begin(&module->functions); iter != map_end(&module->functions); iter = map_next(&module->functions, iter)) {
            SPIRV_function *func = map_val(&module->functions, iter);
            arr_free(func->func.parameter_ids);
            arr_free(func->func.variable_ids);
        }
        for (int iter = map_begin(&module->variables_sc); iter != map_end(&module->variables_sc); iter = map_next(&module->variables_sc, iter)) {
            Variable **var_array = map_val(&module->variables_sc, iter);
            arr_free(var_array);
        }
        for (int iter = map_begin(&module->variables); iter != map_end(&module->variables); iter = map_next(&module->variables, iter)) {
            Variable *var = map_val(&module->variables, iter);
            arr_free(var->member_access);
            arr_free(var->member_name);
        }
        for (int iter = map_begin(&module->decorations); iter != map_end(&module->decorations); iter = map_next(&module->decorations, iter)) {
            SPIRV_opcode **arr = map_val(&module->decorations, iter);
            arr_free(arr);
        }
        for (int iter = map_begin(&module->types); iter != map_end(&module->types); iter = map_next(&module->types, iter)) {
            Type *type = map_val(&module->types, iter);
            if (type->kind == TypeStructure) {
                arr_free(type->structure.members);
            } else if (type->kind == TypeFunction) {
                arr_free(type->function.parameter_types);
            }
        }

        mem_arena_free(&module->allocator);
        arr_free(module->opcode_array);
        map_free(&module->extinst_sets);
        map_free(&module->names);
        map_free(&module->decorations);
        map_free(&module->types);
        map_free(&module->constants);
        map_free(&module->variables);
        map_free(&module->variables_sc);
        map_free(&module->functions);
        arr_free(module->entry_points);
    }
}

size_t spirv_module_opcode_count(SPIRV_module *module) {
	assert(module);
	return arr_len(module->opcode_array);
}

SPIRV_opcode *spirv_module_opcode_by_index(SPIRV_module *module, uint32_t index) {
	assert(module);
	assert(index < arr_len(module->opcode_array));
	return module->opcode_array[index];
}

uint32_t spirv_module_index_for_opcode(SPIRV_module *module, struct SPIRV_opcode *op) {
	assert(module);
	assert(op);

	/* linear search, baby! */
	uint32_t index = 0; 
	while (module->opcode_array[index] < op && index < arr_len(module->opcode_array))
		++index;

	return index;
}

