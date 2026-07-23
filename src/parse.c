#include "parse.h"
#include "zz/base.h"
#include "zz/list.h"
#include "zz/strmap.h"
#include "zz/arena.h"
#include "lex.h"
#include "type.h"

static arena_t obj_arena;
static arena_t node_arena;
static arena_t memb_arena;
static arena_t scope_arena;
static LIST(scope_t) scopes;

/* makes parsing arenas */
void parse_make_arenas(void)
{
	ENSURE(arena_make(&obj_arena, ARENA_DEFAULT_SIZE) == 0,
		   "failed to create object arena");
	ENSURE(arena_make(&node_arena, ARENA_DEFAULT_SIZE) == 0,
		   "failed to create AST node arena");
	ENSURE(arena_make(&memb_arena, ARENA_DEFAULT_SIZE) == 0,
		   "failed to create struct member arena");
	ENSURE(arena_make(&scope_arena, ARENA_DEFAULT_SIZE) == 0,
		   "failed to create scope arena");
	scopes = list_make(obj_t *);
	return;
}

/* deletes parsing arenas */
void parse_delete_arenas(void)
{
	arena_delete(&obj_arena);
	arena_delete(&node_arena);
	arena_delete(&memb_arena);
	arena_delete(&scope_arena);
	list_delete(scopes);
	return;
}

STRMAP(obj_t *) locals = NULL;
STRMAP(obj_t *) globals = NULL;
STRMAP(obj_t *) known_funcs = NULL;
long local_order = 0;
long global_order = 0;

/* straight from type.c */
/* can't miss the naming opportunity */
static void fastcast(node_t **node, type_t *to)
{
	node_t *cast = node_unary(NODE_CAST, *node, (*node)->tok);
	cast->type = to;
	*node = cast;
	return;
}

/* ron's universal number kounter */
/* very important, critical piece of code */
static long runk(int reset)
{
	static int counter = 1;
	if(reset) {
		counter = 0;
	}
	return counter++;
}

/* push new scope */
static void scope_push()
{
	scope_t sc;
	sc.vars = strmap_make(obj_t *);
	sc.types = strmap_make(type_t *);
	list_append(scopes, sc);
	return;
}

/* pop scope */
static void scope_pop()
{
	scope_t sc = list_pop(scopes);
	strmap_delete(sc.vars);
	strmap_delete(sc.types);
	return;
}

/* add var to current scope (& "global" locals) */
static void scope_add_var(obj_t *var)
{
	if(strmap_has(list_peek(scopes).vars, var->name)) {
		scope_el_t *e = *strmap_get(list_peek(scopes).vars, var->name);
		if(e->is_type) {
			compile_err(NULL, "redefinition of '%s' as variable", var->name);
		}
	}
	scope_el_t *elem = arena_alloc(&scope_arena, sizeof(scope_el_t));
	elem->is_type = false;
	elem->var = var;
	strmap_put(list_peek(scopes).vars, var->name, elem);
	strmap_put(locals, var->name, var);
	return;
}

/* add type to current scope */
static void scope_add_type(char *name, type_t *typ)
{
	if(strmap_has(list_peek(scopes).vars, name)) {
		scope_el_t *e = *strmap_get(list_peek(scopes).vars, name);
		if(!e->is_type) {
			compile_err(NULL, "redefinition of '%s' as type",
						typ->ident->content);
		}
	}
	scope_el_t *elem = arena_alloc(&scope_arena, sizeof(scope_el_t));
	elem->is_type = true;
	elem->type = typ;
	strmap_put(list_peek(scopes).vars, name, elem);
	return;
}

/* add tag to current scope */
static void scope_add_tag(type_t *typ)
{
	strmap_put(list_peek(scopes).types, typ->ident->content, typ);
	return;
}

/* makes a node */
node_t *node_make(enum node_kind kind, token_t *tok)
{
	node_t *node = arena_alloc(&node_arena, sizeof(node_t));
	node->kind = kind;
	node->tok = tok;
	return node;
}

/* deallocates a node */
void node_delete(node_t *node)
{
	UNUSED(node);
	// free(node);
	return;
}

/* deallocates a whole node AST tree */
void node_delete_all(node_t *root)
{
#define del(x)              \
	if(x) {                 \
		node_delete_all(x); \
	}

	del(root->lhs);
	del(root->rhs);
	del(root->next);
	del(root->body);
	del(root->cond);
	del(root->then);
	del(root->elze);
	del(root->init);
	del(root->inc);
	del(root->fargs);

	if(root->fname) {
		free(root->fname);
	}

	if(root->label) {
		free(root->label);
	}

#undef del
	node_delete(root);
	return;
}

/* makes a member */
member_t *member_make(type_t *type, token_t *ident, size_t loc)
{
	member_t *memb = arena_alloc(&memb_arena, sizeof(member_t));
	memb->type = type;
	memb->ident = ident;
	memb->loc = loc;
	return memb;
}

/* -- node types -- */

/* make a binop node */
node_t *node_bin(enum node_kind kind, node_t *lhs, node_t *rhs, token_t *tok)
{
	node_t *node = node_make(kind, tok);
	node->lhs = lhs;
	node->rhs = rhs;
	return node;
}

/* make a unaryop node */
node_t *node_unary(enum node_kind kind, node_t *lhs, token_t *tok)
{
	node_t *node = node_make(kind, tok);
	node->lhs = lhs;
	return node;
}

/* make a number node */
node_t *node_num(uint64_t val, token_t *tok)
{
	node_t *node = node_make(NODE_NUM, tok);
	node->num = val;
	return node;
}

static node_t *node_num_tok(token_t *tok)
{
	node_t *node = node_make(NODE_NUM, tok);
	node->num = tok->num;
	node->type = tok->type;
	return node;
}

/* make a variable node */
node_t *node_var(obj_t *var, token_t *tok)
{
	node_t *node = node_make(NODE_VAR, tok);
	node->var = var;
	node->type = var->type;
	return node;
}

/* -- variables/objects -- */

static obj_t *obj_make_noadd(char *name, type_t *type, bool is_func)
{
	obj_t *obj = arena_alloc(&obj_arena, sizeof(obj_t));
	obj->is_func = is_func;
	obj->name = name;
	obj->off = 0;
	obj->type = type;
	return obj;
}

/* create an object with a name `name` */
obj_t *obj_make(char *name, type_t *type, bool is_func)
{
	obj_t *obj = obj_make_noadd(name, type, is_func);
	obj->order = local_order++;
	scope_add_var(obj);
	return obj;
}

/* create a global object */

obj_t *obj_make_global(char *name, type_t *type, bool is_func)
{
	obj_t *obj = obj_make_noadd(name, type, is_func);
	obj->is_global = true;
	obj->data = NULL;
	obj->order = global_order++;
	strmap_put(globals, obj->name, obj);
	return obj;
}

#define STR_SIZE (32)

char *anon_name(int managed)
{
	char *name;
	if(managed) {
		name = zalloc(STR_SIZE);
	} else {
		name = scr_alloc(STR_SIZE);
	}

	snprintf(name, STR_SIZE - 1, ".anon%ld", runk(0));

	return name;
}

