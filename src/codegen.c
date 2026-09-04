#include "bird/bird.h"
#include "codegen.h"
#include "parse.h"
#include "type.h"
#include "zz/base.h"
#include "zz/list.h"
#include "zz/prof.h"
#include <stdlib.h>

static int did_ret = 0;
static ir_func_t *fun;
static obj_t *fun_obj;
static long blk_num;
static ir_blk_t *outblk;
static ir_buildr_t buildr_val;
static ir_buildr_t *build = &buildr_val;

typedef struct flow {
	ir_blk_t *break_to;
	ir_blk_t *continue_to;
} flow_t;

static LIST(flow_t) break_stack;

#define MAKE(ty, r0, r1, r2, imm) ir_inst_make(IR_INST_##ty, r0, r1, r2, imm)

#define INSNAME(name) emit_##name

#define DEF_INS(name, name2, r0, r1, r2, imm, ...)                      \
	static void INSNAME(name)(bool is_32bit __VA_OPT__(, ) __VA_ARGS__) \
	{                                                                   \
		ir_inst_t *ins = MAKE(name2, r0, r1, r2, imm);                  \
		ins->is_32bit = is_32bit;                                       \
		ir_blk_add(outblk, ins);                                        \
		return;                                                         \
	}

DEF_INS(imm, IMM, r0, NULL, NULL, imm, reg_t *r0, uint64_t imm);
DEF_INS(add, ADD, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);

#undef DEF_INS
#undef MAKE

static reg_t *emit_leas_var(ir_buildr_t *build, long off, obj_t *var)
{
	ir_inst_t *ins = ins_leas(reg_make(), off);
	/* absolutely horrid code, but it will work */
	ins->r0->rhs = (reg_t *)var;
	ir_buildr_emit_ins(build, ins);
	return ins->r0;
}

#define LOOPSZ(num)                                      \
	do {                                                 \
		bool is_32bit = (num) <= 4;                      \
		while(left >= (num)) {                           \
			emit_imm(0, data, ptr);                      \
			emit_add(0, toaddr, to, data);               \
			emit_add(0, fromaddr, from, data);           \
			ir_inst_t *loadi = ins_load(data, fromaddr); \
			loadi->size = (num);                         \
			loadi->is_32bit = is_32bit;                  \
			ir_blk_add(outblk, loadi);                   \
			ir_inst_t *storei = ins_store(toaddr, data); \
			storei->size = (num);                        \
			storei->is_32bit = is_32bit;                 \
			ir_blk_add(outblk, storei);                  \
			left -= (num);                               \
			ptr += (num);                                \
		}                                                \
	} while(0)

static void blit(size_t size, reg_t *from, reg_t *to)
{
	outblk = build->insert_blk;
	size_t ptr = 0;
	reg_t *toaddr = reg_make(), *fromaddr = reg_make();
	reg_t *data = reg_make();
	size_t left = size;

	LOOPSZ(8);
	LOOPSZ(4);
	LOOPSZ(2);
	LOOPSZ(1);

	return;
}

static reg_t *emit_load_sz(type_t *typ, reg_t *addr)
{
	reg_t *res = ir_buildr_creat_load(
		build, typ->size < 8 && type_is_signed(typ), typ->size, addr);
	if(typ->kind == TYPE_BOOL) {
		res = ir_buildr_creat_bool(build, typ->size <= 4, res);
	}
	return res;
}

static reg_t *emit_load_obj(type_t *typ, reg_t *addr)
{
	if(typ->kind == TYPE_ARRAY || typ->kind == TYPE_STRUCT) {
		return ir_buildr_copy(build, addr);
	} else {
		return emit_load_sz(typ, addr);
	}
}

static void emit_store_sz(type_t *typ, reg_t *r1, reg_t *r2)
{
	ir_buildr_creat_store(build, typ->size, r1, r2);
	return;
}

static void emit_store_obj(type_t *typ, reg_t *val, reg_t *addr)
{
	if(typ->kind == TYPE_STRUCT) {
		blit(typ->size, addr, val);
	} else {
		emit_store_sz(typ, val, addr);
	}
	return;
}

static reg_t *codegen_expr(node_t *node);

