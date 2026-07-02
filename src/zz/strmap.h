#ifndef STRMAP_H_
#define STRMAP_H_

#include "base.h"

typedef struct zz_hashstr {
	char *str;
	size_t len;
} zz_hashstr_t;

#ifndef STRMAP_DEFAULT_SIZE
#define STRMAP_DEFAULT_SIZE (64)
#endif

typedef struct strmap_hdr {
	/* how many elements in the strmap */
	size_t size;
	/* how much allocated memory for the strmap (capacity) */
	size_t cap;
	/* data (must be void*) */
	void *data[];
} strmap_hdr_t;

// clang-format off
#define STRMAP(V) V*
// clang-format on

void strmap_donotuse_put(void *map, zz_hashstr_t *str, size_t size, void *obj);
void strmap_donotuse_del(void *map, zz_hashstr_t *str);
void *strmap_donotuse_get(void *map, zz_hashstr_t *str, size_t size);
bool strmap_donotuse_has(void *map, zz_hashstr_t *str);
void *strmap_donotuse_make(size_t size, size_t cap);
void strmap_donotuse_delete(void *map);
zz_hashstr_t *strmap_donotuse_tozzstr(char *str);

strmap_hdr_t *strmap_hdr(void *map);
void *strmap_data(strmap_hdr_t *hdr);

#define strmap_put(m, s, i)                                                  \
	do {                                                                     \
		typeof((i)) the_item_do_not_use_this_name = (i);                     \
		strmap_donotuse_put((void *)(m), strmap_donotuse_tozzstr((s)),       \
							sizeof((m)[0]), &the_item_do_not_use_this_name); \
	} while(0)

#define strmap_get(m, s)                                                \
	({                                                                  \
		(typeof(((m))))strmap_donotuse_get(                             \
			(void *)(m), strmap_donotuse_tozzstr((s)), sizeof((m)[0])); \
	})

#define strmap_del(m, s)                                                \
	do {                                                                \
		strmap_donotuse_del((void *)(m), strmap_donotuse_tozzstr((s))); \
	} while(0)

#define strmap_has(m, s) \
	({ strmap_donotuse_has((void *)(m), strmap_donotuse_tozzstr((s))); })

#define strmap_make(type) \
	({ strmap_donotuse_make(sizeof(type), STRMAP_DEFAULT_SIZE); })

#define strmap_delete(v)             \
	do {                             \
		strmap_donotuse_delete((v)); \
	} while(0)

#define strmap_iter(m, k, v, code)                                             \
	do {                                                                       \
		for(size_t iter_indx = 0; iter_indx < strmap_hdr((m))->cap;            \
			iter_indx++) {                                                     \
			if(((zz_hashstr_t **)(strmap_hdr((m))->data[0]))[iter_indx] ==     \
				   NULL ||                                                     \
			   ((zz_hashstr_t **)(strmap_hdr((m))->data[0]))[iter_indx] ==     \
				   (void *)-1) {                                               \
				continue;                                                      \
			}                                                                  \
			k = ((zz_hashstr_t **)(strmap_hdr((m))->data[0]))[iter_indx]->str; \
			v = ((typeof(v) *)(strmap_hdr((m))->data[1]))[iter_indx];          \
			do {                                                               \
				code;                                                          \
			} while(0);                                                        \
		}                                                                      \
	} while(0)

#endif /* STRMAP_H_ */