obj_t *obj_make_str(token_t *str)
{
	char *name = anon_name(1);
	obj_t *obj = obj_make_global(name, str->type, false);
	obj->is_anon = true;
	obj->data = (void *)str->str;
	obj->data_size = str->len;
	return obj;
}

obj_t *obj_make_anon(type_t *type)
{
	char *name = anon_name(1);
	obj_t *obj = obj_make_noadd(name, type, false);
	obj->is_anon = true;
	return obj;
}

/* finds a type given name */
static type_t *find_type(token_t *tok)
{
	char *content = tok->content;

	/* traverse scope */
	if(list_len(scopes)) {
		for(size_t i = list_len(scopes) - 1; i >= 0; i--) {
			scope_t sc = scopes[i];
			scope_el_t **found = strmap_get(sc.vars, content);
			if(found && (*found)->is_type) {
				return (*found)->type;
			}

			if(i == 0) {
				break;
			}
		}
	}

	return NULL;
}

/* finds a struct type (tag) given name */
static type_t *find_tag(token_t *tok)
{
	char *content = tok->content;

	/* traverse scope */
	if(list_len(scopes)) {
		for(size_t i = list_len(scopes) - 1; i >= 0; i--) {
			scope_t sc = scopes[i];
			type_t **found = strmap_get(sc.types, content);
			if(found) {
				return *found;
			}

			if(i == 0) {
				break;
			}
		}
	}

	return NULL;
}

/* finds a local variable given token */
static obj_t *find_var(token_t *tok)
{
	/* traverse list */
	char *str = mystrndup(tok->loc, tok->len);

	if(list_len(scopes)) {
		for(size_t i = list_len(scopes) - 1; i >= 0; i--) {
			scope_t sc = scopes[i];
			scope_el_t **found = strmap_get(sc.vars, str);
			if(found && !(*found)->is_type) {
				free(str);
				return (*found)->var;
			}

			if(i == 0) {
				break;
			}
		}
	}

	if(globals) {
		obj_t **found = strmap_get(globals, str);
		if(found) {
			free(str);
			return *found;
		}
	}

	free(str);

	return NULL;
}

/* delete an object */
void obj_delete(obj_t *obj)
{
	if(obj->data)
		free(obj->data);
	free(obj->name);
	// scr_free(obj);
}

/* deletes all objects in list */
void obj_delete_all(LIST(obj_t *) objs)
{
	if(!objs) {
		return;
	}
	for(size_t i = 0; i < list_len(objs); i++) {
		obj_delete(objs[i]);
	}
	list_delete(objs);
	return;
}

/* --- recursive descent parser --- */

/* forward defs */
static node_t *parse_mul(token_t *tok, token_t **rest);
static node_t *parse_expr(token_t *tok, token_t **rest);
static node_t *parse_prim(token_t *tok, token_t **rest);
static node_t *parse_postfix(token_t *tok, token_t **rest);
static node_t *parse_unary(token_t *tok, token_t **rest);
static node_t *parse_cast(token_t *tok, token_t **rest);
static node_t *parse_relational(token_t *tok, token_t **rest);
static node_t *parse_equality(token_t *tok, token_t **rest);
static node_t *parse_add(token_t *tok, token_t **rest);
static node_t *parse_shift(token_t *tok, token_t **rest);
static node_t *parse_and(token_t *tok, token_t **rest);
static node_t *parse_eor(token_t *tok, token_t **rest);
static node_t *parse_or(token_t *tok, token_t **rest);
static node_t *parse_logand(token_t *tok, token_t **rest);
static node_t *parse_logor(token_t *tok, token_t **rest);
static node_t *parse_cond_expr(token_t *tok, token_t **rest);
static node_t *parse_expr_stmt(token_t *tok, token_t **rest);
static node_t *parse_stmt(token_t *tok, token_t **rest);
static node_t *parse_stmt_expr(token_t *tok, token_t **rest);
static node_t *parse_assign(token_t *tok, token_t **rest);
static node_t *parse_compound_stmt(token_t *tok, token_t **rest);
static type_t *parse_declspec(token_t *tok, token_t **rest);
static node_t *parse_initializer(token_t *tok, token_t **rest);
static type_t *parse_direct_declarator(type_t *root, token_t *tok,
									   token_t **rest);
static type_t *parse_abstract_declarator(type_t *root, token_t *tok,
										 token_t **rest);
static type_t *parse_direct_abstract_declarator(type_t *root, token_t *tok,
												token_t **rest);
static type_t *parse_type_name(token_t *tok, token_t **rest);
static type_t *parse_declarator(type_t *root, token_t *tok, token_t **rest);
static node_t *parse_init_declarator(type_t *root, token_t *tok,
									 token_t **rest);
static node_t *parse_declaration(token_t *tok, token_t **rest);
static type_t *parse_parameter_declaration(token_t *tok, token_t **rest);

static bool is_declspec(token_t *tok)
{
	if(token_eq(tok, "void") || token_eq(tok, "_Bool") ||
	   token_eq(tok, "char") || token_eq(tok, "short") ||
	   token_eq(tok, "long") || token_eq(tok, "int") ||
	   token_eq(tok, "signed") || token_eq(tok, "unsigned") ||
	   token_eq(tok, "_Alignas") || token_eq(tok, "struct") ||
	   token_eq(tok, "union") || token_eq(tok, "typedef") || find_type(tok)) {
		return true;
	}
	return false;
}

static void compute_struct_member_pos(type_t *struc)
{
	size_t align = 0;
	size_t pos = 0;
	for(size_t i = 0; i < list_len(struc->membs); i++) {
		member_t *mem = struc->membs[i];
		pos = align_to(pos, mem->type->align);
		mem->loc = pos;
		pos += mem->type->size;
		if(mem->type->align > align) {
			align = mem->type->align;
		}
	}

	struc->size = align_to(pos, align);
	struc->align = align;
	return;
}

static void compute_union_member_pos(type_t *struc)
{
	/* TODO: possibly deduplicate this? I don't know if in
	 * the future this will change further, so I'm going to keep
	 * it for a while. */
	size_t align = 0;
	size_t pos = 0;
	for(size_t i = 0; i < list_len(struc->membs); i++) {
		member_t *mem = struc->membs[i];
		pos = align_to(pos, mem->type->align);
		mem->loc = 0;
		pos += mem->type->size;
		if(mem->type->align > align) {
			align = mem->type->align;
		}
	}

	struc->size = align_to(pos, align);
	struc->align = align;
	return;
}