/* calculates address of node `node` -- places it into register `reg` */
static reg_t *calc_addr(node_t *node)
{
	if(node->kind == NODE_VAR) {
		if(node->var->is_global) {
			return ir_buildr_creat_lea(build, node->var->glob);
		} else {
			long placement = -node->var->off;
			return emit_leas_var(build, placement, node->var);
		}
	}
	if(node->kind == NODE_DEREF) {
		return codegen_expr(node->lhs);
	}
	if(node->kind == NODE_MEMBER) {
		/* TODO: this presents a really easy algorithm for a SROA-like pass.
		 * since the struct access is basically just chained adds, we can
		 * collapse the adds into 1 instruction and then check if the value
		 * is only used inside those adds/memory. If yes, we can split the variable
		 * out. */
		reg_t *base = calc_addr(node->lhs);
		reg_t *off = ir_buildr_creat_imm64(build, node->memb->loc);
		reg_t *add = ir_buildr_creat_add(build, 0, base, off);
		return add;
	}

	compile_err_node(node, "cannot calculate address of non-variable");

	return NULL;
}

void codegen_stmt(node_t *node);

static reg_t *codegen_cast(node_t *node, reg_t *r)
{
	type_t *from_ty = node->lhs->type;
	type_t *to_ty = node->type;
	size_t from = from_ty->size;
	size_t to = to_ty->size;
	bool from_unsignd = from_ty->unsignd;
	bool to_unsignd = to_ty->unsignd;

	if(to_unsignd == from_unsignd && to == from) {
		return r;
	}

	if(to >= from) {
		return r;
	}

	reg_t *res = ir_buildr_creat_ext(build, false, to_unsignd, to, r);
	return res;
}

