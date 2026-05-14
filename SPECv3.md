# SPEC v3: c-dbase — Lightweight In-Memory Key-Value Store

## 0. Project Coordination

| Field | Value |
|-------|-------|
| **Project folder** | `/home/ai/projects/c-dbase` |
| **GitHub repository** | `tactacam-ryanmoe/c-dbase.git` |
| **Git branch name** | `feat/initial-implementation` |
| **Kanban board** | `c-dbase` |

All agents must execute git commands and code changes strictly inside the project folder above.

## 1. Overview

c-dbase is a standalone, in-memory key-value store written entirely in C99. It stores string values mapped to string keys using a hash table built from scratch with only the C Standard Library (`stdlib.h`, `string.h`, `stdio.h`). The application supports three core operations: **insert**, **get**, and **delete**.

### 1.1 Non-Goals
- Persistence (no disk I/O or serialization).
- Concurrency/thread safety.
- Binary/blob values — keys and values are null-terminated UTF-8 strings.
- Down-sizing on delete — capacity only grows, never shrinks. This is intentional; the store is ephemeral and lightweight.

### 1.2 Usage Assumption

This library is designed for **single-threaded** use only. Calling `kv_insert` or `kv_delete` while a caller holds a pointer returned by `kv_get` results in undefined behavior — the internal chain may be reallocated or the node freed. The caller must not cache pointers from `kv_get` across mutations.

## 2. Requirements

### 2.1 Core Operations

| Operation | Description | Behavior on Edge Cases |
|-----------|-------------|------------------------|
| `kv_insert` | Insert a new key-value pair, or update the value of an existing key. | If key exists, replace old value and free its memory. Returns 0 on success, -1 on allocation failure. |
| `kv_get` | Retrieve the value string for a given key. | Returns a pointer to the internal value string on success, `NULL` if key not found. Caller must **not** free the returned pointer. Returned pointer is invalidated by any subsequent `kv_insert` or `kv_delete`. |
| `kv_delete` | Remove a key-value pair entirely. | Frees both the key and value strings, removes the node from its chain. Returns 0 on success, -1 if key not found. Deleting an already-deleted key returns -1 (idempotent failure). |

### 2.2 Lifecycle

| Operation | Description |
|-----------|-------------|
| `kv_create` | Allocate and initialize a new empty store with a configurable initial capacity. If the requested capacity is less than 16, it is clamped to 16. Returns a pointer to an opaque `KVStore` struct, or `NULL` on failure. |
| `kv_destroy` | Free all memory associated with the store — every node, key, value string, and the backing array. After this call the pointer is invalid. |

## 3. Architecture

### 3.1 File Layout

```
c-dbase/
├── Makefile
├── README.md
└── src/
    ├── kvstore.h      # Public API header
    ├── kvstore.c      # Hash table implementation
    └── main.c         # Test harness
```

### 3.2 Data Structures

#### 3.2.1 Node (Chain Element)

Each entry in the hash table is a linked list node:

```c
typedef struct KVNode {
    char *key;
    char *value;
    struct KVNode *next;
} KVNode;
```

Keys and values are dynamically allocated via `strdup` — there is no upper bound on their length.

#### 3.2.2 Store (Backed by Buckets Array)

```c
typedef struct {
    KVNode **buckets;   // Array of chain heads
    size_t capacity;    // Number of buckets
    size_t count;       // Total live entries
} KVStore;
```

Minimum initial capacity: **16** buckets (enforced by `kv_create`).

### 3.3 Hash Function

The store uses the **djb2** hash algorithm:

```
hash = 5381
for each byte c in key:
    hash = (hash * 33) + c
return hash % capacity
```

### 3.4 Collision Resolution

Collisions are resolved via **separate chaining**. Each bucket index points to the head of a singly-linked list of `KVNode`s. On insert, new nodes are prepended to the chain (O(1)). On get/delete, the chain is traversed linearly comparing keys with `strcmp`.

### 3.5 Resize Policy

When the **load factor** (`count / capacity`) exceeds **0.9**, the store automatically resizes:

