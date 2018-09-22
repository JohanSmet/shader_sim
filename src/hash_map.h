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

typedef struct HashMap {
    size_t len;
    size_t cap;
    void *data;
    uint64_t *keys;
    uint64_t *vals;
    uint8_t *meta;
} HashMap;

inline static size_t map_len(HashMap *map) {
    return map->len;
}

void map_int_int_put(HashMap *map, uint64_t key, uint64_t value);
uint64_t map_int_int_get(HashMap *map, uint64_t key);
bool map_int_int_has(HashMap *map, uint64_t key);

#define map_ptr_ptr_put(map, key, value)    map_int_int_put((map), ((uint64_t) key), ((uint64_t) value))
#define map_ptr_ptr_get(map, key)           (void *) map_int_int_get((map), ((uint64_t) key))
#define map_ptr_ptr_has(map, key)           map_int_int_has((map), ((uint64_t) key))

#define map_int_ptr_put(map, key, value)    map_int_int_put((map), (key), ((uint64_t) value))
#define map_int_ptr_get(map, key)           (void *) map_int_int_get((map), (key))
#define map_int_ptr_has(map, key)           map_int_int_has((map), (key))

#define map_int_str_put(map, key, value)    map_int_ptr_put((map), (key), (value))
#define map_int_str_get(map, key)           map_int_ptr_get((map), (key))
#define map_int_str_has(map, key)           map_int_ptr_has((map), (key))

void map_str_int_put(HashMap *map, const char *key, uint64_t value);
uint64_t map_str_int_get(HashMap *map, const char *key);
bool map_str_int_has(HashMap *map, const char *key);

#define map_str_ptr_put(m, k, v)    map_str_int_put((m), (k), (uint64_t)(v))
#define map_str_ptr_get(m, k)       (void *) map_str_int_get((m),(k))
#define map_str_ptr_has(m, k)       map_str_int_get((m),(k))

#define map_str_str_put(m, k, v)    map_str_ptr_put((map), (key), (value))
#define map_str_str_get(m, k)       map_str_ptr_get((map), (key))
#define map_str_str_has(m, k)       map_str_ptr_has((map), (key))

void map_free(HashMap *map);

// iteration over elements
int map_begin(HashMap *map);
int map_end(HashMap *map);
int map_next(HashMap *map, int cur);

#define map_key(map, idx)      ((void *) (map)->keys[(idx)])
#define map_key_str(map, idx)  ((const char *) (map)->keys[(idx)])
#define map_key_int(map, idx)  ((int64_t) (map)->keys[(idx)])
#define map_val(map, idx)      ((void *) (map)->vals[(idx)])
#define map_val_str(map, idx)  ((const char *) (map)->vals[(idx)])
#define map_val_int(map, idx)  ((int64_t) (map)->vals[(idx)])



#endif // JS_HASH_MAP
