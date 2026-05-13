#include "track.h"     /* redefines malloc/free via macros — must be first */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "kvstore.h"

/* ------------------------------------------------------------------ */
/* Function pointers to libc originals.                               */
/* Bare function designators (no parentheses) are NOT macro-expanded  */
/* by the C preprocessor, so these capture the real libc functions.   */
/* ------------------------------------------------------------------ */
static void *(*real_malloc)(size_t) = malloc;
static void (*real_free)(void *)     = free;

/* ------------------------------------------------------------------ */
/* Shared tracking globals (declared extern in track.h)               */
/* ------------------------------------------------------------------ */
int alloc_count = 0;
int free_count  = 0;

/* --- Memory-tracking functions ----------------------------------- */

void *tracked_malloc(size_t size, const char *file, int line)
{
    (void)file;
    (void)line;
    alloc_count++;
    return real_malloc(size);
}

void tracked_free(void *ptr, const char *file, int line)
{
    (void)file;
    (void)line;
    if (ptr != NULL) {
        free_count++;
        real_free(ptr);
    }
}

void print_alloc_stats(void)
{
    fprintf(stderr, "Allocations: %d, Frees: %d\n", alloc_count, free_count);
    if (alloc_count != free_count) {
        fprintf(stderr, "ERROR: Memory leak detected (%d unfreed)\n",
                alloc_count - free_count);
    }
}

/* --- Internal helpers -------------------------------------------- */

/* strdup calls libc malloc/realloc directly, bypassing our tracking
 * macros.  Wrap it so every allocation goes through tracked_malloc. */
static char *tracked_strdup(const char *s)
{
    size_t len = strlen(s);
    char *dup = malloc(len + 1);     /* macro-expanded to tracked_malloc */
    if (dup == NULL) {
        return NULL;
    }
    strcpy(dup, s);
    return dup;
}

/* ------------------------------------------------------------------ */
/* Internal data structures (§3.2)                                    */
/* ------------------------------------------------------------------ */

typedef struct KVNode {
    char *key;
    char *value;
    struct KVNode *next;
} KVNode;

struct KVStore {
    KVNode **buckets;   /* array of chain heads  */
    size_t   capacity;  /* number of buckets     */
    size_t   count;     /* total live entries    */
};

/* ------------------------------------------------------------------ */
/* djb2 hash function (§3.3)                                         */
/* ------------------------------------------------------------------ */

static unsigned int hash(const char *key, size_t capacity)
{
    unsigned int h = 5381;
    while (*key != '\0') {
        h = (h * 33) + (unsigned char)*key;
        key++;
    }
    return h % (unsigned int)capacity;
}

/* ------------------------------------------------------------------ */
/* Graceful resize (§3.5, §3.5.1)                                    */
/* ------------------------------------------------------------------ */

static void try_resize(KVStore *store)
{
    size_t new_cap = store->capacity * 2;
    KVNode **new_buckets = malloc(new_cap * sizeof(KVNode *));
    if (new_buckets == NULL) {
        /* Graceful failure — silently skip resize (§3.5.1). */
        return;
    }

    /* Zero-initialize the new bucket array. */
    size_t bytes = new_cap * sizeof(KVNode *);
    size_t i;
    for (i = 0; i < bytes; i++) {
        ((unsigned char *)new_buckets)[i] = 0;
    }

    /* Rehash every existing node. */
    for (size_t bi = 0; bi < store->capacity; bi++) {
        KVNode *cur = store->buckets[bi];
        while (cur != NULL) {
            KVNode *next = cur->next;
            unsigned int idx = hash(cur->key, new_cap);
            cur->next = new_buckets[idx];
            new_buckets[idx] = cur;
            cur = next;
        }
    }

    free(store->buckets);
    store->buckets  = new_buckets;
    store->capacity = new_cap;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

KVStore *kv_create(size_t initial_capacity)
{
    if (initial_capacity < 16) {
        initial_capacity = 16;
    }

    KVStore *store = malloc(sizeof(KVStore));
    if (store == NULL) {
        return NULL;
    }

    store->buckets = malloc(initial_capacity * sizeof(KVNode *));
    if (store->buckets == NULL) {
        free(store);
        return NULL;
    }

    /* Zero-initialize buckets array. */
    size_t bytes = initial_capacity * sizeof(KVNode *);
    size_t i;
    for (i = 0; i < bytes; i++) {
        ((unsigned char *)store->buckets)[i] = 0;
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

    free(store->buckets);
    free(store);
}

int kv_insert(KVStore *store, const char *key, const char *value)
{
    if (store == NULL || key == NULL || value == NULL) {
        return -1;
    }

    unsigned int idx = hash(key, store->capacity);
    KVNode *cur = store->buckets[idx];

    /* --- update path: key already exists (§2.1) ------------------- */
    while (cur != NULL) {
        if (strcmp(cur->key, key) == 0) {
            char *new_value = tracked_strdup(value);
            if (new_value == NULL) {
                return -1;
            }
            free(cur->value);
            cur->value = new_value;
            return 0;
        }
        cur = cur->next;
    }

    /* --- insert path: new key ------------------------------------ */
    store->count++;                          /* increment first (§3.5) */

    if ((double)store->count / store->capacity > 0.9) {
        try_resize(store);                   /* may silently fail */
        idx = hash(key, store->capacity);    /* recompute after possible resize */
    }

    char *key_dup   = tracked_strdup(key);
    char *value_dup = tracked_strdup(value);
    if (key_dup == NULL || value_dup == NULL) {
        store->count--;                      /* roll back count */
        free(key_dup);
        free(value_dup);
        return -1;
    }

    KVNode *node = malloc(sizeof(KVNode));
    if (node == NULL) {
        store->count--;
        free(key_dup);
        free(value_dup);
        return -1;
    }

    /* Zero-initialize node fields. */
    node->key   = key_dup;
    node->value = value_dup;
    node->next  = store->buckets[idx];       /* prepend to chain head (§3.4) */
    store->buckets[idx] = node;
    return 0;
}

const char *kv_get(const KVStore *store, const char *key)
{
    if (store == NULL || key == NULL) {
        return NULL;
    }

    unsigned int idx = hash(key, store->capacity);
    for (KVNode *cur = store->buckets[idx]; cur != NULL; cur = cur->next) {
        if (strcmp(cur->key, key) == 0) {
            return cur->value;
        }
    }
    return NULL;
}

int kv_delete(KVStore *store, const char *key)
{
    if (store == NULL || key == NULL) {
        return -1;
    }

    unsigned int idx = hash(key, store->capacity);
    KVNode *prev = NULL;
    KVNode *cur  = store->buckets[idx];

    while (cur != NULL) {
        if (strcmp(cur->key, key) == 0) {
            /* Unlink from chain. */
            if (prev == NULL) {
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
        cur  = cur->next;
    }

    return -1;   /* key not found — idempotent */
}