static type_t *parse_struct_or_union(token_t *tok, token_t **rest)
{
	token_t *marker = tok;
	tok = tok->next;
	/* we have a name if no { */
	char *tag = NULL;
	type_t *tag_ty = NULL;
	type_t *struc = type_clone(TY_VOID);
	if(!token_eq(tok, "{")) {
		if(tok->kind != TOK_IDENT) {
			compile_err(tok->loc, "expected an identifier");
		}

		/* if name already exists, cant do that */
		tag_ty = find_tag(tok);
		struc->ident = tok;

		tag = tok->content;
		tok = tok->next;
	} else {
		/* TODO: hack */
		char *nam = anon_name(0);
		token_t *ident_tok = scr_alloc(sizeof(token_t));
		ident_tok->kind = TOK_IDENT;
		ident_tok->loc = nam;
		ident_tok->len = strnlen(nam, STR_SIZE);
		ident_tok->content = nam;
		struc->ident = ident_tok;
	}

	/* if no { members }, then it is a use of a struct */
	if(!token_eq(tok, "{")) {
		if(!tag_ty) {
			compile_err(tok->loc, "empty %s", marker->content);
		}
		*rest = tok;
		return tag_ty;
	}

	tok = token_skip(tok, "{");

	if(tag_ty) {
		compile_err(tok->loc, "redefinition of '%s'", tag);
	}

	struc->kind = token_eq(marker, "struct") ? TYPE_STRUCT : TYPE_UNION;
	struc->membs = list_make(member_t *);

	while(!token_eq(tok, "}")) {
		type_t *declspec = parse_declspec(tok, &tok);
		type_t *complete = parse_declarator(declspec, tok, &tok);
		tok = token_skip(tok, ";");
		member_t *m = member_make(complete, complete->ident, 0);
		list_append(struc->membs, m);
	}
	tok = token_skip(tok, "}");

	/* assign offsets, find max align */
	if(struc->kind == TYPE_STRUCT) {
		compute_struct_member_pos(struc);
	} else {
		compute_union_member_pos(struc);
	}

	*rest = tok;

	/* if this struct is named, push it into the scope */
	if(tag) {
		scope_add_tag(struc);
	}

	return struc;
}

static type_t *parse_typedef(token_t *tok, token_t **rest)
{
	tok = tok->next; /* skip `typedef` */
	type_t *ty = parse_type_name(tok, &tok);
	char *name = tok->content;
	scope_add_type(name, ty);
	tok = tok->next;
	*rest = tok;
	return ty;
}

static type_t *parse_declspec(token_t *tok, token_t **rest)
{
	uint64_t align = 0;
	type_t *res = NULL;
	bool unsign = false;

	if(token_eq(tok, "struct") || token_eq(tok, "union")) {
		return parse_struct_or_union(tok, rest);
	}

	if(token_eq(tok, "typedef")) {
		return parse_typedef(tok, rest);
	}

	while(is_declspec(tok)) {
		if(token_eq(tok, "long")) {
			tok = token_skip(tok, "long");
			*rest = tok;
			res = type_clone(TY_LONG);
			break;
		}

		if(token_eq(tok, "int")) {
			tok = token_skip(tok, "int");
			*rest = tok;
			res = type_clone(TY_INT);
			break;
		}

		if(token_eq(tok, "short")) {
			tok = token_skip(tok, "short");
			*rest = tok;
			res = type_clone(TY_SHORT);
			break;
		}

		if(token_eq(tok, "char")) {
			tok = token_skip(tok, "char");
			*rest = tok;
			res = type_clone(TY_CHAR);
			break;
		}

		if(token_eq(tok, "void")) {
			tok = token_skip(tok, "void");
			*rest = tok;
			res = type_clone(TY_VOID);
			break;
		}

		if(token_eq(tok, "_Bool")) {
			tok = token_skip(tok, "_Bool");
			*rest = tok;
			res = type_clone(TY_BOOL);
			break;
		}

		if(token_eq(tok, "signed")) {
			tok = token_skip(tok, "signed");
			unsign = false;
			continue;
		}

		if(token_eq(tok, "unsigned")) {
			tok = token_skip(tok, "unsigned");
			unsign = true;
			continue;
		}

		if(token_eq(tok, "_Alignas")) {
			tok = token_skip(tok, "_Alignas");
			tok = token_skip(tok, "(");
			token_t *num = tok;
			if(tok->kind != TOK_NUM) {
				compile_err(tok->loc, "expected a number");
			}
			align = tok->num;

			tok = token_skip(tok->next, ")");
			*rest = tok;
			if(align <= 0) {
				compile_err(num->loc,
							"alignment has to be non-zero, non-negative");
			}
			continue;
		}

		type_t *found = find_type(tok);
		if(found) {
			tok = tok->next;
			res = found;
			break;
		}

		compile_err(tok->loc, "invalid declaration specifier type '%.*s'",
					tok->len, tok->loc);
	}

	if(!res) {
		compile_err(tok->loc, "incomplete declaration specifier");
	}

	if(align) {
		res->align = align;
	}

	res->unsignd = unsign;

	*rest = tok;
	return res;
}

static node_t *parse_initializer(token_t *tok, token_t **rest)
{
	return parse_assign(tok, rest);
}

static type_t *parse_direct_declarator(type_t *root, token_t *tok,
									   token_t **rest)
{
	type_t *type = root;
	if(tok->kind != TOK_IDENT) {
		compile_err(tok->loc, "expected an identifier");
	}

	type->ident = tok;
	tok = tok->next;

	while(token_eq(tok, "[")) {
		tok = token_skip(tok, "[");
		if(tok->kind != TOK_NUM) {
			compile_err(tok->loc, "expected a constant number");
		}
		size_t size = tok->num;
		tok = token_skip(tok->next, "]");
		type = type_arr_to(type, size);
	}

	*rest = tok;
	return type;
}

static type_t *parse_abstract_declarator(type_t *root, token_t *tok,
										 token_t **rest)
{
	type_t *ty = root;
parse:
	if(token_eq(tok, "*")) {
		tok = token_skip(tok, "*");
		ty = type_ptr_to(ty);
		goto parse;
	}

	if(token_eq(tok, "(") || token_eq(tok, "[")) {
		ty = parse_direct_abstract_declarator(ty, tok, &tok);
		goto parse;
	}

	*rest = tok;
	return ty;
}

static type_t *parse_direct_abstract_declarator(type_t *root, token_t *tok,
												token_t **rest)
{
	type_t *ty = root;
parse:
	if(token_eq(tok, "(")) {
		tok = token_skip(tok, "(");
		ty = parse_abstract_declarator(ty, tok, &tok);
		tok = token_skip(tok, ")");
		goto parse;
	}

	if(token_eq(tok, "[")) {
		tok = token_skip(tok, "[");
		if(tok->kind != TOK_NUM) {
			compile_err(tok->loc, "invalid array length");
		}
		ty = type_arr_to(ty, tok->num);
		tok = token_skip(tok->next, "]");
		goto parse;
	}

	*rest = tok;
	return ty;
}

static type_t *parse_type_name(token_t *tok, token_t **rest)
{
	type_t *declspec = parse_declspec(tok, &tok);
	type_t *complete = parse_abstract_declarator(declspec, tok, &tok);
	*rest = tok;
	return complete;
}

static type_t *parse_declarator(type_t *root, token_t *tok, token_t **rest)
{
	type_t *decltype = root;
	while(token_eq(tok, "*")) {
		tok = token_skip(tok, "*");
		decltype = type_ptr_to(decltype);
	}

	decltype = parse_direct_declarator(decltype, tok, &tok);

	*rest = tok;
	return decltype;
}

