#include <stdio.h>
#include "kvstore.h"

int main(void) {
    KVStore *store = kv_create(16);
    if (!store) {
        fprintf(stderr, "Failed to create store\n");
        return 1;
    }
    printf("c-dbase initialized\n");
    kv_destroy(store);
    return 0;
}
