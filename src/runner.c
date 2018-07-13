// runner.h - Johan Smet - BSD-3-Clause (see LICENSE)

#include "runner.h"
#include "runner_lut.h"
#include "utils.h"
#include "dyn_array.h"
#include "spirv_simulator.h"

#include <stdlib.h>
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

RUNNER_FUNC_BEGIN(CmdAssociateInput)

    spirv_sim_variable_associate_data(
        runner->spirv_sim,
        cmd->var_kind,
        cmd->var_if_type, cmd->var_if_index,
        cmd->data, cmd->data_size);
    return true;

RUNNER_FUNC_END

RUNNER_FUNC_BEGIN(CmdAssociateOutput)

    spirv_sim_variable_associate_data(
        runner->spirv_sim,
        cmd->var_kind,
        cmd->var_if_type, cmd->var_if_index,
        cmd->data, cmd->data_size);

    return true;

RUNNER_FUNC_END

RUNNER_FUNC_BEGIN(CmdRun)

    while (!runner->spirv_sim->finished && !runner->spirv_sim->error_msg) {
        spirv_sim_step(runner->spirv_sim);
    }

    return true;

RUNNER_FUNC_END

RUNNER_FUNC_BEGIN(CmdStep)

    if (!runner->spirv_sim->finished && !runner->spirv_sim->error_msg) {
        spirv_sim_step(runner->spirv_sim);
    }
    return true;

RUNNER_FUNC_END

