#include "ir.h"
#include "bird.h"
#include "cfg.h"
#include "regalloc.h"
#include "zz/arena.h"
#include "zz/set.h"

extern int debug;

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

int ir_inst_is_term(enum ins_type type)
{
	return ir_inst_is_br(type) || type == IR_INST_JMP || type == IR_INST_RET ||
		   type == IR_INST_RETI;
}

int ir_inst_is_foldable(enum ins_type type)
{
	return ir_inst_is_assoc(type) || ir_inst_is_cmp(type) ||
		   type == IR_INST_SUB || type == IR_INST_UDIV ||
		   type == IR_INST_SDIV || type == IR_INST_UMOD ||
		   type == IR_INST_SMOD || type == IR_INST_SHL || type == IR_INST_SHR ||
		   type == IR_INST_ASHR || type == IR_INST_MOV || type == IR_INST_NEG ||
		   type == IR_INST_NOT || type == IR_INST_MKBOOL ||
		   type == IR_INST_NOTBOOL || type == IR_INST_ZXT ||
		   type == IR_INST_SXT || type == IR_INST_AND || type == IR_INST_EOR ||
		   type == IR_INST_OR;
}

static int ir_inst_is_br_cond(enum ins_type type)
{
	return type == IR_INST_BREQ || type == IR_INST_BRNE ||
		   type == IR_INST_BRSLT || type == IR_INST_BRSLE ||
		   type == IR_INST_BRSGT || type == IR_INST_BRSGE ||
		   type == IR_INST_BRULT || type == IR_INST_BRULE ||
		   type == IR_INST_BRUGT || type == IR_INST_BRUGE;
}

static int ir_inst_is_br_condimm(enum ins_type type)
{
	return type == IR_INST_BREQI || type == IR_INST_BRNEI ||
		   type == IR_INST_BRSLTI || type == IR_INST_BRSLEI ||
		   type == IR_INST_BRSGTI || type == IR_INST_BRSGEI ||
		   type == IR_INST_BRULTI || type == IR_INST_BRULEI ||
		   type == IR_INST_BRUGTI || type == IR_INST_BRUGEI;
}

/* is this instruction a branch? */
int ir_inst_is_br(enum ins_type type)
{
	return type == IR_INST_BR || ir_inst_is_br_cond(type) ||
		   ir_inst_is_br_condimm(type);
}

/* is this instruction a comparison? */
int ir_inst_is_cmp(enum ins_type type)
{
	return type == IR_INST_EQ || type == IR_INST_NE || type == IR_INST_SLE ||
		   type == IR_INST_SLT || type == IR_INST_SGT || type == IR_INST_SGE ||
		   type == IR_INST_ULE || type == IR_INST_ULT || type == IR_INST_UGT ||
		   type == IR_INST_UGE;
}

/* is this instruction associative/trivally associative? (F(B, C) == F(C, B)) */
int ir_inst_is_assoc(enum ins_type type)
{
	return type == IR_INST_ADD || type == IR_INST_UMUL ||
		   type == IR_INST_SMUL || type == IR_INST_AND || type == IR_INST_OR ||
		   type == IR_INST_EOR;
}

/* can the instruction be immediate folded? */
int ir_inst_can_fold_imm(enum ins_type type)
{
	return type == IR_INST_SHL || type == IR_INST_SHR || type == IR_INST_ASHR ||
		   type == IR_INST_ADD || type == IR_INST_SUB || type == IR_INST_OR ||
		   type == IR_INST_AND || type == IR_INST_EOR || ir_inst_is_cmp(type) ||
		   ir_inst_is_br_cond(type) || type == IR_INST_RET;
}

/* turn instruction type -> immediate instruction type */
int ir_inst_turn_imm(enum ins_type type)
{
#define CASE(x) \
	case x:     \
		return x##I
	switch(type) {
		CASE(IR_INST_ADD);
		CASE(IR_INST_SUB);
		CASE(IR_INST_AND);
		CASE(IR_INST_EOR);
		CASE(IR_INST_OR);
		CASE(IR_INST_SHL);
		CASE(IR_INST_SHR);
		CASE(IR_INST_ASHR);
		CASE(IR_INST_EQ);
		CASE(IR_INST_NE);
		CASE(IR_INST_SLT);
		CASE(IR_INST_SLE);
		CASE(IR_INST_SGT);
		CASE(IR_INST_SGE);
		CASE(IR_INST_ULT);
		CASE(IR_INST_ULE);
		CASE(IR_INST_UGT);
		CASE(IR_INST_UGE);
		CASE(IR_INST_BREQ);
		CASE(IR_INST_BRNE);
		CASE(IR_INST_BRSLT);
		CASE(IR_INST_BRSLE);
		CASE(IR_INST_BRSGT);
		CASE(IR_INST_BRSGE);
		CASE(IR_INST_BRULT);
		CASE(IR_INST_BRULE);
		CASE(IR_INST_BRUGT);
		CASE(IR_INST_BRUGE);
		CASE(IR_INST_RET);
	default:
		return type;
	}
	return type;
#undef CASE
}

/* reset register counter */
void reg_reset_counter(void)
{
	(void)runk(1);
	return;
}

/* make a (new) register */
reg_t *reg_make(void)
{
	reg_t *reg = scr_alloc(sizeof(reg_t));
	wipe(reg, sizeof(reg_t));

	reg->rr = -1;
	reg->spilld = false;
	reg->def = 0;
	reg->last_use = 0;
	reg->vr = runk(0);
	reg->insty = IR_INST_NOP;
	reg->lhs = reg->rhs = NULL;

	return reg;
}

/* make a (new) call register */
callreg_t *callreg_make(reg_t *reg, enum call_argtype class, size_t size)
{
	callreg_t *callreg = scr_alloc(sizeof(callreg_t));
	callreg->r = reg;
	callreg->size = size;
	callreg->type = class;

	return callreg;
}

