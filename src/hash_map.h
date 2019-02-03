// hash_map.h - Johan Smet - BSD-3-Clause (see LICENSE)
//
// an associative container (open addressing, linear probing)
//  - basic implementation to avoid dependencies on external library
//  - limited operations: only add / lookup
//  - hashmap does not copy objects - keep them around long enough or bad things will happen
//  - iterators can be invalidated by changing the container during iteration

#ifndef JS_HASH_MAP_H
#define JS_HASH_MAP_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef union MapValue {
    uint64_t    as_uint64;
    void *      as_ptr;
    const void *as_const_ptr;
} MapValue;

typedef struct HashMap {
    size_t len;
    size_t cap;
    void *data;
    MapValue *keys;
    MapValue *vals;
    uint8_t *meta;
} HashMap;

inline static size_t map_len(HashMap *map) {
    return map->len;
}

void map_int_int_put(HashMap *map, uint64_t key, uint64_t value);
uint64_t map_int_int_get(HashMap *map, uint64_t key);
bool map_int_int_has(HashMap *map, uint64_t key);

void map_ptr_ptr_put(HashMap *map, void *key, void *value); 
void *map_ptr_ptr_get(HashMap *map, void *key);
bool map_ptr_ptr_has(HashMap *map, void *key);

void map_int_ptr_put(HashMap *map, uint64_t key, void *value); 
void *map_int_ptr_get(HashMap *map, uint64_t key);
bool map_int_ptr_has(HashMap *map, uint64_t key);

void map_int_str_put(HashMap *map, uint64_t key, const char *value); 
const char *map_int_str_get(HashMap *map, uint64_t key);
bool map_int_str_has(HashMap *map, uint64_t key);

void map_str_int_put(HashMap *map, const char *key, uint64_t value);
uint64_t map_str_int_get(HashMap *map, const char *key);
bool map_str_int_has(HashMap *map, const char *key);

void map_str_ptr_put(HashMap *map, const char *key, void *value);
void *map_str_ptr_get(HashMap *map, const char *key);
bool map_str_ptr_has(HashMap *map, const char *key);

void map_str_str_put(HashMap *map, const char *key, const char *value);
const char *map_str_str_get(HashMap *map, const char *key);
bool map_str_str_has(HashMap *map, const char *key);

void map_free(HashMap *map);

// iteration over elements
int map_begin(HashMap *map);
int map_end(HashMap *map);
int map_next(HashMap *map, int cur);

#define map_key(map, idx)      ((map)->keys[(idx)].as_ptr)
#define map_key_str(map, idx)  ((const char *) (map)->keys[(idx)].as_const_ptr)
#define map_key_int(map, idx)  ((map)->keys[(idx)].as_uint64)
#define map_val(map, idx)      ((map)->vals[(idx)].as_ptr)
#define map_val_str(map, idx)  ((const char *) (map)->vals[(idx)].as_const_ptr)
#define map_val_int(map, idx)  ((map)->vals[(idx)].as_uint64)



#endif // JS_HASH_MAP
