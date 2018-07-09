// runner.h - Johan Smet - BSD-3-Clause (see LICENSE)
//
// Utility functions

#ifndef JS_SHADER_SIM_UTILITY_H
#define JS_SHADER_SIM_UTILITY_H

#include "types.h"

void fatal_error(const char *fmt, ...);

size_t file_load_binary(const char *filename, int8_t **buffer);
size_t file_load_text(const char *filename, int8_t **buffer);
void file_free(int8_t *buffer);

#endif // JS_SHADER_SIM_UTILITY_H
