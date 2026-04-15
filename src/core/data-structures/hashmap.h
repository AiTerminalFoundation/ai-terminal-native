#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdint.h>

typedef int64_t (*hash_fn_type)(const void *key);
typedef int    (*comparison_fn_type)(const void *a, const void *b);

typedef struct {
    void *key;
    void *value;
    struct Entry *next;
} Entry;

typedef struct {
    Entry **buckets;
    hash_fn_type hash_fn;
    comparison_fn_type comparison_fn;
    int64_t capacity;
    int64_t current_size;
} HashMap;

int hashmap_put(HashMap *hashmap, void *key, void *value);
int hashmap_remove(HashMap *hashmap, void *key);
void* hashmap_get(HashMap *hashmap, void *key);


#endif
