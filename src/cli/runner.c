// runner.h - Johan Smet - BSD-3-Clause (see LICENSE)

#include "runner.h"
#include "runner_lut.h"
#include "utils.h"
#include "dyn_array.h"
#include "spirv_simulator.h"
#include "spirv_text.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <cJSON/cJSON.h>

#define RUNNER_FUNC_BEGIN(kind) \
    static bool Runner ## kind ## _execute(Runner *runner, RunnerCmd *base_cmd) {    \
        assert(runner);    \
        assert(base_cmd);   \
        Runner##kind *cmd = (Runner##kind *) base_cmd; \
        (void) cmd;

#define RUNNER_FUNC_END   }

RUNNER_FUNC_BEGIN(CmdAssociateData)

    spirv_sim_variable_associate_data(
        runner->spirv_sim,
        cmd->var_kind,
        (VariableAccess) {cmd->var_if_type, cmd->var_if_index},
        cmd->data, cmd->data_size);
    return true;

RUNNER_FUNC_END

static void print_registers(SPIRV_simulator *sim, HashMap *reg_map) {
    char *reg_str = NULL;

    // FIXME: sort output by ID
    for (int iter = map_begin(reg_map); iter != map_end(reg_map); iter = map_next(reg_map, iter)) {
        spirv_register_to_string(sim, map_val(reg_map, iter), &reg_str);
        printf("%s\n", reg_str);
        arr_clear(reg_str);
    }

    arr_free(reg_str);
}

RUNNER_FUNC_BEGIN(CmdStep)

    if (!runner->spirv_sim->finished && !runner->spirv_sim->error_msg) {
        printf("Execute %s\n", spirv_text_opcode(spirv_bin_opcode_current(&runner->spirv_bin)));
        spirv_sim_step(runner->spirv_sim);

        print_registers(runner->spirv_sim, &runner->spirv_sim->global_frame.regs);
        if (runner->spirv_sim->current_frame != NULL && runner->spirv_sim->current_frame != & runner->spirv_sim->global_frame) {
            print_registers(runner->spirv_sim, &runner->spirv_sim->current_frame->regs);
        }
    }
    return true;

RUNNER_FUNC_END

RUNNER_FUNC_BEGIN(CmdRun)

    while (!runner->spirv_sim->finished && !runner->spirv_sim->error_msg) {
        RunnerCmdStep_execute(runner, base_cmd);
    }

return true;

RUNNER_FUNC_END

RUNNER_FUNC_BEGIN(CmdCmpOutput)

    SimPointer *result = spirv_sim_retrieve_intf_pointer(
        runner->spirv_sim, VarKindOutput,
        (VariableAccess) {cmd->var_if_type, cmd->var_if_index});

    char *error_msg = NULL;

    if (!result) {
        fatal_error("no result found for that variable");
        return false;
    }

    switch (cmd->data_type) {
            
        case TypeVoid:
            break;
            
        case TypeBool: {
            bool actual = *((bool *) (runner->spirv_sim->memory + result->pointer));
            bool expect = *((bool *) cmd->data);

            if (actual != expect) {
                arr_printf(error_msg, "Variable is %s should be %s\n",
                    (actual) ? "true" : "false",
                    (expect) ? "true" : "false"
                );
            }
            break;
        }

        case TypeInteger:
        case TypeVectorInteger:
        case TypeMatrixInteger: {
            int32_t *actual = (int32_t *) (runner->spirv_sim->memory + result->pointer);
            int32_t *expect = (int32_t *) cmd->data;
            int32_t count = cmd->data_size / sizeof(int32_t);

            for (int32_t i = 0; i < count; ++i) {
                if (actual[i] != expect[i]) {
                    arr_printf(error_msg, "Index [%d] is [%d], should be [%d]\n",
                        i, actual[i], expect[i]
                    );
                }
            }
           break;
        }

        case TypeFloat:
        case TypeVectorFloat:
        case TypeMatrixFloat: {
            float *actual = (float *) (runner->spirv_sim->memory + result->pointer);
            float *expect = (float *) cmd->data;
            int32_t count = cmd->data_size / sizeof(float);

            for (int32_t i = 0; i < count; ++i) {
                if (actual[i] != expect[i]) {
                    arr_printf(error_msg, "Index [%d] is [%f], should be [%f]\n",
                        i, actual[i], expect[i]
                    );
                }
            }
           break;
        }
            
        case TypePointer:
        case TypeFunction:
            break;
    }

    if (error_msg != NULL) {
        printf("CmpOutput: %s", error_msg);
        arr_free(error_msg);
    }

    return true;
