/* main.c - Test harness (SPEC v3, Section 5) */

#include "track.h"
#include <stdio.h>
#include <string.h>
#include "kvstore.h"

size_t kv_capacity(const KVStore *store);
size_t kv_count(const KVStore *store);

int main(void)
{
    int passed = 0;
    const int total = 14;

    /* Test 1: kv_create(8) clamped to capacity 16 */
    { KVStore *s = kv_create(8);
      if (s != NULL && kv_capacity(s) == 16) {
          printf("[PASS] Test 1: kv_create(8) clamped to capacity 16\n");
          passed++;
      } else {
          printf("[FAIL] Test 1: kv_create(8) -- expected non-NULL with cap 16, got %s\n",
                 s ? "cap != 16" : "NULL");
      }
      kv_destroy(s);
    }

    KVStore *store = kv_create(16);
    if (!store) { fprintf(stderr, "FATAL: kv_create failed\n"); return 1; }

    /* Test 2: kv_insert(store, "name", "Alice") == 0 */
    if (kv_insert(store, "name", "Alice") == 0) {
        printf("[PASS] Test 2: kv_insert(store, \"name\", \"Alice\") == 0\n"); passed++;
    } else printf("[FAIL] Test 2: kv_insert -- expected 0, got -1\n");

    /* Test 3: kv_insert(store, "age", "30") == 0 */
    if (kv_insert(store, "age", "30") == 0) {
        printf("[PASS] Test 3: kv_insert(store, \"age\", \"30\") == 0\n"); passed++;
    } else printf("[FAIL] Test 3: kv_insert -- expected 0, got -1\n");

    /* Test 4: kv_get(store, "name") == "Alice" */
    { const char *v = kv_get(store, "name");
      if (v != NULL && strcmp(v, "Alice") == 0) {
          printf("[PASS] Test 4: kv_get(store, \"name\") == \"Alice\"\n"); passed++;
      } else printf("[FAIL] Test 4: kv_get(store, \"name\") -- expected \"Alice\", got %s\n",
                    v ? v : "NULL");
    }

    /* Test 5: kv_get(store, "missing") == NULL */
    if (kv_get(store, "missing") == NULL) {
        printf("[PASS] Test 5: kv_get(store, \"missing\") == NULL\n"); passed++;
    } else printf("[FAIL] Test 5: kv_get(store, \"missing\") -- expected NULL, got non-NULL\n");

    /* Test 6: update name=Bob */
    if (kv_insert(store, "name", "Bob") == 0) {
        printf("[PASS] Test 6: kv_insert(store, \"name\", \"Bob\") == 0 (update)\n"); passed++;
    } else printf("[FAIL] Test 6: kv_insert update -- expected 0, got -1\n");

    /* Test 7: kv_get(store, "name") == "Bob" */
    { const char *v = kv_get(store, "name");
      if (v != NULL && strcmp(v, "Bob") == 0) {
          printf("[PASS] Test 7: kv_get(store, \"name\") == \"Bob\"\n"); passed++;
      } else printf("[FAIL] Test 7: kv_get(store, \"name\") -- expected \"Bob\", got %s\n",
                    v ? v : "NULL");
    }

    /* Test 8: kv_delete(store, "age") == 0 */
    if (kv_delete(store, "age") == 0) {
        printf("[PASS] Test 8: kv_delete(store, \"age\") == 0\n"); passed++;
    } else printf("[FAIL] Test 8: kv_delete -- expected 0, got -1\n");

    /* Test 9: kv_get(store, "age") == NULL after delete */
    if (kv_get(store, "age") == NULL) {
        printf("[PASS] Test 9: kv_get(store, \"age\") == NULL after delete\n"); passed++;
    } else printf("[FAIL] Test 9: kv_get after delete -- expected NULL, got non-NULL\n");

    /* Test 10: kv_delete(store, "nonexist") == -1 */
    if (kv_delete(store, "nonexist") == -1) {
        printf("[PASS] Test 10: kv_delete(store, \"nonexist\") == -1\n"); passed++;
    } else printf("[FAIL] Test 10: kv_delete -- expected -1, got 0\n");

    /* Test 11: resize at count > capacity * 0.9 */
    { const char *keys[] = {"k1","k2","k3","k4","k5",
                            "k6","k7","k8","k9","k10",
                            "k11","k12","k13","k14"};
      size_t cap_before = kv_capacity(store);
      int ok = 1;
      int i;
      for (i = 0; i < 14; i++) {
          if (kv_insert(store, keys[i], "val") != 0) { ok = 0; break; }
      }
      if (ok) {
          const char *v2 = kv_get(store, "name");
          if (!v2 || strcmp(v2, "Bob") != 0) ok = 0;
          for (i = 0; i < 14 && ok; i++) {
              v2 = kv_get(store, keys[i]);
              if (!v2 || strcmp(v2, "val") != 0) ok = 0;
          }
      }
      if (ok && kv_capacity(store) == 32) {
          printf("[PASS] Test 11: resize at >14.4 count -- cap %zu -> 32, all retrievable\n", cap_before);
          passed++;
      } else printf("[FAIL] Test 11: resize -- ok=%d cap=%zu cnt=%zu\n", ok, kv_capacity(store), kv_count(store));
    }

    /* Test 12: kv_destroy */
    kv_destroy(store);
    printf("[PASS] Test 12: kv_destroy completed without crash\n"); passed++;

    /* Test 13: alloc_count == free_count */
    { size_t a = get_alloc_count(), f = get_free_count();
      if (a == f) {
          printf("[PASS] Test 13: no leaks -- alloc_count=%zu == free_count=%zu\n", a, f);
          passed++;
      } else printf("[FAIL] Test 13: MEMORY LEAK -- alloc_count=%zu, free_count=%zu\n", a, f);
    }

    /* Test 14: double-delete safety */
    { KVStore *s = kv_create(16);
      int r1 = kv_insert(s, "x", "y");
      int r2 = kv_delete(s, "x");
      int r3 = kv_delete(s, "x");
      kv_destroy(s);
      if (r1 == 0 && r2 == 0 && r3 == -1) {
          printf("[PASS] Test 14: double-delete safe -- insert=0, delete1=0, delete2=-1\n");
          passed++;
      } else printf("[FAIL] Test 14: double-delete -- got %d, %d, %d\n", r1, r2, r3);
    }

    /* Summary (Section 5.3) */
    { size_t a = get_alloc_count(), f = get_free_count();
      if (a == f) printf("\n--- Memory clean: alloc=%zu == free=%zu ---\n", a, f);
      else        printf("\n*** LEAK: alloc=%zu, free=%zu ***\n", a, f);
    }

    printf("\n%d/%d tests passed\n", passed, total);
    return (passed == total) ? 0 : 1;
}
