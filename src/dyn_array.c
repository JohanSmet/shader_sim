// dyn_array.c - Johan Smet - BSD-3-Clause (see LICENSE)
//
// a dynamic array for C (C++ vector-like growable array) 
//  inspired by stretchy_buffers by stb (http://github.com/nothings/stb)
//  based on code from Bitwise (https://bitwise.handmade.network/)

#include "dyn_array.h"

char *arr__printf(char *array, const char *fmt, ...) {
    va_list args;
    size_t avail = arr_cap(array) - arr_len(array);
    va_start(args, fmt);
    size_t n = vsnprintf(arr_end(array), avail, fmt, args) + 1;
    va_end(args);

    if (n > avail) {
        // array too small, grow and print again
        arr__fit(array, n);
        size_t avail = arr_cap(array) - arr_len(array);
        va_start(args, fmt);
        n = vsnprintf(arr_end(array), avail, fmt, args) + 1;
        va_end(args);
    }

    // don't count zero-terminator in array length so it concatenates easily (terminator is always present!)
    arr__hdr(array)->len += n - 1;
    return array;
}

void *arr__grow(const void *array, size_t new_len, size_t elem_size) {
    size_t dbl_cap = arr_cap(array) * 2;
    size_t new_cap = (new_len > dbl_cap) ? new_len : dbl_cap;
    assert(new_len <= new_cap);
    size_t new_size = sizeof(ArrayHeader) + elem_size * new_cap;

    ArrayHeader *new_hdr;
    new_hdr = (ArrayHeader *) realloc(array ? arr__hdr(array) : 0, new_size);
    if (!array) {
        new_hdr->len = 0;
    }
    new_hdr->cap = new_cap;
    return new_hdr->data;
}