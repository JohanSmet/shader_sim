#include <emscripten.h>
#include <stdlib.h>

#include "types.h"
#include "utils.h"
#include "spirv_binary.h"
#include "spirv_module.h"
#include "spirv_text.h"


typedef struct SimApiContext {
	SPIRV_binary spirv_bin;
    SPIRV_module spirv_module;
    // SPIRV_simulator *spirv_sim;
} SimApiContext;

EMSCRIPTEN_KEEPALIVE
SimApiContext *simapi_create_context(void) {
	SimApiContext *context = (SimApiContext *) malloc(sizeof(SimApiContext));
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
