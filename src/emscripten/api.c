#include <emscripten.h>
#include <stdlib.h>

#include "types.h"
#include "utils.h"
#include "dyn_array.h"
#include "spirv_binary.h"
#include "spirv_module.h"
#include "spirv_simulator.h"
#include "spirv_text.h"
#include "lut.h"


typedef struct SimApiContext {
	SPIRV_binary spirv_bin;
	SPIRV_module spirv_module;
    SPIRV_simulator spirv_sim;
} SimApiContext;

EMSCRIPTEN_KEEPALIVE
SimApiContext *simapi_create_context(void) {
	SimApiContext *context = (SimApiContext *) malloc(sizeof(SimApiContext));
	lut_init();
	return context;
}

EMSCRIPTEN_KEEPALIVE
void simapi_release_context(SimApiContext *context) {
	if (context) {
		free(context);
	}
}

EMSCRIPTEN_KEEPALIVE
bool simapi_spirv_load_binary(SimApiContext *context, const int8_t *binary_data, size_t length) {
	/* spirv_bin_load expects an dyn_array ... */
	int8_t *blob = NULL;
	arr_push_buf(blob, binary_data, length);

	if (!spirv_bin_load(&context->spirv_bin, blob)) {
		arr_free(blob);
		printf("%s\n", context->spirv_bin.error_msg);
		return false;
	}

	spirv_module_load(&context->spirv_module, &context->spirv_bin);

	spirv_sim_init(&context->spirv_sim, &context->spirv_module);
	return true;
}

EMSCRIPTEN_KEEPALIVE
bool simapi_spirv_load_file(SimApiContext *context, const char *filename) {
	int8_t *binary_data = NULL;

    if (file_load_binary(filename, &binary_data) == 0) {
		return false;
	}

	if (!spirv_bin_load(&context->spirv_bin, (int8_t *)binary_data)) {
		printf("%s\n", context->spirv_bin.error_msg);
		return false;
	}

	spirv_module_load(&context->spirv_module, &context->spirv_bin);

	spirv_sim_init(&context->spirv_sim, &context->spirv_module);
	return true;
}

EMSCRIPTEN_KEEPALIVE
void simapi_spirv_reset(SimApiContext *context) {
	spirv_bin_opcode_rewind(&context->spirv_bin);
	spirv_sim_init(&context->spirv_sim, &context->spirv_module);
}

EMSCRIPTEN_KEEPALIVE
void simapi_free_json(SimApiContext *context, char *json) {
	arr_free(json);
}

EMSCRIPTEN_KEEPALIVE
size_t simapi_spirv_opcode_count(SimApiContext *context) {
	return spirv_module_opcode_count(&context->spirv_module);
}

EMSCRIPTEN_KEEPALIVE
const char *simapi_spirv_opcode_text(SimApiContext *context, uint32_t index) {
	SPIRV_opcode *op = spirv_module_opcode_by_index(&context->spirv_module, index);
	return spirv_text_opcode(op);
}

EMSCRIPTEN_KEEPALIVE
size_t simapi_spirv_variable_count(SimApiContext *context) {
	return spirv_module_variable_count(&context->spirv_module);
}

EMSCRIPTEN_KEEPALIVE
size_t simapi_spirv_variable_id(SimApiContext *context, uint32_t index) {
	return spirv_module_variable_id(&context->spirv_module, index);
}

static inline void spirv_type_to_json(SPIRV_module *spirv_module, Type *type, char **output) {
	arr_printf(*output, "{");
	arr_printf(*output, "\"id\": %d,", type->id);
	arr_printf(*output, "\"type\":\"%s%s\",", (type->is_signed && type->count == 1) ? "Signed" : "", lut_lookup_type_kind(type->kind));
	arr_printf(*output, "\"count\": %d,", type->count);
	arr_printf(*output, "\"element_size\": %d", type->element_size);
	if (type->base_type) {
		arr_printf(*output, ",\"base_type\": ");
		spirv_type_to_json(spirv_module, type->base_type, output);
	}
	if (type->kind == TypeMatrixInteger || type->kind == TypeMatrixFloat) {
		arr_printf(*output, ",\"matrix_rows\": %d", type->matrix.num_rows);
		arr_printf(*output, ",\"matrix_cols\": %d", type->matrix.num_cols);
	} else if (type->kind == TypeFunction) {
		arr_printf(*output, ",\"return_type\": ");
		spirv_type_to_json(spirv_module, type->function.return_type, output);

		arr_printf(*output, ",\"parameter_types\": [");
		for (Type **param = type->function.parameter_types; param != arr_end(type->function.parameter_types); ++param) {
			if (param != type->function.parameter_types) {
				arr_printf(*output, ",");
			}
			spirv_type_to_json(spirv_module, *param, output);
		}
		arr_printf(*output, "]");
	} else if (type->kind == TypeStructure) {
		arr_printf(*output, ",\"member_types\": [");
		for (Type **member = type->structure.members; member != arr_end(type->structure.members); ++member) {
			if (member != type->structure.members) {
				arr_printf(*output, ",");
			}
			spirv_type_to_json(spirv_module, *member, output);
		}
		arr_printf(*output, "]");
	}
	const char *name = spirv_module_name_by_id(spirv_module, type->id, -1);
	if (name != NULL) {
		arr_printf(*output, ",\"name\": \"%s\"", name);
	}

	arr_printf(*output, "}");
}

