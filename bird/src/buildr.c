#include "bird.h"
#include "ir.h"

/* make (initialize) a builder */
void ir_buildr_make(ir_buildr_t *build)
{
	build->prog.funcs = list_make(ir_func_t *);
	build->prog.globs = list_make(ir_global_t *);
	build->insert_blk = NULL;
	build->func = NULL;
	build->blknum = 0;
	return;
}

/* makes a new, empty function with name `fname`. sets the insert block
 * to the new empty block in the function */
ir_func_t *ir_buildr_make_func(ir_buildr_t *build, char *fname)
{
	ir_func_t *func = ir_func_make(fname);
	build->func = func;
	list_append(build->prog.funcs, func);
	ir_buildr_set_insert_blk(build, ir_buildr_make_blk(build));
	return func;
}

/* end a function */
void ir_buildr_end_func(ir_buildr_t *build)
{
	reg_reset_counter();
	build->blknum = 0;
	build->insert_blk = NULL;
	build->func = NULL;
	return;
}

/* sets the insert block to the block provided */
void ir_buildr_set_insert_blk(ir_buildr_t *build, ir_blk_t *blk)
{
	build->insert_blk = blk;
	return;
}

/* makes (emits) a new block. does not set the insert block */
ir_blk_t *ir_buildr_make_blk(ir_buildr_t *build)
{
	ir_blk_t *blk = ir_blk_make(NULL);
	blk->num = build->blknum++;
	list_append(list_peek(build->prog.funcs)->blocks, blk);
	return blk;
}

/* emits instruction at current insert block */
void ir_buildr_emit_ins(ir_buildr_t *build, ir_inst_t *ins)
{
	ir_blk_add(build->insert_blk, ins);
	return;
}

/* add a global to the program */
void ir_buildr_add_glob(ir_buildr_t *build, ir_global_t *glob)
{
	list_append(build->prog.globs, glob);
	return;
}

/* delete a builder */
void ir_buildr_delete(ir_buildr_t *build)
{
	for(size_t i = 0; i < list_len(build->prog.funcs); i++) {
		ir_func_delete(build->prog.funcs[i]);
	}

	for(size_t i = 0; i < list_len(build->prog.globs); i++) {
		ir_glob_delete(build->prog.globs[i]);
	}

	list_delete(build->prog.funcs);
	list_delete(build->prog.globs);

	return;
}

/* all the building instructions */

reg_t *ir_buildr_copy(ir_buildr_t *build, reg_t *reg)
{
	reg_t *mov = reg_make();
	ir_blk_add(build->insert_blk, ins_mov(mov, reg));
	return mov;
}

reg_t *ir_buildr_creat_imm64(ir_buildr_t *build, int64_t imm)
{
	return ir_buildr_creat_imm(build, false, imm);
}

reg_t *ir_buildr_creat_imm32(ir_buildr_t *build, int32_t imm)
{
	return ir_buildr_creat_imm(build, true, imm);
}

reg_t *ir_buildr_creat_imm(ir_buildr_t *build, bool is_32bit, int64_t imm)
{
	int64_t x = imm;
	if(is_32bit) {
		x = (int64_t)(*(int32_t *)(&imm));
	}
	reg_t *immr = reg_make();
	ir_inst_t *ins = ins_imm(immr, x);
	ins->is_32bit = is_32bit;
	ir_blk_add(build->insert_blk, ins);
	return immr;
}

#define GEN_BINOP(nam, op)                                                \
	reg_t *nam(ir_buildr_t *build, bool is_32bit, reg_t *lhs, reg_t *rhs) \
	{                                                                     \
		ir_inst_t *ins = ins_##op(reg_make(), lhs, rhs);                  \
		ins->is_32bit = is_32bit;                                         \
		ir_blk_add(build->insert_blk, ins);                               \
		return ins->r0;                                                   \
	}

#define GEN_UNARYOP(nam, op)                                  \
	reg_t *nam(ir_buildr_t *build, bool is_32bit, reg_t *lhs) \
	{                                                         \
		ir_inst_t *ins = ins_##op(reg_make(), lhs);           \
		ins->is_32bit = is_32bit;                             \
		ir_blk_add(build->insert_blk, ins);                   \
		return ins->r0;                                       \
	}

#define GEN_BINOP_SIGN(nam, op)                                             \
	reg_t *nam(ir_buildr_t *build, bool is_32bit, bool unsignd, reg_t *lhs, \
			   reg_t *rhs)                                                  \
	{                                                                       \
		return unsignd ? ir_buildr_creat_u##op(build, is_32bit, lhs, rhs) : \
						 ir_buildr_creat_s##op(build, is_32bit, lhs, rhs);  \
	}

GEN_BINOP(ir_buildr_creat_add, add);
GEN_BINOP(ir_buildr_creat_sub, sub);
GEN_BINOP(ir_buildr_creat_umul, umul);
GEN_BINOP(ir_buildr_creat_udiv, udiv);
GEN_BINOP(ir_buildr_creat_umod, umod);
GEN_BINOP(ir_buildr_creat_smul, smul);
GEN_BINOP(ir_buildr_creat_sdiv, sdiv);
GEN_BINOP(ir_buildr_creat_smod, smod);
GEN_BINOP(ir_buildr_creat_and, and);
GEN_BINOP(ir_buildr_creat_eor, eor);
GEN_BINOP(ir_buildr_creat_or, or);
GEN_BINOP(ir_buildr_creat_shl, shl);
GEN_BINOP(ir_buildr_creat_cmp_eq, eq);
GEN_BINOP(ir_buildr_creat_cmp_ne, ne);

