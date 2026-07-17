#include "set.h"

/* No pointer is ever going to be 0xffffffffffffffff, so make that the tombstone */
#define TOMBSTONE ((void *)-1)
// #define UNUSED ((void*)0)
// ^ this is just NULL

typedef struct lookup {
	uint64_t loc;
	bool exists;
} lookup_t;

static lookup_t set_lookup(void *set, void *item)
{
	void **sset = set;
	set_hdr_t *hdr = set_hdr(set);
	uint64_t hash = fnv1a(&item, sizeof(void *));
	uint64_t mask = hdr->cap - 1;
	hash &= mask;
	uint64_t step = 1; /* TODO: improve */
	for(uint64_t i = hash - step;;) {
		i = (i + step) & mask;
		if(sset[i] == TOMBSTONE) {
			continue;
		}

		if(!sset[i]) {
			return (lookup_t){ .loc = i, .exists = false };
		}

		if(sset[i] == item) {
			return (lookup_t){ .loc = i, .exists = true };
		}
	}

	ASSERT(false, "unreachable");
	return (lookup_t){ .loc = 0, .exists = false };
}

static void *set_rebuild(set_hdr_t *hdr, void *set)
{
	void **sset = set;
	void *newset = set_make(hdr->cap * 2);
	for(size_t i = 0; i < hdr->cap; i++) {
		if(sset[i] == TOMBSTONE || !sset[i]) {
			continue;
		}

		set_donotuse_add(&newset, sset[i]);
	}

	set_delete(set);

	return newset;
}

void set_donotuse_add(void **set, void *item)
{
	void *dset = *set;
	void **sset = dset;
	set_hdr_t *hdr = set_hdr(dset);

	/* if hashmap over 75% capacity, rebuild */
	if(hdr->size >= (3 * hdr->cap) / 4) {
		void *newset = set_rebuild(hdr, dset);
		hdr = set_hdr(newset);
		*set = newset;
		dset = newset;
		sset = dset;
	}

	lookup_t lookup = set_lookup(dset, item);
	if(!lookup.exists) {
		hdr->size++;
	}
	sset[lookup.loc] = item;
	return;
}

void set_del(void *set, void *item)
{
	void **sset = set;
	lookup_t lookup = set_lookup(set, item);
	if(!lookup.exists) {
		return;
	}

	sset[lookup.loc] = TOMBSTONE;
	return;
}

bool set_has(void *set, void *item)
{
	return set_lookup(set, item).exists;
}

void *set_get(void *set, void *item)
{
	void **sset = set;
	lookup_t lookup = set_lookup(set, item);
	if(!lookup.exists) {
		return NULL;
	}

	return sset[lookup.loc];
}

void *set_make(size_t cap)
{
	set_hdr_t *hdr = zalloc(sizeof(set_hdr_t) + (sizeof(void *) * cap));
	hdr->cap = cap;
	hdr->size = 0;

	return set_data(hdr);
}

void *set_empty(void)
{
	return set_make(SET_DEFAULT_SIZE);
}

void set_delete(void *set)
{
	free(set_hdr(set));
	return;
}

set_hdr_t *set_hdr(void *set)
{
	return ((set_hdr_t *)set) - 1;
}

void *set_data(set_hdr_t *hdr)
{
	return &hdr->data[0];
}

void set_reset(void *set)
{
	set_hdr_t *hdr = set_hdr(set);
	wipe(set, (hdr->cap * sizeof(void *)));
	return;
}
