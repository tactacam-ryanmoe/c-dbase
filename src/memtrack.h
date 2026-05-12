#ifndef MEMTRACK_H
#define MEMTRACK_H

#include <stdio.h>
#include <stdlib.h>

static int alloc_count = 0;
static int free_count = 0;

static void *tracked_malloc(size_t size, const char *file, int line) __attribute__((unused));
static void *tracked_malloc(size_t size, const char *file, int line) {
    (void)file; (void)line;
    void *p = malloc(size);
    if (p) alloc_count++;
    return p;
}

static void tracked_free(void *ptr, const char *file, int line) __attribute__((unused));
static void tracked_free(void *ptr, const char *file, int line) {
    (void)file; (void)line;
    if (ptr) { free_count++; free(ptr); }
}

#define malloc(...) tracked_malloc(__VA_ARGS__, __FILE__, __LINE__)
#define free(ptr)   tracked_free(ptr, __FILE__, __LINE__)

#endif /* MEMTRACK_H */
