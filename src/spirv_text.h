// spirv_text.h - Johan Smet - BSD-3-Clause (see LICENSE)
//
// SPIR-V binary to text string conversion

#ifndef JS_SHADER_SIM_SPIRV_TEXT_H
#define JS_SHADER_SIM_SPIRV_TEXT_H

// require forward declarations
typedef struct SPIRV_header SPIRV_header;
typedef struct SPIRV_opcode SPIRV_opcode;

// interface functions
int spirv_text_header_num_lines(SPIRV_header *header);
char *spirv_text_header_line(SPIRV_header *header, int line);

char *spirv_text_opcode(SPIRV_opcode *opcode);

#endif // JS_SHADER_SIM_SPIRV_TEXT_H