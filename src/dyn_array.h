// dyn_array.h - Johan Smet - BSD-3-Clause (see LICENSE)
//
// a dynamic array for C (C++ vector-like growable array) 
//  inspired by stretchy_buffers by stb (http://github.com/nothings/stb)
//  based on code from Bitwise (https://bitwise.handmade.network/)

#ifndef JS_DYN_ARRAY_H
#define JS_DYN_ARRAY_H

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include <assert.h>
#include <stdio.h>

typedef struct ArrayHeader {
    size_t len;
    size_t cap;
    char data[];
} ArrayHeader;

char *arr__printf(char *array, const char *fmt, ...);
void *arr__grow(const void *array, size_t new_len, size_t elem_size);

#define arr__hdr(a) ((ArrayHeader *) (void *) ((char *)(a) - offsetof(ArrayHeader, data)))
#define arr__hdr_const(a) ((const ArrayHeader *) (const void *) ((const char *)(a) - offsetof(ArrayHeader, data)))
#define arr__fits(a, n) ((arr_len(a) + (n) <= arr_cap(a)))
#define arr__fit(a, n) (arr__fits(a, n) ? 0 : ((a) = arr__grow((a), arr_len(a) + (n), sizeof(*(a)))))

#define arr_len(a) ((a) ? arr__hdr_const((a))->len : 0)
#define arr_cap(a) ((a) ? arr__hdr_const((a))->cap : 0)
#define arr_push(a, i) (arr__fit((a), 1), (a)[arr__hdr(a)->len++] = (i))
#define arr_push_buf(a, b, n) (arr__fit((a), n), memcpy(arr_end(a), (b), (n) * sizeof(*(a))), arr__hdr(a)->len+= (n))
#define arr_free(a) ((a) ? (free(arr__hdr(a)), (a) = NULL) : 0)
#define arr_clear(a) ((a) ? arr__hdr((a))->len = 0 : 0)
#define arr_end(a) ((a) + arr_len(a))
#define arr_printf(a, ...) (a) = arr__printf((a), __VA_ARGS__)
#define arr_reserve(a, n) (arr__fit((a), (n)), arr__hdr(a)->len+=(n))

#endif // JS_DYN_ARRAY_H