static node_t *parse_init_declarator(type_t *root, token_t *tok, token_t **rest)
{
	type_t *decltype = parse_declarator(root, tok, &tok);

	obj_t *lval;
	obj_t *found;

	if((found = find_var(decltype->ident))) {
		lval = found;
	} else {
		lval = obj_make(mystrndup(decltype->ident->loc, decltype->ident->len),
						decltype, false);
	}

	/* if not assigned to it's fine, just create the object (declaration) and return */
	if(!token_eq(tok, "=")) {
		*rest = tok;
		UNUSED(lval);
		return NULL;
	}

	/* else assign something to it */
	token_t *eqsign = tok;
	tok = token_skip(tok, "=");
	node_t *initializer = parse_initializer(tok, &tok);
	node_t *assignment = node_bin(NODE_ASSIGN, node_var(lval, decltype->ident),
								  initializer, eqsign);
	assignment->var = lval;

	*rest = tok;
	return assignment;
}

static node_t *parse_declaration(token_t *tok, token_t **rest)
{
	type_t *decltype_base = parse_declspec(tok, &tok);

	node_t head = { 0 };
	node_t *cur = &head;

	if(token_eat(&tok, ";")) {
		*rest = tok;
		goto end;
	}

	node_t *init_decl_list = parse_init_declarator(decltype_base, tok, &tok);

	if(init_decl_list) {
		cur->next = node_unary(NODE_EXPR_STMT, init_decl_list, tok);
		cur = cur->next;
	}

	/* process more declarations */
	while(token_eq(tok, ",")) {
		tok = token_skip(tok, ",");
		init_decl_list = parse_init_declarator(decltype_base, tok, &tok);

		if(init_decl_list) {
			cur->next = node_unary(NODE_EXPR_STMT, init_decl_list, tok);
			cur = cur->next;
		}
	}

	if(!token_eq(tok, ";")) {
		compile_err(tok->loc, "expected a semicolon");
	}

	*rest = tok->next;
end:;
	node_t *blk = node_make(NODE_BLOCK, tok);
	blk->body = head.next;

	return blk;
}

/* overloaded `+` operator w/ pointers */
static node_t *node_add(node_t *lhs, node_t *rhs, token_t *tok)
{
	/* type both args */
	type_propagate(lhs);
	type_propagate(rhs);

	/* default */
	if(type_is_int(lhs->type) && type_is_int(rhs->type)) {
		return node_bin(NODE_ADD, lhs, rhs, tok);
	}

	/* if int + ptr, swap */
	if(type_is_int(lhs->type)) {
		node_t *tmp = lhs;
		lhs = rhs;
		rhs = tmp;
	}

	/* ptr + ptr is invalid */
	if(type_is_ptr(lhs->type) && type_is_ptr(rhs->type)) {
		compile_err(tok->loc, "cannot add two pointers");
		return NULL;
	}

	/* must be ptr + int now */
	/* pointer arithmetic is fun so the int is multiplied by pointer base size */
	node_t *mul =
		lhs->type->to->size > 1 ?
			node_bin(NODE_MUL, node_num(lhs->type->to->size, NULL), rhs, tok) :
			rhs;
	return node_bin(NODE_ADD, lhs, mul, tok);
}

/* overloaded `-` operator w/ pointers
 * Ctrl+C Ctrl+V `node_add` */
static node_t *node_sub(node_t *lhs, node_t *rhs, token_t *tok)
{
	/* type both args */
	type_propagate(lhs);
	type_propagate(rhs);

	/* default */
	if(type_is_int(lhs->type) && type_is_int(rhs->type)) {
		return node_bin(NODE_SUB, lhs, rhs, tok);
	}

	/* int - ptr is invalid */
	if(type_is_int(lhs->type) && type_is_ptr(rhs->type)) {
		compile_err(tok->loc, "cannot subtract pointer from number");
	}

	/* ptr - ptr */
	/* = (ptr-ptr)/(sizeof(ptr[0])) */
	if(type_is_ptr(lhs->type) && type_is_ptr(rhs->type)) {
		node_t *sub = node_bin(NODE_SUB, lhs, rhs, tok);
		node_t *div = lhs->type->to->size > 1 ?
						  node_bin(NODE_DIV, sub,
								   node_num(lhs->type->to->size, tok), tok) :
						  sub;
		return div;
	}

	/* must be ptr - int now */
	/* pointer arithmetic is fun so the int is multiplied by pointer base size */
	node_t *mul =
		lhs->type->to->size > 1 ?
			node_bin(NODE_MUL, node_num(lhs->type->to->size, NULL), rhs, tok) :
			rhs;
	return node_bin(NODE_SUB, lhs, mul, tok);
}

static node_t *parse_compound_stmt(token_t *tok, token_t **rest)
{
	node_t node = { 0 };
	node_t *cur = &node;
	node_t *blk = node_make(NODE_BLOCK, tok);
	scope_push();
	tok = token_skip(tok, "{");
	while(!token_eq(tok, "}")) {
		/* TODO: this is a duct tape solution */
		if(is_declspec(tok)) {
			node_t *decl = parse_declaration(tok, &tok);
			cur->next = decl;
		} else {
			node_t *stmt = parse_stmt(tok, &tok);
			cur->next = stmt;
		}
		cur = cur->next;
		type_propagate(cur);
	}
	tok = token_skip(tok, "}");

	blk->body = node.next;
	*rest = tok;
	scope_pop();
	return blk;
}

static node_t *parse_assign(token_t *tok, token_t **rest)
{
	node_t *node = parse_cond_expr(tok, &tok);
	node_t *op = NULL;

	/* rewrite
	 * A op= B
	 * to
	 * (A = A op B)
	 */

	token_t *save = tok->next;

	if(token_eq(tok, "=")) {
		node = node_bin(NODE_ASSIGN, node, parse_assign(tok->next, &tok),
						tok->next);
		*rest = tok;
		return node;
	}

	if(token_eq(tok, "*=")) {
		op = node_bin(NODE_MUL, node, parse_assign(tok->next, &tok), tok->next);
		goto end;
	}

	if(token_eq(tok, "/=")) {
		op = node_bin(NODE_DIV, node, parse_assign(tok->next, &tok), tok->next);
		goto end;
	}

	if(token_eq(tok, "%=")) {
		op = node_bin(NODE_MOD, node, parse_assign(tok->next, &tok), tok->next);
		goto end;
	}

	if(token_eq(tok, "+=")) {
		op = node_add(node, parse_assign(tok->next, &tok), tok->next);
		goto end;
	}

	if(token_eq(tok, "-=")) {
		op = node_sub(node, parse_assign(tok->next, &tok), tok->next);
		goto end;
	}

	if(token_eq(tok, "<<=")) {
		op = node_bin(NODE_SHL, node, parse_assign(tok->next, &tok), tok->next);
		goto end;
	}

	if(token_eq(tok, ">>=")) {
		op = node_bin(NODE_SHR, node, parse_assign(tok->next, &tok), tok->next);
		goto end;
	}

	if(token_eq(tok, "&=")) {
		op = node_bin(NODE_AND, node, parse_assign(tok->next, &tok), tok->next);
		goto end;
	}

	if(token_eq(tok, "^=")) {
		op = node_bin(NODE_EOR, node, parse_assign(tok->next, &tok), tok->next);
		goto end;
	}

	if(token_eq(tok, "|=")) {
		op = node_bin(NODE_OR, node, parse_assign(tok->next, &tok), tok->next);
		goto end;
	}

end:
	if(op) {
		node = node_bin(NODE_ASSIGN, node, op, save);
	}

	*rest = tok;
	return node;
}

