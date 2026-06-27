#include "base.h"
#include "strb.h"

static INLINE size_t size_max(size_t a, size_t b)
{
	return a > b ? a : b;
}

static void strb_fit(strb_t *strb, size_t to)
{
	if(!strb->cap) {
		strb->cap = size_max(STRB_INITIAL_CAP, to);
		goto realloc_;
	}

	size_t new_size = size_max(strb->cap, to);
	if(new_size == strb->cap) {
		return;
	} else {
		while(strb->cap < to) {
			strb->cap = (strb->cap + 1) * 2;
		}
	}
realloc_:
	strb->str = zrealloc(strb->str, strb->cap);
	return;
}

/* creates a string builder with an optional initial string */
strb_t strb_make(char *initial_str, size_t len)
{
	strb_t strb = { 0 };
	strb.len = len;
	strb_fit(&strb, strb.len);
	if(initial_str) {
		memcpy(strb.str, initial_str, strb.len);
	}

	return strb;
}

/* add content to the string builder */
void strb_add(strb_t *strb, char *content, size_t len)
{
	strb_fit(strb, strb->len + len);
	memcpy(&strb->str[strb->len], content, len);
	strb->len += len;
	return;
}

/* add single character to the builder */
void strb_add_chr(strb_t *strb, char content)
{
	strb_fit(strb, strb->len + 1);
	strb->str[strb->len++] = content;
	return;
}

/* finalizes content in the string builder */
char *strb_build(strb_t *strb)
{
	size_t newcap = strb->len + 1;
	strb->cap = newcap;
	strb->str = zrealloc(strb->str, newcap);
	strb->str[strb->len] = 0;
	return strb->str;
}

/* deletes a string builder */
void strb_delete(strb_t *strb)
{
	free(strb->str);
	strb->str = NULL;
	strb->cap = strb->len = 0;
	return;
}
