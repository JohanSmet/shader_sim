//  allocator.c - Johan Smet - BSD-3-Clause (see LICENSE)
//
//  Memory allocation abstractions

#include "allocator.h"
#include "dyn_array.h"

#include <assert.h>
#include <stdlib.h>

static inline void mem_arena_grow(MemArena *arena, size_t min_size) {
    size_t new_size = ALIGN_UP(MAX(min_size, arena->block_size), arena->align);
    
    arena->ptr = malloc(new_size);
    assert(arena->ptr == PTR_ALIGN_DOWN(arena->ptr, arena->align));
    
    arena->end = arena->ptr + new_size;
    arr_push(arena->blocks, arena->ptr);
}

void mem_arena_init(MemArena *arena, size_t block_size, uint32_t align) {
    assert(arena);
    
    *arena = (MemArena) {
        .block_size = block_size,
        .align = align
    };
}

void *mem_arena_allocate(MemArena *arena, size_t num_bytes) {
    
    assert(arena);
    
    if (num_bytes > (arena->end - arena->ptr)) {
        mem_arena_grow(arena, num_bytes);
    }
    
    void *result = arena->ptr;
    
    arena->ptr = PTR_ALIGN_UP(arena->ptr + num_bytes, arena->align);
    assert(arena->ptr <= arena->end);

    return result;
}

void mem_arena_free(MemArena *arena) {
    assert(arena);
    
    for (uint8_t **iter = arena->blocks; iter != arr_end(arena->blocks); ++iter) {
        free(*iter);
    }
    arr_free(arena->blocks);
}