/* generates code given AST tree */
reg_t *codegen_expr(node_t *node)
{
	type_t *type = node->type;
	if(!node) {
		ERROR("null node passed");
	}

	bool is32 = node->lhs ? node->lhs->type->size <= 4 : node->type->size <= 4;

	/* special cases */
	switch(node->kind) {
	case NODE_NUM: {
		return ir_buildr_creat_imm(build, is32, node->num);
	}
	case NODE_NEG: {
		reg_t *val = codegen_expr(node->lhs);
		return ir_buildr_creat_neg(build, is32, val);
	};
	case NODE_NOT: {
		reg_t *val = codegen_expr(node->lhs);
		return ir_buildr_creat_not(build, is32, val);
	};
	case NODE_LOGNEG: {
		reg_t *val = codegen_expr(node->lhs);
		return ir_buildr_creat_invbool(build, is32, val);
	};
	case NODE_VAR:
	case NODE_MEMBER: {
		reg_t *addr = calc_addr(node);
		return emit_load_obj(type, addr);
	};
	case NODE_ADDR: {
		node->lhs->var->addressed = true;
		return calc_addr(node->lhs);
	}
	case NODE_DEREF: {
		reg_t *addr = codegen_expr(node->lhs);
		return emit_load_obj(type, addr);
	}
	case NODE_CAST: {
		reg_t *val = codegen_expr(node->lhs);
		return codegen_cast(node, val);
	}
	case NODE_ASSIGN: {
		reg_t *lval = calc_addr(node->lhs);
		reg_t *rval = codegen_expr(node->rhs);
		emit_store_obj(type, lval, rval);
		return rval;
	};
	case NODE_COMMA: {
		(void)codegen_expr(node->lhs);
		return codegen_expr(node->rhs);
	};
	case NODE_FUNCALL: {
		LIST(callreg_t *) callargs = list_make(reg_t *);
		node_t *arg = node->fargs;
		for(; arg; arg = arg->next) {
			if(arg->type->kind == TYPE_STRUCT ||
			   arg->type->kind == TYPE_UNION) {
				compile_err_node(
					arg,
					"ABI for structs and unions are not implemented yet, pass by refrence please");
			}
			reg_t *argres = codegen_expr(arg);
			callreg_t *callreg =
				callreg_make(argres, ARG_CLASS_INTEGER, arg->type->size);
			list_append(callargs, callreg);
		}
		return ir_buildr_creat_call(build, node->fname, callargs);
	};

	case NODE_STMT_EXPR: {
		node_t *nod;
		for(nod = node->body; nod->next; nod = nod->next) {
			codegen_stmt(nod);
		}
		return codegen_expr(nod->lhs);
	}; break;

	case NODE_LOGAND: {
		reg_t *lhs = codegen_expr(node->lhs);
		ir_blk_t *left_blk = ir_buildr_make_blk(build);
		ir_blk_t *resume = ir_buildr_make_blk(build);
		reg_t *res = ir_buildr_creat_imm32(build, 0);
		ir_buildr_creat_br_set(build, is32, lhs, left_blk, resume, left_blk);
		reg_t *rhs = codegen_expr(node->rhs);
		ir_inst_t *ins = ins_mkbool(res, rhs);
		ins->is_32bit = is32;
		ir_buildr_emit_ins(build, ins);
		ir_buildr_creat_jmp_set(build, resume);
		return res;
	}

	case NODE_LOGOR: {
		reg_t *lhs = codegen_expr(node->lhs);
		ir_blk_t *right_blk = ir_buildr_make_blk(build);
		ir_blk_t *resume = ir_buildr_make_blk(build);

		reg_t *res = ir_buildr_creat_bool(build, is32, lhs);
		ir_buildr_creat_br_set(build, is32, lhs, resume, right_blk, right_blk);
		reg_t *rhs = codegen_expr(node->rhs);
		ir_inst_t *ins = ins_mkbool(res, rhs);
		ins->is_32bit = is32;
		ir_buildr_emit_ins(build, ins);
		ir_buildr_creat_jmp_set(build, resume);
		return res;
	}

	default:
		break;
	}

	reg_t *lhs = codegen_expr(node->lhs);
	reg_t *rhs = codegen_expr(node->rhs);

	switch(node->kind) {
	default:
		break;

	case NODE_ADD:
		return ir_buildr_creat_add(build, is32, lhs, rhs);
	case NODE_SUB:
		return ir_buildr_creat_sub(build, is32, lhs, rhs);
	case NODE_SHL:
		return ir_buildr_creat_shl(build, is32, lhs, rhs);
	case NODE_SHR:
		return ir_buildr_creat_shr(build, is32, type->unsignd, lhs, rhs);
	case NODE_AND:
		return ir_buildr_creat_and(build, is32, lhs, rhs);
	case NODE_OR:
		return ir_buildr_creat_or(build, is32, lhs, rhs);
	case NODE_EOR:
		return ir_buildr_creat_eor(build, is32, lhs, rhs);
	case NODE_MUL:
		return ir_buildr_creat_mul(build, is32, type->unsignd, lhs, rhs);
	case NODE_DIV:
		return ir_buildr_creat_div(build, is32, type->unsignd, lhs, rhs);
	case NODE_MOD:
		return ir_buildr_creat_mod(build, is32, type->unsignd, lhs, rhs);
	case NODE_EQ:
		return ir_buildr_creat_cmp_eq(build, is32, lhs, rhs);
	case NODE_NE:
		return ir_buildr_creat_cmp_ne(build, is32, lhs, rhs);
	case NODE_LE:
		return ir_buildr_creat_cmp_le(build, is32, type->unsignd, lhs, rhs);
	case NODE_LT:
		return ir_buildr_creat_cmp_lt(build, is32, type->unsignd, lhs, rhs);
	case NODE_GE:
		return ir_buildr_creat_cmp_ge(build, is32, type->unsignd, lhs, rhs);
	case NODE_GT:
		return ir_buildr_creat_cmp_gt(build, is32, type->unsignd, lhs, rhs);
	}

	return NULL;
}

