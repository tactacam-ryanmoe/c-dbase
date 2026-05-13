#ifndef TRACK_H
#define TRACK_H

#include <stddef.h>

/* Allocation counters - defined in kvstore.c */
extern size_t alloc_count;
extern size_t free_count;

/* Accessor functions */
size_t get_alloc_count(void);
size_t get_free_count(void);

/* Tracked allocation wrappers using gcc builtins to bypass macro overrides. */
static inline void *tracked_malloc(size_t size) {
    void *p = __builtin_malloc(size);
    if (p) alloc_count++;
    return p;
}

static inline void tracked_free(void *ptr) {
    if (ptr) free_count++;
    __builtin_free(ptr);
}

static inline void *tracked_calloc(size_t nm, size_t s) {
    void *p = __builtin_calloc(nm, s);
    if (p) alloc_count++;
    return p;
}

/* Tracked strdup - uses tracked_malloc so allocations are counted. */
static inline char *tracked_strdup(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    len++;                       /* null terminator */
    char *dup = tracked_malloc(len);
    if (dup) {
        size_t i;
        for (i = 0; i < len; i++) dup[i] = s[i];
    }
    return dup;
}

/* Override standard allocators with variadic macros so they accept any
   argument form (calls, declarations, sizeof) without preprocessing errors. */
#undef malloc
#define malloc(...) tracked_malloc(__VA_ARGS__)
#undef calloc
#define calloc(...) tracked_calloc(__VA_ARGS__)
#undef free
#define free(...) tracked_free(__VA_ARGS__)

#endif /* TRACK_H */
