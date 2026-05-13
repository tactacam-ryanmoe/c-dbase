/* main.c - Test harness per SPECv3 §5 */

#include "memtrack.h"
#undef malloc
#undef free

#include <stdio.h>
#include <string.h>
#include "kvstore.h"

static int tests_passed;
static int tests_total;

#define ASSERT(cond, test_num, desc) do {                         \
    tests_total++;                                                \
    if (cond) {                                                   \
        printf("[PASS] Test %d: %s\n", (test_num), (desc));       \
        tests_passed++;                                           \
    } else {                                                      \
        printf("[FAIL] Test %d: %s — assertion failed\n",         \
               (test_num), (desc));                               \
    }                                                             \
} while (0)

int main(void)
{
    /* Reset counters for a clean run. */
    alloc_count = 0;
    free_count = 0;

    /* ---- Test 1: kv_create(8) clamps to capacity 16 ---- */
    KVStore *store = kv_create(8);
    ASSERT(store != NULL, 1, "kv_create(8) returns non-NULL (capacity clamped to 16)");

    /* ---- Tests 2-3: Insert two entries ---- */
    int rc;
    rc = kv_insert(store, "name", "Alice");
    ASSERT(rc == 0, 2, "kv_insert name=Alice returns 0");

    rc = kv_insert(store, "age", "30");
    ASSERT(rc == 0, 3, "kv_insert age=30 returns 0");

    /* ---- Test 4: Get existing key ---- */
    const char *val;
    val = kv_get(store, "name");
    ASSERT(val != NULL && strcmp(val, "Alice") == 0, 4, "kv_get(name) returns \"Alice\"");

    /* ---- Test 5: Get missing key ---- */
    val = kv_get(store, "missing");
    ASSERT(val == NULL, 5, "kv_get(missing) returns NULL");

    /* ---- Test 6-7: Update existing key (frees old value) ---- */
    rc = kv_insert(store, "name", "Bob");
    ASSERT(rc == 0, 6, "kv_insert name=Bob (update) returns 0");

    val = kv_get(store, "name");
    ASSERT(val != NULL && strcmp(val, "Bob") == 0, 7, "kv_get(name) returns \"Bob\" after update");

    /* ---- Test 8-9: Delete key ---- */
    rc = kv_delete(store, "age");
    ASSERT(rc == 0, 8, "kv_delete(age) returns 0");

    val = kv_get(store, "age");
    ASSERT(val == NULL, 9, "kv_get(age) returns NULL after delete");

    /* ---- Test 10: Delete non-existent key ---- */
    rc = kv_delete(store, "nonexist");
    ASSERT(rc == -1, 10, "kv_delete(nonexist) returns -1");

    /* ---- Test 11: Trigger resize by inserting enough entries ---- */
    /* After test 8 we have count=1 (just "name" since age was deleted).
     * Capacity=16, threshold is capacity*9/10 = 14. Need count > 14, so count >= 15.
     * Adding 14 more entries brings count to 15, which triggers resize to 32. */
    size_t cap_before = kv_capacity(store);

    for (int i = 0; i < 14; i++) {
        char key[32];
        char val_str[32];
        snprintf(key, sizeof(key), "key%d", i);
        snprintf(val_str, sizeof(val_str), "val%d", i);
        rc = kv_insert(store, key, val_str);
        if (rc != 0) {
            printf("[FAIL] Test 11: kv_insert(key%d) returned %d\n", i, rc);
            tests_total++;
        }
    }

    /* Verify resize occurred — capacity should now be 32. */
    ASSERT(cap_before == 16 && kv_capacity(store) == 32, 11,
           "Store resized to capacity 32 after exceeding load factor threshold");

    /* ---- Test 12: Verify all entries retrievable after resize ---- */
    int all_found = 1;
    val = kv_get(store, "name");
    if (val == NULL || strcmp(val, "Bob") != 0)
        all_found = 0;

    for (int i = 0; i < 14 && all_found; i++) {
        char key[32];
        char expected[32];
        snprintf(key, sizeof(key), "key%d", i);
        snprintf(expected, sizeof(expected), "val%d", i);
        val = kv_get(store, key);
        if (val == NULL || strcmp(val, expected) != 0)
            all_found = 0;
    }
    tests_total++;
    if (all_found) {
        printf("[PASS] Test 12: All 15 entries retrievable after resize\n");
        tests_passed++;
    } else {
        printf("[FAIL] Test 12: Some entries lost after resize\n");
    }

    /* ---- Test 13: kv_destroy and verify no memory leaks ---- */
    kv_destroy(store);
    ASSERT(alloc_count == free_count, 13,
           "alloc_count == free_count (no memory leaks)");

    /* ---- Test 14: Double-delete safety ---- */
    store = kv_create(16);
    kv_insert(store, "x", "y");
    rc = kv_delete(store, "x");
    int first_del_ok = (rc == 0);
    rc = kv_delete(store, "x");
    int second_del_fails = (rc == -1);
    ASSERT(first_del_ok && second_del_fails, 14,
           "First delete returns 0, second delete returns -1 (double-delete safe)");

    kv_destroy(store);

    /* ---- Summary ---- */
    printf("\n%d/%d tests passed\n", tests_passed, tests_total);
    printf("Memory: %zu allocs, %zu frees", alloc_count, free_count);
    if (alloc_count == free_count) {
        printf(" — CLEAN\n");
    } else {
        printf(" — LEAK (%zd unfreed)\n", (long)(alloc_count - free_count));
    }

    return (tests_passed == tests_total) ? 0 : 1;
}