void codegen_stmt(node_t *node)
{
	switch(node->kind) {
	case NODE_EXPR_STMT:
		(void)codegen_expr(node->lhs);
		break;

	/* reuse case_blk for gotos, because why not? */
	case NODE_GOTO:
		if(!node->label_node->case_blk) {
			node->label_node->case_blk = ir_buildr_make_blk(build);
		}
		ir_buildr_creat_jmp(build, node->label_node->case_blk);
		break;

	case NODE_LABEL:
		if(!node->case_blk) {
			node->case_blk = ir_buildr_make_blk(build);
		}
		ir_buildr_creat_jmp_set(build, node->case_blk);
		codegen_stmt(node->then);
		break;

	case NODE_CASE:
	case NODE_DEFAULT:
		ir_buildr_creat_jmp_set(build, node->case_blk);
		codegen_stmt(node->then);
		break;

	case NODE_SWITCH: {
		ir_blk_t *top = ir_buildr_make_blk(build); /* for continue */
		ir_blk_t *resume = ir_buildr_make_blk(build);
		list_append(break_stack,
					((flow_t){ .break_to = resume, .continue_to = top }));
		ir_blk_t *chain;
		ir_blk_t *def_blk = NULL;
		reg_t *ctrl = codegen_expr(node->cond);
		bool is32 = node->cond->type->size <= 4;
		for(node_t *b = node->then->body; b; b = b->next) {
			/* default is exception */
			if(b->kind == NODE_DEFAULT) {
				def_blk = ir_buildr_make_blk(build);
				b->case_blk = def_blk;
				continue;
			}

			/* skip all non-cases, will deal with them later */
			if(b->kind != NODE_CASE) {
				continue;
			}

			b->case_blk = ir_buildr_make_blk(build);
			chain = ir_buildr_make_blk(build);

			/* chain1: %r = cmp.eq %ctrl, #case
			 *         br %r, case_blk, chain2
			 *         ; fallthrough
			 * chain2: ...
			 * basically what we are doing
			 */

			reg_t *casenum, *casenum2, *cmpres;

			/* %casenum = imm #num */
			casenum = ir_buildr_creat_imm(build, is32, b->cond->num);
			if(b->cond->kind == NODE_NUM) {
				/* %cmpres = cmp.eq %ctrl, %casenum */
				cmpres = ir_buildr_creat_cmp_eq(build, is32, ctrl, casenum);
			} else {
				/* %casenum2 = imm #num2 */
				casenum2 = ir_buildr_creat_imm(build, is32, b->cond->num2);
				/* the range is inclusive, so we check
				 * (x >= LOW && x <= HIGH) */
				reg_t *cpart1 = ir_buildr_creat_cmp_ge(
					build, is32, b->cond->type->unsignd, ctrl, casenum);
				reg_t *cpart2 = ir_buildr_creat_cmp_le(
					build, is32, b->cond->type->unsignd, ctrl, casenum2);
				cmpres = ir_buildr_creat_and(build, is32, cpart1, cpart2);
			}
			/* br %cmpres, case_blk, chain */
			ir_buildr_creat_br_set(build, is32, cmpres, b->case_blk, chain,
								   chain);
		}

		/* if there is no default break out of the switch */
		ir_buildr_creat_jmp(build, def_blk ? def_blk : resume);

		/* now handle all stmts in the switch */
		for(node_t *b = node->then->body; b; b = b->next) {
			codegen_stmt(b);
		}

		/* since we don't know where we are, make sure we are at `resume` */
		ir_buildr_creat_jmp_set(build, resume);

		list_back(break_stack);
	}; break;

	case NODE_BREAK: {
		if(list_len(break_stack) == 0) {
			compile_err_node(node, "nothing to break to");
		}
		flow_t f = list_peek(break_stack);
		ir_buildr_creat_jmp(build, f.break_to);
	}; break;

	case NODE_CONTINUE: {
		if(list_len(break_stack) == 0) {
			compile_err_node(node, "nothing to continue to");
		}
		flow_t f = list_peek(break_stack);
		ir_buildr_creat_jmp(build, f.continue_to);
	}; break;

	case NODE_RET: {
		did_ret = 1;
		type_t *rettype = fun_obj->type->to;

		if(node->lhs && rettype->kind != TYPE_VOID) {
			bool is32 = node->lhs->type->size <= 4;
			reg_t *retval = codegen_expr(node->lhs);
			reg_t *ext = ir_buildr_creat_ext(build, is32,
											 node->lhs->type->unsignd,
											 node->lhs->type->size, retval);
			ir_buildr_creat_ret(build, is32, ext);
		} else if(!node->lhs && rettype->kind == TYPE_VOID) {
			ir_buildr_creat_ret(build, false, NULL);
		} else if(node->lhs && rettype->kind == TYPE_VOID) {
			compile_err(node->tok->loc, "function cannot return something");
		} else if(!node->lhs && rettype->kind != TYPE_VOID) {
			compile_err(node->tok->loc, "function has to return something");
		}
	}; break;
	case NODE_BLOCK: {
		/* generate statements for each stmt in block/compound stmt */
		for(node_t *nod = node->body; nod; nod = nod->next) {
			codegen_stmt(nod);
		}
	}; break;
	case NODE_DOWHILE: {
		/* a `while` but we check afterwards */
		ir_blk_t *then = ir_buildr_make_blk(build);
		ir_blk_t *resume = ir_buildr_make_blk(build);
		ir_blk_t *condchk = ir_buildr_make_blk(build);
		list_append(break_stack,
					((flow_t){ .break_to = resume, .continue_to = condchk }));
		ir_buildr_creat_jmp_set(build, then);
		codegen_stmt(node->then);
		ir_buildr_creat_jmp_set(build, condchk);
		reg_t *cond = codegen_expr(node->cond);
		bool is32 = node->cond->type->size <= 4;
		ir_buildr_creat_br_set(build, is32, cond, then, resume, resume);
		list_back(break_stack);
	}; break;
	case NODE_WHILE: {
		/* a `do`/`while` but we check beforewards */
		ir_blk_t *condchk = ir_buildr_make_blk(build);
		ir_blk_t *loop = ir_buildr_make_blk(build);
		ir_blk_t *resume = ir_buildr_make_blk(build);
		list_append(break_stack,
					((flow_t){ .break_to = resume, .continue_to = condchk }));

		ir_buildr_creat_jmp_set(build, condchk);
		reg_t *cond = codegen_expr(node->cond);
		bool is32 = node->cond->type->size <= 4;
		ir_buildr_creat_br_set(build, is32, cond, loop, resume, loop);

		codegen_stmt(node->then);
		ir_buildr_creat_jmp(build, condchk);
		ir_buildr_set_insert_blk(build, resume);
		list_back(break_stack);
	}; break;
	case NODE_FOR: {
		/* initializer */
		codegen_stmt(node->init);

		ir_blk_t *condchk =
			ir_buildr_make_blk(build); /* check if need to go loop or resume */
		ir_blk_t *then = ir_buildr_make_blk(build);
		ir_blk_t *inc = then;
		ir_blk_t *resume = ir_buildr_make_blk(build);

		if(node->inc) {
			inc = ir_buildr_make_blk(build);
		}

		list_append(break_stack,
					((flow_t){ .break_to = resume,
							   .continue_to = node->inc ? inc : condchk }));

		ir_buildr_creat_jmp_set(build, condchk);

		reg_t *cond;
		if(node->cond) {
			cond = codegen_expr(node->cond);
		} else {
			cond = ir_buildr_creat_imm32(build, 1);
		}

		bool is32 = node->cond ? node->cond->type->size <= 4 : true;
		ir_buildr_creat_br_set(build, is32, cond, then, resume, then);

		codegen_stmt(node->then);

		if(node->inc) {
			ir_buildr_creat_jmp_set(build, inc);
			UNUSED(codegen_expr(node->inc));
		}

		ir_buildr_creat_jmp(build, condchk);
		ir_buildr_set_insert_blk(build, resume);
		list_back(break_stack);
	} break;
	case NODE_IF: {
		reg_t *cond = codegen_expr(node->cond);

		ir_blk_t *then = ir_buildr_make_blk(build),
				 *elze = ir_buildr_make_blk(build);
		ir_blk_t *resume;

		if(node->elze == NULL) {
			resume = elze;
		} else {
			resume = ir_buildr_make_blk(build);
		}

		bool is32 = node->cond->type->size <= 4;
		ir_buildr_creat_br_set(build, is32, cond, then, elze, then);
		codegen_stmt(node->then);
		ir_buildr_creat_jmp(build, resume);
		if(node->elze) {
			ir_buildr_set_insert_blk(build, elze);
			codegen_stmt(node->elze);
			ir_buildr_creat_jmp(build, resume);
		}

		ir_buildr_set_insert_blk(build, resume);

	}; break;
	default:
		compile_err_node(node, "invalid stmt");
		break;
	}

	return;
}

