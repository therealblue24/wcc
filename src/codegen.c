#include "codegen.h"
#include "parse.h"
#include "type.h"
#include <stdlib.h>
#include "bird/ir.h"

static ir_func_t *fun;
static obj_t *fun_obj;
long blk_num;
static ir_blk_t *outblk;

#define MAKE(ty, r0, r1, r2, imm) ir_inst_make(IR_INST_##ty, r0, r1, r2, imm)

#define INSNAME(name) emit_##name

#define DEF_INS(name, name2, r0, r1, r2, imm, ...)     \
	static UNUSEDA void INSNAME(name)(__VA_ARGS__)     \
	{                                                  \
		ir_inst_t *ins = MAKE(name2, r0, r1, r2, imm); \
		ir_blk_add(outblk, ins);                       \
		return;                                        \
	}

DEF_INS(nop, NOP, NULL, NULL, NULL, 0, void);
DEF_INS(mov, MOV, r0, r1, NULL, 0, reg_t *r0, reg_t *r1);
DEF_INS(imm, IMM, r0, NULL, NULL, imm, reg_t *r0, uint64_t imm);
DEF_INS(add, ADD, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(sub, SUB, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(shl, SHL, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(shr, SHR, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(ashr, ASHR, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(and, AND, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(or, OR, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(eor, EOR, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(smul, SMUL, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(sdiv, SDIV, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(smod, SMOD, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(udiv, UDIV, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(umul, UMUL, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(umod, UMOD, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);

DEF_INS(eq, EQ, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(ne, NE, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(slt, SLT, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(sle, SLE, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(sgt, SGT, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(sge, SGE, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(ult, ULT, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(ule, ULE, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(ugt, UGT, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(uge, UGE, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(neg, NEG, r0, r1, NULL, 0, reg_t *r0, reg_t *r1);
DEF_INS(not, NOT, r0, r1, NULL, 0, reg_t *r0, reg_t *r1);
DEF_INS(notbool, NOTBOOL, r0, r1, NULL, 0, reg_t *r0, reg_t *r1);
DEF_INS(mkbool, MKBOOL, r0, r1, NULL, 0, reg_t *r0, reg_t *r1);
DEF_INS(leas, LEAS, r0, NULL, NULL, imm, reg_t *r0, long imm);
DEF_INS(ret, RET, NULL, r1, NULL, 0, reg_t *r1);

#undef DEF_INS
#undef MAKE

#define INSNAME2(x) ins_##x
#define GEN_LOAD(name)                                        \
	static UNUSEDA void INSNAME(name)(reg_t * r0, reg_t * r1) \
	{                                                         \
		ir_inst_t *inst = INSNAME2(name)(r0, r1);             \
		ir_blk_add(outblk, inst);                             \
		return;                                               \
	}

GEN_LOAD(loadb);
GEN_LOAD(loadw);
GEN_LOAD(loadl);
GEN_LOAD(load);

#undef GEN_LOAD

#define GEN_STORE(name)                                       \
	static UNUSEDA void INSNAME(name)(reg_t * r1, reg_t * r2) \
	{                                                         \
		ir_inst_t *inst = INSNAME2(name)(r1, r2);             \
		ir_blk_add(outblk, inst);                             \
		return;                                               \
	}

GEN_STORE(storeb);
GEN_STORE(storew);
GEN_STORE(storel);
GEN_STORE(store);

#undef GEN_STORE

static UNUSEDA void emit_leas_var(reg_t *r0, long off, obj_t *var)
{
	ir_inst_t *ins = ins_leas(r0, off);
	/* absolutely horrid code, but it will work */
	ins->r0->rhs = (reg_t *)var;
	ir_blk_add(outblk, ins);
	return;
}

static UNUSEDA void emit_lea(reg_t *res, ir_global_t *glob)
{
	ir_inst_t *ins = ins_lea(res, glob);
	ir_blk_add(outblk, ins);
	return;
}

static UNUSEDA void emit_load_sz(type_t *typ, reg_t *r0, reg_t *r1)
{
	reg_t *new_r0 = reg_make();

	ir_inst_t *ins = ins_load(new_r0, r1);
	if(typ->size < 8 && type_is_signed(typ)) {
		ins->sign_ext = true;
	}
	ins->size = typ->size;
	ir_blk_add(outblk, ins);

	if(typ->kind == TYPE_BOOL) {
		emit_mkbool(r0, new_r0);
	} else {
		ins->r0 = r0;
	}
	return;
}

static UNUSEDA void emit_store_sz(type_t *typ, reg_t *r1, reg_t *r2)
{
	ir_inst_t *ins = ins_store(r1, r2);
	ins->size = typ->size;
	ir_blk_add(outblk, ins);
	return;
}

#define GEN_BRCMP(c, name)                                              \
	static UNUSEDA void emit_##name(reg_t *r1, reg_t *r2, ir_blk_t *fb, \
									ir_blk_t *tb)                       \
	{                                                                   \
		ir_blk_add(outblk, ins_##name(r1, r2, fb, tb));                 \
		return;                                                         \
	}

GEN_BRCMP(IR_INST_BREQ, breq);
GEN_BRCMP(IR_INST_BRNE, brne);
GEN_BRCMP(IR_INST_BRSLT, brslt);
GEN_BRCMP(IR_INST_BRSLE, brsle);
GEN_BRCMP(IR_INST_BRSGT, brsgt);
GEN_BRCMP(IR_INST_BRSGE, brsge);

GEN_BRCMP(IR_INST_BRULT, brult);
GEN_BRCMP(IR_INST_BRULE, brule);
GEN_BRCMP(IR_INST_BRUGT, brugt);
GEN_BRCMP(IR_INST_BRUGE, bruge);

#define DEF_INS(name)                                         \
	static UNUSEDA void INSNAME(name)(reg_t * r0, reg_t * r1) \
	{                                                         \
		ir_inst_t *ins = INSNAME2(name)(r0, r1);              \
		ir_blk_add(outblk, ins);                              \
		return;                                               \
	}

DEF_INS(zxtb);
DEF_INS(sxtb);
DEF_INS(zxtw);
DEF_INS(sxtw);
DEF_INS(zxtl);
DEF_INS(sxtl);

#undef GEN_BRCMP
#undef INSNAME
#undef INSNAME2
#undef DEF_INS

/* odd one(s) out */
static void emit_br(reg_t *on, ir_blk_t *trueblk, ir_blk_t *falseblk)
{
	ir_blk_add(outblk, ins_br(on, falseblk, trueblk));
	return;
}

static void emit_jmp(ir_blk_t *blk)
{
	ir_blk_add(outblk, ins_jmp(blk));
	return;
}

static void emit_call(reg_t *res, char *fname, LIST(callreg_t *) args)
{
	ir_blk_add(outblk, ins_call(res, fname, args));
	return;
}

static ir_blk_t *emit_blk(void)
{
	ir_blk_t *blk = ir_blk_make(NULL);
	blk->num = blk_num++;
	list_append(fun->blocks, blk);
	return blk;
}

static reg_t *codegen_expr(node_t *node);

/* calculates address of node `node` -- places it into register `reg` */
static reg_t *calc_addr(node_t *node)
{
	if(node->kind == NODE_VAR) {
		reg_t *addr = reg_make();
		if(node->var->is_global) {
			emit_lea(addr, node->var->glob);
		} else {
			long placement = -node->var->off;
			emit_leas_var(addr, placement, node->var);
		}
		return addr;
	}
	if(node->kind == NODE_DEREF) {
		return codegen_expr(node->lhs);
	}

	compile_err_node(node, "cannot calculate address of non-variable");

	return NULL;
}

void codegen_stmt(node_t *node);

/* generates code given AST tree */
reg_t *codegen_expr(node_t *node)
{
	type_t *type = node->type;
	if(!node) {
		ERROR("null node passed");
	}

	/* special cases */
	switch(node->kind) {
	case NODE_NUM: {
		reg_t *imm = reg_make();
		emit_imm(imm, node->num);
		return imm;
	}
	case NODE_NEG: {
		reg_t *val = codegen_expr(node->lhs);
		reg_t *neg = reg_make();
		emit_neg(neg, val);
		return neg;
	};
	case NODE_NOT: {
		reg_t *val = codegen_expr(node->lhs);
		reg_t *not = reg_make();
		emit_neg(not, val);
		return not;
	};
	case NODE_LOGNEG: {
		reg_t *val = codegen_expr(node->lhs);
		reg_t *logneg = reg_make();
		emit_notbool(logneg, val);
		return logneg;
	};
	case NODE_VAR: {
		reg_t *addr = calc_addr(node);
		reg_t *val = reg_make();
		if(node->type->kind == TYPE_ARRAY) {
			emit_mov(val, addr);
		} else {
			emit_load_sz(type, val, addr);
		}
		return val;
	};
	case NODE_ADDR: {
		node->lhs->var->addressed = true;
		return calc_addr(node->lhs);
	}
	case NODE_DEREF: {
		reg_t *expr = codegen_expr(node->lhs);
		reg_t *val = reg_make();
		if(node->type->kind == TYPE_ARRAY) {
			emit_mov(val, expr);
		} else {
			emit_load_sz(type, val, expr);
		}
		return val;
	}
	case NODE_ASSIGN: {
		reg_t *lval = calc_addr(node->lhs);
		reg_t *rval = codegen_expr(node->rhs);
		emit_store_sz(type, lval, rval);
		return rval;
	};
	case NODE_FUNCALL: {
		LIST(callreg_t *) callargs = list_make(reg_t *);
		node_t *arg = node->fargs;
		for(; arg; arg = arg->next) {
			reg_t *argres = codegen_expr(arg);
			callreg_t *callreg =
				callreg_make(argres, ARG_CLASS_INTEGER, arg->type->size);
			list_append(callargs, callreg);
		}
		reg_t *res = reg_make();
		emit_call(res, node->fname, callargs);
		return res;
	};

	case NODE_STMT_EXPR: {
		node_t *nod;
		for(nod = node->body; nod->next; nod = nod->next) {
			codegen_stmt(nod);
		}
		return codegen_expr(nod->lhs);
	}; break;

	default:
		break;
	}

	reg_t *lhs = codegen_expr(node->lhs);
	reg_t *rhs = codegen_expr(node->rhs);
	reg_t *res = reg_make();

	switch(node->kind) {
	default:
		break;

	case NODE_ADD:
		emit_add(res, lhs, rhs);
		break;
	case NODE_SUB:
		emit_sub(res, lhs, rhs);
		break;
	case NODE_SHL:
		emit_shl(res, lhs, rhs);
		break;
	case NODE_SHR:
		if(type->unsignd) {
			emit_shr(res, lhs, rhs);
		} else {
			emit_ashr(res, lhs, rhs);
		}
		break;
	case NODE_AND:
		emit_and(res, lhs, rhs);
		break;
	case NODE_OR:
		emit_or(res, lhs, rhs);
		break;
	case NODE_EOR:
		emit_eor(res, lhs, rhs);
		break;
	case NODE_LOGAND: {
		ir_blk_t *left_blk = emit_blk();
		ir_blk_t *resume = emit_blk();
		emit_imm(res, 0);
		emit_br(lhs, left_blk, resume);
		outblk = left_blk;
		emit_mkbool(res, rhs);
		emit_jmp(resume);
		outblk = resume;
		break;
	}
	case NODE_LOGOR: {
		ir_blk_t *right_blk = emit_blk();
		ir_blk_t *resume = emit_blk();

		emit_mkbool(res, lhs);
		emit_br(lhs, resume, right_blk);
		outblk = right_blk;
		emit_mkbool(res, rhs);
		emit_jmp(resume);
		outblk = resume;
		break;
	}
	case NODE_MUL:
		if(type->unsignd) {
			emit_umul(res, lhs, rhs);
		} else {
			emit_smul(res, lhs, rhs);
		}
		break;
	case NODE_DIV:
		if(type->unsignd) {
			emit_udiv(res, lhs, rhs);
		} else {
			emit_sdiv(res, lhs, rhs);
		}
		break;
	case NODE_MOD:
		if(type->unsignd) {
			emit_umod(res, lhs, rhs);
		} else {
			emit_smod(res, lhs, rhs);
		}
		break;
	case NODE_VAR:
		res = calc_addr(node);
		break;
	case NODE_ASSIGN:
		break;
	case NODE_EQ:
		emit_eq(res, lhs, rhs);
		break;
	case NODE_NE:
		emit_ne(res, lhs, rhs);
		break;
	case NODE_LE:
		if(type->unsignd) {
			emit_ule(res, lhs, rhs);
		} else {
			emit_sle(res, lhs, rhs);
		}
		break;
	case NODE_LT:
		if(type->unsignd) {
			emit_ult(res, lhs, rhs);
		} else {
			emit_slt(res, lhs, rhs);
		}
		break;
	case NODE_GE:
		if(type->unsignd) {
			emit_uge(res, lhs, rhs);
		} else {
			emit_sge(res, lhs, rhs);
		}
		break;
	case NODE_GT:
		if(type->unsignd) {
			emit_ugt(res, lhs, rhs);
		} else {
			emit_sgt(res, lhs, rhs);
		}
		break;
	}

	return res;
}

void codegen_stmt(node_t *node)
{
	switch(node->kind) {
	case NODE_EXPR_STMT:
		(void)codegen_expr(node->lhs);
		break;

	case NODE_RET: {
		type_t *rettype = fun_obj->type->to;
		if(node->lhs && rettype->kind != TYPE_VOID) {
			reg_t *retval = codegen_expr(node->lhs);
			reg_t *ext = reg_make();
			ir_inst_t *ins = ins_sxtl(ext, retval);
			ins->size = node->lhs->type->size;
			ir_blk_add(outblk, ins);
			emit_ret(ext);
		} else if(!node->lhs && rettype->kind == TYPE_VOID) {
			emit_ret(NULL);
		} else if(node->lhs && rettype->kind == TYPE_VOID) {
			compile_err(node->tok->loc, "function cannot return something");
		} else if(!node->lhs && rettype->kind != TYPE_VOID) {
			compile_err(node->tok->loc, "function has to return something");
		}
		break;
	}
	case NODE_BLOCK:
		for(node_t *nod = node->body; nod; nod = nod->next) {
			codegen_stmt(nod);
		}
		break;
	case NODE_DOWHILE: {
		ir_blk_t *then = emit_blk();
		ir_blk_t *resume = emit_blk();
		emit_jmp(then);
		outblk = then;
		codegen_stmt(node->then);
		reg_t *cond = codegen_expr(node->cond);
		emit_br(cond, then, resume);
		outblk = resume;
	}; break;
	case NODE_WHILE: {
		ir_blk_t *condchk = emit_blk();
		ir_blk_t *loop = emit_blk();
		ir_blk_t *resume = emit_blk();

		emit_jmp(condchk);

		outblk = condchk;
		reg_t *cond = codegen_expr(node->cond);
		emit_br(cond, loop, resume);

		outblk = loop;
		codegen_stmt(node->then);
		emit_jmp(condchk);
		outblk = resume;
	}; break;
	case NODE_FOR: {
		/* initializer */
		codegen_stmt(node->init);

		ir_blk_t *condchk = emit_blk(); /* check if need to go loop or resume */
		ir_blk_t *then = emit_blk();
		ir_blk_t *resume = emit_blk();

		emit_jmp(condchk);

		outblk = condchk;
		reg_t *cond;
		if(node->cond) {
			cond = codegen_expr(node->cond);
		} else {
			cond = reg_make();
			emit_imm(cond, 1);
		}

		emit_br(cond, then, resume);

		outblk = then;
		codegen_stmt(node->then);
		if(node->inc) {
			UNUSED(codegen_expr(node->inc));
		}

		emit_jmp(condchk);
		outblk = resume;

	} break;
	case NODE_IF: {
		reg_t *cond = codegen_expr(node->cond);

		ir_blk_t *then = emit_blk(), *elze = emit_blk();
		ir_blk_t *resume;

		if(node->elze == NULL) {
			resume = elze;
		} else {
			resume = emit_blk();
		}

		emit_br(cond, then, elze);
		outblk = then;
		codegen_stmt(node->then);
		emit_jmp(resume);
		if(node->elze) {
			outblk = elze;
			codegen_stmt(node->elze);
			emit_jmp(resume);
		}

		outblk = resume;

	}; break;
	default:
		compile_err_node(node, "invalid stmt");
		break;
	}

	return;
}

/* assign globals */
static UNUSEDA void assign_globals(LIST(obj_t *) globals)
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
	}
	return;
}

/* calculate stack frame space needed for function `fn`. returns maximum alignment */
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
		}
	}

	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			if(ins->type == IR_INST_LEAS) {
				obj_t *obj = (obj_t *)ins->r0->rhs;
				if(!obj || obj->addressed || !ins->r0->stack_loc) {
					/* can't optimize sorry */
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
	ir_prog_t prog = { 0 };
	prog.globs = list_make(ir_global_t *);
	prog.funcs = list_make(ir_func_t *);
	blk_num = 0;

	assign_globals(globals);

	for(size_t i = 0; i < list_len(globals); i++) {
		blk_num = 0;
		obj_t *cur_fn = globals[i];

		if(cur_fn && cur_fn->is_global) {
			list_append(prog.globs, cur_fn->glob);
			continue;
		}

		if(!cur_fn) {
			continue;
		}

		ENSURE(cur_fn->is_func, "tried to generate code for a variable");
		reg_reset_counter();
		ir_func_t *func = ir_func_make(cur_fn->name);
		fun = func;
		fun_obj = cur_fn;
		func->args = list_make(callreg_t *);
		type_propagate(cur_fn->body);

		ir_blk_t *blk = emit_blk();
		outblk = blk;

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

		varopt(func);

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
		fun->stack_needed = align_to(cur_fn->stack_size, 16);
		fun->align_needed = align_to(max_align, 16);

		list_append(prog.funcs, fun);
	}

	ir_prog_compile(f, &prog, backend, opt_level);
	for(size_t i = 0; i < list_len(prog.funcs); i++) {
		ir_func_delete(prog.funcs[i]);
	}
	for(size_t i = 0; i < list_len(prog.globs); i++) {
		ir_glob_delete(prog.globs[i]);
	}
	list_delete(prog.funcs);
	list_delete(prog.globs);
	return;
}
