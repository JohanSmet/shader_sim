// hash_map.c - Johan Smet - BSD-3-Clause (see LICENSE)
//
// an associative container (open addressing, linear probing)

#include "hash_map.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define META_EMPTY  0
#define META_IN_USE 2

static uint64_t hash_uint64(uint64_t key) {
    // mixer from MurmurHash3 (public domain -- see https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp)
    key ^= (key >> 33);
    key *= 0xff51afd7ed558ccd;
    key ^= (key >> 33);
    key *= 0xc4ceb9fe1a85ec53;
    key ^= (key >> 33);
    return key;
}

static uint64_t hash_ptr(void *key) {
    return hash_uint64((uint64_t) key);
}

static uint64_t hash_str(const char *ptr) {
    // fnv-hash
    uint64_t x = 0xcbf29ce484222325;

    for (const char *chr = ptr; *chr; ++chr) {
        x ^= (unsigned char) *chr;
        x *= 0x100000001b3;
        x ^= x >> 32;
    }
    
    return x;
}

static_assert(sizeof(MapValue) <= sizeof(uint64_t), "MapValue is bigger that uint64, change hash function!");
#define hash_mapvalue(x)   hash_uint64(*((uint64_t *) (&x)))

typedef void (*HASHMAP_PUT_FUNC)(HashMap *map, MapValue key, MapValue val);

static void hashmap_put_int(HashMap *map, MapValue key, MapValue val) {
    assert(map != NULL);

    uint64_t idx = hash_mapvalue(key) % map->cap;

    for (;;) {

        if (map->meta[idx] == META_EMPTY) {
            map->keys[idx] = key;
            map->vals[idx] = val;
            map->meta[idx] = META_IN_USE;
            map->len++;
            return;
        } else if (memcmp(&map->keys[idx], &key, sizeof(key)) == 0) {
            map->vals[idx] = val;
            return;
        }

        idx = (idx + 1) % map->cap;
    }
}

