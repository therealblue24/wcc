#include "type.h"
#include "zz/arena.h"
#include "zz/strmap.h"
#include "parse.h"

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

type_t *type_ptr_to(type_t *ty)
{
	type_t *typtr = scr_alloc(sizeof(type_t));
	memcpy(typtr, TY_PTR, sizeof(type_t));
	typtr->to = ty;
	typtr->ident = ty->ident;
	typtr->unsignd = true;
	return typtr;
}

type_t *type_arr_to(type_t *type, size_t alen)
{
	type_t *typtr = scr_alloc(sizeof(type_t));
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
	type_t *typtr = scr_alloc(sizeof(type_t));
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

void type_propagate(node_t *node)
{
	if(!node) {
		return;
	}

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
	case NODE_EQ:
	case NODE_LE:
	case NODE_NE:
	case NODE_LT:
	case NODE_GT:
	case NODE_GE:
	case NODE_NUM:
	case NODE_FUNCALL:
		node->type = TY_LONG;
		break;
	case NODE_VAR:
		node->type = node->var->type;
		if(node->var->type->kind == TYPE_VOID) {
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
	case NODE_LOGNEG:
	case NODE_EOR:
	case NODE_MUL:
	case NODE_DIV:
	case NODE_NEG:
		node->type = node->lhs->type;
		break;
	case NODE_ASSIGN:
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
}
