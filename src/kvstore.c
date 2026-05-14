#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Capture real libc malloc/free BEFORE the tracking macros redefine them. */
static void *(*const REAL_MALLOC)(size_t) = malloc;
static void (*const REAL_FREE)(void *)    = free;

#include "kvstore.h"
#include "memtrack.h"

/* ── Private data structures (opaque to callers) ─────────────────── */

typedef struct KVNode {
    char          *key;
    char          *value;
    struct KVNode *next;
} KVNode;

/* Concrete definition completing the opaque typedef from kvstore.h. */
struct KVStore {
    KVNode **buckets;
    size_t   capacity;
    size_t   count;
};

/* ── Memory tracking globals ─────────────────────────────────────── */

size_t alloc_count = 0;
size_t free_count  = 0;

void *tracked_malloc(size_t size, const char *file, int line)
{
    (void)file;
    (void)line;
    void *ptr = REAL_MALLOC(size);
    if (ptr != NULL) {
        alloc_count++;
    }
    return ptr;
}

void tracked_free(void *ptr, const char *file, int line)
{
    (void)file;
    (void)line;
    if (ptr != NULL) {
        REAL_FREE(ptr);
        free_count++;
    }
}

/* ── Internal helpers ────────────────────────────────────────────── */

/* strdup replacement — C99 doesn't require strdup; this uses our tracked malloc. */
static char *string_dup(const char *s)
{
    size_t len = strlen(s) + 1;
    char  *d   = (char *)malloc(len);
    if (d != NULL) {
        memcpy(d, s, len);
    }
    return d;
}

/* ── Hash function (djb2) ────────────────────────────────────────── */

static size_t djb2_hash(const char *key, size_t capacity)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *key++) != '\0') {
        hash = (hash * 33) + c;
    }
    return hash % capacity;
}

/* ── Public API ──────────────────────────────────────────────────── */

KVStore *kv_create(size_t initial_capacity)
{
    if (initial_capacity < 16) {
        initial_capacity = 16;
    }

    KVStore *store = (KVStore *)malloc(sizeof(KVStore));
    if (store == NULL) {
        return NULL;
    }

    store->buckets = (KVNode **)malloc(initial_capacity * sizeof(KVNode *));
    if (store->buckets == NULL) {
        free(store);
        return NULL;
    }

    /* Zero-initialize all bucket pointers. */
    for (size_t i = 0; i < initial_capacity; i++) {
        store->buckets[i] = NULL;
    }

    store->capacity = initial_capacity;
    store->count    = 0;

    return store;
}

void kv_destroy(KVStore *store)
{
    if (store == NULL) {
        return;
    }

    /* Free every node in every chain. */
    for (size_t i = 0; i < store->capacity; i++) {
        KVNode *node = store->buckets[i];
        while (node != NULL) {
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

int kv_insert(KVStore *store, const char *key, const char *value)
{
    if (store == NULL || key == NULL || value == NULL) {
        return -1;
    }

    size_t bucket = djb2_hash(key, store->capacity);
    KVNode *node  = store->buckets[bucket];

    /* Check if key already exists — update in place. */
    while (node != NULL) {
        if (strcmp(node->key, key) == 0) {
            char *new_value = string_dup(value);
            if (new_value == NULL) {
                return -1;
            }
            free(node->value);
            node->value = new_value;
            return 0;
        }
        node = node->next;
    }

    /* Key not found — allocate a new node and prepend. */
    char *new_key   = string_dup(key);
    if (new_key == NULL) {
        return -1;
    }
    char *new_value = string_dup(value);
    if (new_value == NULL) {
        free(new_key);
        return -1;
    }

    KVNode *new_node = (KVNode *)malloc(sizeof(KVNode));
    if (new_node == NULL) {
        free(new_key);
        free(new_value);
        return -1;
    }

    new_node->key   = new_key;
    new_node->value = new_value;
    new_node->next  = store->buckets[bucket];
    store->buckets[bucket] = new_node;
    store->count++;

    return 0;
}

const char *kv_get(const KVStore *store, const char *key)
{
    if (store == NULL || key == NULL) {
        return NULL;
    }

    size_t bucket = djb2_hash(key, store->capacity);
    KVNode *node  = store->buckets[bucket];

    while (node != NULL) {
        if (strcmp(node->key, key) == 0) {
            return node->value;
        }
        node = node->next;
    }

    return NULL;
}

int kv_delete(KVStore *store, const char *key)
{
    if (store == NULL || key == NULL) {
        return -1;
    }

    size_t bucket = djb2_hash(key, store->capacity);
    KVNode *node  = store->buckets[bucket];
    KVNode *prev  = NULL;

    while (node != NULL) {
        if (strcmp(node->key, key) == 0) {
            /* Unlink from chain. */
            if (prev == NULL) {
                store->buckets[bucket] = node->next;
            } else {
                prev->next = node->next;
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

    /* Key not found. */
    return -1;
}
