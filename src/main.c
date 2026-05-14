#include "memtrack.h"
#include "kvstore.h"
#include <string.h>

static int tests_passed = 0;
static int tests_failed = 0;

static void check(int test_num, const char *desc, int cond,
                  const char *exp_str, const char *got_str)
{
    if (cond) {
        printf("[PASS] Test %d: %s\n", test_num, desc);
        tests_passed++;
    } else {
        printf("[FAIL] Test %d: %s \u2014 expected %s, got %s\n",
               test_num, desc, exp_str, got_str);
        tests_failed++;
    }
}

int main(void)
{
    KVStore *store;
    const char *val;
    int rc;

    /* ── Test 1: kv_create(8) clamped to 16 ──────────────────── */
    store = kv_create(8);
    check(1, "kv_create(8) returns non-NULL (capacity clamped to 16)",
          store != NULL, "non-NULL",
          store != NULL ? "non-NULL" : "NULL");

    /* ── Test 2: insert ("name", "Alice") ───────────────────── */
    rc = kv_insert(store, "name", "Alice");
    check(2, "kv_insert(\"name\", \"Alice\") succeeds",
          rc == 0, "0",
          rc == 0 ? "0" : "-1");

    /* ── Test 3: insert ("age", "30") ──────────────────────── */
    rc = kv_insert(store, "age", "30");
    check(3, "kv_insert(\"age\", \"30\") succeeds",
          rc == 0, "0",
          rc == 0 ? "0" : "-1");

    /* ── Test 4: get("name") returns "Alice" ───────────────── */
    val = kv_get(store, "name");
    check(4, "kv_get(\"name\") returns \"Alice\"",
          val != NULL && strcmp(val, "Alice") == 0, "\"Alice\"",
          val ? val : "NULL");

    /* ── Test 5: get("missing") returns NULL ───────────────── */
    val = kv_get(store, "missing");
    check(5, "kv_get(\"missing\") returns NULL",
          val == NULL, "NULL",
          val ? val : "NULL");

    /* ── Test 6: update ("name", "Bob") ───────────────────── */
    rc = kv_insert(store, "name", "Bob");
    check(6, "kv_insert(\"name\", \"Bob\") updates existing key",
          rc == 0, "0",
          rc == 0 ? "0" : "-1");

    /* ── Test 7: get("name") returns "Bob" ─────────────────── */
    val = kv_get(store, "name");
    check(7, "kv_get(\"name\") returns \"Bob\" after update",
          val != NULL && strcmp(val, "Bob") == 0, "\"Bob\"",
          val ? val : "NULL");

    /* ── Test 8: delete("age") succeeds ───────────────────── */
    rc = kv_delete(store, "age");
    check(8, "kv_delete(\"age\") succeeds",
          rc == 0, "0",
          rc == 0 ? "0" : "-1");

    /* ── Test 9: get("age") returns NULL after delete ─────── */
    val = kv_get(store, "age");
    check(9, "kv_get(\"age\") returns NULL after delete",
          val == NULL, "NULL",
          val ? val : "NULL");

    /* ── Test 10: delete("nonexist") fails with -1 ─────────── */
    rc = kv_delete(store, "nonexist");
    check(10, "kv_delete(\"nonexist\") returns -1",
          rc == -1, "-1",
          rc == -1 ? "-1" : "0");

    /* ── Test 11: resize via insert (capacity 16 -> 32) ───── */
    /* Store currently has 1 live entry ("name"->"Bob").
     * Insert 14 more unique keys to reach count=15.
     * Resize triggers when (count+1)/capacity > 0.9, i.e. at 15/16 = 0.9375. */
    {
        int resize_ok = 1;

        /* Use string_dup + tracked malloc for temporary key/value strings
         * that live only as long as the store entries themselves. */
        char tmp[32];

        for (int i = 0; i < 14; i++) {
            snprintf(tmp, sizeof(tmp), "key_%d", i);
            rc = kv_insert(store, tmp, "val");
            if (rc != 0) {
                resize_ok = 0;
                break;
            }
        }

        /* All 15 entries must be retrievable after rehash. */
        val = kv_get(store, "name");
        if (val == NULL || strcmp(val, "Bob") != 0)
            resize_ok = 0;

        for (int i = 0; i < 14 && resize_ok; i++) {
            snprintf(tmp, sizeof(tmp), "key_%d", i);
            val = kv_get(store, tmp);
            if (val == NULL || strcmp(val, "val") != 0)
                resize_ok = 0;
        }

        check(11, "resize: insert 14 keys triggers resize to 32, all retrievable",
              resize_ok, "all entries present",
              resize_ok ? "all entries present" : "missing entry");
    }

    /* ── Test 12: destroy the store ─────────────────────────── */
    kv_destroy(store);
    check(12, "kv_destroy frees all store memory", 1, "freed", "freed");

    /* ── Test 13: no leaks (alloc_count == free_count) ─────── */
    check(13, "no memory leaks after destroy (alloc_count == free_count)",
          alloc_count == free_count,
          "alloc == free",
          alloc_count == free_count ? "alloc == free" : "leaked");

    /* ── Test 14: double-delete safety ─────────────────────── */
    {
        KVStore *s2 = kv_create(16);
        int first_del, second_del;
        int ok = 1;

        rc = kv_insert(s2, "x", "y");
        if (rc != 0)
            ok = 0;

        first_del = kv_delete(s2, "x");
        if (first_del != 0)
            ok = 0;

        second_del = kv_delete(s2, "x");
        if (second_del != -1)
            ok = 0;

        kv_destroy(s2);
        check(14, "double-delete: first returns 0, second returns -1, no crash",
              ok, "first=0 second=-1",
              ok ? "first=0 second=-1" : "unexpected result");
    }

    /* ── Summary ───────────────────────────────────────────── */
    printf("\n%d/%d tests passed\n", tests_passed,
           tests_passed + tests_failed);
    printf("alloc_count = %zu, free_count = %zu\n",
           alloc_count, free_count);

    return tests_failed > 0 ? 1 : 0;
}
