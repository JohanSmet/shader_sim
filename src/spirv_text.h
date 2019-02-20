// spirv_text.h - Johan Smet - BSD-3-Clause (see LICENSE)
//
// SPIR-V binary to text string conversion

#ifndef JS_SHADER_SIM_SPIRV_TEXT_H
#define JS_SHADER_SIM_SPIRV_TEXT_H

#include "types.h"
#include "hash_map.h"

// require forward declarations
struct SPIRV_header;
struct SPIRV_opcode;
struct SPIRV_module;

// types
typedef struct SPIRV_text {
    char *scratch_buf;

    bool use_id_names;
    bool use_type_alias;

    HashMap type_aliases;       // id (int) -> const char * (name)
    HashMap type_aliases_rev;   // const char * (name) -> id (int)
} SPIRV_text;

typedef enum SPIRV_text_flag {
    SPIRV_TEXT_USE_ID_NAMES,
    SPIRV_TEXT_USE_TYPE_ALIAS,
} SPIRV_text_flag;

// interface functions
int spirv_text_header_num_lines(struct SPIRV_header *header);
char *spirv_text_header_line(struct SPIRV_header *header, int line);

void spirv_text_set_flag(struct SPIRV_module *module, SPIRV_text_flag flag, bool value);
char *spirv_text_opcode(struct SPIRV_opcode *opcode, struct SPIRV_module *module);

#endif // JS_SHADER_SIM_SPIRV_TEXT_H