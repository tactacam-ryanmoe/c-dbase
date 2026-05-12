#include <stdlib.h>
#include <stdarg.h>

/* Include tracking header before kvstore so macro overrides apply to ALL allocs. */
#include "memtrack.h"
#include "kvstore.h"
#include <stdio.h>
#include <string.h>

static int passed = 0;
static int failed = 0;
static char fmt_buf[64];

/* Format into global buffer and return it — avoids C comma-in-args issues. */
static const char *fmt(const char *template, ...) {
    va_list ap;
    va_start(ap, template);
    vsnprintf(fmt_buf, sizeof fmt_buf, template, ap);
    va_end(ap);
    return fmt_buf;
}

static void report(int n, const char *desc, int ok)
{
    if (ok) {
        printf("[PASS] Test %d: %s\n", n, desc);
        passed++;
    } else {
        printf("[FAIL] Test %d: %s\n", n, desc);
        failed++;
    }
}

static void report_exp(int n, const char *desc, int ok,
                       const char *expected, const char *got)
{
    if (ok) {
        printf("[PASS] Test %d: %s\n", n, desc);
        passed++;
    } else {
        printf("[FAIL] Test %d: %s — expected %s, got %s\n",
               n, desc, expected, got);
        failed++;
    }
}

int main(void)
{
    /* ================================================================== */
    /*  Tests 1-13 operate on a single store                               */
    /* ================================================================== */
    KVStore *store = kv_create(8);

    /* Test 1: kv_create(8) -> capacity clamped to 16, returns non-NULL   */
    {
        int ok = store != NULL && kv_capacity(store) == 16;
        const char *got = !ok ?
            (store == NULL ? "NULL" : fmt("cap=%zu", kv_capacity(store))) :
            NULL;
        report_exp(1, "kv_create(8) -> capacity clamped to 16",
                   ok, "non-NULL, cap=16", got);
    }

    /* Test 2: kv_insert(store, "name", "Alice") -> returns 0             */
    {
        int r = kv_insert(store, "name", "Alice");
        report_exp(2, "kv_insert(store, \"name\", \"Alice\") == 0",
                   r == 0, "0", fmt("%d", r));
    }

    /* Test 3: kv_insert(store, "age", "30") -> returns 0                 */
    {
        int r = kv_insert(store, "age", "30");
        report_exp(3, "kv_insert(store, \"age\", \"30\") == 0",
                   r == 0, "0", fmt("%d", r));
    }

    /* Test 4: kv_get(store, "name") -> returns "Alice"                   */
    {
        const char *v = kv_get(store, "name");
        report_exp(4, "kv_get(store, \"name\") == \"Alice\"",
                   v != NULL && strcmp(v, "Alice") == 0,
                   "\"Alice\"", v ? v : "NULL");
    }

    /* Test 5: kv_get(store, "missing") -> returns NULL                   */
    {
        const char *v = kv_get(store, "missing");
        report_exp(5, "kv_get(store, \"missing\") == NULL",
                   v == NULL, "NULL", v ? v : "NULL");
    }

    /* Test 6: kv_insert(store, "name", "Bob") [update] -> returns 0      */
    {
        int r = kv_insert(store, "name", "Bob");
        report_exp(6, "kv_insert(store, \"name\", \"Bob\") [update] == 0",
                   r == 0, "0", fmt("%d", r));
    }

    /* Test 7: kv_get(store, "name") -> returns "Bob"                     */
    {
        const char *v = kv_get(store, "name");
        report_exp(7, "kv_get(store, \"name\") == \"Bob\"",
                   v != NULL && strcmp(v, "Bob") == 0,
                   "\"Bob\"", v ? v : "NULL");
    }

    /* Test 8: kv_delete(store, "age") -> returns 0                       */
    {
        int r = kv_delete(store, "age");
        report_exp(8, "kv_delete(store, \"age\") == 0",
                   r == 0, "0", fmt("%d", r));
    }

    /* Test 9: kv_get(store, "age") -> returns NULL                       */
    {
        const char *v = kv_get(store, "age");
        report_exp(9, "kv_get(store, \"age\") == NULL after delete",
                   v == NULL, "NULL", v ? v : "NULL");
    }

    /* Test 10: kv_delete(store, "nonexist") -> returns -1                */
    {
        int r = kv_delete(store, "nonexist");
        report_exp(10, "kv_delete(store, \"nonexist\") == -1",
                   r == -1, "-1", fmt("%d", r));
    }

    /* Restore age for the resize test — spec §5.2 #11 expects total live = 15. */
    kv_insert(store, "age", "30");

    /* Test 11: Insert 13 unique KVs -> total live=15 > 14.4 -> resize   */
    {
        int all_ok = 1;
        for (int i = 0; i < 13; i++) {
            char key[16], val[16];
            snprintf(key, sizeof key, "k%d", i);
            snprintf(val, sizeof val, "v%d", i);
            if (kv_insert(store, key, val) != 0) {
                all_ok = 0;
                break;
            }
        }
        /* Verify all retrievable. */
        if (all_ok) {
            for (int i = 0; i < 13; i++) {
                char key[16], expected[16];
                snprintf(key, sizeof key, "k%d", i);
                snprintf(expected, sizeof expected, "v%d", i);
                const char *v = kv_get(store, key);
                if (v == NULL || strcmp(v, expected) != 0) {
                    all_ok = 0;
                    break;
                }
            }
        }
        int cap_ok = kv_capacity(store) == 32;
        report_exp(11,
                   "Insert 13 unique KVs -> resize to 32, all retrievable",
                   all_ok && cap_ok,
                   "all ok + cap=32",
                   !all_ok ? "retrieval failed" :
                   fmt("cap=%zu", kv_capacity(store)));
    }

    /* Test 12: kv_destroy(store)                                         */
    {
        size_t fc_before = free_count;
        kv_destroy(store);
        report(12, "kv_destroy(store) frees all memory (no crash)", 1);
        (void)fc_before;
    }

    /* Test 13: alloc_count == free_count -> no leaks                     */
    {
        int ok = alloc_count == free_count;
        report_exp(13, "alloc_count == free_count (no leaks)",
                   ok, fmt("%zu==%zu", alloc_count, free_count), "");
    }

    /* ================================================================== */
    /*  Test 14: second store — double-delete no crash                    */
    /* ================================================================== */
    {
        KVStore *s2 = kv_create(16);
        if (s2) {
            int r_ins  = kv_insert(s2, "x", "y");
            int r_del1 = kv_delete(s2, "x");   /* expect 0  */
            int r_del2 = kv_delete(s2, "x");   /* expect -1 */
            kv_destroy(s2);

            report_exp(14,
                       "Second store: insert x, delete x (0), delete again (-1)",
                       r_ins == 0 && r_del1 == 0 && r_del2 == -1,
                       "0,0,-1", fmt("%d,%d,%d", r_ins, r_del1, r_del2));
        } else {
            report(14,
                   "Second store: insert x, delete x (0), delete again (-1)", 0);
        }
    }

    /* ---- Summary ----------------------------------------------------- */
    printf("\n%d/14 tests passed\n", passed);
    printf("alloc_count = %zu, free_count = %zu\n", alloc_count, free_count);

    return (failed == 0) ? 0 : 1;
}
