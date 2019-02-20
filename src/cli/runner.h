// runner.h - Johan Smet - BSD-3-Clause (see LICENSE)
//
// Load and execute runner files

#ifndef JS_SHADER_SIM_RUNNER_H
#define JS_SHADER_SIM_RUNNER_H

#include "spirv_module.h"
#include "spirv_binary.h"

// forward declarations
struct Runner;
struct RunnerCmd;
struct SPIRV_simulator;

typedef enum RunnerLanguage {
    LangSpirV
} RunnerLanguage;

// types
typedef enum RunnerCmdKind {
    CmdAssociateData,
    CmdRun,
    CmdStep,
    CmdCmpOutput
} RunnerCmdKind;

typedef enum RunnerCmpOp {
    CMP_EQ,
    CMP_NEQ,
    CMP_LT,
    CMP_GT,
    CMP_LTEQ,
    CMP_GTEQ
} RunnerCmpOp;

typedef bool (*RUNNER_CMD_FUNC)(struct Runner *runner, struct RunnerCmd *cmd);

typedef struct RunnerCmd {
    RunnerCmdKind   kind;
    RUNNER_CMD_FUNC cmd_func;
} RunnerCmd;

typedef struct RunnerCmdAssociateData {
    RunnerCmd base;
    StorageClass storage_class;
    VariableAccessKind var_if_type;
    uint32_t var_if_index;
    uint8_t *data;
    size_t data_size;
} RunnerCmdAssociateData;

typedef struct RunnerCmdRun {
    RunnerCmd base;
} RunnerCmdRun;

typedef struct RunnerCmdStep {
    RunnerCmd base;
} RunnerCmdStep;

typedef struct RunnerCmdCmpOutput {
    RunnerCmd base;
    RunnerCmpOp op;
    TypeKind data_type;
    VariableAccessKind var_if_type;
    uint32_t var_if_index;
    uint8_t *data;
    size_t data_size;
} RunnerCmdCmpOutput;

typedef struct Runner {
    RunnerLanguage  language;
    SPIRV_binary spirv_bin;
    SPIRV_module spirv_module;
    RunnerCmd **commands;       // dyn_array

    struct SPIRV_simulator *spirv_sim;
} Runner;

// interface functions
bool runner_init(Runner *runner, const char *filename);
void runner_execute(Runner *runner, struct SPIRV_simulator *sim);

#endif // JS_SHADER_SIM_RUNNER_H
