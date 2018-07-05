// spirv_module.h - Johan Smet - BSD-3-Clause (see LICENSE)

#ifndef JS_SHADER_SIM_SPIRV_MODULE_H
#define JS_SHADER_SIM_SPIRV_MODULE_H

#include "hash_map.h"
#include "types.h"

// required  forward declarations
typedef struct SPIRV_binary SPIRV_binary;
typedef struct SPIRV_opcode SPIRV_opcode;

// types
typedef struct IdName {
    const char *name;
    int member_index;
} IdName;

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
    TypeFunction
} TypeKind;

typedef struct Type {
    TypeKind kind;
    int count;          // number of elements (vector), number of columns (matrix)
    union {
        struct {
            int32_t size;   // in bytes
            bool is_signed;
        };
        struct {
            // XXX: storage class
            struct Type *base_type;
        } pointer;
        struct {
            struct Type *return_type;
            struct Type **parameter_types;     // dyn_array
        } function;
    };
} Type;

typedef union ConstantValue {
    int32_t as_int;
    float   as_float;
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
    VarInterfaceBuiltIn = 0,
    VarInterfaceLocation = 1,
} VariableInterface;

typedef struct Variable {
    Type *type;
    const char *name;
    VariableInitializerKind initializer_kind;
    union {
        Constant *initializer_constant;
        struct Variable *initializer_variable;
    };
    VariableKind kind;      // spirv: storage class

    union {
        struct {
            uint32_t index;
            uint32_t type;
        };
        uint64_t key;
    } interface;
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
    SPIRV_opcode *fst_opcode;
    SPIRV_opcode *lst_opcode;
} SPIRV_function;

typedef struct EntryPoint {
    uint32_t func_id;
    SPIRV_function *function;
    ProgramKind kind;
} EntryPoint;

typedef struct SPIRV_module {
    SPIRV_binary *spirv_bin;

    HashMap names;          // id (int) -> IdName *
    HashMap decorations;    // id (int) -> SPIRV_opcode ** (dyn_array)
    HashMap types;          // id (int) -> Type *
    HashMap constants;      // id (int) -> Constant *
    HashMap variables;      // id (int) -> Variable *
    HashMap variables_kind; // kind (StorageClass) -> Variable ** (dyn_array)
    HashMap functions;      // id (int) -> SPIRV_function *

    EntryPoint *entry_points;       // dyn_array
} SPIRV_module;

// interface functions
void spirv_module_load(SPIRV_module *module, SPIRV_binary *binary);

Type *spirv_module_type_by_id(SPIRV_module *module, uint32_t id);

#endif // JS_SHADER_SIM_SPIRV_MODULE_H