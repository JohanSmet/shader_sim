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
    uint32_t pointer;
} SimPointer;

typedef struct SimRegister {
    union {
        float *vec;
        int32_t *svec;
        uint32_t *uvec;
        uint8_t *raw;
    };
    uint32_t id;
    Type *type;
} SimRegister;

#define SPIRV_SIM_MAX_TEMP_REGISTERS    4096

typedef struct SPIRV_simulator {
    SPIRV_module *module;
    
    uint8_t *memory;            // dyn_array
    uint32_t memory_free_start;

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
    VariableAccess access, 
    uint8_t *data,
    size_t data_size
);
void spirv_sim_select_entry_point(SPIRV_simulator *sim, uint32_t index);
void spirv_sim_step(SPIRV_simulator *sim);

SimRegister *spirv_sim_register_by_id(SPIRV_simulator *sim, uint32_t id);
SimPointer *spirv_sim_retrieve_intf_pointer(SPIRV_simulator *sim, VariableKind kind, VariableAccess access);
size_t spirv_register_to_string(SPIRV_simulator *sim, uint32_t reg_idx, char *out_str, size_t out_max);


#endif // JS_SHADER_SIM_SPIRV_SIMULATOR_H
