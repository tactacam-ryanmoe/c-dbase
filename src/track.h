#ifndef TRACK_H
#define TRACK_H

#include <stdio.h>
#include <stdlib.h>

static int alloc_count = 0;
static int free_count = 0;

void *tracked_malloc(size_t size, const char *file, int line);
void tracked_free(void *ptr, const char *file, int line);
void print_alloc_stats(void);

#define malloc(...) tracked_malloc(__VA_ARGS__, __FILE__, __LINE__)
#define free(ptr)   tracked_free(ptr, __FILE__, __LINE__)

#endif /* TRACK_H */
