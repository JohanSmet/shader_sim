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

typedef struct SPIRV_stackframe {
    HashMap regs;             // SPIRV id (uint32_t) -> SimRegister *
    SPIRV_function *func;
    struct SPIRV_opcode *return_addr;
    uint32_t return_id;
    uint32_t heap_start;
    MemArena memory;
} SPIRV_stackframe;

typedef struct SPIRV_simulator {
    SPIRV_module *module;
    HashMap extinst_funcs;  // id (uint32_t) -> SPIRV_SIM_EXTINST_FUNC *
    
    uint8_t *memory;            // dyn_array
    uint32_t memory_free_start;

    HashMap intf_pointers;  // uint64_t -> SimPointer *
    EntryPoint *entry_point;

    /* stackframes */
    SPIRV_stackframe global_frame;
    SPIRV_stackframe *func_frames;      // dyn_array
    SPIRV_stackframe *current_frame;
    struct SPIRV_opcode *jump_to_op;

    bool finished;
    char *error_msg;        // NULL if no error, dynamic string otherwise
} SPIRV_simulator;

void spirv_sim_init(SPIRV_simulator *sim, SPIRV_module *module);
void spirv_sim_shutdown(SPIRV_simulator *sim);
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
void spirv_register_to_string(SPIRV_simulator *sim, SimRegister *reg, char **out_str);


#endif // JS_SHADER_SIM_SPIRV_SIMULATOR_H