/* assign globals */
static void assign_globals(LIST(obj_t *) globals)
{
	for(size_t i = 0; i < list_len(globals); i++) {
		obj_t *glob = globals[i];
		if(!glob || glob->is_func) {
			continue;
		}
		glob->glob = ir_glob_make(glob->name, glob->type->size,
								  glob->type->align, glob->data);
		/* crude string detection */
		if(glob->type->kind == TYPE_ARRAY &&
		   glob->type->to->kind == TYPE_CHAR) {
			glob->glob->is_str = true;
		}

		glob->glob->is_anon = glob->is_anon;
		if(glob->is_static) {
			glob->glob->is_anon = true;
		}
	}
	return;
}

/* calculate stack frame space needed for function `fn`. returns maximum alignment */
static void rewrite_stack_offs(ir_func_t *fun)
{
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		for(ir_inst_t *inst = blk->insts; inst; inst = inst->next) {
			if(inst->type != IR_INST_LEAS) {
				continue;
			}

			inst->imm = -((obj_t *)(inst->r0->rhs))->off;
		}
	}
}

static size_t calc_stack_needed(obj_t *fn, enum ir_arch arch)
{
	size_t max_align = 0;
	size_t space = 0;
	LIST(obj_t *) arrays = list_make(obj_t *);
	for(size_t i = 0; i < list_len(fn->vars); i++) {
		obj_t *obj = fn->vars[i];
		if(obj->is_func || obj->skip) {
			continue;
		}
		if(obj->type->kind == TYPE_ARRAY) {
			/* deal with this later */
			list_append(arrays, obj);
			continue;
		}
		space += obj->type->size;
		space = align_to(space, obj->type->align);
		obj->off = -(long)space;

		if(obj->type->align > max_align) {
			max_align = obj->type->align;
		}
	}

	/* deal with arrays now */
	/* TODO: smarter spill'd register placement */
	for(size_t i = 0; i < list_len(arrays); i++) {
		obj_t *obj = arrays[i];
		if(obj->skip) {
			continue;
		}

		/* align arrays with size >= 16 to alignment 16
		 * if needed. This is needed for system V ABI. */
		if(obj->type->size >= 16 && obj->type->align < 16 &&
		   arch == IR_ARCH_X64_SYSV) {
			obj->type->align = 16;
		}

		space += obj->type->size;
		space = align_to(space, obj->type->align);
		obj->off = -(long)space;

		if(obj->type->align > max_align) {
			max_align = obj->type->align;
		}
	}

	fn->stack_size = space;

	list_delete(arrays);
	return max_align;
}