RUNNER_FUNC_END

#undef RUNNER_FUNC_BEGIN
#undef RUNNER_FUNC_END

static inline RunnerCmdAssociateData *new_cmd_associate_data(void) {
    RunnerCmdAssociateData *cmd = (RunnerCmdAssociateData *) malloc(sizeof(RunnerCmdAssociateData));
    cmd->base.kind = CmdAssociateData;
    cmd->base.cmd_func = RunnerCmdAssociateData_execute;
    return cmd;
}

static inline RunnerCmdRun *new_cmd_run(void) {
    RunnerCmdRun *cmd = (RunnerCmdRun *) malloc(sizeof(RunnerCmdRun));
    cmd->base.kind = CmdRun;
    cmd->base.cmd_func = RunnerCmdRun_execute;
    return cmd;
}

static inline RunnerCmdStep *new_cmd_step(void) {
    RunnerCmdStep *cmd = (RunnerCmdStep *) malloc(sizeof(RunnerCmdStep));
    cmd->base.kind = CmdStep;
    cmd->base.cmd_func = RunnerCmdStep_execute;
    return cmd;
}

static inline RunnerCmdCmpOutput *new_cmd_cmp_output(void) {
    RunnerCmdCmpOutput *cmd = (RunnerCmdCmpOutput *) malloc(sizeof(RunnerCmdCmpOutput));
    cmd->base.kind = CmdCmpOutput;
    cmd->base.cmd_func = RunnerCmdCmpOutput_execute;
    return cmd;
}

static const char *json_string_value(const cJSON *json, const char *field) {
    assert(json);
    assert(field);

    const cJSON *field_json = cJSON_GetObjectItemCaseSensitive(json, field);
    if (field_json == NULL || !cJSON_IsString(field_json)) {
        return NULL;
    }

    return field_json->valuestring;
}

static int32_t json_int_value(const cJSON *json, const char *field, int32_t default_int) {

    assert(json);
    assert(field);

    const cJSON *field_json = cJSON_GetObjectItemCaseSensitive(json, field);
    if (field_json == NULL || !cJSON_IsNumber(field_json)) {
        return default_int;
    }

    return field_json->valueint;
}

static TypeKind parse_data_type(const char *data_type) {

    TypeKind result;

    if (runner_lut_lookup_datatype(data_type, &result)) {
        return result;
    } else {
        fatal_error("parse_data_type(): unknown type '%'", data_type);
        return 0;
    }
}

static VariableKind parse_var_kind(const char *kind_str) {
    if (!strcmp(kind_str, "input")) {
        return VarKindInput;
    } else if (!strcmp(kind_str, "uniform")) {
        return VarKindUniform;
    } else if (!strcmp(kind_str, "uniform_constant")) {
        return VarKindUniformConstant;
    } else {
        fatal_error("Unknown variable kind");
        return 0;
    }
}

static VariableAccessKind parse_var_interface_type(const char *if_type_str) {
    if (!strcmp(if_type_str, "builtin")) {
        return VarAccessBuiltIn;
    } else if (!strcmp(if_type_str, "location")) {
        return VarAccessLocation;
    } else {
        fatal_error("Unknown interface type");
        return 0;
    }
}

static int32_t parse_var_interface_index(VariableAccessKind if_type, const cJSON *json_data) {

    if (if_type == VarAccessLocation) {
        return json_int_value(json_data, "if_index", 0);
    } else if (if_type == VarAccessBuiltIn) {
        int32_t result;
        if (runner_lut_lookup_builtin(json_string_value(json_data, "if_index"), &result)) {
            return result;
        }
    }

    return 0;
}

