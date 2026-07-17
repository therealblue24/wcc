#ifndef PROF_H_
#define PROF_H_

#define TIMEIT(label, code) \
	do {                    \
		prof_begin(label);  \
		code;               \
		prof_end();         \
	} while(0)

extern int do_profile;

/* begin profiling in section */
void prof_begin(char *name);
/* end profiling in section */
void prof_end();

/* initalize profiling stack */
void prof_make();
/* delete profiling stack */
void prof_delete();

#endif /* PROF_H_ */