static inline void spirv_variable_access_to_json(VariableAccess *access, char **output) {
	arr_printf(*output, "{\"kind\": \"%s\"", lut_lookup_variable_access(access->kind));

	if (access->kind == VarAccessLocation) {
		arr_printf(*output, ",\"location\": %d", access->index);
	} else if (access->kind == VarAccessBuiltIn) {
		arr_printf(*output, ",\"builtin\": \"%s\"", lut_lookup_builtin(access->index));
	}
	
	arr_printf(*output, "}");
}

static inline void spirv_variable_to_json(SPIRV_module *spirv_module, Variable *var, char **output) {
	arr_printf(*output, "{");
	arr_printf(*output, "\"id\": %d", var->id);
	arr_printf(*output, ",\"type\": ");
	spirv_type_to_json(spirv_module, var->type, output);
	if (var->name) {
		arr_printf(*output, ",\"name\": \"%s\"", var->name);
	}
	arr_printf(*output, ",\"kind\": \"%s\"", lut_lookup_variable_kind(var->kind));

	arr_printf(*output, ",\"access\": ");
	spirv_variable_access_to_json(&var->access, output);

	/* aggregate type: iterate members */
	if (arr_len(var->member_access) > 0) {
		arr_printf(*output, ",\"members\": [");

		for (uint32_t idx = 0; idx < arr_len(var->member_access); ++idx) {
			if (idx > 0) {
				arr_printf(*output, ",");
			}

			arr_printf(*output, "{\"access\": ");
			spirv_variable_access_to_json(&var->member_access[idx], output);

			if (var->member_name[idx]) {
				arr_printf(*output, ",\"name\": \"%s\"", var->member_name[idx]);
			}
			
			arr_printf(*output, "}");
		}

		arr_printf(*output, "]");
	}

	arr_printf(*output, "}");

}

EMSCRIPTEN_KEEPALIVE 
const char *simapi_spirv_variable_desc(SimApiContext *context, uint32_t id) {
	Variable *var = spirv_module_variable_by_id(&context->spirv_module, id);

	if (!var) {
		return "";
	}
	
	char *json = NULL;
	spirv_variable_to_json(&context->spirv_module, var, &json);

	return json;
}

static inline void spirv_array_to_json(char **output, Type *type, void *data) {

#define FORMAT_ARRAY(t,fmt) 						\
{													\
	t *value = (t *) data;							\
	arr_printf(*output, fmt, value[0]);				\
	for (int32_t i = 1; i < type->count; ++i) {		\
		arr_printf(*output, "," fmt, value[i]); 	\
	}												\
}

	if (type->count > 1) {
		arr_printf(*output, "[");
	}

	switch(type->kind) {

		case TypeBool:
			arr_printf(*output, "%s", *((bool *) data) ? "true" : "false");
			break;

		case TypeInteger:
		case TypeVectorInteger:
		case TypeMatrixInteger: 
			if (type->is_signed) {
				FORMAT_ARRAY(int32_t, "%d");
			} else {
				FORMAT_ARRAY(uint32_t, "%d");
			}
			break;

		case TypeFloat:
		case TypeVectorFloat:
		case TypeMatrixFloat: 
			FORMAT_ARRAY(float, "\"%f\"");
			break;

		case TypePointer:
			FORMAT_ARRAY(uint32_t, "\"0x%.8x\"");
			break;

		case TypeArray: {
			size_t el_delta = type->base_type->element_size * type->base_type->count;
			spirv_array_to_json(output, type->base_type, data);
			void *el = data + el_delta;
			for (int32_t i = 1; i < type->count; ++i) {
				arr_printf(*output, ",");
				spirv_array_to_json(output, type->base_type, el);
				el += el_delta;
			}
			break;
		}

		default:
			break;
	}

	if (type->count > 1) {
		arr_printf(*output, "]");
	}

#undef FORMAT_ARRAY

}