static node_t *parse_stmt(token_t *tok, token_t **rest)
{
	token_t *save = tok;
	/* return */
	if(tok->kind == TOK_KEYWORD && token_eq(tok, "return")) {
		node_t *stmt = node_unary(NODE_RET, NULL, tok);
		if(token_eq(tok->next, ";")) {
			*rest = token_skip(tok->next, ";");
			return stmt;
		}
		stmt->lhs = parse_expr(tok->next, &tok);
		*rest = token_skip(tok, ";");
		return stmt;
	}

	/* if */
	if(tok->kind == TOK_KEYWORD && token_eq(tok, "if")) {
		node_t *iffnod = node_make(NODE_IF, tok);
		tok = token_skip(tok->next, "(");
		node_t *iff = parse_expr(tok, &tok);
		tok = token_skip(tok, ")");
		node_t *then = parse_stmt(tok, &tok);
		node_t *elze = NULL;
		/* else */
		if(tok->kind == TOK_KEYWORD && token_eq(tok, "else")) {
			tok = token_skip(tok, "else");
			elze = parse_stmt(tok, &tok);
		}
		*rest = tok;
		iffnod->cond = iff;
		iffnod->then = then;
		iffnod->elze = elze;
		return iffnod;
	}

	/* for */
	if(tok->kind == TOK_KEYWORD && token_eq(tok, "for")) {
		node_t *fornod = node_make(NODE_FOR, tok);
		tok = token_skip(tok->next, "(");
		node_t *init;
		if(is_declspec(tok)) {
			init = parse_declaration(tok, &tok);
		} else {
			init = parse_expr_stmt(tok, &tok);
		}
		node_t *cond = NULL;
		node_t *inc = NULL;

		if(!token_eq(tok, ";")) {
			cond = parse_expr(tok, &tok);
		}

		tok = token_skip(tok, ";");

		if(!token_eq(tok, ")")) {
			inc = parse_expr(tok, &tok);
		}

		tok = token_skip(tok, ")");

		node_t *then = parse_stmt(tok, &tok);
		*rest = tok;
		fornod->init = init;
		fornod->cond = cond;
		fornod->inc = inc;
		fornod->then = then;
		return fornod;
	}

	/* while */
	if(tok->kind == TOK_KEYWORD && token_eq(tok, "while")) {
		node_t *whilenod = node_make(NODE_WHILE, tok);
		tok = token_skip(tok->next, "(");
		node_t *cond = parse_expr(tok, &tok);
		tok = token_skip(tok, ")");
		node_t *body = parse_stmt(tok, &tok);
		*rest = tok;
		whilenod->cond = cond;
		whilenod->then = body;
		return whilenod;
	}

	/* do while */
	if(tok->kind == TOK_KEYWORD && token_eq(tok, "do")) {
		tok = token_skip(tok, "do");
		node_t *dowhile = node_make(NODE_DOWHILE, tok);
		dowhile->then = parse_stmt(tok, &tok);
		if(!token_eq(tok, "while")) {
			compile_err_node(dowhile, "there is nothing we can `do`");
		}
		tok = token_skip(tok, "while");
		tok = token_skip(tok, "(");
		dowhile->cond = parse_expr(tok, &tok);
		tok = token_skip(tok, ")");
		tok = token_skip(tok, ";");
		*rest = tok;
		return dowhile;
	}

	/* break */
	if(token_eat(&tok, "break")) {
		node_t *break_node = node_make(NODE_BREAK, save);
		tok = token_skip(tok, ";");
		*rest = tok;
		return break_node;
	}

	/* continue */
	if(token_eat(&tok, "continue")) {
		node_t *cont_node = node_make(NODE_CONTINUE, save);
		tok = token_skip(tok, ";");
		*rest = tok;
		return cont_node;
	}

	/* switch */
	if(token_eat(&tok, "switch")) {
		node_t *switch_node = node_make(NODE_SWITCH, save);
		tok = token_skip(tok, "(");
		switch_node->cond = parse_expr(tok, &tok);
		tok = token_skip(tok, ")");
		switch_node->then = parse_stmt(tok, &tok);
		if(switch_node->then->kind != NODE_BLOCK) {
			node_t *blk = node_make(NODE_BLOCK, switch_node->then->tok);
			blk->body = switch_node->then;
			switch_node->then = blk;
		}
		*rest = tok;
		return switch_node;
	}

	/* case num: */
	if(token_eat(&tok, "case")) {
		node_t *case_node = node_make(NODE_CASE, save);
		case_node->cond = node_num_tok(tok);
		tok = tok->next;
		tok = token_skip(tok, ":");
		case_node->then = parse_stmt(tok, &tok);
		*rest = tok;
		return case_node;
	}

	/* default: */
	if(token_eat(&tok, "default")) {
		node_t *def_node = node_make(NODE_DEFAULT, save);
		tok = token_skip(tok, ":");
		def_node->then = parse_stmt(tok, &tok);
		*rest = tok;
		return def_node;
	}

	/* goto */
	if(token_eat(&tok, "goto")) {
		node_t *goto_node = node_make(NODE_GOTO, save);
		if(tok->kind != TOK_IDENT) {
			compile_err(tok->loc, "expected an identifier");
		}
		goto_node->label = mystrndup(tok->loc, tok->len);
		tok = token_skip(tok->next, ";");
		*rest = tok;
		return goto_node;
	}

	/* label: */
	if(tok->kind == TOK_IDENT && tok->next && token_eq(tok->next, ":")) {
		node_t *lbl = node_make(NODE_LABEL, tok);
		lbl->label = mystrndup(tok->loc, tok->len);
		tok = tok->next->next;
		lbl->then = parse_stmt(tok, &tok);
		*rest = tok;
		return lbl;
	}

	/* compound-stmt */
	if(token_eq(tok, "{")) {
		node_t *compound_stmt = parse_compound_stmt(tok, &tok);
		*rest = tok;
		return compound_stmt;
	}

	return parse_expr_stmt(tok, rest);
}

static node_t *parse_expr_stmt(token_t *tok, token_t **rest)
{
	if(token_eq(tok, ";")) {
		*rest = tok->next;
		return node_make(NODE_BLOCK, tok);
	}
	node_t *expr = parse_expr(tok, &tok);
	*rest = token_skip(tok, ";");
	return node_unary(NODE_EXPR_STMT, expr, tok);
}

static node_t *parse_add(token_t *tok, token_t **rest)
{
	node_t *node = parse_mul(tok, &tok);
parse:
	if(token_eq(tok, "+")) {
		node = node_add(node, parse_mul(tok->next, &tok), tok);
		goto parse;
	}

	if(token_eq(tok, "-")) {
		node = node_sub(node, parse_mul(tok->next, &tok), tok);
		goto parse;
	}

	*rest = tok;
	return node;
}

