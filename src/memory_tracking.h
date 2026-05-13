#ifndef MEMORY_TRACKING_H
#define MEMORY_TRACKING_H

#include <stdio.h>
#include <stdlib.h>

/* Capture pointers to the real libc functions before we override malloc/free. */
static void *(*const real_malloc)(size_t) = malloc;
static void (*const real_free)(void *)    = free;

/* Global counters shared across translation units — defined in main.c. */
extern int alloc_count;
extern int free_count;

/* Wrappers that record source location and update counters. */
static inline void *tracked_malloc(size_t size, const char *file, int line)
{
    (void)file;
    (void)line;
    alloc_count++;
    return real_malloc(size);
}

static inline void tracked_free(void *ptr, const char *file, int line)
{
    (void)file;
    (void)line;
    if (ptr == NULL)
        return;
    free_count++;
    real_free(ptr);
}

/* Print the current allocation counts. Call from main() at exit. */
static inline void report_alloc_counts(void)
{
    printf("Allocations: %d, Frees: %d\n", alloc_count, free_count);
}

/* Override stdlib malloc/free so every call in this translation unit is tracked. */
#undef malloc
#undef free
#define malloc(...) tracked_malloc(__VA_ARGS__, __FILE__, __LINE__)
#define free(ptr)   tracked_free(ptr, __FILE__, __LINE__)

#endif /* MEMORY_TRACKING_H */
