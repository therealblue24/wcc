/* Inspired by https://nullprogram.com/blog/2026/05/06/ (without the atomics) and
 * https://github.com/RealNeGate/Cuik/tree/master/common . */
#include "strmap.h"
#include "arena.h"
#include "base.h"

/* No pointer is ever going to be 0xffffffffffffffff, so make that the tombstone */
#define TOMBSTONE ((void *)-1)
// #define UNUSED ((void*)0)
// ^ this is just NULL

zz_hashstr_t *strmap_donotuse_tozzstr(char *str)
{
	size_t len = strlen(str);
	zz_hashstr_t hashstr = { .str = str, .len = len };
	zz_hashstr_t *ptr = zalloc(sizeof(hashstr));
	*ptr = hashstr;
	return ptr;
}

static void zzstr_delete(zz_hashstr_t *str)
{
	free(str);
}

static uint64_t hashstr(zz_hashstr_t *s)
{
	return fnv1a(s->str, s->len);
}

static bool hashstr_eq(zz_hashstr_t *a, zz_hashstr_t *b)
{
	return a->len == b->len && strncmp(a->str, b->str, a->len) == 0;
}

void *strmap_donotuse_make(size_t size, size_t cap)
{
	strmap_hdr_t *hdr = zalloc(sizeof(strmap_hdr_t) + (2 * sizeof(void *)));

	hdr->size = 0;
	hdr->cap = cap;

	hdr->data[0] = zalloc(sizeof(zz_hashstr_t *) * cap);
	hdr->data[1] = zalloc(size * cap);

	return strmap_data(hdr);
}

static void strmap_rebuild(void *map, size_t size)
{
	strmap_hdr_t *hdr = strmap_hdr(map);

	zz_hashstr_t **oldkeys = hdr->data[0];
	uint8_t *oldvals = (void *)hdr->data[1];

	size_t oldcap = hdr->cap;
	hdr->cap *= 2;
	hdr->data[0] = zalloc(sizeof(zz_hashstr_t *) * hdr->cap);
	hdr->data[1] = zalloc(size * hdr->cap);

	/* iterate over all elems */
	for(size_t i = 0; i < oldcap; i++) {
		if(oldkeys[i] == NULL || oldkeys[i] == TOMBSTONE) {
			continue;
		}
		strmap_donotuse_put(map, oldkeys[i], size, &oldvals[i * size]);
	}

	free(oldkeys);
	free(oldvals);
	return;
}

void strmap_donotuse_delete(void *map)
{
	if(!map) {
		return;
	}
	strmap_hdr_t *hdr = strmap_hdr(map);

	zz_hashstr_t **keys = hdr->data[0];
	for(size_t i = 0; i < hdr->cap; i++) {
		if(keys[i] != NULL && keys[i] != TOMBSTONE) {
			zzstr_delete(keys[i]);
			keys[i] = NULL;
		}
	}

	free((void *)hdr->data[0]);
	free((void *)hdr->data[1]);
	free(hdr);

	return;
}

typedef struct lookup {
	uint64_t loc;
	bool exists;
} lookup_t;

static lookup_t strmap_lookup(void *map, zz_hashstr_t *str)
{
	strmap_hdr_t *hdr = strmap_hdr(map);
	zz_hashstr_t **keys = (zz_hashstr_t **)hdr->data[0];
	uint64_t hash = hashstr(str);
	uint64_t mask = hdr->cap - 1;
	hash &= mask;
	uint64_t step = 1; /* TODO: improve */
	for(uint64_t i = hash - step;;) {
		i = (i + step) & mask;
		if(keys[i] == TOMBSTONE) {
			continue;
		}

		if(keys[i] == NULL) {
			return (lookup_t){ .loc = i, .exists = false };
		}

		/* check if it's really it */
		if(hashstr_eq(str, keys[i])) {
			/* yup */
			return (lookup_t){ .loc = i, .exists = true };
		}
	}

	/* you don't get here */
	ASSERT(false, "unreachable");
	return (lookup_t){ .loc = 0, .exists = false };
}

void strmap_donotuse_del(void *map, zz_hashstr_t *str)
{
	strmap_hdr_t *hdr = strmap_hdr(map);

	zz_hashstr_t **strs = (zz_hashstr_t **)hdr->data[0];
	lookup_t lookup = strmap_lookup(map, str);
	zzstr_delete(str);
	if(!lookup.exists) {
		return;
	}

	hdr->size--;
	zzstr_delete(strs[lookup.loc]);
	strs[lookup.loc] = TOMBSTONE;
	return;
}

void strmap_donotuse_put(void *map, zz_hashstr_t *str, size_t size, void *obj)
{
	strmap_hdr_t *hdr = strmap_hdr(map);
	zz_hashstr_t **keys = (zz_hashstr_t **)hdr->data[0];
	uint8_t *vals = (uint8_t *)hdr->data[1];

	/* if hashmap over 75% capacity, rebuild */
	if(hdr->size >= (3 * hdr->cap) / 4) {
		strmap_rebuild(map, size);
		keys = (zz_hashstr_t **)hdr->data[0];
		vals = (uint8_t *)hdr->data[1];
		hdr = strmap_hdr(map);
	}

	lookup_t lookup = strmap_lookup(map, str);
	if(lookup.exists) {
		zzstr_delete(str);
		memcpy(&vals[lookup.loc * size], obj, size);
		return;
	}

	/* insert new item */
	hdr->size++;

	keys[lookup.loc] = str;
	memcpy(&vals[lookup.loc * size], obj, size);
	return;
}

void *strmap_donotuse_get(void *map, zz_hashstr_t *str, size_t size)
{
	strmap_hdr_t *hdr = strmap_hdr(map);
	lookup_t lookup = strmap_lookup(map, str);
	zzstr_delete(str);
	if(!lookup.exists) {
		return NULL;
	}

	uint8_t *ptr = (uint8_t *)hdr->data[1];
	return &ptr[size * lookup.loc];
}

bool strmap_donotuse_has(void *map, zz_hashstr_t *str)
{
	bool res = strmap_lookup(map, str).exists;
	zzstr_delete(str);
	return res;
}

strmap_hdr_t *strmap_hdr(void *map)
{
	return ((strmap_hdr_t *)map) - 1;
}

void *strmap_data(strmap_hdr_t *hdr)
{
	return &hdr->data[0];
}