/* variable optimization */
/* optimize out load/stores, place it in own register */
static void varopt(ir_func_t *func)
{
	/* make sure leas are not used outside of loads and stores */
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			if(ins->type == IR_INST_LEAS) {
				ins->r0->stack_loc = true;
				continue;
			} else if(ins->r0) {
				ins->r0->stack_loc = false;
			}

			if(ins->r0 && ins->r0->stack_loc && ins->type != IR_INST_LOAD &&
			   ins->type != IR_INST_STORE) {
				ins->r0->stack_loc = false;
			}

			if(ins->r1 && ins->r1->stack_loc && ins->type != IR_INST_LOAD &&
			   ins->type != IR_INST_STORE) {
				ins->r1->stack_loc = false;
			}

			if(ins->r2 && ins->r2->stack_loc && ins->type != IR_INST_LOAD &&
			   ins->type != IR_INST_STORE) {
				ins->r2->stack_loc = false;
			}

			if(ins->type == IR_INST_CALL) {
				for(size_t j = 0; j < list_len(ins->call_args); j++) {
					if(ins->call_args[j]->r->stack_loc) {
						ins->call_args[j]->r->stack_loc = false;
					}
				}
			}
		}
	}

	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			if(ins->type == IR_INST_LEAS) {
				obj_t *obj = (obj_t *)ins->r0->rhs;
				if(!obj) {
					goto cant;
				}
				if(obj->addressed) {
					goto cant;
				}

				if(!ins->r0->stack_loc) {
cant:
					/* can't optimize sorry */
					obj->addressed = true;
					obj->skip = false;
					continue;
				}

				if(!obj->eq_reg) {
					obj->eq_reg = reg_make();
				}

				obj->skip = true;

				/* else make new reg, store in lhs */
				ins->r0->lhs = obj->eq_reg;
				ins->type = IR_INST_NOP;
			}

			if(ins->type == IR_INST_LOAD) {
				obj_t *obj = (obj_t *)ins->r1->rhs;
				if(!obj || !obj->skip || obj->addressed) {
					continue;
				}

				reg_t *replacement = ins->r1->lhs;

				/* rewrite %var = load %addr into %var = (EXT) %var_reg */
				bool ext = !obj->type->unsignd;
				size_t sz = obj->type->size;
				ins->type = ext ? IR_INST_SXT : IR_INST_ZXT;
				ins->size = sz;
				if(ins->size == 8) {
					ins->type = IR_INST_MOV;
				}
				ins->r1 = replacement;
				continue;
			}

			if(ins->type == IR_INST_STORE) {
				obj_t *obj = (obj_t *)ins->r1->rhs;
				if(!obj || !obj->skip || obj->addressed) {
					continue;
				}

				reg_t *replacement = ins->r1->lhs;

				/* rewrite store %addr, %var into %var_reg = %var */
				ins->type = IR_INST_MOV;
				ins->r1 = ins->r2;
				ins->r0 = replacement;
				continue;
			}
		}
	}
	return;
}

