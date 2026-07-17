/* this is a really bad profiler, but it works ig */
#include "prof.h"
#include "list.h"
#include "strmap.h"
#include "base.h"
#include <time.h>

typedef struct pentry {
	long start, end;
	char *name;
	int depth;
} pentry_t;

LIST(pentry_t) profile_stack = NULL;
LIST(pentry_t) done = NULL;
int depth = 0;

void prof_begin(char *name)
{
	pentry_t e = {
		.start = clock(),
		.name = name,
		.depth = depth++,
	};
	list_append(profile_stack, e);
}

static void report()
{
	/* collect stuff */
	STRMAP(float) times = strmap_make(float);

	for(size_t i = list_len(done) - 1; i >= 0; i--) {
		pentry_t ent = done[i];
		long d = ent.end - ent.start;
		const float f = (float)CLOCKS_PER_SEC / 1000.f;
		float ms = (float)d / f;

		if(strmap_has(times, ent.name)) {
			float have = *strmap_get(times, ent.name);
			ms += have;
		}

		strmap_put(times, ent.name, ms);

		if(i == 0) {
			break;
		}
	}

	for(size_t i = list_len(done) - 1; i >= 0; i--) {
		pentry_t ent = done[i];
		if(!strmap_has(times, ent.name)) {
			if(i == 0) {
				break;
			}
			continue;
		}

		float ms = *strmap_get(times, ent.name);
		strmap_del(times, ent.name);

		for(int d = 0; d < ent.depth; d++) {
			printf("\t");
		}

		printf("%.2fms\t%s\n", ms, ent.name);

		if(i == 0) {
			break;
		}
	}

	strmap_delete(times);
	return;
}

void prof_end()
{
	long e = clock();
	depth--;
	pentry_t ent = list_pop(profile_stack);
	ent.end = e;
	list_append(done, ent);

	if(list_len(profile_stack) == 0) {
		if(do_profile) {
			report();
			fflush(stdout);
		}

		list_hdr(done)->size = 0;
	}
	return;
}

void prof_make()
{
	profile_stack = list_make(pentry_t);
	done = list_make(pentry_t);
}

void prof_delete()
{
	list_delete(profile_stack);
	list_delete(done);
}
