#include "type.h"
#include "zz/arena.h"
#include "zz/strmap.h"
#include "parse.h"
#include <stdint.h>

static arena_t type_arena;

type_t *TY_INT = &(type_t){ .kind = TYPE_INT,
							.size = 4,
							.align = 4,
							.to = NULL,
							.unsignd = false };

type_t *TY_LONG = &(type_t){ .kind = TYPE_LONG,
							 .size = 8,
							 .align = 8,
							 .to = NULL,
							 .unsignd = false };

type_t *TY_CHAR = &(type_t){ .kind = TYPE_CHAR,
							 .size = 1,
							 .align = 1,
							 .to = NULL,
							 .unsignd = false };

type_t *TY_SHORT = &(type_t){ .kind = TYPE_SHORT,
							  .size = 2,
							  .align = 2,
							  .to = NULL,
							  .unsignd = false };

type_t *TY_UINT = &(type_t){ .kind = TYPE_INT,
							 .size = 4,
							 .align = 4,
							 .to = NULL,
							 .unsignd = true };

type_t *TY_ULONG = &(type_t){ .kind = TYPE_LONG,
							  .size = 8,
							  .align = 8,
							  .to = NULL,
							  .unsignd = true };

type_t *TY_UCHAR = &(type_t){ .kind = TYPE_CHAR,
							  .size = 1,
							  .align = 1,
							  .to = NULL,
							  .unsignd = true };

type_t *TY_USHORT = &(type_t){ .kind = TYPE_SHORT,
							   .size = 2,
							   .align = 2,
							   .to = NULL,
							   .unsignd = true };

type_t *TY_VOID =
	&(type_t){ .kind = TYPE_VOID, .size = 0, .align = 1, .to = NULL };

type_t *TY_BOOL =
	&(type_t){ .kind = TYPE_BOOL, .size = 1, .align = 1, .to = NULL };

type_t *TY_PTR =
	&(type_t){ .kind = TYPE_PTR, .size = 8, .align = 8, .to = NULL };

/* makes type arenas */
void type_make_arenas(void)
{
	ENSURE(arena_make(&type_arena, ARENA_DEFAULT_SIZE) == 0,
		   "failed to create type arena");
	return;
}

/* deletes type arenas */
void type_delete_arenas(void)
{
	arena_delete(&type_arena);
}

bool type_is_int(type_t *ty)
{
	return ty && (ty->kind == TYPE_INT || ty->kind == TYPE_LONG ||
				  ty->kind == TYPE_SHORT || ty->kind == TYPE_CHAR ||
				  ty->kind == TYPE_BOOL);
}

bool type_is_ptr(type_t *ty)
{
	return ty && (ty->kind == TYPE_PTR || ty->kind == TYPE_ARRAY);
}

bool type_is_signed(type_t *ty)
{
	return !ty->unsignd;
}

type_t *type_clone(type_t *ty)
{
	type_t *typtr = arena_alloc(&type_arena, sizeof(type_t));
	*typtr = *ty;
	return typtr;
}

type_t *type_ptr_to(type_t *ty)
{
	type_t *typtr = arena_alloc(&type_arena, sizeof(type_t));
	memcpy(typtr, TY_PTR, sizeof(type_t));
	typtr->to = ty;
	typtr->ident = ty->ident;
	typtr->unsignd = true;
	return typtr;
}

type_t *type_arr_to(type_t *type, size_t alen)
{
	type_t *typtr = arena_alloc(&type_arena, sizeof(type_t));
	typtr->align = type->align;
	typtr->size = type->size * alen;
	typtr->kind = TYPE_ARRAY;
	typtr->to = type;
	typtr->alen = alen;
	typtr->ident = type->ident;
	typtr->unsignd = type->unsignd;
	return typtr;
}

type_t *type_func_to(type_t *ret_ty)
{
	type_t *typtr = arena_alloc(&type_arena, sizeof(type_t));
	typtr->size = typtr->align = 0;
	typtr->kind = TYPE_FUNC;
	typtr->to = ret_ty;
	return typtr;
}

static type_t *type_deref(type_t *ty)
{
	if(type_is_ptr(ty)) {
		return ty->to;
	} else {
		compile_err(ty->ident->loc, "tried to dereference non-pointer");
		return TY_LONG;
	}
}

static type_t *infer_type(uint64_t num)
{
	int64_t nums = (int64_t)num;
	/* int */
	if(INT32_MIN >= nums && nums <= INT32_MAX) {
		return TY_INT;
	}

	/* unsigned int */
	if(num <= UINT32_MAX) {
		return TY_UINT;
	}

	/* long */
	if(INT64_MIN >= nums && nums <= INT64_MAX) {
		return TY_LONG;
	}

	return TY_ULONG;
}

/* can't miss the naming opportunity */
static void fastcast(node_t **node, type_t *to)
{
	node_t *cast = node_unary(NODE_CAST, *node, (*node)->tok);
	cast->type = to;
	*node = cast;
	return;
}

