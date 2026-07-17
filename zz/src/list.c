#include "list.h"
#include "base.h"

list_hdr_t *list_hdr(void *list)
{
	return ((list_hdr_t *)list) - 1;
}

void *list_data(list_hdr_t *hdr)
{
	return &hdr->data[0];
}

/* Makes a list. `count` is initial capacity, `size` is the size
 * of the type. */
void *list_donotuse_make(size_t count, size_t size)
{
	list_hdr_t *hdr = zalloc((count * size) + sizeof(list_hdr_t));

	hdr->cap = count;
	hdr->size = 0;

	return list_data(hdr);
}

/* Deletes a list. */
void list_donotuse_delete(void *list)
{
	free(list_hdr(list));
	return;
}

/* Resizes/reserves space of list, using `count` as cap */
void *list_donotuse_reserve(void *list, size_t count, size_t size)
{
	list_hdr_t *newhdr = (list_hdr_t *)zrealloc(
		list_hdr(list), (count * size) + sizeof(list_hdr_t));

	newhdr->cap = count;

	return list_data(newhdr);
}

/* reserve but only if hdr->size + count > hdr->cap */
/* and reserves with LIST_NEW_SIZE(hdr->size + count) */
void *list_donotuse_fit(void *list, size_t count, size_t size)
{
	list_hdr_t *hdr = list_hdr(list);
	if(hdr->size + count > hdr->cap) {
		return list_donotuse_reserve(list, LIST_NEW_SIZE(hdr->size + count),
									 size);
	}
	return list;
}
