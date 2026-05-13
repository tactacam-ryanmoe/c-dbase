/* kvstore.c - Hash table implementation */

#define MEMTRACK_IMPLEMENTATION
#include "memtrack.h"
#undef malloc
#undef free

#include <stdlib.h>
#include <string.h>

/* ---- Internal data structures (spec §3.2) ---- */

typedef struct KVNode {
    char *key;
    char *value;
    struct KVNode *next;
} KVNode;

typedef struct {
    KVNode **buckets;   /* Array of chain heads     */
    size_t capacity;    /* Number of buckets         */
    size_t count;       /* Total live entries         */
} KVStore;

/* ---- Internal helpers (spec §3.3) ---- */

static size_t hash_key(const char *key, size_t capacity)
{
    size_t h = 5381;
    for (size_t i = 0; key[i] != '\0'; i++)
        h = (h * 33) + (unsigned char)key[i];
    return h % capacity;
}

/* strdup wrapper — libc strdup does NOT go through our malloc macro. */
static char *my_strdup(const char *s)
{
    size_t len = strlen(s) + 1;
    char *d = malloc(len);
    if (d == NULL)
        return NULL;
    memcpy(d, s, len);
    return d;
}

/* Memory tracking functions (spec §5.1) */

void *tracked_malloc(size_t size, const char *file, int line)
{
    (void)file;
    (void)line;
    alloc_count++;
    return malloc(size);
}

void tracked_free(void *ptr, const char *file, int line)
{
    (void)file;
    (void)line;
    if (ptr == NULL)
        return;
    free_count++;
    free(ptr);
}

void get_mem_stats(size_t *out_alloc, size_t *out_free)
{
    if (out_alloc) *out_alloc = alloc_count;
    if (out_free)  *out_free  = free_count;
}

/* ---- Resize helper ---- */

static void resize(KVStore *store) __attribute__((unused));

static void resize(KVStore *store)
{
    size_t new_cap = store->capacity * 2;
    KVNode **new_buckets = malloc(new_cap * sizeof(KVNode *));
    if (new_buckets == NULL)
        return;  /* graceful failure — leave existing table as-is */
    memset(new_buckets, 0, new_cap * sizeof(KVNode *));

    for (size_t i = 0; i < store->capacity; i++) {
        KVNode *node = store->buckets[i];
        while (node != NULL) {
            KVNode *next = node->next;
            size_t idx = hash_key(node->key, new_cap);
            node->next = new_buckets[idx];
            new_buckets[idx] = node;
            node = next;
        }
    }

    free(store->buckets);
    store->buckets = new_buckets;
    store->capacity = new_cap;
}

/* ---- Public API (spec §4) ---- */

KVStore *kv_create(size_t initial_capacity)
{
    if (initial_capacity < 16)
        initial_capacity = 16;

    KVStore *store = malloc(sizeof(KVStore));
    if (store == NULL)
        return NULL;

    store->buckets = malloc(initial_capacity * sizeof(KVNode *));
    if (store->buckets == NULL) {
        free(store);
        return NULL;
    }
    memset(store->buckets, 0, initial_capacity * sizeof(KVNode *));

    store->capacity = initial_capacity;
    store->count = 0;
    return store;
}

void kv_destroy(KVStore *store)
{
    if (store == NULL)
        return;

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
    if (store == NULL || key == NULL || value == NULL)
        return -1;

    /* Check for existing key — update in place */
    size_t idx = hash_key(key, store->capacity);
    KVNode *node = store->buckets[idx];
    while (node != NULL) {
        if (strcmp(node->key, key) == 0) {
            char *new_val = my_strdup(value);
            if (new_val == NULL)
                return -1;
            free(node->value);
            node->value = new_val;
            return 0;
        }
        node = node->next;
    }

    /* New key — allocate and prepend */
    KVNode *new_node = malloc(sizeof(KVNode));
    if (new_node == NULL)
        return -1;

    new_node->key = my_strdup(key);
    if (new_node->key == NULL) {
        free(new_node);
        return -1;
    }

    new_node->value = my_strdup(value);
    if (new_node->value == NULL) {
        free(new_node->key);
        free(new_node);
        return -1;
    }

    new_node->next = store->buckets[idx];
    store->buckets[idx] = new_node;
    store->count++;

    /* Resize logic is handled separately (T9.1). */

    return 0;
}

const char *kv_get(const KVStore *store, const char *key)
{
    if (store == NULL || key == NULL)
        return NULL;

    size_t idx = hash_key(key, store->capacity);
    KVNode *node = store->buckets[idx];
    while (node != NULL) {
        if (strcmp(node->key, key) == 0)
            return node->value;
        node = node->next;
    }

    return NULL;
}

int kv_delete(KVStore *store, const char *key)
{
    if (store == NULL || key == NULL)
        return -1;

    size_t idx = hash_key(key, store->capacity);
    KVNode **pp = &store->buckets[idx];

    while (*pp != NULL) {
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