EMSCRIPTEN_KEEPALIVE 
const char *simapi_spirv_variable_data(SimApiContext *context, uint32_t id, uint32_t member) {
	Variable *var = spirv_module_variable_by_id(&context->spirv_module, id);

	if (!var) {
		return "";
	}

	VariableAccess *access = &var->access;
	Type *data_type = var->type->base_type;
		
	if (data_type->kind == TypeStructure && member != -1) {
		access = &var->member_access[member];
		data_type = data_type->structure.members[member];
	}

	SimPointer *mem = spirv_sim_retrieve_intf_pointer(
			&context->spirv_sim,
			var->kind, 
			*access);

	char *json = NULL;

	spirv_array_to_json(&json, data_type, context->spirv_sim.memory + mem->pointer);
	return json;
}

EMSCRIPTEN_KEEPALIVE
void simapi_spirv_variable_data_set_float(SimApiContext *context, uint32_t id, uint32_t member, uint32_t index, float value) {
	Variable *var = spirv_module_variable_by_id(&context->spirv_module, id);

	if (!var) {
		return;
	}

	VariableAccess *access = &var->access;
	if (var->type->base_type->kind == TypeStructure && member != -1) {
		access = &var->member_access[member];
	}

	SimPointer *mem = spirv_sim_retrieve_intf_pointer(
			&context->spirv_sim,
			var->kind, 
			*access);

	float *data = (float *) (context->spirv_sim.memory + mem->pointer);
	data[index] = value;
}

EMSCRIPTEN_KEEPALIVE
void simapi_spirv_variable_data_set_int(SimApiContext *context, uint32_t id, uint32_t member, uint32_t index, int32_t value) {
	Variable *var = spirv_module_variable_by_id(&context->spirv_module, id);

	if (!var) {
		return;
	}

	VariableAccess *access = &var->access;
	if (var->type->base_type->kind == TypeStructure && member != -1) {
		access = &var->member_access[member];
	}

	SimPointer *mem = spirv_sim_retrieve_intf_pointer(
			&context->spirv_sim,
			var->kind, 
			*access);

	int32_t *data = (int32_t *) (context->spirv_sim.memory + mem->pointer);
	data[index] = value;
}

EMSCRIPTEN_KEEPALIVE
uint32_t simapi_spirv_current_line(SimApiContext *context) {
	SPIRV_opcode *current_op = spirv_bin_opcode_current(&context->spirv_bin);
	return spirv_module_index_for_opcode(&context->spirv_module, current_op);
}

EMSCRIPTEN_KEEPALIVE
uint32_t simapi_spirv_select_entry_point(SimApiContext *context, uint32_t index) {
	spirv_sim_select_entry_point(&context->spirv_sim, index); 
	return simapi_spirv_current_line(context);
}

EMSCRIPTEN_KEEPALIVE
uint32_t simapi_spirv_step(SimApiContext *context) {
	spirv_sim_step(&context->spirv_sim);
	return simapi_spirv_current_line(context);
}

EMSCRIPTEN_KEEPALIVE
bool simapi_spirv_execution_finished(SimApiContext *context) {
	return context->spirv_sim.finished;
}

EMSCRIPTEN_KEEPALIVE
uint32_t simapi_spirv_first_free_register(SimApiContext *context) {
	return context->spirv_sim.reg_free_start;
}

EMSCRIPTEN_KEEPALIVE
const char *simapi_spirv_register(SimApiContext *context, uint32_t reg_idx) {

	char *json = NULL;
	SimRegister *reg = &context->spirv_sim.temp_regs[reg_idx];

	arr_printf(json, "{\"id\": %d", reg->id);
	arr_printf(json, ",\"type\":");
	spirv_type_to_json(&context->spirv_module, reg->type, &json);
	arr_printf(json, ",\"value\": ");
	spirv_array_to_json(&json, reg->type, reg->raw);
	arr_printf(json, "}");

	return json;
}