/* delete a register */
void reg_delete(reg_t *reg)
{
	(void)reg;
	// free(reg);
	// handled by scratch allocator
	return;
}

/* make an IR instruction */
ir_inst_t *ir_inst_make(enum ins_type type, reg_t *r0, reg_t *r1, reg_t *r2,
						uint64_t imm)
{
	ir_inst_t *ins = zalloc(sizeof(ir_inst_t));
	ins->next = NULL;
	ins->type = type;
	ins->r0 = r0;
	ins->r1 = r1;
	ins->r2 = r2;
	ins->false_blk = NULL;
	ins->true_blk = NULL;
	ins->imm = imm;
	ins->noopt = false;
	ins->sign_ext = false;

	return ins;
}

#define MAKE(ty, r0, r1, r2, imm) \
	return ir_inst_make(IR_INST_##ty, r0, r1, r2, imm)

#define INSNAME(name) ins_##name

#define DEF_INS(name, name2, r0, r1, r2, imm, ...) \
	ir_inst_t *INSNAME(name)(__VA_ARGS__)          \
	{                                              \
		MAKE(name2, r0, r1, r2, imm);              \
	}

DEF_INS(nop, NOP, NULL, NULL, NULL, 0, void);
DEF_INS(mov, MOV, r0, r1, NULL, 0, reg_t *r0, reg_t *r1);
DEF_INS(imm, IMM, r0, NULL, NULL, imm, reg_t *r0, uint64_t imm);
DEF_INS(add, ADD, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(sub, SUB, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(smul, SMUL, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(sdiv, SDIV, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(smod, SMOD, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(udiv, UDIV, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(umul, UMUL, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(umod, UMOD, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(shl, SHL, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(shr, SHR, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(ashr, ASHR, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(and, AND, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(or, OR, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
DEF_INS(eor, EOR, r0, r1, r2, 0, reg_t *r0, reg_t *r1, reg_t *r2);
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
DEF_INS(mkbool, MKBOOL, r0, r1, NULL, 0, reg_t *r0, reg_t *r1);
DEF_INS(notbool, NOTBOOL, r0, r1, NULL, 0, reg_t *r0, reg_t *r1);
DEF_INS(leas, LEAS, r0, NULL, NULL, imm, reg_t *r0, long imm);
DEF_INS(ret, RET, NULL, r1, NULL, 0, reg_t *r1);

#undef DEF_INS
#undef INSNAME
#undef MAKE

/* the odd one(s) out */

#define GEN_LOAD(c, name, s)                               \
	ir_inst_t *ins_##name(reg_t *r0, reg_t *r1)            \
	{                                                      \
		ir_inst_t *ins = ir_inst_make(c, r0, r1, NULL, 0); \
		ins->size = s;                                     \
		return ins;                                        \
	}

GEN_LOAD(IR_INST_LOAD, loadb, 1);
GEN_LOAD(IR_INST_LOAD, loadw, 2);
GEN_LOAD(IR_INST_LOAD, loadl, 4);
GEN_LOAD(IR_INST_LOAD, load, 8);

#undef GEN_LOAD

#define GEN_STORE(c, name, s)                              \
	ir_inst_t *ins_##name(reg_t *r1, reg_t *r2)            \
	{                                                      \
		ir_inst_t *ins = ir_inst_make(c, NULL, r1, r2, 0); \
		ins->size = s;                                     \
		return ins;                                        \
	}

GEN_STORE(IR_INST_STORE, storeb, 1);
GEN_STORE(IR_INST_STORE, storew, 2);
GEN_STORE(IR_INST_STORE, storel, 4);
GEN_STORE(IR_INST_STORE, store, 8);
#undef GEN_STORE

#define GEN_LOADS(c, name, s)                                  \
	ir_inst_t *ins_##name(reg_t *r0, long imm)                 \
	{                                                          \
		ir_inst_t *ins = ir_inst_make(c, r0, NULL, NULL, imm); \
		ins->size = s;                                         \
		return ins;                                            \
	}

GEN_LOADS(IR_INST_LOADS, loadsb, 1);
GEN_LOADS(IR_INST_LOADS, loadsw, 2);
GEN_LOADS(IR_INST_LOADS, loadsl, 4);
GEN_LOADS(IR_INST_LOADS, loads, 8);

#undef GEN_LOADS

#define GEN_STORES(c, name, s)                                 \
	ir_inst_t *ins_##name(reg_t *r1, long imm)                 \
	{                                                          \
		ir_inst_t *ins = ir_inst_make(c, NULL, r1, NULL, imm); \
		ins->size = s;                                         \
		return ins;                                            \
	}

GEN_STORES(IR_INST_STORES, storesb, 1);
GEN_STORES(IR_INST_STORES, storesw, 2);
GEN_STORES(IR_INST_STORES, storesl, 4);
GEN_STORES(IR_INST_STORES, stores, 8);

#undef GEN_STORES

#define GEN_BRCMP(c, name)                                                  \
	ir_inst_t *ins_##name(reg_t *r1, reg_t *r2, ir_blk_t *fb, ir_blk_t *tb) \
	{                                                                       \
		ir_inst_t *ins = ir_inst_make(c, NULL, r1, r2, 0);                  \
		ins->false_blk = fb;                                                \
		ins->true_blk = tb;                                                 \
		return ins;                                                         \
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

#undef GEN_BRCMP

#define GEN_EXT(c, name, s)                                 \
	ir_inst_t *ins_##name(reg_t *r0, reg_t *r1)             \
	{                                                       \
		ir_inst_t *inst = ir_inst_make(c, r0, r1, NULL, 0); \
		inst->size = s;                                     \
		return inst;                                        \
	}

GEN_EXT(IR_INST_ZXT, zxtb, 1);
GEN_EXT(IR_INST_SXT, sxtb, 1);
GEN_EXT(IR_INST_ZXT, zxtw, 2);
GEN_EXT(IR_INST_SXT, sxtw, 2);
GEN_EXT(IR_INST_ZXT, zxtl, 4);
GEN_EXT(IR_INST_SXT, sxtl, 4);

#undef GEN_EXT

ir_inst_t *ins_br(reg_t *on, ir_blk_t *falseb, ir_blk_t *trueb)
{
	ir_inst_t *ins = ir_inst_make(IR_INST_BR, NULL, on, NULL, 0);
	ins->false_blk = falseb;
	ins->true_blk = trueb;
	return ins;
}

ir_inst_t *ins_call(reg_t *res, char *fname, LIST(callreg_t *) args)
{
	ir_inst_t *ins = ir_inst_make(IR_INST_CALL, res, NULL, NULL, 0);
	ins->fname = fname;
	ins->call_args = args;
	return ins;
}

ir_inst_t *ins_jmp(ir_blk_t *blk)
{
	ir_inst_t *ins = ir_inst_make(IR_INST_JMP, NULL, NULL, NULL, 0);
	ins->true_blk = blk;
	return ins;
}

ir_inst_t *ins_lea(reg_t *res, ir_global_t *glob)
{
	ir_inst_t *ins = ir_inst_make(IR_INST_LEA, res, 0, 0, 0);
	ins->label = glob;
	return ins;
}

/* delete an IR instruction */
void ir_inst_delete(ir_inst_t *ins)
{
	if(ins->type == IR_INST_CALL) {
		list_delete(ins->call_args);
	}
	if(ins->type == IR_INST_PHI) {
		list_delete(ins->phi_args);
		list_delete(ins->phi_preds);
	}
	free(ins);
	return;
}

/* make an IR block */
ir_blk_t *ir_blk_make(ir_inst_t *insts)
{
	ir_blk_t *blk = zalloc(sizeof(ir_blk_t));

	blk->insts = insts;
	blk->visited = false;
	blk->pred = list_make(ir_blk_t *);
	blk->succ = list_make(ir_blk_t *);
	blk->incomplete_phis = list_make(ir_inst_t *);
	blk->dom = list_make(ir_blk_t *);
	blk->regs_def = set_empty();
	blk->regs_in = set_empty();
	blk->regs_out = set_empty();
	blk->tail = NULL;
	return blk;
}

/* add instruction `inst` to block `blk` */
void ir_blk_add(ir_blk_t *blk, ir_inst_t *inst)
{
	if(!blk->insts) {
		blk->tail = inst;
		blk->insts = inst;
		return;
	}
	blk->tail->next = inst;
	blk->tail = inst;
	return;
}

/* delete an IR block */
void ir_blk_delete(ir_blk_t *blk)
{
	list_delete(blk->pred);
	list_delete(blk->succ);
	list_delete(blk->incomplete_phis);
	list_delete(blk->dom);
	set_delete(blk->regs_def);
	set_delete(blk->regs_in);
	set_delete(blk->regs_out);
	free(blk);
	return;
}

/* delete an IR block and instructions */
void ir_blk_delete_all(ir_blk_t *blk)
{
	ir_inst_t *nxt = NULL;
	for(ir_inst_t *head = blk->insts; head; head = nxt) {
		nxt = head->next;
		ir_inst_delete(head);
	}
	ir_blk_delete(blk);
	return;
}

/* make a global variable */
ir_global_t *ir_glob_make(char *name, size_t size, size_t align, uint8_t *data)
{
	ir_global_t *glob = zalloc(sizeof(ir_global_t));
	glob->name = name;
	glob->size = size;
	glob->align = align;
	glob->data = data;
	if(data) {
		glob->has_data = true;
	}

	return glob;
}

/* delete a global variable */
void ir_glob_delete(ir_global_t *glob)
{
	free(glob);
	return;
}

/* make an IR function */
ir_func_t *ir_func_make(char *name)
{
	ir_func_t *fn = zalloc(sizeof(ir_func_t));

	fn->name = name;
	fn->blocks = list_make(ir_blk_t *);

	return fn;
}

/* delete an IR function (aka all blocks, extras) */
void ir_func_delete(ir_func_t *fun)
{
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		ir_blk_delete_all(blk);
	}
	list_delete(fun->blocks);
	list_delete(fun->args);
	free(fun->alloc_used);
	free(fun);
	return;
}

static void ir_fix_ins(ir_inst_t *ins)
{
	reg_t *r0 = ins->r0;
	reg_t *r1 = ins->r1;
	reg_t *r2 = ins->r2;
#define FIX(t, R0, R1, R2) \
	case IR_INST_##t:      \
		r0 = R0;           \
		r1 = R1;           \
		r2 = R2;           \
		break
#define xx NULL
	switch(ins->type) {
		FIX(NOP, xx, xx, xx);
		FIX(MOV, r0, r1, xx);
		FIX(IMM, r0, xx, xx);
		FIX(NEG, r0, r1, xx);
		FIX(NOT, r0, r1, xx);
		FIX(MKBOOL, r0, r1, xx);
		FIX(NOTBOOL, r0, r1, xx);
		FIX(BREQ, xx, r1, r2);
		FIX(BRNE, xx, r1, r2);
		FIX(BRSLT, xx, r1, r2);
		FIX(BRSLE, xx, r1, r2);
		FIX(BRSGT, xx, r1, r2);
		FIX(BRSGE, xx, r1, r2);
		FIX(BRULT, xx, r1, r2);
		FIX(BRULE, xx, r1, r2);
		FIX(BRUGT, xx, r1, r2);
		FIX(BRUGE, xx, r1, r2);
		FIX(ZXT, r0, r1, xx);
		FIX(SXT, r0, r1, xx);
		FIX(LOAD, r0, r1, xx);
		FIX(STORE, xx, r1, r2);
		FIX(LEAS, r0, xx, xx);
		FIX(LEA, r0, xx, xx);
		FIX(LOADS, r0, xx, xx);
		FIX(STORES, xx, r1, xx);
		FIX(LOADSS, r0, xx, xx);
		FIX(STORESS, xx, r1, xx);
		FIX(BR, xx, r1, xx);
		FIX(JMP, xx, xx, xx);
		FIX(RET, xx, r1, xx);
		FIX(RETI, xx, xx, xx);
		FIX(CALL, r0, xx, xx);
		FIX(ANDI, r0, r1, xx);
		FIX(ORI, r0, r1, xx);
		FIX(EORI, r0, r1, xx);
		FIX(ADDI, r0, r1, xx);
		FIX(SUBI, r0, r1, xx);
		FIX(SHLI, r0, r1, xx);
		FIX(SHRI, r0, r1, xx);
		FIX(ASHRI, r0, r1, xx);
		FIX(BREQI, xx, r1, xx);
		FIX(BRNEI, xx, r1, xx);
		FIX(BRSLTI, xx, r1, xx);
		FIX(BRSLEI, xx, r1, xx);
		FIX(BRSGTI, xx, r1, xx);
		FIX(BRSGEI, xx, r1, xx);
		FIX(BRULTI, xx, r1, xx);
		FIX(BRULEI, xx, r1, xx);
		FIX(BRUGTI, xx, r1, xx);
		FIX(BRUGEI, xx, r1, xx);
		FIX(EQI, r0, r1, xx);
		FIX(NEI, r0, r1, xx);
		FIX(SLTI, r0, r1, xx);
		FIX(SLEI, r0, r1, xx);
		FIX(SGTI, r0, r1, xx);
		FIX(SGEI, r0, r1, xx);
		FIX(ULTI, r0, r1, xx);
		FIX(ULEI, r0, r1, xx);
		FIX(UGTI, r0, r1, xx);
		FIX(UGEI, r0, r1, xx);
	default:
		break;
	}
#undef FIX
#undef xx

	ins->r0 = r0;
	ins->r1 = r1;
	ins->r2 = r2;

	ir_blk_t *falseblk = NULL;
	ir_blk_t *trueblk = NULL;

	if(ir_inst_is_br(ins->type)) {
		falseblk = ins->false_blk;
		trueblk = ins->true_blk;
	}
	if(ins->type == IR_INST_JMP) {
		trueblk = ins->true_blk;
	}

	if(ins->type == IR_INST_IMM || ins->type == IR_INST_MOV) {
		ins->sign_ext = false;
		ins->size = ins->is_32bit ? 4 : 8;
	}

	ins->false_blk = falseblk;
	ins->true_blk = trueblk;

	if((ins->type == IR_INST_LOAD || ins->type == IR_INST_LOADS ||
		ins->type == IR_INST_LOADSS) &&
	   ins->is_32bit && ins->size == 8) {
		ins->size = 4;
	}

	if((ins->type == IR_INST_ZXT || ins->type == IR_INST_SXT) &&
	   ins->is_32bit && ins->size == 8) {
		ins->size = 4;
		ins->type = IR_INST_ZXT;
	}

	if((ins->type == IR_INST_ZXT || ins->type == IR_INST_SXT) &&
	   !ins->is_32bit && ins->size == 8) {
		ins->type = IR_INST_MOV;
	}
}

static ir_inst_t *find_last_or_flow_ins(ir_inst_t *root)
{
	ir_inst_t *ret = root;
	for(; ret; ret = ret->next) {
		if(ir_inst_is_term(ret->type)) {
			return ret;
		}
	}
	ASSERT(ir_inst_is_term(ret->type), "IR is not constructed properly");
	return NULL;
}

static void po_visit(ir_func_t *fun, ir_blk_t *blk)
{
	if(!blk || blk->visited) {
		return;
	}

	blk->visited = true;
	po_visit(fun, blk->tail->true_blk);
	po_visit(fun, blk->tail->false_blk);
	fun->blocks[fun->rpo_indx]->postnum = fun->rpo_indx;
	fun->rpo_indx++;
	return;
}

/* computes reverse postorder of block */
void ir_blk_rpo(ir_func_t *fun)
{
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		fun->blocks[i]->visited = false;
		fun->blocks[i]->tail = find_last_or_flow_ins(fun->blocks[i]->insts);
	}
	/* compute postorder */
	fun->rpo_indx = 0;
	po_visit(fun, fun->blocks[0]);

	/* reverse it */
	size_t l = list_len(fun->blocks) - 1;
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		size_t r = l - i;
		if(i == r)
			continue;
		long tmp = fun->blocks[i]->postnum;
		fun->blocks[i]->postnum = fun->blocks[r]->postnum;
		fun->blocks[r]->postnum = tmp;
	}

	return;
}

/* fixes IR function */
void ir_fix(ir_func_t *func)
{
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			ir_fix_ins(ins);
		}

		if(!blk->insts) {
			blk->insts = ins_ret(NULL);
		}
	}
	return;
}

/* -- union find mechanism -- */
/* dst = src; */
void ir_union(reg_t *dst, reg_t *src)
{
	if(!dst) {
		return;
	}
	ir_find(dst)->uf = src;
	return;
}

/* return (val of src); */
reg_t *ir_find(reg_t *src)
{
	if(!src) {
		return src;
	}
	reg_t *cur = src;
	while(cur->uf) {
		cur = cur->uf;
		if(cur == src || cur == cur->uf) {
			break;
		}
	}
	return cur;
}

#define REPLACE(x)              \
	do {                        \
		if((x)) {               \
			(x) = ir_find((x)); \
		}                       \
	} while(0)

/* rewrites whole function via ir_uf_find */
void ir_rewrite(ir_func_t *fun)
{
#define APPLY                                                           \
	for(size_t i = 0; i < list_len(fun->blocks); i++) {                 \
		ir_blk_t *blk = fun->blocks[i];                                 \
		for(ir_inst_t *inst = blk->insts; inst; inst = inst->next) {    \
			REPLACE(inst->r0);                                          \
			REPLACE(inst->r1);                                          \
			REPLACE(inst->r2);                                          \
			if(inst->type == IR_INST_CALL) {                            \
				for(size_t j = 0; j < list_len(inst->call_args); j++) { \
					REPLACE(inst->call_args[j]->r);                     \
				}                                                       \
			}                                                           \
			if(inst->type == IR_INST_PHI) {                             \
				for(size_t j = 0; j < list_len(inst->phi_args); j++) {  \
					REPLACE(inst->phi_args[j]);                         \
				}                                                       \
			}                                                           \
			if(inst->type == IR_INST_PMOV) {                            \
				for(size_t j = 0; j < list_len(inst->pmov_args); j++) { \
					REPLACE(inst->pmov_args[j].dst);                    \
					REPLACE(inst->pmov_args[j].src);                    \
				}                                                       \
			}                                                           \
		}                                                               \
	}

	/* rewrite function */
	APPLY;

#undef REPLACE
#define REPLACE(x)          \
	do {                    \
		if((x)) {           \
			(x)->uf = NULL; \
		}                   \
	} while(0)

	/* clear union find */
	APPLY;

#undef APPLY
#undef REPLACE
	return;
}

static void print_phiarg(ir_inst_t *phi, int indx, int mode)
{
	reg_t *r = phi->phi_args[indx];
	long rn;
	if(mode == 'v') {
		rn = r ? r->vr : -1;
	} else {
		rn = r ? r->rr : -1;
	}

	printf("[BB%ld, r%ld]", phi->phi_preds[indx]->num, rn);
	return;
}

/* print IR instruction */
void ir_print_inst(ir_inst_t *ins, int mode)
{
#define out(...)         \
	printf(__VA_ARGS__); \
	break;

	long r0, r1, r2;

	if(mode == 'r') {
		r0 = ins->r0 ? (long)ins->r0->rr : -1;
		r1 = ins->r1 ? (long)ins->r1->rr : -1;
		r2 = ins->r2 ? (long)ins->r2->rr : -1;
	} else if(mode == 'v') {
		r0 = ins->r0 ? (long)ins->r0->vr : -1;
		r1 = ins->r1 ? (long)ins->r1->vr : -1;
		r2 = ins->r2 ? (long)ins->r2->vr : -1;
	}
	uint64_t imm = ins->imm;
	char *suf =
		(char *[]){ "", ".i8", ".i16", "", ".i32", "", "", "", "" }[ins->size];
	char *ext = ins->sign_ext ? ".x" : "";

	switch(ins->type) {
	case IR_INST_NOP:
		out("nop");
	case IR_INST_MOV:
		out("r%ld = r%ld", r0, r1);
	case IR_INST_IMM:
		out("r%ld = #%lld", r0, imm);
	case IR_INST_ADD:
		out("r%ld = add r%ld, r%ld", r0, r1, r2);
	case IR_INST_SUB:
		out("r%ld = sub r%ld, r%ld", r0, r1, r2);
	case IR_INST_ADDI:
		out("r%ld = add r%ld, #%lld", r0, r1, imm);
	case IR_INST_SUBI:
		out("r%ld = sub r%ld, #%lld", r0, r1, imm);
	case IR_INST_SHL:
		out("r%ld = shl r%ld, r%ld", r0, r1, r2);
	case IR_INST_SHR:
		out("r%ld = shr r%ld, r%ld", r0, r1, r2);
	case IR_INST_ASHR:
		out("r%ld = ashr r%ld, r%ld", r0, r1, r2);
	case IR_INST_ROL:
		out("r%ld = rol r%ld, r%ld", r0, r1, r2);
	case IR_INST_ROR:
		out("r%ld = ror r%ld, r%ld", r0, r1, r2);
	case IR_INST_SHLI:
		out("r%ld = shl r%ld, #%lld", r0, r1, imm);
	case IR_INST_SHRI:
		out("r%ld = shr r%ld, #%lld", r0, r1, imm);
	case IR_INST_ASHRI:
		out("r%ld = ashr r%ld, #%lld", r0, r1, imm);
	case IR_INST_AND:
		out("r%ld = and r%ld, r%ld", r0, r1, r2);
	case IR_INST_OR:
		out("r%ld = or r%ld, r%ld", r0, r1, r2);
	case IR_INST_EOR:
		out("r%ld = eor r%ld, r%ld", r0, r1, r2);
	case IR_INST_ANDI:
		out("r%ld = and r%ld, %lld", r0, r1, imm);
	case IR_INST_ORI:
		out("r%ld = or r%ld, %lld", r0, r1, imm);
	case IR_INST_EORI:
		out("r%ld = eor r%ld, %lld", r0, r1, imm);
	case IR_INST_SMUL:
		out("r%ld = smul r%ld, r%ld", r0, r1, r2);
	case IR_INST_SDIV:
		out("r%ld = sdiv r%ld, r%ld", r0, r1, r2);
	case IR_INST_SMOD:
		out("r%ld = smod r%ld, r%ld", r0, r1, r2);
	case IR_INST_UMUL:
		out("r%ld = umul r%ld, r%ld", r0, r1, r2);
	case IR_INST_UDIV:
		out("r%ld = udiv r%ld, r%ld", r0, r1, r2);
	case IR_INST_UMOD:
		out("r%ld = umod r%ld, r%ld", r0, r1, r2);
	case IR_INST_NEG:
		out("r%ld = neg r%ld", r0, r1);
	case IR_INST_NOT:
		out("r%ld = not r%ld", r0, r1);
	case IR_INST_MKBOOL:
		out("r%ld = mkbool r%ld", r0, r1);
	case IR_INST_NOTBOOL:
		out("r%ld = notbool r%ld", r0, r1);
	case IR_INST_EQ:
		out("r%ld = cmp.eq r%ld, r%ld", r0, r1, r2);
	case IR_INST_NE:
		out("r%ld = cmp.ne r%ld, r%ld", r0, r1, r2);
	case IR_INST_SLT:
		out("r%ld = cmp.slt r%ld, r%ld", r0, r1, r2);
	case IR_INST_SLE:
		out("r%ld = cmp.sle r%ld, r%ld", r0, r1, r2);
	case IR_INST_SGT:
		out("r%ld = cmp.sgt r%ld, r%ld", r0, r1, r2);
	case IR_INST_SGE:
		out("r%ld = cmp.sge r%ld, r%ld", r0, r1, r2);
	case IR_INST_ULT:
		out("r%ld = cmp.ult r%ld, r%ld", r0, r1, r2);
	case IR_INST_ULE:
		out("r%ld = cmp.ule r%ld, r%ld", r0, r1, r2);
	case IR_INST_UGT:
		out("r%ld = cmp.ugt r%ld, r%ld", r0, r1, r2);
	case IR_INST_UGE:
		out("r%ld = cmp.uge r%ld, r%ld", r0, r1, r2);
	case IR_INST_EQI:
		out("r%ld = cmp.eq r%ld, #%lld", r0, r1, imm);
	case IR_INST_NEI:
		out("r%ld = cmp.ne r%ld, #%lld", r0, r1, imm);
	case IR_INST_SLTI:
		out("r%ld = cmp.slt r%ld, #%lld", r0, r1, imm);
	case IR_INST_SLEI:
		out("r%ld = cmp.sle r%ld, #%lld", r0, r1, imm);
	case IR_INST_SGTI:
		out("r%ld = cmp.sgt r%ld, #%lld", r0, r1, imm);
	case IR_INST_SGEI:
		out("r%ld = cmp.sge r%ld, #%lld", r0, r1, imm);
	case IR_INST_ULTI:
		out("r%ld = cmp.ult r%ld, #%lld", r0, r1, imm);
	case IR_INST_ULEI:
		out("r%ld = cmp.ule r%ld, #%lld", r0, r1, imm);
	case IR_INST_UGTI:
		out("r%ld = cmp.ugt r%ld, #%lld", r0, r1, imm);
	case IR_INST_UGEI:
		out("r%ld = cmp.uge r%ld, #%lld", r0, r1, imm);
	case IR_INST_LOAD:
		out("r%ld = load%s%s r%ld", r0, suf, ext, r1);
	case IR_INST_STORE:
		out("store%s r%ld, r%ld", suf, r1, r2);
	case IR_INST_LOADS:
		out("r%ld = loads%s%s #%ld", r0, suf, ext, (long)imm);
	case IR_INST_LOADSS:
		out("r%ld = spill_load%s%s #%ld", r0, suf, ext, (long)imm);
	case IR_INST_STORES:
		out("stores%s #%ld, r%ld", suf, (long)imm, r1);
	case IR_INST_STORESS:
		out("spill_store%s #%ld, r%ld", suf, (long)imm, r1);
	case IR_INST_ZXT:
		out("r%ld = zero_ext%s r%ld", r0, suf, r1);
	case IR_INST_SXT:
		out("r%ld = sign_ext%s r%ld", r0, suf, r1);
	case IR_INST_BR:
		out("br r%ld, BB%ld, BB%ld", r1, ins->true_blk->num,
			ins->false_blk->num);
	case IR_INST_CALL: {
		if(ins->r0) {
			printf("r%ld = ", r0);
		}
		printf("call %s", ins->fname);
		for(size_t i = 0; i < list_len(ins->call_args); i++) {
			reg_t *r = ins->call_args[i]->r;
			long n = mode == 'r' ? r->rr : r->vr;
			printf(", r%ld", n);
		}
	}; break;
	case IR_INST_PHI: {
		printf("r%ld = phi ", r0);
		size_t count = list_len(ins->phi_args);
		if(count >= 1) {
			print_phiarg(ins, 0, mode);
		}

		size_t i = 1;
		while(i < count) {
			printf(", ");
			print_phiarg(ins, i++, mode);
		}
	}; break;
	case IR_INST_PMOV: {
		printf("{\n");
		for(size_t i = 0; i < list_len(ins->pmov_args); i++) {
			reg_pmov_t mov = ins->pmov_args[i];
			printf("\tr%ld = r%ld\n", mov.dst->vr, mov.src->vr);
		}
		printf("\t}");
	}; break;
	case IR_INST_BREQ:
		out("br.eq r%ld, r%ld, BB%ld, BB%ld", r1, r2, ins->true_blk->num,
			ins->false_blk->num);
	case IR_INST_BRNE:
		out("br.ne r%ld, r%ld, BB%ld, BB%ld", r1, r2, ins->true_blk->num,
			ins->false_blk->num);
	case IR_INST_BRSLT:
		out("br.slt r%ld, r%ld, BB%ld, BB%ld", r1, r2, ins->true_blk->num,
			ins->false_blk->num);
	case IR_INST_BRSLE:
		out("br.sle r%ld, r%ld, BB%ld, BB%ld", r1, r2, ins->true_blk->num,
			ins->false_blk->num);
	case IR_INST_BRSGT:
		out("br.sgt r%ld, r%ld, BB%ld, BB%ld", r1, r2, ins->true_blk->num,
			ins->false_blk->num);
	case IR_INST_BRSGE:
		out("br.sge r%ld, r%ld, BB%ld, BB%ld", r1, r2, ins->true_blk->num,
			ins->false_blk->num);
	case IR_INST_BRULT:
		out("br.ult r%ld, r%ld, BB%ld, BB%ld", r1, r2, ins->true_blk->num,
			ins->false_blk->num);
	case IR_INST_BRULE:
		out("br.ule r%ld, r%ld, BB%ld, BB%ld", r1, r2, ins->true_blk->num,
			ins->false_blk->num);
	case IR_INST_BRUGT:
		out("br.ugt r%ld, r%ld, BB%ld, BB%ld", r1, r2, ins->true_blk->num,
			ins->false_blk->num);
	case IR_INST_BRUGE:
		out("br.uge r%ld, r%ld, BB%ld, BB%ld", r1, r2, ins->true_blk->num,
			ins->false_blk->num);
	case IR_INST_BREQI:
		out("br.eq r%ld, #%lld, BB%ld, BB%ld", r1, imm, ins->true_blk->num,
			ins->false_blk->num);
	case IR_INST_BRNEI:
		out("br.ne r%ld, #%lld, BB%ld, BB%ld", r1, imm, ins->true_blk->num,
			ins->false_blk->num);
	case IR_INST_BRSLTI:
		out("br.slt r%ld, #%lld, BB%ld, BB%ld", r1, imm, ins->true_blk->num,
			ins->false_blk->num);
	case IR_INST_BRSLEI:
		out("br.sle r%ld, #%lld, BB%ld, BB%ld", r1, imm, ins->true_blk->num,
			ins->false_blk->num);
	case IR_INST_BRSGTI:
		out("br.sgt r%ld, #%lld, BB%ld, BB%ld", r1, imm, ins->true_blk->num,
			ins->false_blk->num);
	case IR_INST_BRSGEI:
		out("br.sge r%ld, #%lld, BB%ld, BB%ld", r1, imm, ins->true_blk->num,
			ins->false_blk->num);
	case IR_INST_BRULTI:
		out("br.ult r%ld, #%lld, BB%ld, BB%ld", r1, imm, ins->true_blk->num,
			ins->false_blk->num);
	case IR_INST_BRULEI:
		out("br.ule r%ld, #%lld, BB%ld, BB%ld", r1, imm, ins->true_blk->num,
			ins->false_blk->num);
	case IR_INST_BRUGTI:
		out("br.ugt r%ld, #%lld, BB%ld, BB%ld", r1, imm, ins->true_blk->num,
			ins->false_blk->num);
	case IR_INST_BRUGEI:
		out("br.uge r%ld, #%lld, BB%ld, BB%ld", r1, imm, ins->true_blk->num,
			ins->false_blk->num);
	case IR_INST_RET: {
		if(ins->r1) {
			out("ret r%ld", r1);
		} else {
			out("ret");
		}
	}
	case IR_INST_RETI:
		out("ret #%lld", imm);

	case IR_INST_LEAS:
		out("r%ld = leas #%ld", r0, (long)imm);
	case IR_INST_LEA:
		out("r%ld = lea %s", r0, ins->label->name);
	case IR_INST_JMP:
		out("jmp BB%ld", ins->true_blk->num);
	default:
		out("????");
	}

	// if(ins->r0) {
	// 	printf("\n\tlive [%ld, %ld)", ins->r0->def, ins->r0->last_use);
	// }

	return;
#undef imm
#undef out
#undef r0
#undef r1
#undef r2
}
static void print_reglist(SET(reg_t *) list)
{
	reg_t *reg;
	set_iter(list, reg, { printf("r%ld, ", reg->vr); });
	printf("\n");
}

static void print_blklist(LIST(ir_blk_t *) list)
{
	for(size_t i = 0; i < list_len(list); i++) {
		ir_blk_t *blk = list[i];
		printf("BB%ld, ", blk->num);
	}
	printf("\n");
}

static UNUSEDA void ir_dump_stats(ir_func_t *fun)
{
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		printf("BB%ld:\n", blk->num);

		printf("\tpreds = ");
		print_blklist(blk->pred);

		printf("\tdomt = ");
		print_blklist(blk->dom);

		printf("\tsuccs = ");
		if(blk->tail->false_blk) {
			printf("BB%ld, ", blk->tail->false_blk->num);
		}
		if(blk->tail->true_blk) {
			printf("BB%ld, ", blk->tail->true_blk->num);
		}
		printf("\n");

		printf("\tdef = ");
		print_reglist(blk->regs_def);
		printf("\tin = ");
		print_reglist(blk->regs_in);
		printf("\tout = ");
		print_reglist(blk->regs_out);
	}
}

/* dump IR */
void ir_dump(ir_func_t *fun, int mode)
{
	// ir_dump_stats(fun);
	printf("func %s(", fun->name);

	for(size_t i = 0; i < list_len(fun->args); i++) {
		callreg_t *arg = fun->args[i];
		printf("i%zu #%ld, ", arg->size * 8, arg->r->off);
	}
	printf("%s) stack:%ld\n", list_len(fun->args) ? "" : "void",
		   fun->stack_needed);

	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		printf("BB%ld: ; preds = ", blk->num);
		for(size_t j = 0; j < list_len(blk->pred); j++) {
			printf("BB%ld, ", blk->pred[j]->num);
		}
		printf("nest = %llu\n", blk->loop_nest);

		for(ir_inst_t *inst = blk->insts; inst; inst = inst->next) {
			int num = inst->is_32bit ? 32 : 64;
			printf("(i%d)\t", num);
			ir_print_inst(inst, mode);
			putchar('\n');
		}
	}

	printf("}\n");
	return;
}

/* removes nops */
void ir_nopremover(ir_func_t *fun)
{
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];

		ir_inst_t *nop = ir_inst_make(IR_INST_NOP, NULL, NULL, NULL, 0);
		nop->next = blk->insts;
		blk->insts = nop;

		ir_inst_t *prev = nop;
		ir_inst_t *nxt = blk->insts->next;
		for(ir_inst_t *ins = blk->insts->next; ins; ins = nxt) {
			nxt = ins->next;

			if(ins->type == IR_INST_NOP) {
				prev->next = nxt;
				ir_inst_delete(ins);
			} else {
				prev = ins;
			}
		}

		blk->insts = blk->insts->next;
		ir_inst_delete(nop);
	}
}

/* codegen an IR function */
/* assumes it has been finalized */
void ir_func_emit(FILE *f, ir_func_t *fun, enum ir_arch arch)
{
	switch(arch) {
	case IR_ARCH_AARCH64_APPLE:
		ir_func_emit_aarch64_apple(f, fun);
		break;
	case IR_ARCH_X64_SYSV:
		ir_func_emit_x64_sysv(f, fun);
		break;
	default:
		break;
	}
}

static UNUSEDA void ir_print_graph(ir_prog_t *prog)
{
	printf("CFG:\n");
	printf("digraph {\n");
	for(size_t i = 0; i < list_len(prog->funcs); i++) {
		ir_func_t *func = prog->funcs[i];
		ir_fix(func);
		printf("//%s\n", func->name);
		for(size_t j = 0; j < list_len(func->blocks); j++) {
			ir_blk_t *blk = func->blocks[j];

			blk->tail = find_last_or_flow_ins(blk->insts);
			if(blk->tail->type == IR_INST_RET ||
			   blk->tail->type == IR_INST_RETI) {
				printf("\tBB%zu -> Ret_%s\n", blk->num, func->name);
			} else if(blk->tail->type == IR_INST_JMP) {
				printf("\tBB%zu -> BB%zu\n", blk->num,
					   blk->tail->true_blk->num);
			} else if(ir_inst_is_br(blk->tail->type)) {
				printf("\tBB%zu -> BB%zu\n", blk->num,
					   blk->tail->true_blk->num);
				printf("\tBB%zu -> BB%zu\n", blk->num,
					   blk->tail->false_blk->num);
			}
		}
	}
	printf("}\n");
	printf("Dom-tree:\n");
	printf("digraph {\n");
	for(size_t i = 0; i < list_len(prog->funcs); i++) {
		ir_func_t *func = prog->funcs[i];
		printf("//%s\n", func->name);
		for(size_t j = 0; j < list_len(func->blocks); j++) {
			ir_blk_t *blk = func->blocks[j];
			ir_blk_t *dom = blk->idom;
			printf("\t\"BB%zu (%llu)\" -> \"BB%zu (%llu)\"\n", dom->num,
				   dom->loop_nest, blk->num, blk->loop_nest);
		}
	}
	printf("}\n");
	return;
}

/* generates code for an IR program */
/* handles all the function finalization stuff */
void ir_prog_compile(FILE *f, ir_prog_t *prog, enum ir_arch arch, int opt)
{
	switch(arch) {
	case IR_ARCH_AARCH64_APPLE:
		ir_prog_begin_aarch64_apple(f, prog);
		break;
	case IR_ARCH_X64_SYSV:
		ir_prog_begin_x64_sysv(f, prog);
		break;
	default:
		break;
	}

	/* global vars */
	for(size_t i = 0; i < list_len(prog->globs); i++) {
		ir_global_t *glob = prog->globs[i];
		switch(arch) {
		case IR_ARCH_AARCH64_APPLE:
			ir_glob_emit_aarch64_apple(f, glob);
			break;
		case IR_ARCH_X64_SYSV:
			ir_glob_emit_x64_sysv(f, glob);
			break;
		default:
			break;
		}
	}

	/* functions */
	long acc = 0;
	for(size_t i = 0; i < list_len(prog->funcs); i++) {
		ir_func_t *func = prog->funcs[i];
		ir_fix(func);

		for(size_t j = 0; j < list_len(func->blocks); j++) {
			ir_blk_t *blk = func->blocks[j];
			blk->tail = find_last_or_flow_ins(blk->insts);
			ir_inst_t *nxt;
			for(ir_inst_t *inst = blk->tail->next; inst; inst = nxt) {
				nxt = inst->next;
				ir_inst_delete(inst);
			}
			blk->tail->next = NULL;
		}

		ir_basic_block_placement(func);
		ir_blk_liveness(func);
		ir_coalesce(func);
		ir_opt(func, opt, arch);
		for(size_t j = 0; j < list_len(func->blocks); j++) {
			ir_blk_t *blk = func->blocks[j];
			blk->num = acc++;
		}
		ir_blk_flow(func);
		ir_finalize(func, arch == IR_ARCH_AARCH64_APPLE ? 10 : 11, opt, arch);

		if(debug) {
			printf("Final IR:\n");
			printf("\tvirtual:\n");
			ir_dump(func, 'v');
			printf("\treal:\n");
			ir_dump(func, 'r');
			printf("====\n");
		}
		ir_fix(func);
		ir_blk_flow(func);

		ir_func_emit(f, func, arch);
		ir_blk_flow(func);
		ir_blk_rpo(func);
		ir_blk_dom(func);
		ir_blk_domf(func);
		ir_blk_loopnest(func);
	}

	// ir_print_graph(prog);
	// for(size_t i = 0; i < list_len(prog->funcs); i++) {
	// 	ir_func_t *fn = prog->funcs[i];
	// 	ir_dump_stats(fn);
	// }

	return;
}
