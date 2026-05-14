#ifndef MEMTRACK_H
#define MEMTRACK_H

#include <stddef.h>
#include <stdio.h>

/* Global allocation/free counters for leak detection at exit. */
extern size_t alloc_count;
extern size_t free_count;

void *tracked_malloc(size_t size, const char *file, int line);
void    tracked_free(void *ptr, const char *file, int line);

#define malloc(...)  tracked_malloc(__VA_ARGS__, __FILE__, __LINE__)
#define free(ptr)    tracked_free(ptr, __FILE__, __LINE__)

#endif /* MEMTRACK_H */
