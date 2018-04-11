// main.c - Johan Smet - BSD-3-Clause (see LICENSE)

#include <stdio.h>

#include "types.h"
#include "dyn_array.h"
#include "spirv_binary.h"
#include "spirv_text.h"

void fatal_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("FATAL: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    exit(1);
}

int8_t *load_binary_file(const char *filename) {
    // a truly platform independant method of getting the size of a file has a lot of caveats
    //  shader binaries shouldn't be large files, just read in blocks and realloc when necessary
    enum {BLOCK_SIZE = 2048};
    int8_t block[BLOCK_SIZE];
    int8_t *contents = NULL;

    FILE *fp = fopen(filename, "rb");

    if (!fp) {
        fatal_error("Error opening file %s", filename);
        return NULL;
    }

    while (!feof(fp)) {
        size_t read = fread(block, 1, BLOCK_SIZE, fp);
        arr_push_buf(contents, block, read);
    }

    fclose(fp);

    return contents;
}

void disassemble_spirv_shader(int8_t *data) {

    SPIRV_binary spirv_bin;

    if (!spirv_bin_load(&spirv_bin, data)) {
        fatal_error(spirv_bin.error_msg);
        return;
    }

    char **output_lines = NULL;

    for (int idx = 0; idx < spirv_text_header_num_lines(spirv_bin_header(&spirv_bin)); ++idx) {
        char *line = spirv_text_header_line(spirv_bin_header(&spirv_bin), idx);
        arr_push(output_lines, line);
    }

    for (SPIRV_opcode *op = spirv_bin_opcode_current(&spirv_bin); op != spirv_bin_opcode_end(&spirv_bin); op = spirv_bin_opcode_next(&spirv_bin)) {
        char *line = spirv_text_opcode(op);
        arr_push(output_lines, line);
    }

    for (char **it = output_lines; it != arr_end(output_lines); ++it) {
        printf("%s\n", *it);
        arr_free(*it);
    }

    arr_free(output_lines);
}

int main(int argc, char *argv[]) {

    // handle command line arguments (keep it simple for now)
    if (argc != 2) {
        printf("Usage: %s <shader_file>\n", argv[0]);
        return -1;
    }

    const char *input_filename = argv[1];

    // load shader
    int8_t *shader_bin = load_binary_file(input_filename);

    if (!shader_bin) {
        return -2;
    }

    // only supported action is disassemble for now
    disassemble_spirv_shader(shader_bin);

    arr_free(shader_bin);

    return 0;
}