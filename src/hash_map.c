// hash_map.c - Johan Smet - BSD-3-Clause (see LICENSE)
//
// an associative container (open addressing, linear probing)

#include "hash_map.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define META_EMPTY  0
#define META_IN_USE 2

uint64_t hash_uint64(uint64_t key) {
    // mixer from MurmurHash3 (public domain -- see https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp)
    key ^= (key >> 33);
    key *= 0xff51afd7ed558ccd;
    key ^= (key >> 33);
    key *= 0xc4ceb9fe1a85ec53;
    key ^= (key >> 33);
    return key;
}

uint64_t hash_ptr(void *key) {
    return hash_uint64((uint64_t) key);
}

uint64_t hash_str(const char *ptr) {
    // fnv-hash
    uint64_t x = 0xcbf29ce484222325;

    for (const char *chr = ptr; *chr; ++chr) {
        x ^= *chr;
        x *= 0x100000001b3;
        x ^= x >> 32;
    }
    
    return x;
}

typedef void (*HASHMAP_PUT_FUNC)(HashMap *map, uint64_t key, uint64_t val);

static void hashmap_put_int(HashMap *map, uint64_t key, uint64_t val) {
    uint64_t idx = hash_uint64(key) % map->cap;

    for (;;) {
        idx = idx % map->cap;

        if (map->meta[idx] == META_EMPTY) {
            map->keys[idx] = key;
            map->vals[idx] = val;
            map->meta[idx] = META_IN_USE;
            map->len++;
            return;
        } else if (map->keys[idx] == key) {
            map->vals[idx] = val;
            return;
        }

        ++idx;
    }
}

static void hashmap_put_str(HashMap *map, uint64_t key, uint64_t val) {
    uint64_t idx = hash_str((const char *) key) % map->cap;

    for (;;) {
        idx = idx % map->cap;

        if (map->meta[idx] == META_EMPTY) {
            map->keys[idx] = key;
            map->vals[idx] = val;
            map->meta[idx] = META_IN_USE;
            map->len++;
            return;
        } else if (strcmp((const char *) map->keys[idx], (const char *) key) == 0) {
            map->vals[idx] = val;
            return;
        }

        ++idx;
    }
}

static void hashmap_grow(HashMap *map, size_t new_cap, HASHMAP_PUT_FUNC put_func) {
    if (new_cap < 16) {
        new_cap = 16;
    }

    HashMap new_map = (HashMap) {
        .len = 0,
        .cap = new_cap,
        .data = calloc(new_cap, sizeof(uint64_t) * 2 + sizeof(uint8_t))
    };
    new_map.keys = (uint64_t *) new_map.data;
    new_map.vals = new_map.keys + new_cap;
    new_map.meta = (uint8_t *) (new_map.vals + new_cap);

    for (uint64_t idx = 0; idx < map->cap; ++idx) {
        if (map->meta[idx] != META_EMPTY) {
            put_func(&new_map, map->keys[idx], map->vals[idx]);
        }
    }

    free(map->data);
    *map = new_map;
}

void map_int_int_put(HashMap *map, uint64_t key, uint64_t value) {

    // double container capacity when it becomes half full
    if (map->len * 2 >= map->cap) {
        hashmap_grow(map, 2 * map->cap, hashmap_put_int);
    }

    hashmap_put_int(map, key, value);
}

uint64_t map_int_int_get(HashMap *map, uint64_t key) {
    assert(map);

    if (map->cap == 0) {
        return 0;
    }

    uint64_t idx = hash_uint64(key) % map->cap;

    for (;;) {
        idx = idx % map->cap;
        
        if (map->meta[idx] == META_EMPTY) {
            return 0;
        } else if (map->keys[idx] == key) {
            return map->vals[idx];
        }
        ++idx;
    }

    return 0;
}

bool map_int_int_has(HashMap *map, uint64_t key) {
    
    assert(map);
    
    if (map->cap == 0) {
        return false;
    }
    
    uint64_t idx = hash_uint64(key) % map->cap;
    return map->meta[idx] == META_IN_USE;
}

void map_str_int_put(HashMap *map, const char *key, uint64_t value) {

    // double the container capacity when it becomes half full
    if (map->len * 2 >= map->cap) {
        hashmap_grow(map, 2 * map->cap, hashmap_put_str);
    }

    hashmap_put_str(map, (uint64_t) key, value);
}

uint64_t map_str_int_get(HashMap *map, const char *key) {
    assert(map);

    if (map->cap == 0) {
        return 0;
    }

    uint64_t idx = hash_str(key) % map->cap;

    for (;;) {
        idx = idx % map->cap;

        if (map->meta[idx] == META_EMPTY) {
            return 0;
        } else if (strcmp((const char *) map->keys[idx], key) == 0) {
            return map->vals[idx];
        }
        ++idx;
    }

    return 0;
}

bool map_str_int_has(HashMap *map, const char *key) {
    assert(map);
    
    if (map->cap == 0) {
        return false;
    }
    
    uint64_t idx = hash_str(key) % map->cap;
    return map->meta[idx] == META_IN_USE;
}

void map_free(HashMap *map) {
    assert(map);
    
    free(map->data);
    *map = (HashMap) {0};
}

int map_begin(HashMap *map) {
    int result = 0;
    
    if (map->cap == 0) {
        return result;
    }
    
    while (map->meta[result] != META_IN_USE && result < map->cap) {
        ++result;
    }

    return result;
}

int map_end(HashMap *map) {
    return map->cap;
}

int map_next(HashMap *map, int cur) {
    int result = cur + 1;

    while (map->meta[result] != META_IN_USE && result < map->cap) {
        ++result;
    }

    return result;
}
