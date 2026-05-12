#include "memtrack.h"
#include "kvstore.h"
#include <stdlib.h>
#include <string.h>

/* Internal node — one entry in a collision chain. */
typedef struct KVNode {
    char *key;
    char *value;
    struct KVNode *next;
} KVNode;

/* Full store body (opaque in kvstore.h). */
struct KVStore {
    KVNode **buckets;
    size_t capacity;
    size_t count;
};

/* ------------------------------------------------------------------ */
/* djb2 hash — spec §3.3                                              */
/* hash = 5381; for each byte c: hash = (hash * 33) + c               */
/* returns hash % capacity                                             */
/* ------------------------------------------------------------------ */
static size_t kv_hash(const char *key, size_t capacity) __attribute__((unused));
/* Note: unused attribute is temporary — removed once kv_insert/kv_get/kv_delete are implemented. */
static size_t kv_hash(const char *key, size_t capacity)
{
    unsigned long hash = 5381;
    while (*key) {
        hash = hash * 33 + (unsigned char)(*key);
        key++;
    }
    return (size_t)(hash % capacity);
}

/* ------------------------------------------------------------------ */
/* kv_create — spec §2.2                                              */
/* Clamp initial_capacity to min 16. Allocate store and buckets array  */
/* (zeroed). Return NULL on any allocation failure.                    */
/* ------------------------------------------------------------------ */
KVStore *kv_create(size_t initial_capacity)
{
    if (initial_capacity < 16) {
        initial_capacity = 16;
    }

    KVStore *store = (KVStore *)malloc(sizeof(KVStore));
    if (!store) {
        return NULL;
    }

    store->buckets = (KVNode **)calloc(initial_capacity, sizeof(KVNode *));
    if (!store->buckets) {
        free(store);
        return NULL;
    }

    store->capacity = initial_capacity;
    store->count = 0;
    return store;
}

/* ------------------------------------------------------------------ */
/* kv_destroy — spec §2.2                                             */
/* Iterate all chains, free every key/value/node. Free buckets array,  */
/* then the store struct itself. Safe to call with NULL argument.      */
/* ------------------------------------------------------------------ */
void kv_destroy(KVStore *store)
{
    if (!store) {
        return;
    }

    for (size_t i = 0; i < store->capacity; i++) {
        KVNode *node = store->buckets[i];
        while (node) {
            KVNode *next = node->next;
            free(node->key);
            free(node->value);
            free(node);
            node = next;
        }
    }

    free(store->buckets);
    free(store);
}
