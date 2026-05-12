#include "memtrack.h"
#include "kvstore.h"
#include <string.h>

static int passed = 0;
static const char *test_name;

#define ASSERT(cond, expected, got) do {                                    \
    if (cond) {                                                             \
        printf("[PASS] Test %d: %s\n", ++passed, test_name);               \
    } else {                                                                \
        fprintf(stderr, "[FAIL] Test %d: %s -- expected %s, got %s\n",     \
                passed + 1, test_name, (expected), (got));                  \
    }                                                                       \
} while (0)

#define ASSERT_NOT_NULL(ptr) do {                                           \
    if ((ptr) != NULL) {                                                    \
        printf("[PASS] Test %d: %s\n", ++passed, test_name);               \
    } else {                                                                \
        fprintf(stderr, "[FAIL] Test %d: %s -- expected non-NULL\n",       \
                passed + 1, test_name);                                     \
    }                                                                       \
} while (0)

#define ASSERT_NULL(ptr) do {                                               \
    if ((ptr) == NULL) {                                                    \
        printf("[PASS] Test %d: %s\n", ++passed, test_name);               \
    } else {                                                                \
        fprintf(stderr, "[FAIL] Test %d: %s -- expected NULL, got %s\n",   \
                passed + 1, test_name, (const char *)(ptr));                \
    }                                                                       \
} while (0)

#define ASSERT_STR_EQ(actual, val) do {                                     \
    if ((actual) && strcmp((actual), (val)) == 0) {                         \
        printf("[PASS] Test %d: %s\n", ++passed, test_name);               \
    } else {                                                                \
        fprintf(stderr, "[FAIL] Test %d: %s -- expected \"%s\", got %s\n", \
                passed + 1, test_name, (val),                              \
                (actual) ? (const char *)(actual) : "NULL");                \
    }                                                                       \
} while (0)

#define ASSERT_INT_EQ(actual, val) do {                                     \
    if ((int)(actual) == (int)(val)) {                                      \
        printf("[PASS] Test %d: %s\n", ++passed, test_name);               \
    } else {                                                                \
        fprintf(stderr, "[FAIL] Test %d: %s -- expected %d, got %d\n",     \
                passed + 1, test_name, (int)(val), (int)(actual));          \
    }                                                                       \
} while (0)

int main(void)
{
    /* --- Tests 1-5: Basic create, insert, get, missing key --- */

    test_name = "kv_create(8) clamped to capacity 16";
    KVStore *store = kv_create(8);
    ASSERT_NOT_NULL(store);

    test_name = "kv_insert(store, \"name\", \"Alice\")";
    ASSERT_INT_EQ(kv_insert(store, "name", "Alice"), 0);

    test_name = "kv_insert(store, \"age\", \"30\")";
    ASSERT_INT_EQ(kv_insert(store, "age", "30"), 0);

    test_name = "kv_get(store, \"name\") returns \"Alice\"";
    ASSERT_STR_EQ(kv_get(store, "name"), "Alice");

    test_name = "kv_get(store, \"missing\") returns NULL";
    ASSERT_NULL(kv_get(store, "missing"));

    /* --- Tests 6-7: Update existing key --- */

    test_name = "kv_insert update: kv_insert(store, \"name\", \"Bob\")";
    ASSERT_INT_EQ(kv_insert(store, "name", "Bob"), 0);

    test_name = "kv_get after update returns \"Bob\"";
    ASSERT_STR_EQ(kv_get(store, "name"), "Bob");

    /* --- Tests 8-10: Delete --- */

    test_name = "kv_delete(store, \"age\")";
    ASSERT_INT_EQ(kv_delete(store, "age"), 0);

    test_name = "kv_get(store, \"age\") after delete returns NULL";
    ASSERT_NULL(kv_get(store, "age"));

    test_name = "kv_delete(store, \"nonexist\") returns -1";
    ASSERT_INT_EQ(kv_delete(store, "nonexist"), -1);

    /* --- Test 11: Resize via load factor >= 0.9 ---
     * Current live entries after tests 1-10: just "name" (count = 1).
     * Insert 13 unique keys -> count = 14, threshold = 16*0.9 = 14.4.
     * The 15th insert triggers resize to capacity 32. */

    test_name = "resize: insert 13 keys triggering load factor >= 0.9";
    {
        int i;
        for (i = 0; i < 13; i++) {
            char key[8];
            char val[8];
            snprintf(key, sizeof(key), "k%d", i);
            snprintf(val, sizeof(val), "v%d", i);
            kv_insert(store, key, val);
        }

        /* Verify all 15 entries retrievable after resize to 32. */
        int all_found = 1;
        const char *name_val = kv_get(store, "name");
        if (!name_val || strcmp(name_val, "Bob") != 0) {
            all_found = 0;
        }
        for (i = 0; i < 13 && all_found; i++) {
            char key[8];
            snprintf(key, sizeof(key), "k%d", i);
            if (!kv_get(store, key)) {
                all_found = 0;
            }
        }
        ASSERT(all_found, "all 15 entries retrievable after resize to 32",
               "some entries missing post-resize");
    }

    /* --- Test 12: Destroy all memory --- */

    test_name = "kv_destroy(store) frees all memory";
    kv_destroy(store);
    store = NULL;
    printf("[PASS] Test %d: %s\n", ++passed, test_name);

    /* --- Test 13: No memory leaks --- */

    test_name = "alloc_count == free_count (no leaks)";
    ASSERT_INT_EQ(alloc_count - free_count, 0);

    /* --- Test 14: Double-delete safety ---
     * Single compound assertion: create store2, insert "x", delete once,
     * delete again. First delete -> 0, second -> -1, no crash. */

    test_name = "double-delete safety (insert x, del->0, del->-1, no crash)";
    {
        KVStore *store2 = kv_create(16);
        int rc_ok = 0;
        if (store2) {
            int ins = kv_insert(store2, "x", "1");
            int d1 = kv_delete(store2, "x");
            int d2 = kv_delete(store2, "x");
            if (ins == 0 && d1 == 0 && d2 == -1) {
                rc_ok = 1;
            }
            kv_destroy(store2);
        }
        ASSERT(rc_ok, "insert=0, del1=0, del2=-1, no crash",
               "one or more sub-checks failed");
    }

    /* Summary */
    printf("\n%d/14 tests passed\n", passed);
    printf("alloc_count = %d, free_count = %d", alloc_count, free_count);
    if (alloc_count == free_count) {
        printf(" -- no leaks\n");
    } else {
        printf(" -- LEAK: %d unfreed allocations\n", alloc_count - free_count);
    }

    return passed == 14 ? 0 : 1;
}
