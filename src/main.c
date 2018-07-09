// main.c - Johan Smet - BSD-3-Clause (see LICENSE)

#include <stdio.h>

#include "utils.h"
#include "dyn_array.h"
#include "spirv_binary.h"
#include "spirv_text.h"
#include "spirv_module.h"
#include "spirv_simulator.h"

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
    int8_t *shader_bin = load_binary_file(input_filename);

    if (!shader_bin) {
        return -2;
    }

    // only supported action is disassemble for now
    disassemble_spirv_shader(shader_bin);

    SPIRV_module shader_module;
    SPIRV_binary shader_binary;
    SPIRV_simulator sim;

    spirv_bin_load(&shader_binary, shader_bin);
    spirv_module_load(&shader_module, &shader_binary);
    spirv_sim_init(&sim, &shader_module);

    float input_position[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float input_color[] = {0.5f, 0.6f, 0.7f, 1.0f};
    float output_position[] = {0.0f, 0.0f, 0.0f, 0.0f};
    float output_color[] = {0.0f, 0.0f, 0.0f, 0.0f};

    spirv_sim_variable_associate_data(
        &sim, 
        VarKindInput,
        VarInterfaceLocation, 0, 
        (uint8_t *) input_position, sizeof(input_position));
    spirv_sim_variable_associate_data(
        &sim, 
        VarKindInput,
        VarInterfaceLocation, 1, 
        (uint8_t *) input_color, sizeof(input_color));
    spirv_sim_variable_associate_data(
        &sim, 
        VarKindOutput,
        VarInterfaceBuiltIn, SpvBuiltInPosition,  
        (uint8_t *) output_position, sizeof(output_position));
    spirv_sim_variable_associate_data(
        &sim, 
        VarKindOutput,
        VarInterfaceLocation, 0, 
        (uint8_t *) output_color, sizeof(output_color));


    while (!sim.finished && !sim.error_msg) {
        spirv_sim_step(&sim);
    }

    if (sim.error_msg) {
        printf("%s\n", sim.error_msg);
    }

    arr_free(shader_bin);

    return 0;
}