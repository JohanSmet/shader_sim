// hash_map.c - Johan Smet - BSD-3-Clause (see LICENSE)
//
// an associative container (open addressing, linear probing)

#include "hash_map.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define KEY_EMPTY   NULL

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

typedef void (*HASHMAP_PUT_FUNC)(HashMap *map, void *key, void *val);

static void hashmap_put_ptr(HashMap *map, void *key, void *val) {
    uint64_t idx = hash_ptr(key) % map->cap;

    for (;;) {
        idx = idx % map->cap;

        if (map->keys[idx] == KEY_EMPTY) {
            map->keys[idx] = key;
            map->vals[idx] = val;
            map->len++;
            return;
        } else if (map->keys[idx] == key) {
            map->vals[idx] = val;
            return;
        }

        ++idx;
    }
}

static void hashmap_put_str(HashMap *map, void *key, void *val) {
    const char *str_key = (const char *) key;

    uint64_t idx = hash_str(key) % map->cap;

    for (;;) {
        idx = idx % map->cap;

        if (map->keys[idx] == KEY_EMPTY) {
            map->keys[idx] = key;
            map->vals[idx] = val;
            map->len++;
            return;
        } else if (strcmp(map->keys[idx], key) == 0) {
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
        .data = calloc(new_cap * 2, sizeof(void *))
    };
    new_map.keys = new_map.data;
    new_map.vals = new_map.keys + new_cap;

    for (uint64_t idx = 0; idx < map->cap; ++idx) {
        if (map->keys[idx] != KEY_EMPTY) {
            put_func(&new_map, map->keys[idx], map->vals[idx]);
        }
    }

    free(map->data);
    *map = new_map;
}

void map_ptr_ptr_put(HashMap *map, void *key, void *value) {
    assert(key);

    // double container capacity when it becomes half full
    if (map->len * 2 >= map->cap) {
        hashmap_grow(map, 2 * map->cap, hashmap_put_ptr);
    }

    hashmap_put_ptr(map, key, value);
}

void *map_ptr_ptr_get(HashMap *map, void *key) {
    assert(map);
    assert(key);

    if (map->cap == 0) {
        return NULL;
    }

    uint64_t idx = hash_ptr(key) % map->cap;

    for (;;) {
        idx = idx % map->cap;

        if (map->keys[idx] == key) {
            return map->vals[idx];
        } else if (map->keys[idx] == KEY_EMPTY) {
            return NULL;
        }
        ++idx;
    }

    return NULL;
}

void map_str_ptr_put(HashMap *map, const char *key, void *value) {
    assert(key);

    // double the container capacity when it becomes half full
    if (map->len * 2 >= map->cap) {
        hashmap_grow(map, 2 * map->cap, hashmap_put_str);
    }

    hashmap_put_str(map, (void *) key, value);
}

void *map_str_ptr_get(HashMap *map, const char *key) {
    assert(map);
    assert(key);

    if (map->cap == 0) {
        return NULL;
    }

    uint64_t idx = hash_str(key) % map->cap;

    for (;;) {
        idx = idx % map->cap;

        if (strcmp((const char *) map->keys[idx], key) == 0) {
            return map->vals[idx];
        } else if (map->keys[idx] == KEY_EMPTY) {
            return NULL;
        }
        ++idx;
    }

    return NULL;
}

int map_begin(HashMap *map) {
    int result = 0;

    while (map->keys[result] == NULL && result < map->cap) {
        ++result;
    }

    return result;
}

int map_end(HashMap *map) {
    return map->cap;
}

int map_next(HashMap *map, int cur) {
    int result = cur + 1;

    while (map->keys[result] == NULL && result < map->cap) {
        ++result;
    }

    return result;
}