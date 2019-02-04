// runner.h - Johan Smet - BSD-3-Clause (see LICENSE)
//
// Utility functions

#ifndef JS_SHADER_SIM_UTILITY_H
#define JS_SHADER_SIM_UTILITY_H

#include "types.h"

void fatal_error(const char *fmt, ...) __attribute__((noreturn));

size_t file_load_binary(const char *filename, int8_t **buffer);
size_t file_load_text(const char *filename, int8_t **buffer);
void file_free(int8_t *buffer);

void path_fix_separator(const char *path_in, char *path_out);
void path_dirname(const char *path_in, char *dir);
void path_append(char *path, const char *suffix);

#endif // JS_SHADER_SIM_UTILITY_H
