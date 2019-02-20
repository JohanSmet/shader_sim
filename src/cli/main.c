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

void execute_runner(const char *filename) {
    SPIRV_simulator sim;
    Runner runner;
    runner_init(&runner, filename);

    runner_execute(&runner, &sim);

    if (sim.error_msg) {
        printf("%s\n", sim.error_msg);
    } 
}

void disassemble_spirv_shader(const char *filename) {

    int8_t *data;

    if (file_load_binary(filename, &data) == 0) {
        return;
    }

    SPIRV_binary spirv_bin;
    SPIRV_module spirv_mod;

    if (!spirv_bin_load(&spirv_bin, data)) {
        fatal_error(spirv_bin.error_msg);
        return;
    }

    spirv_module_load(&spirv_mod, &spirv_bin);
    spirv_bin_opcode_rewind(&spirv_bin);

    spirv_text_set_flag(&spirv_mod, SPIRV_TEXT_USE_ID_NAMES, true);

    char **output_lines = NULL;

    for (int idx = 0; idx < spirv_text_header_num_lines(spirv_bin_header(&spirv_bin)); ++idx) {
        char *line = spirv_text_header_line(spirv_bin_header(&spirv_bin), idx);
        arr_push(output_lines, line);
    }

    for (SPIRV_opcode *op = spirv_bin_opcode_current(&spirv_bin); op != spirv_bin_opcode_end(&spirv_bin); op = spirv_bin_opcode_next(&spirv_bin)) {
        char *line = spirv_text_opcode(op, &spirv_mod);
        arr_push(output_lines, line);
    }

    for (char **it = output_lines; it != arr_end(output_lines); ++it) {
        printf("%s\n", *it);
        arr_free(*it);
    }

    arr_free(output_lines);
    spirv_module_free(&spirv_mod);
    spirv_bin_free(&spirv_bin);
    arr_free(data);
}

int main(int argc, char *argv[]) {

    // handle command line arguments (keep it simple for now)
    if (argc != 3) {
        printf("Usage: %s [-r|-d] <input-file>\n", argv[0]);
        printf("  -r runner.json : to run the specified runner script\n");
        printf("  -d binary shader : to display the dissassembled shader\n");
        return -1;
    }

    const char *input_cmd = argv[1];
    const char *input_filename = argv[2];

    if (!strcmp(input_cmd, "-r")) {
        execute_runner(input_filename);
    } else if (!strcmp(input_cmd, "-d")) {
        disassemble_spirv_shader(input_filename);
    } else {
        printf("Invalid command (%s)\n", input_cmd);
        return -1;
    }

    return 0;
}