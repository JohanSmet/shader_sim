// dyn_array.h - Johan Smet - BSD-3-Clause (see LICENSE)
//
// a dynamic array for C (C++ vector-like growable array) 
//  inspired by stretchy_buffers by stb (http://github.com/nothings/stb)
//  based on code from Bitwise (https://bitwise.handmade.network/)

#ifndef JS_DYN_ARRAY_H
#define JS_DYN_ARRAY_H

#include <stdlib.h>
#include <stddef.h>
#include <assert.h>

typedef struct ArrayHeader {
    size_t len;
    size_t cap;
    char data[];
} ArrayHeader;

#define arr__hdr(a) ((ArrayHeader *) ((char *)(a) - offsetof(ArrayHeader, data)))
#define arr__fits(a, n) ((arr_len(a) + (n) <= arr_cap(a)))
#define arr__fit(a, n) (arr__fits(a, n) ? 0 : ((a) = arr__grow((a), arr_len(a) + (n), sizeof(*(a)))))

#define arr_len(a) ((a) ? arr__hdr((a))->len : 0)
#define arr_cap(a) ((a) ? arr__hdr((a))->cap : 0)
#define arr_push(a, i) (arr__fit((a), 1), (a)[arr__hdr(a)->len++] = (i))
#define arr_free(a) ((a) ? (free(arr__hdr(a)), (a) = NULL) : 0)
#define arr_clear(a) ((a) ? arr__hdr((a))->len = 0 : 0)
#define arr_end(a) ((a) + arr_len(a))

static void *arr__grow(const void *array, size_t new_len, size_t elem_size) {
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

#endif // JS_DYN_ARRAY_H