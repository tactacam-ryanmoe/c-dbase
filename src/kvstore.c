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
/* Internal strdup replacement — not part of C99, so we provide it    */
/* to avoid -Wimplicit-function-declaration under -std=c99.           */
/* ------------------------------------------------------------------ */
static char *kv_strdup(const char *s)
{
    size_t len = strlen(s) + 1;
    char *dup = (char *)malloc(len);
    if (dup) {
        memcpy(dup, s, len);
    }
    return dup;
}

/* ------------------------------------------------------------------ */
/* djb2 hash — spec §3.3                                              */
/* hash = 5381; for each byte c: hash = (hash * 33) + c               */
/* returns hash % capacity                                             */
/* ------------------------------------------------------------------ */
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

/* ------------------------------------------------------------------ */
/* kv_resize — internal, spec §3.5                                     */
/* Double capacity and rehash all nodes into the new buckets array.    */
/* If new array allocation fails: silently absorb (no error returned). */
/* ------------------------------------------------------------------ */
static void kv_resize(KVStore *store)
{
    size_t new_capacity = store->capacity * 2;

    KVNode **new_buckets = (KVNode **)calloc(new_capacity, sizeof(KVNode *));
    if (!new_buckets) {
        /* §3.5.1: silently absorb — insert into existing table instead */
        return;
    }

    /* Rehash every node from old chains into new chains (prepend). */
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
}

/* ------------------------------------------------------------------ */
/* kv_insert — spec §2.1 + §3.4 + §3.5                                */
/* Insert or update a key-value pair. Both strings are copied via      */
/* kv_strdup internally. Returns 0 on success, -1 on allocation failure.  */
/* Resize is silently absorbed per §3.5.1.                             */
/* ------------------------------------------------------------------ */
int kv_insert(KVStore *store, const char *key, const char *value)
{
    if (!store || !key || !value) {
        return -1;
    }

    /* Check resize: load factor >= 0.9 (use integer math to avoid float). */
    if ((store->count * 10) / store->capacity >= 9) {
        kv_resize(store);
    }

    size_t idx = kv_hash(key, store->capacity);

    /* Traverse chain — update if key already exists. */
    KVNode *node = store->buckets[idx];
    while (node) {
        if (strcmp(node->key, key) == 0) {
            free(node->value);
            node->value = (char *)kv_strdup(value);
            return node->value ? 0 : -1;
        }
        node = node->next;
    }

    /* Key not found — prepend new node. */
    KVNode *new_node = (KVNode *)malloc(sizeof(KVNode));
    if (!new_node) {
        return -1;
    }

    new_node->key = (char *)kv_strdup(key);
    if (!new_node->key) {
        free(new_node);
        return -1;
    }

    new_node->value = (char *)kv_strdup(value);
    if (!new_node->value) {
        free(new_node->key);
        free(new_node);
        return -1;
    }

    new_node->next = store->buckets[idx];
    store->buckets[idx] = new_node;
    store->count++;
    return 0;
}

/* ------------------------------------------------------------------ */
/* kv_get — spec §2.1                                                 */
/* Return pointer to internal value string, or NULL if key not found.  */
/* Caller must NOT free the returned pointer.                          */
/* ------------------------------------------------------------------ */
const char *kv_get(const KVStore *store, const char *key)
{
    if (!store || !key) {
        return NULL;
    }

    size_t idx = kv_hash(key, store->capacity);
    KVNode *node = store->buckets[idx];

    while (node) {
        if (strcmp(node->key, key) == 0) {
            return node->value;
        }
        node = node->next;
    }

    return NULL;
}

/* ------------------------------------------------------------------ */
/* kv_delete — spec §2.1                                              */
/* Remove a key-value pair from the store. Frees key, value, and node  */
/* memory. Unlinks the node from its chain (head or mid/tail).         */
/* Returns 0 on success, -1 if key not found (safe to call repeatedly).*/
/* ------------------------------------------------------------------ */
int kv_delete(KVStore *store, const char *key)
{
    if (!store || !key) {
        return -1;
    }

    size_t idx = kv_hash(key, store->capacity);
    KVNode *node = store->buckets[idx];
    KVNode *prev = NULL;

    /* Traverse chain to find the matching key. */
    while (node) {
        if (strcmp(node->key, key) == 0) {
            /* Unlink: head-of-chain or mid/tail. */
            if (prev) {
                prev->next = node->next;
            } else {
                store->buckets[idx] = node->next;
            }

            free(node->key);
            free(node->value);
            free(node);
            store->count--;
            return 0;
        }
        prev = node;
        node = node->next;
    }

    return -1; /* Key not found. */
}
