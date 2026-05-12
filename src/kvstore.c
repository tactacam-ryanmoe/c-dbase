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

void *tracked_calloc(size_t nmemb, size_t size, const char *file, int line) {
    (void)file;
    (void)line;
    size_t total = nmemb * size;
    void *ptr = _real_malloc(total);
    if (ptr != NULL) {
        size_t i;
        unsigned char *p = (unsigned char *)ptr;
        for (i = 0; i < total; i++) {
            p[i] = 0;
        }
        alloc_count++;
    }
    return ptr;
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
    if (store == NULL) {
        return;
    }
    /* Walk all chains: free key, value, then node itself for each entry. */
    for (size_t i = 0; i < store->capacity; i++) {
        KVNode *cur = store->buckets[i];
        while (cur != NULL) {
            KVNode *next = cur->next;
            free(cur->key);
            free(cur->value);
            free(cur);
            cur = next;
        }
    }
    /* Free the buckets array, then the store struct. */
    free(store->buckets);
    free(store);
}

int kv_insert(KVStore *store, const char *key, const char *value) {
    if (store == NULL || key == NULL || value == NULL) {
        return -1;
    }

    size_t idx = bucket_index(store, key);
    KVNode *cur = store->buckets[idx];

    /* Update existing key: walk chain, replace value in place. */
    while (cur != NULL) {
        if (strcmp(cur->key, key) == 0) {
            size_t vlen = strlen(value) + 1;
            char *new_val = (char *)malloc(vlen);
            if (new_val == NULL) {
                return -1;
            }
            memcpy(new_val, value, vlen);
            free(cur->value);
            cur->value = new_val;
            return 0;
        }
        cur = cur->next;
    }

    /* New key — resize before inserting if load factor would exceed 0.9 (§3.5). */
    if ((double)(store->count + 1) / store->capacity > 0.9) {
        size_t new_cap = store->capacity * 2;
        KVNode **new_buckets = (KVNode **)calloc(new_cap, sizeof(KVNode *));
        if (new_buckets != NULL) {
            /* Rehash every node from the old table into the new one. */
            for (size_t i = 0; i < store->capacity; i++) {
                KVNode *node = store->buckets[i];
                while (node != NULL) {
                    KVNode *next = node->next;
                    size_t ni = djb2_hash(node->key, new_cap);
                    node->next = new_buckets[ni];
                    new_buckets[ni] = node;
                    node = next;
                }
            }
            free(store->buckets);
            store->buckets = new_buckets;
            store->capacity = new_cap;
            /* Recompute idx for the new capacity. */
            idx = bucket_index(store, key);
        }
        /* If calloc failed, silently continue with current table (§3.5.1). */
    }

    /* Allocate node, copy strings, prepend to chain. */
    KVNode *node = (KVNode *)malloc(sizeof(KVNode));
    if (node == NULL) {
        return -1;
    }
    {
        size_t klen = strlen(key) + 1;
        node->key = (char *)malloc(klen);
        if (node->key == NULL) {
            free(node);
            return -1;
        }
        memcpy(node->key, key, klen);
    }
    {
        size_t vlen = strlen(value) + 1;
        node->value = (char *)malloc(vlen);
        if (node->value == NULL) {
            free(node->key);
            free(node);
            return -1;
        }
        memcpy(node->value, value, vlen);
    }
    node->next = store->buckets[idx];
    store->buckets[idx] = node;
    store->count++;
    return 0;
}

const char *kv_get(const KVStore *store, const char *key) {
    if (store == NULL || key == NULL) {
        return NULL;
    }

    size_t idx = bucket_index((KVStore *)store, key);
    KVNode *cur = store->buckets[idx];

    while (cur != NULL) {
        if (strcmp(cur->key, key) == 0) {
            return cur->value;
        }
        cur = cur->next;
    }
    return NULL;
}

int kv_delete(KVStore *store, const char *key) {
    if (store == NULL || key == NULL) {
        return -1;
    }

    size_t idx = bucket_index(store, key);
    KVNode *cur = store->buckets[idx];
    KVNode *prev = NULL;

    while (cur != NULL) {
        if (strcmp(cur->key, key) == 0) {
            /* Unlink from chain. */
            if (prev == NULL) {
                /* Head-of-chain: advance the bucket pointer. */
                store->buckets[idx] = cur->next;
            } else {
                prev->next = cur->next;
            }
            free(cur->key);
            free(cur->value);
            free(cur);
            store->count--;
            return 0;
        }
        prev = cur;
        cur = cur->next;
    }
    return -1; /* Key not found. */
}

size_t kv_capacity(const KVStore *store) {
    if (store == NULL) return 0;
    return store->capacity;
}

size_t kv_count(const KVStore *store) {
    if (store == NULL) return 0;
    return store->count;
}
