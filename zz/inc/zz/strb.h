/* string builder */

#ifndef STRB_H_
#define STRB_H_

#include "base.h"

#define STRB_INITIAL_CAP (32)

typedef struct strb {
	char *str; /* string */
	size_t len; /* length (size) of string */
	size_t cap; /* allocated capacity of string */
} strb_t;

/* creates a string builder with an optional initial string */
strb_t strb_make(char *initial_str, size_t len);

/* add content to the string builder */
void strb_add(strb_t *strb, char *content, size_t len);

/* add single character to the builder */
void strb_add_chr(strb_t *strb, char content);

/* finalizes content in the string builder */
char *strb_build(strb_t *strb);

/* deletes a string builder */
void strb_delete(strb_t *strb);

#endif /* STRB_H_ */