static node_t *parse_shift(token_t *tok, token_t **rest)
{
	node_t *node = parse_add(tok, &tok);
parse:
	if(token_eq(tok, "<<")) {
		node = node_bin(NODE_SHL, node, parse_add(tok->next, &tok), tok);
		goto parse;
	}

	if(token_eq(tok, ">>")) {
		node = node_bin(NODE_SHR, node, parse_add(tok->next, &tok), tok);
		goto parse;
	}

	*rest = tok;
	return node;
}

static node_t *parse_and(token_t *tok, token_t **rest)
{
	node_t *node = parse_equality(tok, &tok);
parse:
	if(token_eq(tok, "&")) {
		node = node_bin(NODE_AND, parse_equality(tok->next, &tok), node, tok);
		goto parse;
	}

	*rest = tok;
	return node;
}

static node_t *parse_eor(token_t *tok, token_t **rest)
{
	node_t *node = parse_and(tok, &tok);
parse:
	if(token_eq(tok, "^")) {
		node = node_bin(NODE_EOR, parse_and(tok->next, &tok), node, tok);
		goto parse;
	}

	*rest = tok;
	return node;
}

static node_t *parse_or(token_t *tok, token_t **rest)
{
	node_t *node = parse_eor(tok, &tok);
parse:
	if(token_eq(tok, "|")) {
		node = node_bin(NODE_OR, parse_eor(tok->next, &tok), node, tok);
		goto parse;
	}

	*rest = tok;
	return node;
}

static node_t *parse_logand(token_t *tok, token_t **rest)
{
	node_t *node = parse_or(tok, &tok);
parse:
	if(token_eq(tok, "&&")) {
		node = node_bin(NODE_LOGAND, node, parse_or(tok->next, &tok), tok);
		goto parse;
	}

	*rest = tok;
	return node;
}

static node_t *parse_logor(token_t *tok, token_t **rest)
{
	node_t *node = parse_logand(tok, &tok);
parse:
	if(token_eq(tok, "||")) {
		node = node_bin(NODE_LOGOR, node, parse_logand(tok->next, &tok), tok);
		goto parse;
	}

	*rest = tok;
	return node;
}

/* cond ? x : y   -> ({ T res; if(cond) { res = x; } else { res = y } res;  }) */
static node_t *make_cond_expr(node_t *cond, node_t *x, node_t *y, token_t *tok)
{
	type_propagate(x);
	type_propagate(y);
	node_t *res = node_var(obj_make_anon(x->type), tok);
	type_propagate(res);

	node_t *res_stmt = node_unary(NODE_EXPR_STMT, res, tok);
	node_t *assignx =
		node_unary(NODE_EXPR_STMT, node_bin(NODE_ASSIGN, res, x, tok), tok);
	node_t *assigny =
		node_unary(NODE_EXPR_STMT, node_bin(NODE_ASSIGN, res, y, tok), tok);

	node_t *iff = node_make(NODE_IF, tok);
	iff->cond = cond;
	iff->then = assignx;
	iff->elze = assigny;

	node_t *stmt1 = node_unary(NODE_EXPR_STMT, res, tok);
	stmt1->next = iff;
	iff->next = res_stmt;

	node_t *stmt_expr = node_make(NODE_STMT_EXPR, tok);
	node_t *blk = node_make(NODE_BLOCK, tok);
	blk->next = stmt1;
	stmt_expr->body = blk;
	return stmt_expr;
}

/* cond ? x : y */
static node_t *parse_cond_expr(token_t *tok, token_t **rest)
{
	token_t *save = tok;
	node_t *logor = parse_logor(tok, &tok);
	if(token_eat(&tok, "?")) {
		node_t *x = parse_expr(tok, &tok);
		tok = token_skip(tok, ":");
		node_t *y = parse_cond_expr(tok, &tok);
		logor = make_cond_expr(logor, x, y, save);
	}
	*rest = tok;
	return logor;
}

static node_t *parse_relational(token_t *tok, token_t **rest)
{
	node_t *node = parse_shift(tok, &tok);
parse:
	if(token_eq(tok, "<")) {
		node = node_bin(NODE_LT, node, parse_shift(tok->next, &tok), tok);
		goto parse;
	}

	if(token_eq(tok, "<=")) {
		node = node_bin(NODE_LE, node, parse_shift(tok->next, &tok), tok);
		goto parse;
	}

	if(token_eq(tok, ">")) {
		node = node_bin(NODE_GT, node, parse_shift(tok->next, &tok), tok);
		goto parse;
	}

	if(token_eq(tok, ">=")) {
		node = node_bin(NODE_GE, node, parse_shift(tok->next, &tok), tok);
		goto parse;
	}

	*rest = tok;
	return node;
}

static node_t *parse_equality(token_t *tok, token_t **rest)
{
	node_t *node = parse_relational(tok, &tok);
parse:
	if(token_eq(tok, "==")) {
		node = node_bin(NODE_EQ, node, parse_relational(tok->next, &tok), tok);
		goto parse;
	}

	if(token_eq(tok, "!=")) {
		node = node_bin(NODE_NE, node, parse_relational(tok->next, &tok), tok);
		goto parse;
	}
	*rest = tok;
	return node;
}

static node_t *parse_mul(token_t *tok, token_t **rest)
{
	node_t *node = parse_cast(tok, rest);
	tok = *rest;

parse:
	if(token_eq(tok, "*")) {
		node = node_bin(NODE_MUL, node, parse_cast(tok->next, &tok), tok);
		goto parse;
	}

	if(token_eq(tok, "/")) {
		node = node_bin(NODE_DIV, node, parse_cast(tok->next, &tok), tok);
		goto parse;
	}

	if(token_eq(tok, "%")) {
		node = node_bin(NODE_MOD, node, parse_cast(tok->next, &tok), tok);
		goto parse;
	}

	*rest = tok;
	return node;
}

static node_t *parse_expr(token_t *tok, token_t **rest)
{
	node_t *node = parse_assign(tok, &tok);
parse:
	if(token_eq(tok, ",")) {
		node = node_bin(NODE_COMMA, node, parse_assign(tok->next, &tok), tok);
		goto parse;
	}
	*rest = tok;
	return node;
}

static node_t *parse_stmt_expr(token_t *tok, token_t **rest)
{
	node_t *stmt_expr = node_make(NODE_STMT_EXPR, tok);

	tok = token_skip(tok, "(");
	node_t *compound_stmt = parse_compound_stmt(tok, &tok);
	stmt_expr->body = compound_stmt->body;
	node_delete(compound_stmt);
	tok = token_skip(tok, ")");

	*rest = tok;
	return stmt_expr;
}

