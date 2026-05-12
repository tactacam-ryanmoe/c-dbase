#include <stdlib.h>

/* Capture pointers to the real libc allocators before memtrack.h overrides them. */
static void *(*_real_malloc)(size_t) = malloc;
static void  (*_real_free)(void *)   = free;

#include <string.h>
#include <stdio.h>
#include "kvstore.h"
#include "memtrack.h"

/* ---- Internal data structures (§3.2) ---- */

typedef struct KVNode {
    char *key;
    char *value;
    struct KVNode *next;
} KVNode;

/* Full definition of the opaque type declared in kvstore.h. */
struct KVStore {
    KVNode **buckets;   /* Array of chain heads           */
    size_t   capacity;  /* Number of buckets              */
    size_t   count;     /* Total live entries             */
};

/* ---- Memory tracking (§5.1) ---- */

size_t alloc_count = 0;
size_t free_count  = 0;

void *tracked_malloc(size_t size, const char *file, int line) {
    (void)file;
    (void)line;
    void *ptr = _real_malloc(size);
    if (ptr != NULL) {
        alloc_count++;
    }
    return ptr;
}

void tracked_free(void *ptr, const char *file, int line) {
    (void)file;
    (void)line;
    if (ptr == NULL) {
        return;
    }
    _real_free(ptr);
    free_count++;
}

/* ---- Hash function (§3.3) ---- */

static size_t djb2_hash(const char *key, size_t capacity) {
    unsigned long hash = 5381;
    const char *c = key;
    while (*c) {
        hash = hash * 33 + (unsigned char)*c;
        c++;
    }
    return (size_t)(hash % capacity);
}

static size_t bucket_index(KVStore *store, const char *key) {
    return djb2_hash(key, store->capacity);
}

/* ---- Public API (§4) ---- */

KVStore *kv_create(size_t initial_capacity) {
    if (initial_capacity < 16) {
        initial_capacity = 16;
    }
    KVStore *store = (KVStore *)malloc(sizeof(KVStore));
    if (store == NULL) {
        return NULL;
    }
    store->buckets = (KVNode **)calloc(initial_capacity, sizeof(KVNode *));
    if (store->buckets == NULL) {
        free(store);
        return NULL;
    }
    store->capacity = initial_capacity;
    store->count = 0;
    return store;
}

void kv_destroy(KVStore *store) {
    /* TODO: Iterate all chains, free keys/values/nodes, then buckets array. */
    (void)store;
}

int kv_insert(KVStore *store, const char *key, const char *value) {
    if (store == NULL || key == NULL || value == NULL) {
        return -1;
    }
    /* TODO: Implement separate chaining insert with resize at 0.9 load factor. */
    (void)bucket_index(store, key);
    return -1;
}

const char *kv_get(const KVStore *store, const char *key) {
    if (store == NULL || key == NULL) {
        return NULL;
    }
    /* TODO: Implement chain traversal with strcmp. */
    (void)bucket_index((KVStore *)store, key);
    return NULL;
}

int kv_delete(KVStore *store, const char *key) {
    if (store == NULL || key == NULL) {
        return -1;
    }
    /* TODO: Implement chain traversal and node removal. */
    (void)bucket_index(store, key);
    return -1;
}
