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
bool simapi_spirv_load_file(SimApiContext *context, const char *filename) {
	int8_t *binary_data = NULL;

    if (file_load_binary(filename, &binary_data) == 0) {
		return false;
	}

	if (!spirv_bin_load(&context->spirv_bin, binary_data)) {
		return false;
	}

	spirv_module_load(&context->spirv_module, &context->spirv_bin);

	spirv_sim_init(&context->spirv_sim, &context->spirv_module);

	return true;
}

EMSCRIPTEN_KEEPALIVE
bool simapi_spirv_load_binary(const uint8_t *blob) {
	return true;
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

static inline void spirv_type_to_json(Type *type, char **output) {
	arr_printf(*output, "{");
	arr_printf(*output, "\"id\": %d,", type->id);
	arr_printf(*output, "\"type\":\"%s%s\",", (type->is_signed && type->count == 1) ? "Signed" : "", lut_lookup_type_kind(type->kind));
	arr_printf(*output, "\"count\": %d,", type->count);
	arr_printf(*output, "\"element_size\": %d", type->element_size);
	if (type->base_type) {
		arr_printf(*output, ",\"base_type\": ");
		spirv_type_to_json(type->base_type, output);
	}
	if (type->kind == TypeMatrixInteger || type->kind == TypeMatrixFloat) {
		arr_printf(*output, ",\"matrix_rows\": %d", type->matrix.num_rows);
		arr_printf(*output, ",\"matrix_cols\": %d", type->matrix.num_cols);
	} else if (type->kind == TypeFunction) {
		arr_printf(*output, ",\"return_type\": ");
		spirv_type_to_json(type->function.return_type, output);

		arr_printf(*output, ",\"parameter_types\": [");
		for (Type **param = type->function.parameter_types; param != arr_end(type->function.parameter_types); ++param) {
			if (param != type->function.parameter_types) {
				arr_printf(*output, ",");
			}
			spirv_type_to_json(*param, output);
		}
		arr_printf(*output, "]");
	} else if (type->kind == TypeStructure) {
		arr_printf(*output, ",\"member_types\": [");
		for (Type **member = type->structure.members; member != arr_end(type->structure.members); ++member) {
			if (member != type->structure.members) {
				arr_printf(*output, ",");
			}
			spirv_type_to_json(*member, output);
		}
		arr_printf(*output, "]");
	}
	arr_printf(*output, "}");
}


static inline void spirv_variable_to_json(Variable *var, char **output) {
	arr_printf(*output, "{");
	arr_printf(*output, "\"id\": %d", var->id);
	arr_printf(*output, ",\"member_id\": %d", var->member_id);
	arr_printf(*output, ",\"type\": ");
	spirv_type_to_json(var->type, output);
	if (var->name) {
		arr_printf(*output, ",\"name\": \"%s\"", var->name);
	}
	arr_printf(*output, ",\"kind\": \"%s\"", lut_lookup_variable_kind(var->kind));
	arr_printf(*output, ",\"interface\": \"%s\"", lut_lookup_variable_interface(var->if_type));
	if (var->if_type == VarInterfaceLocation) {
		arr_printf(*output, ",\"location\": %d", var->if_index);
	} else if (var->if_type == VarInterfaceBuiltIn) {
		arr_printf(*output, ",\"builtin\": \"%s\"", lut_lookup_builtin(var->if_index));
	}

	if (var->members) {
		arr_printf(*output, ",\"member_types\": [");
		for (Variable **member = var->members; member != arr_end(var->members); ++member) {
			if (member != var->members) {
				arr_printf(*output, ",");
			}
			spirv_variable_to_json(*member, output);
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
	spirv_variable_to_json(var, &json);

	return json;
}

static inline void spirv_variable_data_to_json(Variable *var, char **output) {

}	

EMSCRIPTEN_KEEPALIVE 
const char *simapi_spirv_variable_data(SimApiContext *context, uint32_t id) {
	Variable *var = spirv_module_variable_by_id(&context->spirv_module, id);

	if (!var) {
		return "";
	}

	SimPointer *mem = spirv_sim_retrieve_intf_pointer(
			&context->spirv_sim,
			var->kind, 
			var->if_type, 
			var->if_index);

	char *json = NULL;
	arr_printf(json, "[");

	Type *data_type = var->type->base_type;

	switch(data_type->kind) {
		case TypeVoid:
		case TypePointer:
		case TypeFunction:
		case TypeStructure:
			break;

		case TypeBool:
			arr_printf(json, "%s", *((bool *) (context->spirv_sim.memory + mem->pointer)) ? "true" : "false");
			break;

		case TypeInteger:
		case TypeVectorInteger:
		case TypeMatrixInteger: {
			int32_t *value = (int32_t *) (context->spirv_sim.memory + mem->pointer);
			arr_printf(json, "%d", value[0]);
			for (int32_t i = 1; i < data_type->count; ++i) {
				arr_printf(json, ",%d", value[i]);
			}
			break;
		}

		case TypeFloat:
		case TypeVectorFloat:
		case TypeMatrixFloat: {
			float *value = (float *) (context->spirv_sim.memory + mem->pointer);
			arr_printf(json, "%f", value[0]);
			for (int32_t i = 1; i < data_type->count; ++i) {
				arr_printf(json, ",%f", value[i]);
			}
			break;
		}

	}

	arr_printf(json, "]");

	return json;
}

EMSCRIPTEN_KEEPALIVE
void simapi_spirv_variable_data_set_float(SimApiContext *context, uint32_t id, uint32_t index, float value) {
	Variable *var = spirv_module_variable_by_id(&context->spirv_module, id);

	if (!var) {
		return;
	}

	SimPointer *mem = spirv_sim_retrieve_intf_pointer(
			&context->spirv_sim,
			var->kind, 
			var->if_type, 
			var->if_index);

	float *data = (float *) (context->spirv_sim.memory + mem->pointer);
	data[index] = value;
}
