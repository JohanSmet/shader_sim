// spirv_module.c - Johan Smet - BSD-3-Clause (see LICENSE)

#include "spirv_module.h"
#include "spirv_binary.h"
#include "spirv/spirv.h"

#include "dyn_array.h"

#include <assert.h>
#include <stdlib.h>

#include <stdio.h>

static IdName *new_id_name(const char *name, int member_index) {
    IdName *result = malloc(sizeof(IdName));
    result->name = name;
    result->member_index = member_index;
    return result;
}

static Type *new_type(TypeKind kind) {
    Type *result = malloc(sizeof(Type));
    result->kind = kind;
    return result;
}

static Constant *new_constant(Type *type) {
    Constant *result = malloc(sizeof(Constant));
    result->type = type;
    return result;
}

static Variable *new_variable(Type *type) {
    Variable *result = malloc(sizeof(Variable));
    result->type = type;
    result->name = NULL;
    result->initializer_kind = InitializerNone;
    result->initializer_variable = NULL;
    return result;
}

static Type *type_by_id(SPIRV_module *module, uint32_t id) {
    return map_int_ptr_get(&module->types, id);
}

static IdName *name_by_id(SPIRV_module *module, uint32_t id) {
    return map_int_ptr_get(&module->names, id);
}

static Constant *constant_by_id(SPIRV_module *module, uint32_t id) {
    return map_int_ptr_get(&module->constants, id);
}

static Variable *variable_by_id(SPIRV_module *module, uint32_t id) {
    return map_int_ptr_get(&module->variables, id);
}

static void define_name(SPIRV_module *module, uint32_t id, const char *name, int member_index) {
    map_int_ptr_put(&module->names, id, new_id_name(name, member_index));
}

static inline bool is_opcode_type(SPIRV_opcode *op) {
    return op->op.kind >= SpvOpTypeVoid && op->op.kind <= SpvOpTypeForwardPointer;
}

static void handle_opcode_type(SPIRV_module *module, SPIRV_opcode *op) {
    Type *type = NULL;

    switch (op->op.kind) {                                  // FIXME: support all types
        case SpvOpTypeVoid:
            type = new_type(TypeVoid);
            break;
        case SpvOpTypeBool:
            type = new_type(TypeBool);
            break;
        case SpvOpTypeInt:
            type = new_type(TypeInteger);
            break;
        case SpvOpTypeFloat:
            type = new_type(TypeFloat);
            break;
        case SpvOpTypeVector: {
            Type *base_type = type_by_id(module, op->optional[1]);
            if (base_type->kind == TypeInteger) {
                type = new_type(TypeVectorInteger);
                type->count = op->optional[2];
            } else if (base_type->kind == TypeFloat) {
                type = new_type(TypeVectorFloat);
                type->count = op->optional[2];
            }
            break;
        }
        case SpvOpTypeMatrix: {
            Type *base_type = type_by_id(module, op->optional[1]);
            if (base_type->kind == TypeVectorInteger) {
                type = new_type(TypeMatrixInteger);
                type->count = op->optional[2];
            } else if (base_type->kind == TypeVectorFloat) {
                type = new_type(TypeMatrixFloat);
                type->count = op->optional[2];
            }
            break;
        }
        case SpvOpTypePointer: 
            type = new_type(TypePointer);
            type->pointer.base_type = type_by_id(module, op->optional[2]);
            break;

        case SpvOpTypeFunction:
            type = new_type(TypeFunction);
            type->function.return_type = type_by_id(module, op->optional[1]);
            for (int idx = 2; idx < op->op.length - 2; ++idx) {
                arr_push(type->function.parameter_types, type_by_id(module, op->optional[idx]));
            }
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

    switch (op->op.kind) {                                  // FIXME: support all types
        case SpvOpConstantTrue:
            constant = new_constant(type_by_id(module, op->optional[0]));
            constant->value.as_int = true;
            break;
        case SpvOpConstantFalse:
            constant = new_constant(type_by_id(module, op->optional[0]));
            constant->value.as_int = true;
            break;
        case SpvOpConstant:
            constant = new_constant(type_by_id(module, op->optional[0]));
            constant->value.as_int = op->optional[2];       // FIXME: wider types
            break;
    }

    if (constant) {
        map_int_ptr_put(&module->constants, op->optional[1], constant);
    }

}

static void handle_opcode_variable(SPIRV_module *module, SPIRV_opcode *op) {
    uint32_t var_id = op->optional[1];
    Variable *var = new_variable(type_by_id(module, op->optional[0]));

    // optional name that was defined earlier
    IdName *name = name_by_id(module, var_id);
    if (name) {
        var->name = name->name;
    }

    // FIXME: handle storage class

    // initializer - can be a constant or a (global) variable
    if (op->op.length == 5) {
        Constant *constant = constant_by_id(module, op->optional[3]);
        if (constant) {
            var->initializer_kind = InitializerConstant;
            var->initializer_constant = constant;
        } else {
            Variable *var = variable_by_id(module, op->optional[3]);
            if (var) {
                var->initializer_kind = InitializerVariable;
                var->initializer_variable = var;
            }
        }
    }

    map_int_ptr_put(&module->variables, var_id, var);
}

void spirv_module_load(SPIRV_module *module, SPIRV_binary *binary) {
    assert(module);
    assert(binary);

    *module = (SPIRV_module) {0};
    module->spirv_bin = binary;

    for (SPIRV_opcode *op = spirv_bin_opcode_rewind(binary); op != spirv_bin_opcode_end(binary); op = spirv_bin_opcode_next(binary)) {
        if (op->op.kind == SpvOpName) {
            define_name(module, 
                        op->optional[0], 
                        (const char *) &op->optional[1], -1);
        } else if (op->op.kind == SpvOpMemberName) {
            define_name(module, 
                        op->optional[0], 
                        (const char *) &op->optional[2],
                        op->optional[1]);
        } else if (is_opcode_type(op)) {
            handle_opcode_type(module, op);
        } else if (is_opcode_constant(op)) {
            handle_opcode_constant(module, op);
        } else if (op->op.kind == SpvOpVariable) {
            handle_opcode_variable(module, op);
        }
    }
}

void spirv_module_dump_info(SPIRV_module *module) {
    assert(module);
    assert(module->spirv_bin);

    printf(" ********************************************* \n");
    printf("names: %ld\n", map_len(&module->names));
    printf("types: %ld\n", map_len(&module->types));
    printf("constants: %ld\n", map_len(&module->constants));
    printf("variables: %ld\n", map_len(&module->variables));
}