#ifndef MEMTRACK_H
#define MEMTRACK_H

#include <stddef.h>

/* Global allocation/free counters (§5.1). */
extern size_t alloc_count;
extern size_t free_count;

/* Tracked allocation/deallocation functions. */
void *tracked_malloc(size_t size, const char *file, int line);
void  tracked_free(void *ptr, const char *file, int line);
void *tracked_calloc(size_t nmemb, size_t size, const char *file, int line);

/* Override standard allocators with tracking wrappers. */
#undef malloc
#undef free
#undef calloc
#define malloc(sz)           tracked_malloc((sz), __FILE__, __LINE__)
#define free(ptr)            tracked_free((ptr), __FILE__, __LINE__)
#define calloc(n, s)         tracked_calloc((n), (s), __FILE__, __LINE__)

#endif /* MEMTRACK_H */
