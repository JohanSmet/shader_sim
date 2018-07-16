// types.h - Johan Smet - BSD-3-Clause (see LICENSE)
//
// Easy include to make sure commonly used basic types are defined

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// some basic operation
#define MIN(a,b)    ((a) <= (b) ? (a) : (b))
#define MAX(a,b)    ((a) >= (b) ? (a) : (b))
#define IS_POW2(x)  ((x) != 0 && ((x) & ((x)-1)) == 0)

#define ALIGN_DOWN(x, a) ((x) & ~((a) - 1))
#define ALIGN_UP(x, a) ALIGN_DOWN((x) + (a) - 1, (a))

#define PTR_ALIGN_DOWN(x, a) ((void *) ALIGN_DOWN((uintptr_t) (x), (a)))
#define PTR_ALIGN_UP(x, a) ((void *) ALIGN_UP((uintptr_t) (x), (a)))