static node_t *parse_prim(token_t *tok, token_t **rest)
{
	/* statement expr */
	if(tok->next && token_eq(tok, "(") && token_eq(tok->next, "{")) {
		return parse_stmt_expr(tok, rest);
	}

	/* ( expr ) case */
	if(token_eq(tok, "(")) {
		node_t *node = parse_expr(tok->next, &tok);
		*rest = token_skip(tok, ")");
		return node;
	}

	/* ident args? case */
	if(tok->kind == TOK_IDENT) {
		/* function call */
		if(token_eq(tok->next, "(")) {
			node_t *fun = node_make(NODE_FUNCALL, tok);
			fun->fname = mystrndup(tok->loc, tok->len);
			fun->fargs = NULL;

			if(!strmap_has(known_funcs, fun->fname)) {
				compile_err(tok->loc, "unknown function '%s'", fun->fname);
			}

			obj_t *fn = *strmap_get(known_funcs, fun->fname);

			tok = tok->next;
			tok = token_skip(tok, "(");

			node_t head = {};
			node_t *cur = &head;
			obj_t *farg = fn->args;

			while(!token_eq(tok, ")")) {
				if(cur != &head)
					tok = token_skip(tok, ",");
				node_t *arg = parse_assign(tok, &tok);
				fastcast(&arg, farg->type);
				cur->next = arg;
				cur = cur->next;
				farg = farg->next;
			}

			fun->fargs = head.next;

			tok = token_skip(tok, ")");
			*rest = tok;
			return fun;
		}

		/* variable */
		obj_t *obj = find_var(tok);
		if(!obj) {
			compile_err(tok->loc, "unknown variable '%.*s'", tok->len,
						tok->loc);
		}
		node_t *node = node_var(obj, tok);
		*rest = tok->next;
		return node;
	}

	/* num case */
	if(tok->kind == TOK_NUM) {
		node_t *node = node_num_tok(tok);
		*rest = tok->next;
		return node;
	}

	/* string case */
	if(tok->kind == TOK_STR) {
		node_t *node = node_var(obj_make_str(tok), tok);
		*rest = tok->next;
		return node;
	}

	compile_err(tok->loc, "expected an expression");

	return NULL;
}

/* remember that x[y] is eq to *(x + y) */
/* a[1][2] == *(a + 1)[2] == *(*(a + 1) + 2) */
static node_t *parse_postfix(token_t *tok, token_t **rest)
{
	node_t *prim = parse_prim(tok, &tok);

parse:;
	token_t *marker = tok;
	if(token_eq(tok, "[")) {
		tok = token_skip(tok, "[");
		token_t *marker_add = tok;
		node_t *indx = parse_expr(tok, &tok);
		tok = token_skip(tok, "]");

		prim = node_unary(NODE_DEREF, node_add(prim, indx, marker_add), marker);
		goto parse;
	}

	if(token_eq(tok, ".")) {
		tok = token_skip(tok, ".");
memb_parse:;
		node_t *v = prim;
		type_propagate(v);
		prim = node_unary(NODE_MEMBER, prim, marker);
		if(tok->kind != TOK_IDENT) {
			compile_err(tok->loc, "expected an identifer");
		}

		if(v->type->kind != TYPE_STRUCT && v->type->kind != TYPE_UNION) {
			compile_err(tok->loc, "expected a struct or union");
		}

		/* find the member */
		/* TODO: use stringmaps for this, but there is a challenge in that
		 * stringmaps are currently unordered. so I guess we will use
		 * the lists in the meanwhile while i find up a solution for this */
		size_t i = 0;
		bool found = false;
		for(; i < list_len(v->type->membs); i++) {
			if(strncmp(v->type->membs[i]->ident->loc, tok->loc, tok->len) ==
			   0) {
				found = true;
				break;
			}
		}

		if(!found) {
			compile_err(tok->loc, "'%.*s' is not a member of struct or union",
						tok->len, tok->loc);
		}

		prim->memb = v->type->membs[i];
		tok = tok->next;
		goto parse;
	}

	if(token_eq(tok, "->")) {
		tok = token_skip(tok, "->");
		prim = node_unary(NODE_DEREF, prim, marker);
		goto memb_parse;
	}

	/* i++  = ({ T i2 = i; i = i + 1; i2; })
	 * i--  = ({ T i2 = i; i = i - 1; i2; })
	 */

	if(token_eq(tok, "++")) {
		node_t *stmt_expr = node_make(NODE_STMT_EXPR, tok);

		type_propagate(prim);
		obj_t *i2 = obj_make_anon(prim->type);
		node_t *i2v = node_var(i2, tok);
		node_t *stmt1 = node_unary(NODE_EXPR_STMT,
								   node_bin(NODE_ASSIGN, i2v, prim, tok), tok);
		node_t *stmt2 =
			node_unary(NODE_EXPR_STMT,
					   node_bin(NODE_ASSIGN, prim,
								node_add(prim, node_num(1, tok), tok), tok),
					   tok);
		node_t *stmt3 = node_unary(NODE_EXPR_STMT, i2v, tok);
		stmt1->next = stmt2;
		stmt2->next = stmt3;

		node_t *blk = node_make(NODE_BLOCK, tok);
		blk->next = stmt1;

		stmt_expr->body = blk;
		tok = token_skip(tok, "++");
		prim = stmt_expr;
		goto parse;
	}

	if(token_eq(tok, "--")) {
		node_t *stmt_expr = node_make(NODE_STMT_EXPR, tok);

		type_propagate(prim);
		obj_t *i2 = obj_make_anon(prim->type);
		node_t *i2v = node_var(i2, tok);
		node_t *stmt1 = node_unary(NODE_EXPR_STMT,
								   node_bin(NODE_ASSIGN, i2v, prim, tok), tok);
		node_t *stmt2 =
			node_unary(NODE_EXPR_STMT,
					   node_bin(NODE_ASSIGN, prim,
								node_sub(prim, node_num(1, tok), tok), tok),
					   tok);
		node_t *stmt3 = node_unary(NODE_EXPR_STMT, i2v, tok);
		stmt1->next = stmt2;
		stmt2->next = stmt3;

		node_t *blk = node_make(NODE_BLOCK, tok);
		blk->next = stmt1;

		stmt_expr->body = blk;
		tok = token_skip(tok, "--");
		prim = stmt_expr;
		goto parse;
	}

	*rest = tok;

	return prim;
}