static inline size_t allocate_data(Type *type, int32_t member, uint8_t **data) {
    assert(data);
    assert(type);
    
    if (type->kind == TypePointer) {
        type = type->base_type;
    }

    if (type->kind == TypeStructure && member != -1) {
        type = type->structure.members[member];
    }
    
    size_t data_size = type->element_size * type->count;
    *data = (uint8_t *) calloc(1, data_size);
    return data_size;
}

static void parse_values(const cJSON *json_values, Type *type, int32_t member, uint8_t *data) {
    
    if (type->kind == TypePointer) {
        type = type->base_type;
    }

    if (type->kind == TypeStructure && member != -1) {
        type = type->structure.members[member];
    }

    if (type->kind == TypeInteger && cJSON_IsNumber(json_values)) {
        *((int32_t *) data) = json_values->valueint;
    } else if (type->kind == TypeFloat && cJSON_IsNumber(json_values)) {
        *((float *) data) = json_values->valuedouble;
    } else if ((type->kind == TypeVectorInteger || type->kind == TypeMatrixInteger) && cJSON_IsArray(json_values)) {
        int32_t idx = 0;
        int32_t *int_data = (int32_t *) data;
        for (const cJSON *iter=json_values->child; iter && idx < type->count; iter=iter->next) {
            int_data[idx++] = iter->valueint;
        }
    } else if ((type->kind == TypeVectorFloat || type->kind == TypeMatrixFloat) && cJSON_IsArray(json_values)) {
        int32_t idx = 0;
        float *flt_data = (float *) data;
        for (const cJSON *iter=json_values->child; iter && idx < type->count; iter=iter->next) {
            flt_data[idx++] = iter->valuedouble;
        }
    } else if (type->kind == TypeArray) {
        uint8_t *ptr = data;
        int32_t idx = 0;
        for (const cJSON *iter=json_values->child; iter && idx < type->count; iter=iter->next) {
            parse_values(iter, type->base_type, -1, ptr);
            ptr += type->element_size;
            ++idx;
        }
    } else if (type->kind == TypeStructure) {
        uint8_t *ptr = data;
        int32_t idx = 0;
        
        for (const cJSON *iter=json_values->child; iter && idx < type->count; iter=iter->next) {
            Type *mem_type = type->structure.members[idx];
            parse_values(iter, type, idx++, ptr);
            ptr += mem_type->element_size * mem_type->count;
        }
    }
}

static RunnerCmpOp parse_cmp_op(const char *op_str) {

    RunnerCmpOp result;

    if (runner_lut_lookup_cmp_op(op_str, &result)) {
        return result;
    } else {
        fatal_error("%s: unknown comparison operator '%s'", __FUNCTION__, op_str);
        return 0;
    }
}

static RunnerCmd *parse_command(Runner *runner, const char *cmd, const cJSON *json_data) {

    Variable *var = NULL;
    int32_t var_member = 0;

    if (!strcmp(cmd, "associate_data")) {
        RunnerCmdAssociateData *cmd = new_cmd_associate_data();
        cmd->var_kind = parse_var_kind(json_string_value(json_data, "kind"));
        cmd->var_if_type = parse_var_interface_type(json_string_value(json_data, "if_type"));
        cmd->var_if_index = parse_var_interface_index(cmd->var_if_type, json_data);
        
        if (!spirv_module_variable_by_access(
                &runner->spirv_module,
                cmd->var_kind, 
                (VariableAccess) {cmd->var_if_type, cmd->var_if_index},
                &var, &var_member)) {
            fatal_error("Unknown variable (%d/%d/%d)", cmd->var_kind, cmd->var_if_type, cmd->var_if_index);
            return 0;
        }
        
        cmd->data_size = allocate_data(var->type, var_member, &cmd->data);   
        
        const cJSON *values = cJSON_GetObjectItemCaseSensitive(json_data, "value");
        parse_values(values, var->type, var_member, cmd->data);

        return (RunnerCmd *) cmd;
    } else if (!strcmp(cmd, "run")) {
        RunnerCmdRun *cmd = new_cmd_run();
        return (RunnerCmd *) cmd;
    } else if (!strcmp(cmd, "step")) {
        RunnerCmdStep *cmd = new_cmd_step();
        return (RunnerCmd *) cmd;
    } else if (!strcmp(cmd, "cmp_output")) {
        RunnerCmdCmpOutput *cmd = new_cmd_cmp_output();

        cmd->op = parse_cmp_op(json_string_value(json_data, "operator"));
        cmd->data_type = parse_data_type(json_string_value(json_data, "data_type"));
        cmd->var_if_type = parse_var_interface_type(json_string_value(json_data, "if_type"));
        cmd->var_if_index = parse_var_interface_index(cmd->var_if_type, json_data);
        
        if (!spirv_module_variable_by_access(
                &runner->spirv_module,
                VarKindOutput, 
                (VariableAccess) {cmd->var_if_type, cmd->var_if_index},
                &var, &var_member)) {
            fatal_error("Unknown output variable (%d/%d)", cmd->var_if_type, cmd->var_if_index);
            return 0;
        }
        
        cmd->data_size = allocate_data(var->type, var_member, &cmd->data);

        const cJSON *values = cJSON_GetObjectItemCaseSensitive(json_data, "value");
        parse_values(values, var->type, var_member, cmd->data);

        return (RunnerCmd *) cmd;
    }

    return NULL;
}