static GEN_BINOP(ir_buildr_creat_ule, ule);
static GEN_BINOP(ir_buildr_creat_ult, ult);
static GEN_BINOP(ir_buildr_creat_uge, uge);
static GEN_BINOP(ir_buildr_creat_ugt, ugt);
static GEN_BINOP(ir_buildr_creat_sle, sle);
static GEN_BINOP(ir_buildr_creat_slt, slt);
static GEN_BINOP(ir_buildr_creat_sge, sge);
static GEN_BINOP(ir_buildr_creat_sgt, sgt);

GEN_UNARYOP(ir_buildr_creat_neg, neg);
GEN_UNARYOP(ir_buildr_creat_not, not);
GEN_UNARYOP(ir_buildr_creat_bool, mkbool);
GEN_UNARYOP(ir_buildr_creat_invbool, notbool);

GEN_BINOP_SIGN(ir_buildr_creat_mul, mul);
GEN_BINOP_SIGN(ir_buildr_creat_div, div);
GEN_BINOP_SIGN(ir_buildr_creat_mod, mod);

GEN_BINOP_SIGN(ir_buildr_creat_cmp_le, le);
GEN_BINOP_SIGN(ir_buildr_creat_cmp_lt, lt);
GEN_BINOP_SIGN(ir_buildr_creat_cmp_ge, ge);
GEN_BINOP_SIGN(ir_buildr_creat_cmp_gt, gt);

static GEN_BINOP(ushr, shr);
static GEN_BINOP(ashr, ashr);

reg_t *ir_buildr_creat_shr(ir_buildr_t *build, bool is_32bit, bool unsignd,
						   reg_t *lhs, reg_t *rhs)
{
	return unsignd ? ushr(build, is_32bit, lhs, rhs) :
					 ashr(build, is_32bit, lhs, rhs);
}

reg_t *ir_buildr_creat_leas(ir_buildr_t *build, int64_t off)
{
	ir_inst_t *ins = ins_leas(reg_make(), off);
	ir_blk_add(build->insert_blk, ins);
	return ins->r0;
}

reg_t *ir_buildr_creat_lea(ir_buildr_t *build, ir_global_t *glob)
{
	ir_inst_t *ins = ins_lea(reg_make(), glob);
	ir_blk_add(build->insert_blk, ins);
	return ins->r0;
}

reg_t *ir_buildr_creat_load(ir_buildr_t *build, bool ext, size_t size,
							reg_t *addr)
{
	ir_inst_t *load = ins_load(reg_make(), addr);
	load->sign_ext = ext;
	if(!ext) {
		load->is_32bit = size <= 4;
	}
	load->size = size;
	ir_blk_add(build->insert_blk, load);
	return load->r0;
}

void ir_buildr_creat_store(ir_buildr_t *build, size_t size, reg_t *addr,
						   reg_t *data)
{
	ir_inst_t *store = ins_store(addr, data);
	store->size = size;
	store->is_32bit = size <= 4;
	ir_blk_add(build->insert_blk, store);
	return;
}

reg_t *ir_buildr_creat_ext(ir_buildr_t *build, bool is_32bit, bool unsignd,
						   size_t size, reg_t *data)
{
	enum ins_type ty = unsignd ? IR_INST_ZXT : IR_INST_SXT;
	/* some optimization */
	if(is_32bit && size >= 4) {
		return ir_buildr_copy(build, data);
	}
	if(!is_32bit && size >= 8) {
		return ir_buildr_copy(build, data);
	}
	ir_inst_t *ext = ir_inst_make(ty, reg_make(), data, NULL, 0);
	ext->size = size;
	ext->is_32bit = is_32bit;
	ir_blk_add(build->insert_blk, ext);
	return ext->r0;
}

void ir_buildr_creat_jmp(ir_buildr_t *build, ir_blk_t *blk)
{
	ir_blk_add(build->insert_blk, ins_jmp(blk));
	return;
}

void ir_buildr_creat_br(ir_buildr_t *build, bool is_32bit, reg_t *cond,
						ir_blk_t *true_blk, ir_blk_t *false_blk)
{
	ir_inst_t *br = ins_br(cond, false_blk, true_blk);
	br->is_32bit = is_32bit;
	ir_blk_add(build->insert_blk, br);
	return;
}

void ir_buildr_creat_ret(ir_buildr_t *build, bool is_32bit, reg_t *data)
{
	ir_inst_t *ret = ins_ret(data);
	ret->is_32bit = is_32bit;
	ir_blk_add(build->insert_blk, ret);
	return;
}

reg_t *ir_buildr_creat_call(ir_buildr_t *build, char *fname,
							LIST(callreg_t *) args)
{
	ir_inst_t *call = ins_call(reg_make(), fname, args);
	/* TODO: proper calling support (in internal IR) */
	ir_blk_add(build->insert_blk, call);
	return call->r0;
}

void ir_buildr_creat_asm(ir_buildr_t *build, char *asm, size_t asmlen)
{
	LIST(callreg_t *) args = list_make(callreg_t *);
	ir_inst_t *call = ins_call(NULL, "__builtin_wcc_asm", args);
	call->is_asm = true;
	call->asmsrc = asm;
	call->asmlen = asmlen;
	ir_blk_add(build->insert_blk, call);
	return;
}

void ir_buildr_creat_jmp_set(ir_buildr_t *build, ir_blk_t *blk)
{
	ir_buildr_creat_jmp(build, blk);
	ir_buildr_set_insert_blk(build, blk);
	return;
}

void ir_buildr_creat_br_set(ir_buildr_t *build, bool is_32bit, reg_t *cond,
							ir_blk_t *true_blk, ir_blk_t *false_blk,
							ir_blk_t *set_to)
{
	ir_buildr_creat_br(build, is_32bit, cond, true_blk, false_blk);
	ir_buildr_set_insert_blk(build, set_to);
	return;
}