static void hashmap_put_str(HashMap *map, MapValue key, MapValue val) {
    uint64_t idx = hash_str((const char *) key.as_ptr) % map->cap;

    for (;;) {
        idx = idx % map->cap;

        if (map->meta[idx] == META_EMPTY) {
            map->keys[idx] = key;
            map->vals[idx] = val;
            map->meta[idx] = META_IN_USE;
            map->len++;
            return;
        } else if (strcmp((const char *) map->keys[idx].as_ptr, (const char *) key.as_ptr) == 0) {
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
        .data = calloc(new_cap, sizeof(MapValue) * 2 + sizeof(uint8_t))
    };
    new_map.keys = (MapValue *) new_map.data;
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

static MapValue *hashmap_get_int(HashMap *map, MapValue key, uint64_t hash) {
    assert(map);

    if (map->cap == 0) {
        return NULL;
    }

    uint64_t idx = hash % map->cap;

    for (;;) {
        if (map->meta[idx] == META_EMPTY) {
            return NULL;
        } else if (memcmp(&map->keys[idx], &key, sizeof(key)) == 0) {
            return &map->vals[idx];
        }
        idx = (idx + 1) % map->cap;
    }
}

static MapValue *hashmap_get_str(HashMap *map, MapValue key, uint64_t hash)
{
    assert(map);

    if (map->cap == 0) {
        return NULL;
    }

    uint64_t idx = hash % map->cap;

    for (;;) {
        if (map->meta[idx] == META_EMPTY) {
            return NULL;
        } else if (strcmp(map->keys[idx].as_const_ptr, key.as_const_ptr) == 0) {
            return &map->vals[idx];
        }
        idx = (idx + 1) % map->cap;
    }
}

/*
 * interface - map<int, int>
 */

void map_int_int_put(HashMap *map, uint64_t key, uint64_t value) {
    assert(map);

    // double the container capacity when it becomes half full
    if (map->len * 2 >= map->cap) {
        hashmap_grow(map, 2 * map->cap, hashmap_put_int);
    }

    hashmap_put_int(map, (MapValue) {.as_uint64=key}, (MapValue) {.as_uint64=value});
}

uint64_t map_int_int_get(HashMap *map, uint64_t key) {
    assert(map);

    MapValue *val = hashmap_get_int(map,(MapValue) {.as_uint64 = key}, hash_uint64(key));
    return (val != NULL) ? val->as_uint64 : 0;
}

bool map_int_int_has(HashMap *map, uint64_t key) {
    assert(map);

    MapValue *val = hashmap_get_int(map,(MapValue) {.as_uint64 = key}, hash_uint64(key));
    return val != NULL;
}

/*
 * interface - map<ptr, ptr>
 */

void map_ptr_ptr_put(HashMap *map, void *key, void *value) {
    assert(map);

    // double the container capacity when it becomes half full
    if (map->len * 2 >= map->cap) {
        hashmap_grow(map, 2 * map->cap, hashmap_put_int);
    }

    hashmap_put_int(map, (MapValue) {.as_ptr=key}, (MapValue) {.as_ptr=value});
}

void *map_ptr_ptr_get(HashMap *map, void *key) {
    assert(map);

    MapValue *val = hashmap_get_int(map,(MapValue) {.as_ptr = key}, hash_ptr(key));
    return (val != NULL) ? val->as_ptr : NULL;
}

bool map_ptr_ptr_has(HashMap *map, void *key) {
    assert(map);

    MapValue *val = hashmap_get_int(map,(MapValue) {.as_ptr = key}, hash_ptr(key));
    return val != NULL;
}

/*
 * interface - map<int, ptr>
 */

void map_int_ptr_put(HashMap *map, uint64_t key, void *value) {
    // double the container capacity when it becomes half full
    if (map->len * 2 >= map->cap) {
        hashmap_grow(map, 2 * map->cap, hashmap_put_int);
    }

    hashmap_put_int(map, (MapValue) {.as_uint64=key}, (MapValue) {.as_ptr=value});
}

void *map_int_ptr_get(HashMap *map, uint64_t key) {
    assert(map);

    MapValue *val = hashmap_get_int(map,(MapValue) {.as_uint64 = key}, hash_uint64(key));
    return (val != NULL) ? val->as_ptr : NULL;
}

bool map_int_ptr_has(HashMap *map, uint64_t key) {
    assert(map);

    MapValue *val = hashmap_get_int(map,(MapValue) {.as_uint64 = key}, hash_uint64(key));
    return val != NULL;
}

/*
 * interface - map<int, str>
 */

void map_int_str_put(HashMap *map, uint64_t key, const char *value) {
    // double the container capacity when it becomes half full
    if (map->len * 2 >= map->cap) {
        hashmap_grow(map, 2 * map->cap, hashmap_put_int);
    }

    hashmap_put_int(map, (MapValue) {.as_uint64=key}, (MapValue) {.as_const_ptr=value});
}

const char *map_int_str_get(HashMap *map, uint64_t key) {
    assert(map);

    MapValue *val = hashmap_get_int(map,(MapValue) {.as_uint64 = key}, hash_uint64(key));
    return (val != NULL) ? val->as_const_ptr : NULL;
}

bool map_int_str_has(HashMap *map, uint64_t key) {
    assert(map);

    MapValue *val = hashmap_get_int(map,(MapValue) {.as_uint64 = key}, hash_uint64(key));
    return val != NULL;
}

/*
 * interface - map<str, int>
 */

void map_str_int_put(HashMap *map, const char *key, uint64_t value) {
    assert(map);

    // double the container capacity when it becomes half full
    if (map->len * 2 >= map->cap) {
        hashmap_grow(map, 2 * map->cap, hashmap_put_str);
    }

    hashmap_put_str(map, (MapValue) {.as_const_ptr = key}, (MapValue) {.as_uint64 = value});
}

uint64_t map_str_int_get(HashMap *map, const char *key) {
    assert(map);

    MapValue *val = hashmap_get_str(map,(MapValue) {.as_const_ptr = key}, hash_str(key));
    return (val != NULL) ? val->as_uint64 : 0;
}

bool map_str_int_has(HashMap *map, const char *key) {
    assert(map);
    
    MapValue *val = hashmap_get_str(map,(MapValue) {.as_const_ptr = key}, hash_str(key));
    return val != NULL;
}

/*
 * interface - map<str, ptr>
 */

void map_str_ptr_put(HashMap *map, const char *key, void *value) {
    assert(map);

    // double the container capacity when it becomes half full
    if (map->len * 2 >= map->cap) {
        hashmap_grow(map, 2 * map->cap, hashmap_put_str);
    }

    hashmap_put_str(map, (MapValue) {.as_const_ptr = key}, (MapValue) {.as_ptr = value});
}

void *map_str_ptr_get(HashMap *map, const char *key) {
    assert(map);

    MapValue *val = hashmap_get_str(map,(MapValue) {.as_const_ptr = key}, hash_str(key));
    return (val != NULL) ? val->as_ptr : 0;
}

bool map_str_ptr_has(HashMap *map, const char *key) {
    assert(map);

    MapValue *val = hashmap_get_str(map,(MapValue) {.as_const_ptr = key}, hash_str(key));
    return val != NULL;
}

/*
 * interface - map<str, str>
 */

void map_str_str_put(HashMap *map, const char *key, const char *value) {
    assert(map);

    // double the container capacity when it becomes half full
    if (map->len * 2 >= map->cap) {
        hashmap_grow(map, 2 * map->cap, hashmap_put_str);
    }

    hashmap_put_str(map, (MapValue) {.as_const_ptr = key}, (MapValue) {.as_const_ptr = value});
}

const char *map_str_str_get(HashMap *map, const char *key) {
    assert(map);

    MapValue *val = hashmap_get_str(map,(MapValue) {.as_const_ptr = key}, hash_str(key));
    return (val != NULL) ? val->as_const_ptr : 0;
}

bool map_str_str_has(HashMap *map, const char *key) {
    assert(map);

    MapValue *val = hashmap_get_str(map,(MapValue) {.as_const_ptr = key}, hash_str(key));
    return val != NULL;
}

/*
 * other interface functions
 */

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
    
    while (map->meta[result] != META_IN_USE && result < (int) map->cap) {
        ++result;
    }

    return result;
}

int map_end(HashMap *map) {
    return (int) map->cap;
}

int map_next(HashMap *map, int cur) {
    int result = cur + 1;

    while (result < (int) map->cap && map->meta[result] != META_IN_USE) {
        ++result;
    }

    return result;
}
