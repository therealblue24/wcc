#ifndef ARENA_H_
#define ARENA_H_

#include "base.h"

typedef struct region {
	struct region *prev; /* previous region */
	size_t size; /* size (used) of this region */
	size_t cap; /* capacity (allocated size) of this region */
	void *data; /* allocated data for this region */
} region_t;

typedef struct arena {
	region_t *tail; /* linked list of regions */
	region_t *head;
	size_t defsize; /* default size for new regions */
} arena_t;

#define ARENA_DEFAULT_ALIGN (2 * sizeof(void *))
#define ARENA_DEFAULT_SIZE (16 * 1024)

/* makes a new region with size `size`. */
region_t *region_make(size_t size);

/* deletes a region */
void region_delete(region_t *reg);

/* makes a new arena with default size `defsize`. returns nonzero on failure */
int arena_make(arena_t *ar, size_t defsize);

/* deletes a whole arena */
void arena_delete(arena_t *ar);

/* allocates memory from arena with alignment `align` */
void *arena_alloc_aligned(arena_t *ar, size_t size, size_t align);

/* allocates memory from arena */
void *arena_alloc(arena_t *ar, size_t size);

/* no-op : for porting */
void arena_free(arena_t *ar, void *ptr);

/* returns size of allocation in arena */
size_t arena_alloc_size(void *ptr);

/* reallocates memory in arena with alignment `align` */
void *arena_realloc_aligned(arena_t *ar, void *ptr, size_t new_size,
							size_t align);

/* reallocates memory in arena
 * NOTE: This does NOT reallocate with the same alignment as the original
 * allocation. You have to keep track of that. */
void *arena_realloc(arena_t *ar, void *ptr, size_t new_size);

/* scratch allocator */
/* arena allocator interface but uses a global arena */

int scr_init(void);
void scr_cleanup(void);

void *scr_alloc(size_t size);
void *scr_alloc_aligned(size_t size, size_t align);

void *scr_realloc(void *ptr, size_t new_size);
void *scr_realloc_aligned(void *ptr, size_t new_size, size_t align);

#endif /* ARENA_H_ */
