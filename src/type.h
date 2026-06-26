#ifndef TYPE_H_
#define TYPE_H_

#include "zz/base.h"
#include "lex.h"

enum type_kind {
	TYPE_VOID, /* void */
	TYPE_BOOL, /* _Bool */
	TYPE_CHAR, /* char */
	TYPE_SHORT, /* short */
	TYPE_INT, /* int */
	TYPE_LONG, /* long */
	TYPE_PTR, /* a pointer */
	TYPE_FUNC, /* a function */
	TYPE_ARRAY, /* an array */
};

typedef struct type {
	enum type_kind kind;
	size_t size; /* size, alignment */
	size_t align;
	struct type *to; /* a pointer to? */
	token_t *ident; /* identifier of type */
	size_t alen; /* array length */
	bool unsignd; /* is this type unsigned? */
} type_t;

extern type_t *TY_VOID;
extern type_t *TY_BOOL;
extern type_t *TY_CHAR;
extern type_t *TY_SHORT;
extern type_t *TY_INT;
extern type_t *TY_LONG;
extern type_t *TY_UCHAR;
extern type_t *TY_USHORT;
extern type_t *TY_UINT;
extern type_t *TY_ULONG;
extern type_t *TY_PTR;

bool type_is_int(type_t *ty);
bool type_is_ptr(type_t *ty);
bool type_is_signed(type_t *ty);

type_t *type_clone(type_t *ty);
type_t *type_ptr_to(type_t *ty);
type_t *type_arr_to(type_t *ty, size_t alen);
type_t *type_func_to(type_t *ret_ty);

struct node;
void type_propagate(struct node *node);

#endif /* TYPE_H_ */