bool runner_init(Runner *runner, const char *filename) {
    assert(runner);
    assert(filename);

    runner_lut_init();

    /* load JSON runner config */
    int8_t *json_data = NULL;
    size_t json_size = file_load_text(filename, &json_data);

    if (json_size == 0) {
        return false;
    }

    /* parse JSON config */
    cJSON *json = cJSON_Parse((char *) json_data);

    if (json == NULL) {
        /* FIXME: handle JSON error */
        goto end;
    }

    /* language */
    const cJSON *lang = cJSON_GetObjectItemCaseSensitive(json, "language");

    if (lang == NULL) {
        fatal_error("runner_init(): missing language property");
    } else if (!cJSON_IsString(lang)) {
        fatal_error("runner_init(): language property should be a string");
    } else if (!strcmp(lang->valuestring, "spirv")) {
        runner->language = LangSpirV;
    } else {
        fatal_error("runner_init(): '%s' is not a valid language", lang->valuestring);
    }

    /* shader file */
    const cJSON *file = cJSON_GetObjectItemCaseSensitive(json, "file");

    if (file == NULL) {
        fatal_error("runner_init(): missing file property");
    } else if (!cJSON_IsString(file)) {
        fatal_error("runner_init(): file property should be a string");
    } else {
        /* file path is relative to the runner config file */
        /* FIXME: this is shady AF because we don't impose a max length on the filename */
        char full_path[2048];
        path_dirname(filename, full_path);
        path_append(full_path, file->valuestring);

        int8_t *binary_data = NULL;

        if (file_load_binary(full_path, &binary_data) == 0) {
            fatal_error("runner_init(): unable to load shader '%s'", file->valuestring);
        }

        if (!spirv_bin_load(&runner->spirv_bin, binary_data)) {
            goto end;
        }

        spirv_module_load(&runner->spirv_module, &runner->spirv_bin);
    }

    /* commands */
    const cJSON *commands = cJSON_GetObjectItemCaseSensitive(json, "commands");

    if (commands == NULL) {
        fatal_error("runner_init(): no commands?");
    } else if (!cJSON_IsArray(commands)) {
        fatal_error("runner_init(): commands property should be an array");
    }

    for (const cJSON *json_cmd = commands->child; json_cmd != NULL; json_cmd = json_cmd->next) {
        const cJSON *cmd_type = cJSON_GetObjectItemCaseSensitive(json_cmd, "command");

        if (cmd_type == NULL || !cJSON_IsString(cmd_type)) {
            continue;
        }

        RunnerCmd *cmd = parse_command(runner, cmd_type->valuestring, json_cmd);
        arr_push(runner->commands, cmd);
    }

end:
    cJSON_Delete(json);
    file_free(json_data);
    return true;
}

void runner_execute(Runner *runner, struct SPIRV_simulator *sim) {
    assert(runner);
    assert(sim);

    runner->spirv_sim = sim;
    spirv_sim_init(sim, &runner->spirv_module, SPIRV_SIM_DEFAULT_ENTRYPOINT);

    for (RunnerCmd **iter = runner->commands; iter != arr_end(runner->commands); ++iter) {
        (*iter)->cmd_func(runner, *iter);
    }
}
