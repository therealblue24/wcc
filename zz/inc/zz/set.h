/* pointer set, only meant to be used with pointers */
#ifndef SET_H_
#define SET_H_

#include "base.h"

#ifndef SET_DEFAULT_SIZE
#define SET_DEFAULT_SIZE (64)
#endif /* SET_DEFAULT_SIZE */

typedef struct set_hdr {
	/* how many elements in the strmap */
	size_t size;
	/* how much allocated memory for the strmap (capacity) */
	size_t cap;
	/* data (can be any type, using VLAs) */
	uint8_t data[];
} set_hdr_t;

// clang-format off
#define SET(x) x*
// clang-format on

void set_donotuse_add(void **set, void *item);
void set_del(void *set, void *item);
bool set_has(void *set, void *item);
void *set_get(void *set, void *item);

#define set_add(set, item) set_donotuse_add((void **)((set)), (item))

void *set_make(size_t cap);
void set_reset(void *set);
void *set_empty(void);
void set_delete(void *set);

set_hdr_t *set_hdr(void *set);
void *set_data(set_hdr_t *hdr);

/* kinda stupid, but dont want to pollute namespace */
#define DONOTUSE_SET_TOMBSTONE ((void *)-1)

#define set_iter(set, v, code)                               \
	do {                                                     \
		void **donotuse_sset = (void **)((set));             \
		for(size_t i = 0; i < set_hdr((set))->cap; i++) {    \
			if(donotuse_sset[i] == DONOTUSE_SET_TOMBSTONE || \
			   !donotuse_sset[i]) {                          \
				continue;                                    \
			}                                                \
			v = (typeof(*set))(donotuse_sset[i]);            \
			code;                                            \
		}                                                    \
	} while(0)

#endif /* SET_H_ */
