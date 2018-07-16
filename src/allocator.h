//
//  allocator.c - Johan Smet - BSD-3-Clause (see LICENSE)
//
//  Memory allocation abstractions

#ifndef JS_ALLOCATOR_H
#define JS_ALLOCATOR_H

#include "types.h"

// types
typedef struct MemArena MemArena;
struct MemArena {
    size_t block_size;
    uintptr_t align;
    
    uint8_t *ptr;
    uint8_t *end;
    uint8_t **blocks;   // dyn_array
};

#define ARENA_DEFAULT_SIZE (1024 * 1024)
#define ARENA_DEFAULT_ALIGN 8

// interface functions
void mem_arena_init(MemArena *arena, size_t block_size, uint32_t align);
void *mem_arena_allocate(MemArena *arena, size_t num_bytes);
void mem_arena_free(MemArena *arena);


#endif /* JS_ALLOCATOR_H */
