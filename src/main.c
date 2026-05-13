#include "track.h"     /* redefines malloc/free — must be first */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kvstore.h"

/* --- Test helpers ------------------------------------------------- */

static int pass = 0;
static int fail = 0;

static void check(int test, int cond, const char *desc)
{
    if (cond) {
        printf("[PASS] Test %d: %s\n", test, desc);
        pass++;
    } else {
        printf("[FAIL] Test %d: %s\n", test, desc);
        fail++;
    }
}

/* ------------------------------------------------------------------ */

int main(void)
{
    /* ---- Test 1: kv_create clamps capacity < 16 to 16 ------------ */
    KVStore *store = kv_create(8);
    check(1, store != NULL, "kv_create(8) returns non-NULL (clamped to 16)");

    /* ---- Test 2: basic insert ------------------------------------ */
    int rc = kv_insert(store, "name", "Alice");
    check(2, rc == 0, "insert(name=Alice) returns 0");

    /* ---- Test 3: second insert ----------------------------------- */
    rc = kv_insert(store, "age", "30");
    check(3, rc == 0, "insert(age=30) returns 0");

    /* ---- Test 4: get existing key -------------------------------- */
    const char *val = kv_get(store, "name");
    check(4, val != NULL && strcmp(val, "Alice") == 0,
          "get(name) returns Alice");

    /* ---- Test 5: get missing key --------------------------------- */
    val = kv_get(store, "missing");
    check(5, val == NULL, "get(missing) returns NULL");

    /* ---- Test 6: update existing key ----------------------------- */
    rc = kv_insert(store, "name", "Bob");
    check(6, rc == 0, "update(name=Bob) returns 0");

    /* ---- Test 7: get updated value -------------------------------- */
    val = kv_get(store, "name");
    check(7, val != NULL && strcmp(val, "Bob") == 0,
          "get(name) returns Bob after update");

    /* ---- Test 8: delete key -------------------------------------- */
    rc = kv_delete(store, "age");
    check(8, rc == 0, "delete(age) returns 0");

    /* ---- Test 9: get deleted key --------------------------------- */
    val = kv_get(store, "age");
    check(9, val == NULL, "get(age) returns NULL after delete");

    /* ---- Test 10: delete non-existent key ------------------------- */
    rc = kv_delete(store, "nonexist");
    check(10, rc == -1, "delete(nonexist) returns -1");

    /* ---- Test 11: trigger resize --------------------------------- */
    /* After test 8 (deleted "age"): live count = 1 ("name" only).
     * Capacity is 16. Insert 14 more keys → count=15, exceeds
     * 16 × 0.9 = 14.4, so resize fires at capacity→32. */
    {
        char key[32];
        int all_inserted = 1;
        int i;
        for (i = 1; i <= 14; i++) {
            snprintf(key, sizeof(key), "k%d", i);
            rc = kv_insert(store, key, "value");
            if (rc != 0) {
                all_inserted = 0;
                break;
            }
        }

        /* All entries must still be retrievable after rehash. */
        int all_retrievable = all_inserted;
        if (all_inserted) {
            val = kv_get(store, "name");
            if (val == NULL || strcmp(val, "Bob") != 0)
                all_retrievable = 0;
            for (i = 1; i <= 14 && all_retrievable; i++) {
                snprintf(key, sizeof(key), "k%d", i);
                val = kv_get(store, key);
                if (val == NULL || strcmp(val, "value") != 0)
                    all_retrievable = 0;
            }
        }
        check(11, all_inserted && all_retrievable,
              "resize triggered — all 15 entries retrievable after resize to 32");
    }

    /* ---- Test 12: destroy store ---------------------------------- */
    kv_destroy(store);
    check(12, 1, "kv_destroy freed all memory without crash");

    /* ---- Test 13: no memory leaks -------------------------------- */
    check(13, alloc_count == free_count,
          "alloc_count == free_count (no leaks after destroy)");

    /* ---- Test 14: double-delete safety ---------------------------- */
    KVStore *store2 = kv_create(0);   /* clamped to 16 */
    int r1 = -99;
    int r2 = -99;
    if (store2 != NULL) {
        kv_insert(store2, "x", "y");
        r1 = kv_delete(store2, "x");
        r2 = kv_delete(store2, "x");
        kv_destroy(store2);
    }
    check(14, store2 != NULL && r1 == 0 && r2 == -1,
          "double-delete safety: first delete=0, second=-1, no crash");

    /* ---- Final leak check after all cleanup ---------------------- */
    if (alloc_count != free_count) {
        printf("[WARN] Memory leak at exit: alloc=%d free=%d\n",
               alloc_count, free_count);
    }

    /* ---- Summary ------------------------------------------------- */
    printf("\n%d/14 tests passed\n", pass);
    print_alloc_stats();

    return (fail == 0 && alloc_count == free_count) ? 0 : 1;
}
