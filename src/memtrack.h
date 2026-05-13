#ifndef MEMTRACK_H
#define MEMTRACK_H

#include <stdlib.h>

/* Declare prototypes BEFORE the macro definitions so the macros can
 * reference them without implicit declaration warnings.               */
void *tracked_malloc(size_t size, const char *file, int line);
void   tracked_free(void *ptr, const char *file, int line);
void   get_mem_stats(size_t *out_alloc, size_t *out_free);

#ifndef MEMTRACK_IMPLEMENTATION

/* Extern declarations for non-implementation units (main.c). */
extern size_t alloc_count;
extern size_t free_count;

#else /* MEMTRACK_IMPLEMENTATION — kvstore.c only */

size_t alloc_count;
size_t free_count;

#endif /* MEMTRACK_IMPLEMENTATION */

/* Redirect all malloc/free calls through tracking.
 * In kvstore.c, tracked_malloc/tracked_free call libc via function
 * pointers (libc_malloc/libc_free) captured before this include —
 * no infinite recursion.
 * In main.c, the macros link to kvstore.o's implementations.         */
#define malloc(size)   tracked_malloc((size), __FILE__, __LINE__)
#define free(ptr)      tracked_free((ptr), __FILE__, __LINE__)

#endif /* MEMTRACK_H */
