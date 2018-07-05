// spirv_simulator.h - Johan Smet - BSD-3-Clause (see LICENSE)
//
// SPIR-V simulator

#ifndef JS_SHADER_SIM_SPIRV_SIMULATOR_H
#define JS_SHADER_SIM_SPIRV_SIMULATOR_H

#include "types.h"
#include "spirv_module.h"

// types
typedef struct VariableData {
    uint8_t *buffer;
    size_t size;
} VariableData;

typedef union SimRegister {
    float vec[4];
    struct {
        float x;
        float y;
        float z;
        float w;
    };
} SimRegister;

#define SPIRV_SIM_MAX_TEMP_REGISTERS    4096

typedef struct SPIRV_simulator {
    SPIRV_module *module;
    HashMap variable_data;  // uint64_t -> VariableData

    SimRegister temp_regs[SPIRV_SIM_MAX_TEMP_REGISTERS];
    uint32_t reg_free_start;

    HashMap assigned_regs;  // SPIRV id (uint32_t) -> register index (uint32_t)
    HashMap object_types;   // SPIRV id (uint32_t) -> Type *

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


#endif // JS_SHADER_SIM_SPIRV_SIMULATOR_H