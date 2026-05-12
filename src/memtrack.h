#ifndef MEMTRACK_H
#define MEMTRACK_H

#include <stdio.h>
#include <stdlib.h>

/* Cross-TU tracking counters — defined once in kvstore.c. */
extern int alloc_count;
extern int free_count;

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

static void *tracked_calloc(size_t nmemb, size_t size, const char *file, int line) __attribute__((unused));
static void *tracked_calloc(size_t nmemb, size_t size, const char *file, int line) {
    (void)file; (void)line;
    void *p = calloc(nmemb, size);
    if (p) alloc_count++;
    return p;
}

#define malloc(...)     tracked_malloc(__VA_ARGS__, __FILE__, __LINE__)
#define calloc(...)     tracked_calloc(__VA_ARGS__, __FILE__, __LINE__)
#define free(ptr)       tracked_free(ptr, __FILE__, __LINE__)

#endif /* MEMTRACK_H */
