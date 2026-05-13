#ifndef MEMTRACK_H
#define MEMTRACK_H

#include <stdlib.h>

#ifndef MEMTRACK_IMPLEMENTATION

extern size_t alloc_count;
extern size_t free_count;

void *tracked_malloc(size_t size, const char *file, int line);
void   tracked_free(void *ptr, const char *file, int line);
void   get_mem_stats(size_t *out_alloc, size_t *out_free);

#else /* MEMTRACK_IMPLEMENTATION */

static size_t alloc_count;
static size_t free_count;

#endif /* MEMTRACK_IMPLEMENTATION */

#define malloc(...)  tracked_malloc(__VA_ARGS__, __FILE__, __LINE__)
#define free(ptr)    tracked_free((ptr), __FILE__, __LINE__)

#endif /* MEMTRACK_H */
