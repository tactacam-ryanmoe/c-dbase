#ifndef KVSTORE_H
#define KVSTORE_H

#include <stddef.h>

typedef struct KVStore KVStore;

/* Create a store with the given initial bucket capacity (min 16). */
KVStore *kv_create(size_t initial_capacity);

/* Free all memory for the store. */
void kv_destroy(KVStore *store);

/* Insert or update a key-value pair. Copies both strings internally.
   Returns 0 on success, -1 on allocation failure. */
int kv_insert(KVStore *store, const char *key, const char *value);

/* Get the value for a key. Returns NULL if not found. Caller must NOT free.
   The returned pointer is invalidated by any subsequent kv_insert or kv_delete. */
const char *kv_get(const KVStore *store, const char *key);

/* Delete a key-value pair. Returns 0 on success, -1 if key not found. */
int kv_delete(KVStore *store, const char *key);

#endif /* KVSTORE_H */