1. Allocate a new buckets array with `capacity * 2`.
2. Rehash every existing node into the new array (recompute bucket index).
3. Free the old buckets array.
4. Update `capacity` field.

Resize is transparent to the caller and occurs inside `kv_insert`.

#### 3.5.1 Graceful Failure on Resize

If step 1 (malloc for the new bucket array) fails, `kv_insert` does **not** propagate an error from the resize attempt. Instead:
- The new node is inserted into the existing (overloaded) table regardless.
- The store remains fully functional; load factor stays above 0.9 until a future resize succeeds.
- `kv_insert` returns 0 if the key/value allocation succeeded, or -1 only if allocating the key/value themselves failed.

The resize failure is silently absorbed — no error code is returned to the caller for this condition alone.

## 4. Public API (`kvstore.h`)

```c
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
```

## 5. Test Harness (`main.c`)

The test harness exercises all operations and verifies clean memory management through internal alloc/free counting (no external tools).

### 5.1 Memory Tracking

Custom `malloc`/`free` wrappers implemented as **macros** maintain two global counters: `alloc_count` and `free_count`. All source files include a tracking header that defines:

```c
#define malloc(...) tracked_malloc(__VA_ARGS__, __FILE__, __LINE__)
#define free(ptr)   tracked_free(ptr, __FILE__, __LINE__)
```

At program exit, the harness asserts `alloc_count == free_count`.

### 5.2 Test Sequence

| # | Action | Expected Result |
|---|--------|-----------------|
| 1 | `kv_create(8)` — capacity clamped to 16 | Returns non-NULL store pointer. |
| 2 | `kv_insert(store, "name", "Alice")` | Returns 0. |
| 3 | `kv_insert(store, "age", "30")` | Returns 0. |
| 4 | `kv_get(store, "name")` | Returns `"Alice"`. |
| 5 | `kv_get(store, "missing")` | Returns `NULL`. |
| 6 | `kv_insert(store, "name", "Bob")` | Returns 0 (update). |
| 7 | `kv_get(store, "name")` | Returns `"Bob"`. Old `"Alice"` memory freed. |
| 8 | `kv_delete(store, "age")` | Returns 0. |
| 9 | `kv_get(store, "age")` | Returns `NULL`. |
| 10 | `kv_delete(store, "nonexist")` | Returns -1. |
| 11 | Insert 13 additional unique key-value pairs (bringing live count to 15, exceeding 16 × 0.9 = 14.4) to trigger resize. | Store resizes to capacity 32. All entries remain retrievable via `kv_get`. |
| 12 | `kv_destroy(store)` | All memory freed. |
| 13 | Assert `alloc_count == free_count`. | Pass — no leaks. |
| 14 | Create a second store, insert "x", delete "x", then delete "x" again. | First delete returns 0, second delete returns -1. No crash or double-free. |

### 5.3 Output Format

Each test prints a single line:
- Pass: `[PASS] Test N: description`
- Fail: `[FAIL] Test N: description — expected X, got Y`

At the end, print a summary: `N/N tests passed` and the alloc/free counts.

## 6. Build System (`Makefile`)

```makefile
CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -pedantic -g
TARGET  = c-dbase
SRCS    = src/kvstore.c src/main.c
OBJS    = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c src/kvstore.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
```

## 7. Acceptance Criteria

1. **Compiles cleanly** with `gcc -std=c99 -Wall -Wextra -pedantic` — zero warnings.
2. **All tests pass** including resize-triggering test, leak detection, and double-delete safety.
3. **Memory is clean** — `alloc_count == free_count` at exit.
4. **djb2 hash function** is used and correctly buckets keys.
5. **Chaining** handles collisions; the resize test implicitly validates this works under load.
6. **Code follows C99** — no C11/C17/POSIX extensions. Only `stdlib.h`, `string.h`, `stdio.h` included.

## 8. Constraints

- No external libraries beyond libc.
- No platform-specific code (Linux/macOS compatible).
- No error handling via errno — all errors returned as integer codes from the API.
- Single-threaded use only — no mutex, atomic, or thread-safe guarantees.