static node_t *parse_unary(token_t *tok, token_t **rest)
{
	if(token_eq(tok, "+")) {
		node_t *node = parse_unary(tok->next, rest);
		return node;
	}

	if(token_eq(tok, "-")) {
		return node_unary(NODE_NEG, parse_unary(tok->next, rest), tok);
	}

	if(token_eq(tok, "&")) {
		return node_unary(NODE_ADDR, parse_unary(tok->next, rest), tok);
	}

	if(token_eq(tok, "*")) {
		return node_unary(NODE_DEREF, parse_unary(tok->next, rest), tok);
	}

	if(token_eq(tok, "~")) {
		return node_unary(NODE_NOT, parse_unary(tok->next, rest), tok);
	}

	if(token_eq(tok, "!")) {
		return node_unary(NODE_LOGNEG, parse_unary(tok->next, rest), tok);
	}

	/* sizeof, _Alignof */

	if(token_eq(tok, "sizeof")) {
		tok = token_skip(tok, "sizeof");
		type_t *ty;
		if(token_eq(tok, "(") && tok->next && is_declspec(tok->next)) {
			ty = parse_type_name(tok->next, &tok);
			tok = token_skip(tok, ")");
		} else {
			node_t *expr = parse_unary(tok, &tok);
			ty = expr->type;
		}
		*rest = tok;
		return node_num(ty->size, tok);
	}

	if(token_eq(tok, "_Alignof")) {
		tok = token_skip(tok, "_Alignof");
		type_t *ty;
		if(token_eq(tok, "(") && tok->next && is_declspec(tok->next)) {
			ty = parse_type_name(tok->next, &tok);
			tok = token_skip(tok, ")");
		} else {
			node_t *expr = parse_unary(tok, &tok);
			ty = expr->type;
		}
		*rest = tok;
		return node_num(ty->align, tok);
	}

	/* pre increment/decrement */
	/* ++i  = (i += 1) = (i = i + 1)
	 * --i  = (i -= 1) = (i = i - 1) */
	if(token_eq(tok, "++")) {
		token_t *save = tok;
		tok = token_skip(tok, "++");
		node_t *unary = parse_unary(tok, &tok);
		node_t *inc = node_add(unary, node_num(1, save), save);
		*rest = tok;
		return node_bin(NODE_ASSIGN, unary, inc, save);
	}

	if(token_eq(tok, "--")) {
		token_t *save = tok;
		tok = token_skip(tok, "--");
		node_t *unary = parse_unary(tok, &tok);
		node_t *dec = node_sub(unary, node_num(1, save), save);
		*rest = tok;
		return node_bin(NODE_ASSIGN, unary, dec, save);
	}

	return parse_postfix(tok, rest);
}

static node_t *parse_cast(token_t *tok, token_t **rest)
{
	token_t *save = tok;
	node_t *node;
	if(token_eq(tok, "(")) {
		node = node_unary(NODE_CAST, NULL, tok);
		tok = tok->next;
		if(!is_declspec(tok)) {
			goto unary;
		}
		node->type = parse_type_name(tok, &tok);
		tok = token_skip(tok, ")");
		node->lhs = parse_cast(tok, &tok);
		*rest = tok;
		return node;
	}
unary:;
	node = parse_unary(save, &tok);
	*rest = tok;
	return node;
}

static type_t *parse_parameter_declaration(token_t *tok, token_t **rest)
{
	type_t *base = parse_declspec(tok, &tok);
	type_t *complete = parse_declarator(base, tok, &tok);

	*rest = tok;
	return complete;
}

static void parse_global_var(type_t *decltype, token_t *tok, token_t **rest)
{
	obj_t *found = NULL;

	if(!(found = find_var(decltype->ident))) {
		(void)obj_make_global(mystrndup(decltype->ident->loc,
										decltype->ident->len),
							  decltype, false);
	}

	while(token_eq(tok, ",")) {
		tok = token_skip(tok, ",");
		if(!(found = find_var(decltype->ident))) {
			(void)obj_make_global(mystrndup(decltype->ident->loc,
											decltype->ident->len),
								  decltype, false);
		}
	}

	if(!token_eq(tok, ";")) {
		compile_err(tok->loc, "expected a semicolon");
	}
	tok = token_skip(tok, ";");

	*rest = tok;
	return;
}

static obj_t *parse_function_def(type_t *decltype, token_t *tok, token_t **rest)
{
	local_order = 0;
	strmap_delete(locals);
	locals = strmap_make(obj_t *);

	obj_t *func;
	char *name = mystrndup(decltype->ident->loc, decltype->ident->len);
	obj_t **possible = strmap_get(known_funcs, name);
	if(possible) {
		func = *possible;
		free(name);
	} else {
		func = obj_make_noadd(name, type_func_to(decltype), true);
		func->order = local_order++;
	}

	scope_push();

	tok = token_skip(tok, "(");

	if(token_eq(tok, "void")) {
		token_t *nxt = token_skip(tok, "void");
		if(token_eat(&nxt, ")")) {
			tok = nxt;
			func->args = NULL;
			goto end;
		}
	}

	if(token_eq(tok, ")")) {
		tok = token_skip(tok, ")");
		func->args = NULL;
		goto end;
	}

	/* parse parameters */

	obj_t head = {};
	obj_t *cur = &head;

	type_t *paramtype = parse_parameter_declaration(tok, &tok);
	obj_t *param =
		obj_make(mystrndup(paramtype->ident->loc, paramtype->ident->len),
				 paramtype, false);
	param->addressed = true;
	cur->next = param;
	cur = cur->next;

	while(token_eq(tok, ",")) {
		tok = token_skip(tok, ",");
		paramtype = parse_parameter_declaration(tok, &tok);
		param =
			obj_make(mystrndup(paramtype->ident->loc, paramtype->ident->len),
					 paramtype, false);
		param->addressed = true;
		cur->next = param;
		cur = cur->next;
	}

	obj_t *params = head.next;
	func->args = params;

	tok = token_skip(tok, ")");

end:

	strmap_put(known_funcs, func->name, func);
	if(token_eat(&tok, ";")) {
		scope_pop();
		strmap_delete(locals);
		locals = NULL;
		*rest = tok;
		return NULL;
	}

	func->body = parse_compound_stmt(tok, &tok);

	scope_pop();
	*rest = tok;
	return func;
}

static int cmp_order(const void *a, const void *b)
{
	obj_t *obj_a = *(obj_t **)a;
	obj_t *obj_b = *(obj_t **)b;
	return obj_a->order - obj_b->order;
}

/* does the parsing */
parse_res_t parse_do(token_t *toks)
{
	scope_push();
	token_t *tok = toks;
	globals = strmap_make(obj_t *);
	global_order = 0;
	local_order = 0;
	known_funcs = strmap_make(obj_t *);
	while(tok->kind != TOK_END) {
		type_t *declspec = parse_declspec(tok, &tok);
		/* struct def or typedef */
		if(token_eat(&tok, ";")) {
			continue;
		}
		type_t *decl = parse_declarator(declspec, tok, &tok);

		obj_t *obj = NULL;

		/* function */
		if(token_eq(tok, "(")) {
			obj = parse_function_def(decl, tok, &tok);
			if(!obj) {
				goto out;
			}

			LIST(obj_t *) locals_list = list_make(obj_t *);
			UNUSEDA char *key;
			obj_t *val;
			strmap_iter(locals, key, val, { list_append(locals_list, val); });
			qsort(locals_list, list_len(locals_list), sizeof(obj_t *),
				  cmp_order);

			obj->vars = locals_list;
			obj->stack_size = -1;
			strmap_put(globals, obj->name, obj);
		} else {
			/* global variable */
			parse_global_var(decl, tok, &tok);
		}
out:

		strmap_delete(locals);
		locals = NULL;
	}
	scope_pop();

	/* collect globals */

	LIST(obj_t *) globals_list = list_make(obj_t *);
	UNUSEDA char *key;
	obj_t *val;
	strmap_iter(globals, key, val, { list_append(globals_list, val); });
	qsort(globals_list, list_len(globals_list), sizeof(obj_t *), cmp_order);
	strmap_delete(globals);

	return (parse_res_t){ .globals = globals_list };
}
