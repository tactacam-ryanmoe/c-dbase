/* kvstore.c - Hash table KV store (SPEC v3) */

/* track.h must be included FIRST so malloc/free macros override for all code. */
#include "track.h"
#include <string.h>
#include "kvstore.h"

typedef struct KVNode {
    char *key;
    char *value;
    struct KVNode *next;
} KVNode;

struct KVStore {
    KVNode **buckets;
    size_t   capacity;
    size_t   count;
};

/* Actual definitions of allocation counters (declared extern in track.h). */
size_t alloc_count = 0;
size_t free_count  = 0;

/* Accessor functions. */
size_t get_alloc_count(void) { return alloc_count; }
size_t get_free_count(void)  { return free_count;  }

static unsigned int djb2(const char *key) {
    unsigned int hash = 5381;
    while (*key) {
        hash = hash * 33 + (unsigned char)*key;
        key++;
    }
    return hash;
}

KVStore *kv_create(size_t initial_capacity) {
    if (initial_capacity < 16) initial_capacity = 16;
    KVStore *store = malloc(sizeof(KVStore));
    if (!store) return NULL;
    store->buckets = calloc(initial_capacity, sizeof(KVNode *));
    if (!store->buckets) { free(store); return NULL; }
    store->capacity = initial_capacity;
    store->count = 0;
    return store;
}

void kv_destroy(KVStore *store) {
    if (!store) return;
    size_t i;
    for (i = 0; i < store->capacity; i++) {
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

static int needs_resize(KVStore *store) {
    return (store->count + 1) * 10 > store->capacity * 9;
}

static void rehash_all(KVStore *store, KVNode **nb, size_t nc) {
    size_t i;
    for (i = 0; i < store->capacity; i++) {
        KVNode *node = store->buckets[i];
        while (node) {
            KVNode *next = node->next;
            unsigned int idx = djb2(node->key) % (unsigned int)nc;
            node->next = nb[idx];
            nb[idx] = node;
            node = next;
        }
    }
}

static void kv_resize(KVStore *store) {
    size_t nc = store->capacity * 2;
    KVNode **nb = calloc(nc, sizeof(KVNode *));
    if (!nb) return;
    rehash_all(store, nb, nc);
    free(store->buckets);
    store->buckets = nb;
    store->capacity = nc;
}

static KVNode *alloc_node(const char *key, const char *value) {
    KVNode *node = malloc(sizeof(KVNode));
    if (!node) return NULL;
    node->key   = tracked_strdup(key);
    if (!node->key) { free(node); return NULL; }
    node->value = tracked_strdup(value);
    if (!node->value) { free(node->key); free(node); return NULL; }
    node->next = NULL;
    return node;
}

int kv_insert(KVStore *store, const char *key, const char *value) {
    if (!store || !key || !value) return -1;
    unsigned int idx = djb2(key) % (unsigned int)store->capacity;
    KVNode *node = store->buckets[idx];
    while (node) {
        if (strcmp(node->key, key) == 0) {
            char *nv = tracked_strdup(value);
            if (!nv) return -1;
            free(node->value);
            node->value = nv;
            return 0;
        }
        node = node->next;
    }
    if (needs_resize(store)) kv_resize(store);
    idx = djb2(key) % (unsigned int)store->capacity;
    KVNode *nn = alloc_node(key, value);
    if (!nn) return -1;
    nn->next = store->buckets[idx];
    store->buckets[idx] = nn;
    store->count++;
    return 0;
}

const char *kv_get(const KVStore *store, const char *key) {
    if (!store || !key) return NULL;
    unsigned int idx = djb2(key) % (unsigned int)store->capacity;
    KVNode *node = store->buckets[idx];
    while (node) {
        if (strcmp(node->key, key) == 0) return node->value;
        node = node->next;
    }
    return NULL;
}

int kv_delete(KVStore *store, const char *key) {
    if (!store || !key) return -1;
    unsigned int idx = djb2(key) % (unsigned int)store->capacity;
    KVNode **pp = &store->buckets[idx];
    while (*pp) {
        if (strcmp((*pp)->key, key) == 0) {
            KVNode *v = *pp;
            *pp = v->next;
            free(v->key);
            free(v->value);
            free(v);
            store->count--;
            return 0;
        }
        pp = &(*pp)->next;
    }
    return -1;
}

size_t kv_capacity(const KVStore *store) {
    if (!store) return 0;
    return store->capacity;
}

size_t kv_count(const KVStore *store) {
    if (!store) return 0;
    return store->count;
}
