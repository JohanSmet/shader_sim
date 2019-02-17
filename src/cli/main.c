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

int main(int argc, char *argv[]) {

    // handle command line arguments (keep it simple for now)
    if (argc != 2) {
        printf("Usage: %s <shader_file>\n", argv[0]);
        return -1;
    }

    const char *input_filename = argv[1];

    SPIRV_simulator sim;
    Runner runner;
    runner_init(&runner, input_filename);

    runner_execute(&runner, &sim);

    if (sim.error_msg) {
        printf("%s\n", sim.error_msg);
    } 

    return 0;
}