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
    void **keys;
    void **vals;
    uint8_t *meta;
} HashMap;

inline static size_t map_len(HashMap *map) {
    return map->len;
}

void map_ptr_ptr_put(HashMap *map, void *key, void *value);
void *map_ptr_ptr_get(HashMap *map, void *key);
bool map_ptr_ptr_has(HashMap *map, void *key);

#define map_int_int_put(map, key, value)    map_ptr_ptr_put((map), (void *) ((int64_t) key), (void *) ((int64_t) value))
#define map_int_int_get(map, key)           (int64_t) map_ptr_ptr_get((map), (void *) ((int64_t) key))
#define map_int_int_has(map, key)           map_ptr_ptr_has((map), (void *) ((int64_t) key))

#define map_int_ptr_put(map, key, value)    map_ptr_ptr_put((map), (void *) ((int64_t) key), (value))
#define map_int_ptr_get(map, key)           map_ptr_ptr_get((map), (void *) ((int64_t) key))
#define map_int_ptr_has(map, key)           map_ptr_ptr_has((map), (void *) ((int64_t) key))

#define map_int_str_put(map, key, value)    map_int_ptr_put((map), (key), (value))
#define map_int_str_get(map, key)           map_int_ptr_get((map), (key))
#define map_int_str_has(map, key)           map_int_ptr_has((map), (key))

void map_str_ptr_put(HashMap *map, const char *key, void *value);
void *map_str_ptr_get(HashMap *map, const char *key);
bool map_str_ptr_has(HashMap *map, const char *key);

#define map_str_int_put(m, k, v)    map_str_ptr_put((m), (k), (void *)(v))
#define map_str_int_get(m, k)       (int64_t) map_str_ptr_get((m),(k))
#define map_str_int_has(m, k)       map_str_ptr_get((m),(k))

#define map_str_str_put(m, k, v)    map_str_ptr_put((map), (key), (value))
#define map_str_str_get(m, k)       map_str_ptr_get((map), (key))
#define map_str_str_has(m, k)       map_str_ptr_has((map), (key))

void map_free(HashMap *map);

// iteration over elements
int map_begin(HashMap *map);
int map_end(HashMap *map);
int map_next(HashMap *map, int cur);

#define map_key(map, idx)      ((map)->keys[(idx)])
#define map_key_str(map, idx)  ((const char *) (map)->keys[(idx)])
#define map_key_int(map, idx)  ((int64_t) (map)->keys[(idx)])
#define map_val(map, idx)      ((map)->vals[(idx)])
#define map_val_str(map, idx)  ((const char *) (map)->vals[(idx)])
#define map_val_int(map, idx)  ((int64_t) (map)->vals[(idx)])



#endif // JS_HASH_MAP
