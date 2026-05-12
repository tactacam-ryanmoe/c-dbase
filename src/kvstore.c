#include "kvstore.h"

KVStore *kv_create(size_t initial_capacity) {
    (void)initial_capacity;
    return NULL;
}

void kv_destroy(KVStore *store) {
    (void)store;
}

int kv_insert(KVStore *store, const char *key, const char *value) {
    (void)store;
    (void)key;
    (void)value;
    return -1;
}

const char *kv_get(const KVStore *store, const char *key) {
    (void)store;
    (void)key;
    return NULL;
}

int kv_delete(KVStore *store, const char *key) {
    (void)store;
    (void)key;
    return -1;
}