/* generates code for a function */
void codegen_func(FILE *f, LIST(obj_t *) globals, int opt_level,
				  enum ir_arch backend)
{
	ir_buildr_make(build);
	blk_num = 0;

	assign_globals(globals);
	reg_reset_counter();

	break_stack = list_make(flow_t);

	for(size_t i = 0; i < list_len(globals); i++) {
		blk_num = 0;
		obj_t *cur_fn = globals[i];

		if(cur_fn && cur_fn->is_global) {
			ir_buildr_add_glob(build, cur_fn->glob);
			continue;
		}

		if(!cur_fn) {
			continue;
		}

		ENSURE(cur_fn->is_func, "tried to generate code for a variable");
		ir_func_t *func = ir_buildr_make_func(build, cur_fn->name);
		func->is_local = cur_fn->is_static;
		fun = func;
		fun_obj = cur_fn;
		func->args = list_make(callreg_t *);

		outblk = build->insert_blk;

		(void)calc_stack_needed(cur_fn, backend);

		obj_t *fnargs = cur_fn->args;
		for(; fnargs; fnargs = fnargs->next) {
			reg_t *reg = reg_make();
			reg->off = fnargs->off;
			callreg_t *r =
				callreg_make(reg, ARG_CLASS_INTEGER, fnargs->type->size);
			list_append(func->args, r);
		}

		fun->stack_needed = align_to(cur_fn->stack_size, 16);
		codegen_stmt(cur_fn->body);
		if(cur_fn->type->to->kind != TYPE_VOID && !did_ret &&
		   strcmp(cur_fn->name, "main") != 0) {
			compile_err_node(cur_fn->body, "function must return something");
		}
		did_ret = 0;
		list_hdr(break_stack)->size = 0;

		varopt(func);
		ir_fix(func);

		ir_inst_t *nop = ins_nop();
		nop->next = fun->blocks[0]->insts;
		fun->blocks[0]->insts = nop;
		ir_inst_t *trail = nop;

		for(size_t i = 0; i < list_len(cur_fn->vars); i++) {
			obj_t *var = cur_fn->vars[i];
			if(!var->skip) {
				continue;
			}

			ir_inst_t *nxt = trail->next;
			ir_inst_t *imml = ins_imm(var->eq_reg, 0);
			imml->next = nxt;
			trail->next = imml;
		}

		fun->blocks[0]->insts = fun->blocks[0]->insts->next;
		ir_inst_delete(nop);

		size_t max_align = calc_stack_needed(cur_fn, backend);
		rewrite_stack_offs(func);
		fun->stack_needed = align_to(cur_fn->stack_size, 16);
		fun->align_needed = align_to(max_align, 16);

		ir_buildr_end_func(build);
	}

	TIMEIT("ir", { ir_prog_compile(f, &build->prog, backend, opt_level); });
	list_delete(break_stack);
	ir_buildr_delete(build);
	return;
}