RUNNER_FUNC_BEGIN(CmdCmpOutput)

    VariableData *result = spirv_sim_retrieve_var(
        runner->spirv_sim, VarKindOutput,
        cmd->var_if_type, cmd->var_if_index);

    char *error_msg = NULL;

    if (!result) {
        fatal_error("no result found for that variable");
        return false;
    }

    switch (cmd->data_type) {
            
        case TypeVoid:
            break;
            
        case TypeBool: {
            bool actual = *((bool *) result->buffer);
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
            int32_t *actual = (int32_t *) result->buffer;
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
            float *actual = (float *) result->buffer;
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

static inline RunnerCmdAssociateInput *new_cmd_associate_input(void) {
    RunnerCmdAssociateInput *cmd = (RunnerCmdAssociateInput *) malloc(sizeof(RunnerCmdAssociateInput));
    cmd->base.kind = CmdAssociateInput;
    cmd->base.cmd_func = RunnerCmdAssociateInput_execute;
    cmd->var_kind = VarKindInput;
    return cmd;
}

static inline RunnerCmdAssociateOutput *new_cmd_associate_output(void) {
    RunnerCmdAssociateOutput *cmd = (RunnerCmdAssociateOutput *) malloc(sizeof(RunnerCmdAssociateOutput));
    cmd->base.kind = CmdAssociateOutput;
    cmd->base.cmd_func = RunnerCmdAssociateOutput_execute;
    cmd->var_kind = VarKindOutput;
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

static VariableInterface parse_var_interface_type(const char *if_type_str) {
    if (!strcmp(if_type_str, "builtin")) {
        return VarInterfaceBuiltIn;
    } else if (!strcmp(if_type_str, "location")) {
        return VarInterfaceLocation;
    } else {
        fatal_error("Unknown interface type");
        return 0;
    }
}

static int32_t parse_var_interface_index(VariableInterface if_type, const cJSON *json_data) {

    if (if_type == VarInterfaceLocation) {
        return json_int_value(json_data, "if_index", 0);
    } else if (if_type == VarInterfaceBuiltIn) {
        int32_t result;
        if (runner_lut_lookup_builtin(json_string_value(json_data, "if_index"), &result)) {
            return result;
        }
    }

    return 0;
}

static inline size_t allocate_data(TypeKind kind, int32_t elements, uint8_t **data) {
    assert(data);
    size_t data_size = 0;

    switch (kind) {
        case TypeInteger:
        case TypeVectorInteger:
        case TypeMatrixInteger:
            data_size = sizeof(int32_t);
            break;
        case TypeFloat:
        case TypeVectorFloat:
        case TypeMatrixFloat:
            data_size = sizeof(float);
            break;
        case TypePointer:
        case TypeFunction:
            data_size = sizeof(int32_t *);
            break;
        default:
            return data_size;
    }

    data_size *= elements;
    *data = (uint8_t *) calloc(1, data_size);
    return data_size;
}

static void parse_values(const cJSON *json_values, TypeKind data_type, int32_t elements, uint8_t *data) {
/* FIXME: support all types */

    if (data_type == TypeInteger && cJSON_IsNumber(json_values)) {
        *((int32_t *) data) = json_values->valueint;
    } else if (data_type == TypeFloat && cJSON_IsNumber(json_values)) {
        *((float *) data) = json_values->valuedouble;
    } else if (data_type == TypeVectorInteger && cJSON_IsArray(json_values)) {
        int32_t idx = 0;
        int32_t *int_data = (int32_t *) data;
        for (const cJSON *iter=json_values->child; iter && idx < elements; iter=iter->next) {
            int_data[idx++] = iter->valueint;
        }
    } else if (data_type == TypeVectorFloat && cJSON_IsArray(json_values)) {
        int32_t idx = 0;
        float *flt_data = (float *) data;
        for (const cJSON *iter=json_values->child; iter && idx < elements; iter=iter->next) {
            flt_data[idx++] = iter->valuedouble;
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

static RunnerCmd *parse_command(const char *cmd, const cJSON *json_data) {

    if (!strcmp(cmd, "associate_input")) {
        RunnerCmdAssociateInput *cmd = new_cmd_associate_input();
        TypeKind data_type = parse_data_type(json_string_value(json_data, "data_type"));
        int32_t elements = json_int_value(json_data, "elements", 0);

        cmd->var_if_type = parse_var_interface_type(json_string_value(json_data, "if_type"));
        cmd->var_if_index = parse_var_interface_index(cmd->var_if_type, json_data);
        cmd->data_size = allocate_data(data_type, elements, &cmd->data);

        const cJSON *values = cJSON_GetObjectItemCaseSensitive(json_data, "value");
        parse_values(values, data_type, elements, cmd->data);

        return (RunnerCmd *) cmd;
    } else if (!strcmp(cmd, "associate_output")) {
        RunnerCmdAssociateOutput *cmd = new_cmd_associate_output();
        TypeKind data_type = parse_data_type(json_string_value(json_data, "data_type"));
        int32_t elements = json_int_value(json_data, "elements", 0);

        cmd->var_if_type = parse_var_interface_type(json_string_value(json_data, "if_type"));
        cmd->var_if_index = parse_var_interface_index(cmd->var_if_type, json_data);
        cmd->data_size = allocate_data(data_type, elements, &cmd->data);

        return (RunnerCmd *) cmd;
    } else if (!strcmp(cmd, "run")) {
        RunnerCmdRun *cmd = new_cmd_run();
        return (RunnerCmd *) cmd;
    } else if (!strcmp(cmd, "step")) {
        RunnerCmdStep *cmd = new_cmd_step();
        return (RunnerCmd *) cmd;
    } else if (!strcmp(cmd, "cmp_output")) {
        RunnerCmdCmpOutput *cmd = new_cmd_cmp_output();
        int32_t elements = json_int_value(json_data, "elements", 0);

        cmd->op = parse_cmp_op(json_string_value(json_data, "operator"));
        cmd->data_type = parse_data_type(json_string_value(json_data, "data_type"));
        cmd->var_if_type = parse_var_interface_type(json_string_value(json_data, "if_type"));
        cmd->var_if_index = parse_var_interface_index(cmd->var_if_type, json_data);
        cmd->data_size = allocate_data(cmd->data_type, elements, &cmd->data);

        const cJSON *values = cJSON_GetObjectItemCaseSensitive(json_data, "value");
        parse_values(values, cmd->data_type, elements, cmd->data);

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

        RunnerCmd *cmd = parse_command(cmd_type->valuestring, json_cmd);
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
    spirv_sim_init(sim, &runner->spirv_module);

    for (RunnerCmd **iter = runner->commands; iter != arr_end(runner->commands); ++iter) {
        (*iter)->cmd_func(runner, *iter);
    }
}
