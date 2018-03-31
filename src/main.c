// main.c - Johan Smet - BSD-3-Clause (see LICENSE)

#include <stdio.h>
#include "dyn_array.h"

int main(int argc, char *argv[]) {

    int *test_array = NULL;

    assert(arr_len(test_array) == 0);

    arr_push(test_array, 5);
    assert(arr_len(test_array) == 1);
    assert(test_array[0] == 5);

    arr_free(test_array);
    assert(arr_len(test_array) == 0);

    enum {ARRAY_LEN = 1024};
    for (int idx = 1; idx <= ARRAY_LEN; ++idx) {
        arr_push(test_array, idx);
        assert(arr_len(test_array) == idx);
    }
    assert(arr_len(test_array) == ARRAY_LEN);

    int value = 1;
    for (int *it = test_array; it != arr_end(test_array); ++it) {
        assert(*it == value++);
    }


    return 0;
}