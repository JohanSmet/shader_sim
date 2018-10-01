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

typedef struct Type {
    uint32_t id;
    TypeKind kind;
    int count;                  // number of elements
    int32_t element_size;       // in bytes
    bool is_signed;             // only for Integer/VectorInteger/MatrixInteger
    struct Type *base_type;     // only for Pointer/Array/Vector/Matrix
    union {
        struct {
            MatrixKind  kind;
            int32_t     num_rows;
            int32_t     num_cols;
        } matrix;
        struct {
            struct Type *return_type;
            struct Type **parameter_types;     // dyn_array
        } function;
        struct {
            struct Type **members;             // dyn_array
        } structure;
    };
} Type;

typedef union ConstantValue {
    int32_t as_int;
    float   as_float;
    int32_t *as_int_array;
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

typedef enum VariableKind {
    VarKindUniformConstant = 0,
    VarKindInput,
    VarKindUniform,
    VarKindOutput,
    VarKindWorkgroup,
    VarKindCrossWorkgroup,
    VarKindPrivate,
    VarKindFunction,
    VarKindGeneric,
    VarKindPushConstant,
    VarKindAtomicCounter,
    VarKindImage,
    VarKindStorageBuffer,
} VariableKind;

typedef enum VariableInterface {
    VarInterfaceNone = 0,
    VarInterfaceBuiltIn = 1,
    VarInterfaceLocation = 2,
} VariableInterface;

typedef struct Variable {
	uint32_t id;
	uint32_t member_id;
    Type *type;                 // actual type the variable is pointing to
    const char *name;           // optional
    uint32_t array_elements;
    VariableInitializerKind initializer_kind;
    union {
        Constant *initializer_constant;
        struct Variable *initializer_variable;
    };
    struct Variable **members;
    VariableKind kind;      // spirv: storage class
    VariableInterface if_type;
    uint32_t if_index;
} Variable;

typedef struct Function {
    uint32_t id;
    Type *type;
    const char *name;
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

    struct SPIRV_opcode **opcode_array;	// dyn_array

    HashMap names;          // id (int) -> const char *
    HashMap decorations;    // id (int) -> SPIRV_opcode ** (dyn_array)
    HashMap types;          // id (int) -> Type *
    HashMap constants;      // id (int) -> Constant *
    HashMap variables;      // id (int) -> Variable *
    HashMap variables_kind; // kind (StorageClass) -> Variable ** (dyn_array)
    HashMap functions;      // id (int) -> SPIRV_function *

    EntryPoint *entry_points;       // dyn_array
} SPIRV_module;

// interface functions
void spirv_module_load(SPIRV_module *module, struct SPIRV_binary *binary);
void spirv_module_free(SPIRV_module *module);

Type *spirv_module_type_by_id(SPIRV_module *module, uint32_t id);
Constant *spirv_module_constant_by_id(SPIRV_module *mpdule, uint32_t id);
uint32_t spirv_module_variable_count(SPIRV_module *module);
uint32_t spirv_module_variable_id(SPIRV_module *module, uint32_t index);
Variable *spirv_module_variable_by_id(SPIRV_module *mpdule, uint32_t id);
Variable *spirv_module_variable_by_intf(SPIRV_module *module, VariableKind kind, VariableInterface if_type, uint32_t if_index);

size_t spirv_module_opcode_count(SPIRV_module *module);
struct SPIRV_opcode *spirv_module_opcode_by_index(SPIRV_module *module, uint32_t index);
uint32_t spirv_module_index_for_opcode(SPIRV_module *module, struct SPIRV_opcode *op);

#endif // JS_SHADER_SIM_SPIRV_MODULE_H
