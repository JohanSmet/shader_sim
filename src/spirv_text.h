// spirv_text.h - Johan Smet - BSD-3-Clause (see LICENSE)
//
// SPIR-V binary to text string conversion

#ifndef JS_SHADER_SIM_SPIRV_TEXT_H
#define JS_SHADER_SIM_SPIRV_TEXT_H

// require forward declarations
struct SPIRV_header;
struct SPIRV_opcode;
struct SPIRV_module;

// interface functions
int spirv_text_header_num_lines(struct SPIRV_header *header);
char *spirv_text_header_line(struct SPIRV_header *header, int line);

char *spirv_text_opcode(struct SPIRV_opcode *opcode, struct SPIRV_module *module);

#endif // JS_SHADER_SIM_SPIRV_TEXT_H