#include "arena.h"
#include "base.h"

/* makes a new region with size `size`. */
region_t *region_make(size_t size)
{
	/* allocate data */
	void *data = malloc(size);
	if(!data) {
		return NULL;
	}
	wipe(data, size); /* zero it out */

	region_t *reg = calloc(1, sizeof(region_t));
	if(!reg) {
		return NULL;
	}
	reg->data = data;
	reg->size = 0;
	reg->cap = size;
	reg->prev = NULL;
	return reg;
}

/* deletes a region */
void region_delete(region_t *reg)
{
	if(!reg) {
		return;
	}
	free(reg->data);
	return;
}

/* makes a new arena with default size `defsize`. returns nonzero on failure */
int arena_make(arena_t *ar, size_t defsize)
{
	if(!ar) {
		return 1;
	}

	ar->tail = region_make(defsize);
	ENSURE_RET(ar->tail, 1, "failed to allocate memory for region");
	ar->head = ar->tail;

	ar->defsize = defsize;

	return 0;
}

/* deletes a whole arena */
void arena_delete(arena_t *ar)
{
	if(!ar) {
		return;
	}

	region_t *cur = ar->tail;
	region_t *nxt = NULL;

	while(nxt) {
		nxt = cur->prev;
		region_delete(cur);
		cur = nxt;
	}

	return;
}

/* find a suitable region for allocation */
static region_t *find_suitable_region(arena_t *ar, size_t size)
{
	region_t *search = ar->tail;
	while(search) {
		if((search->size + size) < search->cap) {
			return search;
		}
		search = search->prev;
	}
	return NULL;
}

/* maximum of 2 numbers */
static size_t max(size_t a, size_t b)
{
	return a > b ? a : b;
}

/* allocates memory from arena with alignment `align` */
void *arena_alloc_aligned(arena_t *ar, size_t size, size_t align)
{
	size_t search_size = size + align + sizeof(size_t);
	region_t *reg = find_suitable_region(ar, search_size);

	if(!reg) {
		/* add region */
		region_t *newreg =
			region_make(max(ar->defsize, size + align + sizeof(size_t) + 1));
		if(!newreg) {
			return NULL;
		}
		ar->head->prev = newreg;
		ar->head = newreg;
		reg = newreg;
	}

	/* allocate memory */
	reg->size = align_to(reg->size, align);
	void *sptr = (void *)((size_t)reg->data + (size_t)reg->size);
	void *ptr = (void *)((size_t)reg->data + (size_t)reg->size + 8);
	reg->size += size + 8;

	*((size_t *)sptr) = size;

	return ptr;
}

/* allocates memory from arena */
void *arena_alloc(arena_t *ar, size_t size)
{
	return arena_alloc_aligned(ar, size, ARENA_DEFAULT_ALIGN);
}

/* no-op : for porting */
void arena_free(arena_t *ar, void *ptr)
{
	(void)ar;
	(void)ptr;
	return;
}

/* returns size of allocation in arena */
size_t arena_alloc_size(void *ptr)
{
	size_t ptrv = (size_t)ptr;
	ptrv -= 8;
	void *sptr = (void *)ptrv;
	return *(size_t *)sptr;
}

/* reallocates memory in arena with alignment `align` */
void *arena_realloc_aligned(arena_t *ar, void *ptr, size_t new_size,
							size_t align)
{
	size_t oldsize = arena_alloc_size(ptr);
	void *mem = arena_alloc_aligned(ar, new_size, align);
	memcpy(mem, ptr, oldsize);
	return mem;
}

/* reallocates memory in arena
 * NOTE: This does NOT reallocate with the same alignment as the original
 * allocation. You have to keep track of that. */
void *arena_realloc(arena_t *ar, void *ptr, size_t new_size)
{
	return arena_realloc_aligned(ar, ptr, new_size, ARENA_DEFAULT_ALIGN);
}

/* scratch allocator */

static arena_t scratch;

int scr_init(void)
{
	if(arena_make(&scratch, ARENA_DEFAULT_SIZE)) {
		return 1;
	}
	return 0;
}
void scr_cleanup(void)
{
	arena_delete(&scratch);
}

void *scr_alloc(size_t size)
{
	void *mem = arena_alloc(&scratch, size);
	ENSURE(mem, "failed to allocate %zu bytes of memory", size);
	return mem;
}

void *scr_alloc_aligned(size_t size, size_t align)
{
	void *mem = arena_alloc_aligned(&scratch, size, align);
	ENSURE(mem, "failed to allocate %zu bytes of memory", size);
	return mem;
}

/* are these even used? */
void *scr_realloc(void *ptr, size_t new_size)
{
	return arena_realloc(&scratch, ptr, new_size);
}

void *scr_realloc_aligned(void *ptr, size_t new_size, size_t align)
{
	return arena_realloc_aligned(&scratch, ptr, new_size, align);
}
