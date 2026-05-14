#include "kvstore.h"
#include <stdlib.h>
#include <string.h>
#include "memory_tracking.h"

/* ---- Internal structures (opaque in kvstore.h) ---- */

typedef struct KVNode {
    char *key;
    char *value;
    struct KVNode *next;
} KVNode;

struct KVStore {
    KVNode **buckets;
    size_t capacity;
    size_t count;
};

/* ---- djb2 hash (spec §3.3) ---- */

static size_t kv_hash(const char *key, size_t capacity)
{
    unsigned long hash = 5381;
    for (int i = 0; key[i] != '\0'; i++) {
        hash = hash * 33 + (unsigned char)key[i];
    }
    return (size_t)(hash % capacity);
}

/* ---- Lifecycle (spec §2.2) ---- */

KVStore *kv_create(size_t initial_capacity)
{
    if (initial_capacity < 16)
        initial_capacity = 16;

    KVStore *store = (KVStore *)malloc(sizeof(KVStore));
    if (!store)
        return NULL;

    store->capacity = initial_capacity;
    store->count = 0;
    store->buckets = (KVNode **)malloc(sizeof(KVNode *) * initial_capacity);
    if (!store->buckets) {
        free(store);
        return NULL;
    }
    memset(store->buckets, 0, sizeof(KVNode *) * initial_capacity);

    return store;
}

void kv_destroy(KVStore *store)
{
    if (!store)
        return;

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

/* ---- Internal resize helper (spec §3.5) ---- */

static int kv_resize(KVStore *store)
{
    size_t new_capacity = store->capacity * 2;
    KVNode **new_buckets = (KVNode **)malloc(sizeof(KVNode *) * new_capacity);
    if (!new_buckets)
        return -1; /* silently absorbed by caller per §3.5.1 */

    memset(new_buckets, 0, sizeof(KVNode *) * new_capacity);

    for (size_t i = 0; i < store->capacity; i++) {
        KVNode *node = store->buckets[i];
        while (node) {
            KVNode *next = node->next;
            size_t idx = kv_hash(node->key, new_capacity);
            node->next = new_buckets[idx];
            new_buckets[idx] = node;
            node = next;
        }
    }

    free(store->buckets);
    store->buckets = new_buckets;
    store->capacity = new_capacity;
    return 0;
}

/* ---- Core operations (spec §2.1) ---- */

int kv_insert(KVStore *store, const char *key, const char *value)
{
    if (!store || !key || !value)
        return -1;

    /* Check for existing key — update in place */
    size_t idx = kv_hash(key, store->capacity);
    KVNode *node = store->buckets[idx];
    while (node) {
        if (strcmp(node->key, key) == 0) {
            char *new_val = (char *)malloc(strlen(value) + 1);
            if (!new_val)
                return -1;
            strcpy(new_val, value);
            free(node->value);
            node->value = new_val;
            return 0;
        }
        node = node->next;
    }

    /* New key: check load factor and resize before inserting (spec §3.5) */
    if ((store->count + 1) > (size_t)(store->capacity * 0.9)) {
        kv_resize(store); /* silently ignored on failure per §3.5.1 */
    }

    /* Allocate node and strings */
    KVNode *new_node = (KVNode *)malloc(sizeof(KVNode));
    if (!new_node)
        return -1;
    new_node->key = (char *)malloc(strlen(key) + 1);
    if (!new_node->key) {
        free(new_node);
        return -1;
    }
    strcpy(new_node->key, key);
    new_node->value = (char *)malloc(strlen(value) + 1);
    if (!new_node->value) {
        free(new_node->key);
        free(new_node);
        return -1;
    }
    strcpy(new_node->value, value);

    /* Prepend to chain */
    idx = kv_hash(key, store->capacity);
    new_node->next = store->buckets[idx];
    store->buckets[idx] = new_node;
    store->count++;
    return 0;
}

const char *kv_get(const KVStore *store, const char *key)
{
    if (!store || !key)
        return NULL;

    size_t idx = kv_hash(key, store->capacity);
    const KVNode *node = store->buckets[idx];
    while (node) {
        if (strcmp(node->key, key) == 0)
            return node->value;
        node = node->next;
    }
    return NULL;
}

int kv_delete(KVStore *store, const char *key)
{
    if (!store || !key)
        return -1;

    size_t idx = kv_hash(key, store->capacity);
    KVNode **pp = &store->buckets[idx];
    while (*pp) {
        if (strcmp((*pp)->key, key) == 0) {
            KVNode *victim = *pp;
            *pp = victim->next;
            free(victim->key);
            free(victim->value);
            free(victim);
            store->count--;
            return 0;
        }
        pp = &(*pp)->next;
    }
    return -1; /* not found */
}
