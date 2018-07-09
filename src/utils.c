// utils.c - Johan Smet - BSD-3-Clause (see LICENSE)

#include "utils.h"
#include "dyn_array.h"

#include <stdio.h>
#include <assert.h>

void fatal_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("FATAL: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    exit(1);
}

size_t file_load_binary(const char *filename, int8_t **buffer) {
    // a truly platform independant method of getting the size of a file has a lot of caveats
    //  shader binaries shouldn't be large files, just read in blocks and realloc when necessary
    enum {BLOCK_SIZE = 2048};
    int8_t block[BLOCK_SIZE];
    size_t total_size = 0;

    assert(filename);
    assert(buffer);

    FILE *fp = fopen(filename, "rb");

    if (!fp) {
        fatal_error("Error opening file %s", filename);
        return total_size;
    }

    while (!feof(fp)) {
        size_t read = fread(block, 1, BLOCK_SIZE, fp);
        arr_push_buf(*buffer, block, read);
        total_size += read;
    }

    fclose(fp);

    return total_size;
}

size_t file_load_text(const char *filename, int8_t **buffer) {
    // a truly platform independant method of getting the size of a file has a lot of caveats
    //  shader binaries shouldn't be large files, just read in blocks and realloc when necessary
    enum {BLOCK_SIZE = 2048};
    int8_t block[BLOCK_SIZE];
    size_t total_size = 0;

    assert(filename);
    assert(buffer);

    FILE *fp = fopen(filename, "rb");

    if (!fp) {
        fatal_error("Error opening file %s", filename);
        return total_size;
    }

    while (!feof(fp)) {
        size_t read = fread(block, 1, BLOCK_SIZE, fp);
        arr_push_buf(*buffer, block, read);
        total_size += read;
    }

    fclose(fp);

    return total_size;
}

void file_free(int8_t *buffer) {
    assert(buffer);
    arr_free(buffer);
}