/* 6.3.1.8 usual arithmetic conversions */
static type_t *usual_arith_conv_type(type_t *lt, type_t *rt)
{
	type_t *promote_to;

	/* if pointers are involved, decay the left pointer and use that */
	if(type_is_ptr(lt) || type_is_ptr(rt)) {
		promote_to = type_ptr_to(lt->to);
		goto promote;
	} else {
		/* promote both sides to "natural" integer if it's shorter */
		if(lt->size < 4) {
			lt = lt->unsignd ? TY_UINT : TY_INT;
		}
		if(rt->size < 4) {
			rt = rt->unsignd ? TY_UINT : TY_INT;
		}

		/* if sizes are incompatible, select greater one */
		if(lt->size != rt->size) {
			promote_to = lt->size >= rt->size ? lt : rt;
			goto promote;
		}

		/* unsigned takes precedence */
		if(lt->unsignd) {
			promote_to = lt;
			goto promote;
		}
		if(rt->unsignd) {
			promote_to = rt;
			goto promote;
		}

		/* both are same */
		promote_to = lt;
		goto promote;
	}
promote:
	return promote_to;
}

static bool quick_type_same_chk(type_t *left, type_t *right)
{
	if(left == right) {
		return true;
	}

	if(left->unsignd == right->unsignd && left->kind == right->kind &&
	   left->align == right->align) {
		enum type_kind k = left->kind;
		if(k == TYPE_VOID || type_is_int(left) || k == TYPE_PTR) {
			return true;
		}
	}

	if(memcmp(left, right, sizeof(type_t)) == 0) {
		return true;
	}

	return false;
}

static void usual_arith_conv(node_t **lhs, node_t **rhs)
{
	type_t *promote_to = usual_arith_conv_type((*lhs)->type, (*rhs)->type);
	if(!quick_type_same_chk((*lhs)->type, promote_to))
		fastcast(lhs, promote_to);
	if(!quick_type_same_chk((*rhs)->type, promote_to))
		fastcast(rhs, promote_to);
	return;
}

void type_propagate(node_t *node)
{
	if(!node) {
		return;
	}
	if(node->typed) {
		return;
	}
	node->typed = true;

	type_propagate(node->lhs);
	type_propagate(node->rhs);
	type_propagate(node->next);
	type_propagate(node->cond);
	type_propagate(node->then);
	type_propagate(node->elze);
	type_propagate(node->init);
	type_propagate(node->inc);

	for(node_t *b = node->body; b; b = b->next) {
		type_propagate(b);
	}
	for(node_t *b = node->fargs; b; b = b->next) {
		type_propagate(b);
	}

	switch(node->kind) {
	case NODE_NUM:
		if(node->type)
			break;
		node->type = infer_type(node->num);
		break;

	case NODE_FUNCALL: {
		obj_t **fnptr = strmap_get(known_funcs, node->fname);
		if(!fnptr) {
			compile_err_node(node, "unknown function '%s'", node->fname);
		}
		obj_t *fn = *fnptr;
		node->type = fn->type->to;
	} break;

	case NODE_EQ:
	case NODE_LE:
	case NODE_NE:
	case NODE_LT:
	case NODE_GT:
	case NODE_GE:
		node->type = node->lhs->type->unsignd ? TY_UINT : TY_INT;
		break;

	case NODE_COMMA:
		node->type = node->rhs->type;
		break;

	case NODE_VAR:
		node->type = node->var->type;
		if(node->var->type->kind == TYPE_VOID) {
			compile_err(node->lhs->type->ident->loc, "invalid void decltype");
		}
		break;
	case NODE_MEMBER:
		node->type = node->memb->type;
		if(node->type->kind == TYPE_VOID) {
			compile_err(node->lhs->type->ident->loc, "invalid void decltype");
		}
		break;
	case NODE_ADD:
	case NODE_SUB:
	case NODE_SHL:
	case NODE_SHR:
	case NODE_AND:
	case NODE_OR:
	case NODE_MOD:
	case NODE_LOGAND:
	case NODE_LOGOR:
	case NODE_EOR:
	case NODE_MUL:
	case NODE_DIV:
		usual_arith_conv(&node->lhs, &node->rhs);
		node->type = node->lhs->type;
		break;
	case NODE_LOGNEG:
	case NODE_NOT:
	case NODE_NEG:
		node->type = usual_arith_conv_type(TY_INT, node->lhs->type);
		fastcast(&node->lhs, node->type);
		break;
	case NODE_ASSIGN:
		fastcast(&node->rhs, node->lhs->type);
		node->type = node->lhs->type;
		if(node->lhs->type->kind == TYPE_VOID) {
			compile_err(node->lhs->type->ident->loc, "invalid void decltype");
		}
		break;
	case NODE_ADDR:
		if(node->lhs->type->kind == TYPE_ARRAY) {
			node->type = type_ptr_to(node->lhs->type->to);
		} else {
			node->type = type_ptr_to(node->lhs->type);
		}
		if(node->var) {
			node->var->addressed = true;
		}
		if(node->lhs->type->kind == TYPE_VOID) {
			compile_err(node->lhs->type->ident->loc, "invalid void decltype");
		}
		break;
	case NODE_DEREF:
		node->type = type_deref(node->lhs->type);
		if(node->lhs->type->kind == TYPE_VOID) {
			compile_err(node->lhs->type->ident->loc, "invalid void decltype");
		}
		break;
	case NODE_STMT_EXPR: {
		if(!node->body) {
			compile_err(node->tok->loc,
						"statement expression must have at least 1 statement");
		}

		node_t *end = node->body;
		while(end->next) {
			end = end->next;
		}

		if(end->kind != NODE_EXPR_STMT) {
			compile_err(
				node->tok->loc,
				"statement expression must end with expression statement");
		}

		node->type = end->lhs->type;
	}; break;

	default:
		break;
	}
	return;
}
