// spirv_module.h - Johan Smet - BSD-3-Clause (see LICENSE)

#ifndef JS_SHADER_SIM_SPIRV_MODULE_H
#define JS_SHADER_SIM_SPIRV_MODULE_H

#include "hash_map.h"
#include "types.h"
#include "allocator.h"

// required  forward declarations
struct SPIRV_binary;
struct SPIRV_opcode;

// types
typedef enum TypeKind {
    TypeVoid,
    TypeBool,
    TypeInteger,
    TypeFloat,
    TypeVectorInteger,
    TypeVectorFloat,
    TypeMatrixInteger,
    TypeMatrixFloat,
    TypePointer,
    TypeFunction,
    TypeArray,
    TypeStructure
} TypeKind;

typedef enum MatrixKind {
    MatrixRowMajor,
    MatrixColMajor
} MatrixKind;

typedef enum StorageClass {
    ClassUniformConstant = 0,
    ClassInput,
    ClassUniform,
    ClassOutput,
    ClassWorkgroup,
    ClassCrossWorkgroup,
    ClassPrivate,
    ClassFunction,
    ClassGeneric,
    ClassPushConstant,
    ClassAtomicCounter,
    ClassImage,
    ClassStorageBuffer,
} StorageClass;

typedef struct Type {
    uint32_t id;
    TypeKind kind;
    uint32_t count;             // number of elements
    uint32_t element_size;      // in bytes
    bool is_signed;             // only for Integer/VectorInteger/MatrixInteger
    struct Type *base_type;     // only for Pointer/Array/Vector/Matrix
    union {
        struct {
            MatrixKind  kind;
            uint32_t    num_rows;
            uint32_t    num_cols;
        } matrix;
        struct {
            struct Type *return_type;
            struct Type **parameter_types;     // dyn_array
        } function;
        struct {
            struct Type **members;             // dyn_array
        } structure;
        struct {
            StorageClass storage_class;
        } pointer;
    };
} Type;

typedef union ConstantValue {
    int32_t as_int;
    uint32_t as_uint;
    float   as_float;
    int32_t *as_int_array;
    uint32_t *as_uint_array;
    float   *as_float_array;
} ConstantValue;

typedef struct Constant {
    Type *type;
    ConstantValue value;   
} Constant;

typedef enum VariableInitializerKind {
    InitializerNone,
    InitializerConstant,
    InitializerVariable
} VariableInitializerKind;

typedef enum VariableAccessKind {
    VarAccessNone = 0,
    VarAccessBuiltIn = 1,
    VarAccessLocation = 2,
} VariableAccessKind;

typedef struct VariableAccess {
    VariableAccessKind kind;
    int32_t index;
} VariableAccess;

typedef struct Variable {
	uint32_t id;
    Type *type;                 // actual type the variable is pointing to
    const char *name;           // optional
    uint32_t array_elements;
    VariableInitializerKind initializer_kind;
    union {
        Constant *initializer_constant;
        struct Variable *initializer_variable;
    };
    StorageClass kind; // spirv: storage class
    VariableAccess access;

    VariableAccess *member_access;  // dyn_array
    const char **member_name;       // dyn_array
} Variable;

typedef struct Function {
    uint32_t id;
    Type *type;
    const char *name;
    uint32_t *parameter_ids;        // dyn_array
    uint32_t *variable_ids;         // dyn_array
} Function;

typedef enum ProgramKind {
    VertexProgram,
    FragmentProgram,
    GeometryProgram,
    ComputeProgram
} ProgramKind;

typedef struct SPIRV_function {
    Function func;
    struct SPIRV_opcode *fst_opcode;
    struct SPIRV_opcode *lst_opcode;
} SPIRV_function;

typedef struct EntryPoint {
    uint32_t func_id;
    SPIRV_function *function;
    ProgramKind kind;
} EntryPoint;

typedef struct SPIRV_module {
    struct SPIRV_binary *spirv_bin;
    MemArena allocator;

    struct SPIRV_text   *text;
    struct SPIRV_opcode **opcode_array;	// dyn_array

    HashMap extinst_sets;   // id (int) -> const char *
    HashMap names;          // id (int) -> const char *
    HashMap decorations;    // id (int) -> SPIRV_opcode ** (dyn_array)
    HashMap types;          // id (int) -> Type *
    HashMap constants;      // id (int) -> Constant *
    HashMap variables;      // id (int) -> Variable *
    HashMap variables_sc;   // kind (StorageClass) -> Variable ** (dyn_array)
    HashMap functions;      // id (int) -> SPIRV_function *
    HashMap labels;         // id (int) -> SPIRV_opcode *

    EntryPoint *entry_points;       // dyn_array
} SPIRV_module;

// interface functions
void spirv_module_load(SPIRV_module *module, struct SPIRV_binary *binary);
void spirv_module_free(SPIRV_module *module);

Type *spirv_module_type_by_id(SPIRV_module *module, uint32_t id);
const char *spirv_module_name_by_id(SPIRV_module *module, uint32_t id, int32_t member);
Constant *spirv_module_constant_by_id(SPIRV_module *mpdule, uint32_t id);
uint32_t spirv_module_variable_count(SPIRV_module *module);
uint32_t spirv_module_variable_id(SPIRV_module *module, uint32_t index);
Variable *spirv_module_variable_by_id(SPIRV_module *mpdule, uint32_t id);
bool spirv_module_variable_by_access(
    SPIRV_module *module, 
    StorageClass kind, 
    VariableAccess access,
    Variable **ret_var, 
    int32_t *ret_member); 

SPIRV_function *spirv_module_function_by_id(SPIRV_module *module, uint32_t id);

size_t spirv_module_opcode_count(SPIRV_module *module);
struct SPIRV_opcode *spirv_module_opcode_by_index(SPIRV_module *module, uint32_t index);
uint32_t spirv_module_index_for_opcode(SPIRV_module *module, struct SPIRV_opcode *op);

struct SPIRV_opcode *spirv_module_opcode_by_label(SPIRV_module *module, uint32_t label_id);

static inline bool spirv_type_is_integer(Type *type) {
    return type->kind == TypeInteger ||
           type->kind == TypeVectorInteger ||
           type->kind == TypeMatrixInteger;
}

static inline bool spirv_type_is_signed_integer(Type *type) {
    return spirv_type_is_integer(type ) && type->is_signed;
}

static inline bool spirv_type_is_unsigned_integer(Type *type) {
    return spirv_type_is_integer(type ) && !type->is_signed;
}

static inline bool spirv_type_is_float(Type *type) {
    return type->kind == TypeFloat ||
           type->kind == TypeVectorFloat ||
           type->kind == TypeMatrixFloat;
}

static inline bool spirv_type_is_scalar(Type *type) {
   return (type->kind == TypeInteger || type->kind == TypeFloat || type->kind == TypeBool) &&
          type->count == 1;
}

static inline bool spirv_type_is_vector(Type *type) {
    return type->kind == TypeVectorInteger || type->kind == TypeVectorFloat;
}

static inline bool spirv_type_is_matrix(Type *type) {
    return type->kind == TypeMatrixInteger || type->kind == TypeMatrixFloat;
}


#endif // JS_SHADER_SIM_SPIRV_MODULE_H
