#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include "kvstore.h"

/* djb2 hash function */
static unsigned int djb2(const char *key) {
    unsigned int hash = 5381;
    while (*key) {
        hash = hash * 33 + (unsigned char)*key;
        key++;
    }
    return hash;
}

KVStore *kv_create(size_t initial_capacity) {
    if (initial_capacity < 16) {
        initial_capacity = 16;
    }
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

static int needs_resize(KVStore *store) {
    return (store->count + 1) * 10 > store->capacity * 9;
}

static void rehash_all(KVStore *store, KVNode **new_buckets, size_t new_capacity) {
    for (size_t i = 0; i < store->capacity; i++) {
        KVNode *node = store->buckets[i];
        while (node) {
            KVNode *next = node->next;
            unsigned int idx = djb2(node->key) % (unsigned int)new_capacity;
            node->next = new_buckets[idx];
            new_buckets[idx] = node;
            node = next;
        }
    }
}

static int kv_resize(KVStore *store) {
    size_t new_capacity = store->capacity * 2;
    KVNode **new_buckets = calloc(new_capacity, sizeof(KVNode *));
    if (!new_buckets) return -1;
    rehash_all(store, new_buckets, new_capacity);
    free(store->buckets);
    store->buckets = new_buckets;
    store->capacity = new_capacity;
    return 0;
}

static KVNode *alloc_node(const char *key, const char *value) {
    KVNode *node = malloc(sizeof(KVNode));
    if (!node) return NULL;
    node->key = strdup(key);
    if (!node->key) { free(node); return NULL; }
    node->value = strdup(value);
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
            char *new_value = strdup(value);
            if (!new_value) return -1;
            free(node->value);
            node->value = new_value;
            return 0;
        }
        node = node->next;
    }
    if (needs_resize(store)) {
        kv_resize(store);
        idx = djb2(key) % (unsigned int)store->capacity;
    }
    KVNode *new_node = alloc_node(key, value);
    if (!new_node) return -1;
    new_node->next = store->buckets[idx];
    store->buckets[idx] = new_node;
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
    return -1;
}
