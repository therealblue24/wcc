#ifndef BASE_H_
#define BASE_H_

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <time.h>
#include <inttypes.h>

#ifndef typeof
#define typeof __typeof__
#endif

// clang-format off
#define you_know_you_can_theoretically_name_this_whatever_you_want_so_im_gonna_test_that_so_this_is_the_identity_macro_basically_and_it_is_used_for_stringifying_expressions_in_a_macro(x) x

/* turns `x` into a string, compile-time */
#define xstr(x) you_know_you_can_theoretically_name_this_whatever_you_want_so_im_gonna_test_that_so_this_is_the_identity_macro_basically_and_it_is_used_for_stringifying_expressions_in_a_macro(#x)

// clang-format on

/* aligns `v` up to multiple of `to` */
size_t align_to(size_t v, size_t to);

/* calloc(1, size) -- never returns NULL, always errors out */
void *zalloc(size_t size);

/* calloc() but never returns NULL, always errors out */
void *zcalloc(size_t count, size_t size);

/* realloc() but always errors out */
void *zrealloc(void *ptr, size_t size);

/* strcpy() for memory */
void *memdup(void *mem, size_t size);

/* memdup() but allocates extra space */
void *memdup_extra(void *mem, size_t src_size, size_t dst_size);

/* fnv1a hash */
uint64_t fnv1a(void *data, size_t size);

/* memset(mem, 0, size) */
#define wipe(mem, size)           \
	do {                          \
		memset((mem), 0, (size)); \
	} while(0)

/* errors out program with a reason (can be used like printf) */
#define ERROR(reas, ...)                                                    \
	do {                                                                    \
		fprintf(stderr, "ERROR: %s:%d: %s: " reas "\n", __FILE__, __LINE__, \
				__func__ __VA_OPT__(, ) __VA_ARGS__);                       \
		exit(EXIT_FAILURE);                                                 \
	} while(0);

/* like error, but warns and doesn't exit out */
#define WARN(reas, ...)                                                    \
	do {                                                                   \
		fprintf(stderr, "WARN: %s:%d: %s: " reas "\n", __FILE__, __LINE__, \
				__func__ __VA_OPT__(, ) __VA_ARGS__);                      \
	} while(0);

/* assertion macros */
#ifdef NDEBUG

#define ASSERT(cond, reas, ...) \
	do {                        \
	} while(0)

#define ASSERT_RET(cond, ret, reas, ...) \
	do {                                 \
	} while(0)
#else

/* Assertion failed: (cond), function __func__, file __FILE__, line __LINE__.
 * Reason: reas     */
#define ASSERT(cond, reas, ...)                                               \
	if(!(cond)) {                                                             \
		fprintf(stderr,                                                       \
				"Assertion failed: " xstr(                                    \
					(cond)) ", function %s, file %s, line %d.\nReason: " reas \
							"\n",                                             \
				__func__, __FILE__, __LINE__ __VA_OPT__(, ) __VA_ARGS__);     \
		exit(EXIT_FAILURE);                                                   \
	}

#define ASSERT_RET(cond, ret, reas, ...)                                      \
	if(!(cond)) {                                                             \
		fprintf(stderr,                                                       \
				"Assertion failed: " xstr(                                    \
					(cond)) ", function %s, file %s, line %d.\nReason: " reas \
							"\n",                                             \
				__func__, __FILE__, __LINE__ __VA_OPT__(, ) __VA_ARGS__);     \
		return (ret);                                                         \
	}
#endif

/* like ASSERT but is never impacted by NDEBUG */
#define ENSURE(cond, reas, ...)                                               \
	if(!(cond)) {                                                             \
		fprintf(stderr,                                                       \
				"Assertion failed: " xstr(                                    \
					(cond)) ", function %s, file %s, line %d.\nReason: " reas \
							"\n",                                             \
				__func__, __FILE__, __LINE__ __VA_OPT__(, ) __VA_ARGS__);     \
		exit(EXIT_FAILURE);                                                   \
	}

#define ENSURE_RET(cond, ret, reas, ...)                                      \
	if(!(cond)) {                                                             \
		fprintf(stderr,                                                       \
				"Assertion failed: " xstr(                                    \
					(cond)) ", function %s, file %s, line %d.\nReason: " reas \
							"\n",                                             \
				__func__, __FILE__, __LINE__ __VA_OPT__(, ) __VA_ARGS__);     \
		return (ret);                                                         \
	}

/* attrs */
#define PACKED __attribute__((packed))
#define INLINE inline __attribute__((always_inline))
#define UNUSEDA __attribute__((unused))
#define UNUSED(var) (void)var

/* does string `thing` start with `with`? */
int starts_with(char *thing, char *with);

/* strdup() but portable */
char *mystrdup(char *str);
/* strndup() but portable */
char *mystrndup(char *str, size_t n);

/* reads a file; turns "\r\n" -> "\n" */
char *file_reader(FILE *f);

void report_time(char *l, long s, long e);

#if defined(__x86_64__) || defined(_M_X64)
#define DEFAULT_BACKEND IR_ARCH_X64_SYSV
#endif /* x86 */

#if defined(__aarch64__) || defined(_M_ARM64)
#define DEFAULT_BACKEND IR_ARCH_AARCH64_APPLE
#endif /* arm */

#ifndef DEFAULT_BACKEND
#warning no default backend for this arch, defaulting to x86
#define DEFAULT_BACKEND IR_ARCH_X64_SYSV
#endif /* DEFAULT_BACKEND */

#endif /* BASE_H_ */
