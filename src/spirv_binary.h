// spirv_binary.h - Johan Smet - BSD-3-Clause (see LICENSE)
//
// Data structures and functions to read and process SPIR-V binaries

#ifndef JS_SHADER_SIM_SPIRV_BINARY_H
#define JS_SHADER_SIM_SPIRV_BINARY_H

#include "types.h"

// types
typedef struct SPIRV_header {
    uint32_t magic;
    uint8_t  version_high;
    uint8_t  version_low;
    uint32_t generator;
    uint32_t bound_ids;
    uint32_t reserved;
} SPIRV_header;

typedef struct SPIRV_opcode {
    union {
        uint32_t opcode;
        struct {
            uint16_t kind;
            uint16_t length;
        } op;
    };
    uint32_t optional[];
} SPIRV_opcode;

typedef struct SPIRV_binary {
    int8_t *binary_data;    // dynamic array
    size_t word_len;        // number of 32-bit words in the binary

    uint32_t *fst_op;       // pointer to the first opcode
    uint32_t *cur_op;       // pointer to current opcode
    uint32_t *end_op;       // pointer just beyond last opcode

    const char *error_msg;  // NULL if no error, static string otherwise

    SPIRV_header header;
} SPIRV_binary;

// interface functions
bool spirv_bin_load(SPIRV_binary *spirv, int8_t *data);

SPIRV_header *spirv_bin_header(SPIRV_binary *spirv);
SPIRV_opcode *spirv_bin_opcode_rewind(SPIRV_binary *spirv);
SPIRV_opcode *spirv_bin_opcode_jump_to(SPIRV_binary *spirv, SPIRV_opcode *op);
SPIRV_opcode *spirv_bin_opcode_current(SPIRV_binary *spirv);
SPIRV_opcode *spirv_bin_opcode_next(SPIRV_binary *spirv);
SPIRV_opcode *spirv_bin_opcode_end(SPIRV_binary *spirv);

const char *spriv_bin_error_msg(SPIRV_binary *spirv);

#endif // JS_SHADER_SIM_SPIRV_BINARY_H