// spirv_binary.h - Johan Smet - BSD-3-Clause (see LICENSE)
//
// Data structures and functions to read and process SPIR-V binaries

#include "spirv_binary.h"
#include "dyn_array.h"

#include "spirv/spirv.h"

static size_t spirv_parse_header(uint32_t *ops, SPIRV_header *header) {
    assert(ops != NULL);
    assert(header != NULL);

    header->magic = ops[0];
    header->version_high = (ops[1] & 0x00ff0000) >> 16;
    header->version_low  = (ops[1] & 0x0000ff00) >> 8;
    header->generator = ops[2];
    header->bound_ids = ops[3];
    header->reserved = ops[4];
    return 5;
}

bool spirv_bin_load(SPIRV_binary *spirv, int8_t *data) {
    assert(spirv != NULL);
    assert(data != NULL);

    // initialization
    *spirv = (SPIRV_binary) {};

    // do some basic validation on the binary
    if (arr_len(data) < 20) {
        spirv->error_msg = "SPIR-V binary too small (should be at least 20 bytes)";
        return false;
    }

    if (arr_len(data) % 4 != 0) {
        spirv->error_msg = "SPIR-V binary length should be a multiple of 32bit";
        return false;
    }

    // SPIR-V opcodes are 32bit words
    spirv->word_len = arr_len(data) / 4;
    spirv->cur_op = (uint32_t *) data;
    spirv->end_op = spirv->cur_op + spirv->word_len;

    // read header
    spirv->cur_op += spirv_parse_header(spirv->cur_op, &spirv->header);

    if (spirv->header.magic != SpvMagicNumber) {
        spirv->error_msg = "This is not a SPIR-V binary";
        return false;
    }

    spirv->fst_op = spirv->cur_op;

    return true;
}

SPIRV_header *spirv_bin_header(SPIRV_binary *spirv) {
    assert(spirv != NULL);
    return &spirv->header;
}

SPIRV_opcode *spirv_bin_opcode_rewind(SPIRV_binary *spirv) {
    assert(spirv != NULL);
    spirv->cur_op = spirv->fst_op;
    return (SPIRV_opcode *) spirv->cur_op;
}

SPIRV_opcode *spirv_bin_opcode_jump_to(SPIRV_binary *spirv, SPIRV_opcode *op) {
    assert(spirv != NULL);
    assert((uint32_t *) op >= spirv->fst_op && (uint32_t *) op <= spirv->end_op);

    spirv->cur_op = (uint32_t *) op;
    return (SPIRV_opcode *) spirv->cur_op;
}

SPIRV_opcode *spirv_bin_opcode_current(SPIRV_binary *spirv) {
    assert(spirv != NULL);
    return (SPIRV_opcode *) spirv->cur_op;
}

SPIRV_opcode *spirv_bin_opcode_next(SPIRV_binary *spirv) {
    assert(spirv != NULL);
    spirv->cur_op += ((SPIRV_opcode *) spirv->cur_op)->op.length;
    return (SPIRV_opcode *) spirv->cur_op;
}

SPIRV_opcode *spirv_bin_opcode_end(SPIRV_binary *spirv) {
    assert(spirv != NULL);
    return (SPIRV_opcode *) spirv->end_op;
}

const char *spriv_bin_error_msg(SPIRV_binary *spirv) {
    assert(spirv != NULL);
    return spirv->error_msg;
}