// spirv_simulator.h - Johan Smet - BSD-3-Clause (see LICENSE)
//
// SPIR-V simulator

#ifndef JS_SHADER_SIM_SPIRV_SIMULATOR_H
#define JS_SHADER_SIM_SPIRV_SIMULATOR_H

#include "types.h"
#include "allocator.h"
#include "spirv_module.h"

// types
typedef struct SimPointer {
    Type *type;
    uint8_t *memory;
} SimPointer;

typedef struct SimVarMemory {
    Variable *var_desc;
    size_t size;
    uint8_t memory[];
} SimVarMemory;

typedef struct SimRegister {
    union {
        float *vec;
        int32_t *svec;
        uint32_t *uvec;
    };
    uint32_t id;
    Type *type;
} SimRegister;

#define SPIRV_SIM_MAX_TEMP_REGISTERS    4096

typedef struct SPIRV_simulator {
    SPIRV_module *module;
    HashMap var_memory;     // id (uint32_t) -> SimVarMemory *
    HashMap mem_pointers;   // id (uint32_t) -> SimPointer *
    HashMap intf_pointers;  // uint64_t -> SimPointer *

    SimRegister temp_regs[SPIRV_SIM_MAX_TEMP_REGISTERS];
    uint32_t reg_free_start;
    HashMap assigned_regs;  // SPIRV id (uint32_t) -> register index (uint32_t)
    MemArena reg_data;

    bool finished;
    char *error_msg;        // NULL if no error, dynamic string otherwise
} SPIRV_simulator;

void spirv_sim_init(SPIRV_simulator *sim, SPIRV_module *module);
void spirv_sim_variable_associate_data(
    SPIRV_simulator *sim, 
    VariableKind kind,
    VariableInterface if_type, 
    uint32_t if_index,
    uint8_t *data,
    size_t data_size
);
void spirv_sim_select_entry_point(SPIRV_simulator *sim, uint32_t index);
void spirv_sim_step(SPIRV_simulator *sim);

SimRegister *spirv_sim_register_by_id(SPIRV_simulator *sim, uint32_t id);
SimPointer *spirv_sim_retrieve_intf_pointer(SPIRV_simulator *sim, VariableKind kind, VariableInterface if_type, int32_t if_index);
size_t spirv_register_to_string(SPIRV_simulator *sim, uint32_t reg_idx, char *out_str, size_t out_max);


#endif // JS_SHADER_SIM_SPIRV_SIMULATOR_H
