// main.c - Johan Smet - BSD-3-Clause (see LICENSE)

#include <stdio.h>

#include "utils.h"
#include "dyn_array.h"
#include "spirv_binary.h"
#include "spirv_text.h"
#include "spirv_module.h"
#include "spirv_simulator.h"
#include "runner.h"

#include "spirv/spirv.h"

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
/*    int8_t *shader_bin = NULL;
    if (file_load_binary(input_filename, &shader_bin) == 0) {
        return -2;
    }

    // only supported action is disassemble for now
    disassemble_spirv_shader(shader_bin);
    file_free(shader_bin);
*/

    SPIRV_simulator sim;
    Runner runner;
    runner_init(&runner, input_filename);

    runner_execute(&runner, &sim);

    if (sim.error_msg) {
        printf("%s\n", sim.error_msg);
    } 

    return 0;
}