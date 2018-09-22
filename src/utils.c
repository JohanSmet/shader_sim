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


#define LINUX_PATH_SEPARATOR '/'
#define WIN32_PATH_SEPARATOR '\\'

#ifdef PLATFORM_LINUX
    #define SYSTEM_PATH_SEPARATOR   LINUX_PATH_SEPARATOR
    #define OTHER_PATH_SEPARATOR    WIN32_PATH_SEPARATOR
#elif PLATFORM_WINDOWS
    #define SYSTEM_PATH_SEPARATOR   WIN32_PATH_SEPARATOR
    #define OTHER_PATH_SEPARATOR    LINUX_PATH_SEPARATOR
#elif PLATFORM_DARWIN
    #define SYSTEM_PATH_SEPARATOR   LINUX_PATH_SEPARATOR
    #define OTHER_PATH_SEPARATOR    WIN32_PATH_SEPARATOR
#elif PLATFORM_EMSCRIPTEN
    #define SYSTEM_PATH_SEPARATOR   LINUX_PATH_SEPARATOR
    #define OTHER_PATH_SEPARATOR    WIN32_PATH_SEPARATOR
#else
    #error Unsupported platform
#endif

void path_fix_separator(const char *path_in, char *path_out) {
    assert(path_in);
    assert(path_out);

    strcpy(path_out, path_in);

    char *match = strchr(path_out, OTHER_PATH_SEPARATOR);
    while (match) {
        *match++ = SYSTEM_PATH_SEPARATOR;
        match = strchr(match, OTHER_PATH_SEPARATOR);
    }
}

void path_dirname(const char *path_in, char *dir) {
    assert(path_in);
    assert(dir);

    const char *lstsep = strrchr(path_in, SYSTEM_PATH_SEPARATOR);

    if (!lstsep) {
        *dir = '\0';
    } else {
        int count = lstsep - path_in;
        strncpy(dir, path_in, count);
        dir[count] = '\0';
    }
}

void path_append(char *path, const char *suffix) {
    assert(path);
    assert(suffix);

    size_t len = strlen(path);

    if (len > 0 && path[len-1] != SYSTEM_PATH_SEPARATOR) {
        path[len++] = SYSTEM_PATH_SEPARATOR;
        path[len] = '\0';
    }

    strcat(path, suffix);
}
