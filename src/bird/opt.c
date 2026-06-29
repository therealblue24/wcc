#include "bird.h"
#include "ir.h"
#include "ssa.h"

extern int debug;

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

/* optimize
 * %reg = leas #off
 * ...
 * %other_reg = load %reg
 * to
 * %other_reg = loads #off
 * and then
 * %reg = leas #off
 
 */

static int ir_stackopt(ir_func_t *func)
{
	int changed = 0;
	/* first, check which registers are leas */
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			if(ins->type == IR_INST_LEAS) {
				ins->r0->stack_loc = true;
				ins->r0->stack_off = ins->imm;
			} else if(ins->r0) {
				ins->r0->stack_loc = false;
			}

			/* however if we use this register outside of loads and stores
			 * we can't optimize it */
			if(ins->r1 && ins->r1->stack_loc && ins->type != IR_INST_LOAD &&
			   ins->type != IR_INST_STORE) {
				ins->r1->stack_loc = false;
			}
			if(ins->r2 && ins->r2->stack_loc && ins->type != IR_INST_LOAD &&
			   ins->type != IR_INST_STORE) {
				ins->r2->stack_loc = false;
			}

			if(ins->type == IR_INST_CALL) {
				for(size_t i = 0; i < list_len(ins->call_args); i++) {
					reg_t *r = ins->call_args[i]->r;
					if(r && r->stack_loc && i >= 6) {
						r->stack_loc = false;
					}
				}
			}

			/* edge case */
			if(ins->r2 && ins->r2->stack_loc && ins->type == IR_INST_STORE) {
				ins->r2->stack_loc = false;
			}
		}
	}

	/* now, simply replace the load %leas_reg with loads #off */
	/* and store %leas_reg, %reg with stores %reg, #off */
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			if(ins->type != IR_INST_LOAD && ins->type != IR_INST_STORE) {
				continue;
			}

			if(ins->type == IR_INST_LOAD && ins->r1->stack_loc) {
				ins->type = IR_INST_LOADS;
				ins->imm = -ins->r1->stack_off;
				changed = 1;
			}

			if(ins->type == IR_INST_STORE && ins->r1->stack_loc) {
				reg_t *r1 = ins->r1;
				ins->r1 = ins->r2;
				ins->type = IR_INST_STORES;
				ins->imm = -r1->stack_off;
				changed = 1;
			}
		}
	}

	return changed;
}

static int cmp_to_br(enum ins_type ins)
{
	if(!ir_inst_is_cmp(ins)) {
		return IR_INST_NOP;
	}
	switch(ins) {
	case IR_INST_EQ:
		return IR_INST_BREQ;
	case IR_INST_NE:
		return IR_INST_BRNE;
	case IR_INST_SLT:
		return IR_INST_BRSLT;
	case IR_INST_SLE:
		return IR_INST_BRSLE;
	case IR_INST_SGT:
		return IR_INST_BRSGT;
	case IR_INST_SGE:
		return IR_INST_BRSGE;
	case IR_INST_ULT:
		return IR_INST_BRULT;
	case IR_INST_ULE:
		return IR_INST_BRULE;
	case IR_INST_UGT:
		return IR_INST_BRUGT;
	case IR_INST_UGE:
		return IR_INST_BRUGE;
	default:
		return IR_INST_NOP;
	}
}

/* optimize
 * %cond = cmp.XX %r1, %r2
 * ...
 * br %cond, T, F
 * ->
 * br.XX %r1, %r2, T, F
 */
static int ir_branchopt(ir_func_t *func)
{
	int changed = 0;

	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			if(ins->type == IR_INST_BR && ir_inst_is_cmp(ins->r1->insty)) {
				reg_t *cmp = ins->r1;
				ins->type = cmp_to_br(cmp->insty);
				ins->r1 = cmp->lhs;
				ins->r2 = cmp->rhs;
				ins->imm = cmp->imm;
				changed = 1;
			}
		}
	}
	return changed;
}

static int inverse_brcmp(enum ins_type ty)
{
	switch(ty) {
	case IR_INST_BREQ:
		return IR_INST_BRNE;
	case IR_INST_BRNE:
		return IR_INST_BREQ;
	case IR_INST_BRSLT:
		return IR_INST_BRSGE;
	case IR_INST_BRSLE:
		return IR_INST_BRSGT;
	case IR_INST_BRSGT:
		return IR_INST_BRSLE;
	case IR_INST_BRSGE:
		return IR_INST_BRSLT;
	case IR_INST_BRULT:
		return IR_INST_BRUGE;
	case IR_INST_BRULE:
		return IR_INST_BRUGT;
	case IR_INST_BRUGT:
		return IR_INST_BRULE;
	case IR_INST_BRUGE:
		return IR_INST_BRULT;
	default:
		return IR_INST_NOP;
	}
}

static void ir_placemarks(ir_func_t *func)
{
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *inst = blk->insts; inst; inst = inst->next) {
			if(inst->r0) {
				inst->r0->insty = inst->type;
				inst->r0->lhs = inst->r1;
				inst->r0->rhs = inst->r2;
				inst->r0->imm = inst->imm;
				inst->r0->size = inst->size;
				inst->r0->from = inst;
			}

			if(inst->type == IR_INST_PHI) {
				for(size_t i = 0; i < list_len(inst->phi_args); i++) {
					inst->phi_args[i]->phi_related = true;
				}
				/* do we need this? */
				/* inst->r0->phi_related = true; */
			}
		}
	}
}

static uint64_t zxt(uint64_t x, uint64_t size)
{
	uint64_t m = (1ULL << size) - 1;
	return x & m;
}

static uint64_t sxt(uint64_t x, uint64_t size)
{
	/* thank you https://stackoverflow.com/a/17719010 */
	uint64_t mask = 1ULL << (size - 1);
	return (x ^ mask) - mask;
}

static void ir_fold_ins_unaryop(ir_inst_t *ins)
{
	uint64_t ua = ins->r1->imm;
	int64_t sa = *((int64_t *)&ins->r1->imm);

	ins->r1 = NULL;
	ins->r2 = NULL;

	switch(ins->type) {
	case IR_INST_MOV:
		ins->imm = ua;
		break;
	case IR_INST_NEG:
		ins->imm = -sa;
		break;
	case IR_INST_NOT:
		ins->imm = ~ua;
		break;
	case IR_INST_MKBOOL:
		ins->imm = (ua != 0);
		break;
	case IR_INST_NOTBOOL:
		ins->imm = (ua == 0);
		break;
	case IR_INST_ZXT:
		ins->imm = zxt(ua, ins->size * 8);
		break;
	case IR_INST_SXT:
		ins->imm = sxt(ua, ins->size * 8);
		break;
	default:
		break;
	}

	ins->type = IR_INST_IMM;
	ins->r0->insty = IR_INST_IMM;
	ins->r0->imm = ins->imm;
	return;
}

static void ir_fold_ins_binop(ir_inst_t *ins)
{
	uint64_t ua = ins->r1->imm;
	uint64_t ub = ins->r2->imm;
	int64_t sa = *((int64_t *)&ins->r1->imm);
	int64_t sb = *((int64_t *)&ins->r2->imm);

	ins->r1 = NULL;
	ins->r2 = NULL;

	switch(ins->type) {
	case IR_INST_ADD:
		ins->imm = ua + ub;
		break;
	case IR_INST_SUB:
		ins->imm = ua - ub;
		break;
	case IR_INST_SMUL:
		ins->imm = sa * sb;
		break;
	case IR_INST_UMUL:
		ins->imm = ua * ub;
		break;
	case IR_INST_SDIV:
		if(sb == 0) {
			ins->imm = 0;
		} else {
			ins->imm = sa / sb;
		}
		break;
	case IR_INST_UDIV:
		if(ub == 0) {
			ins->imm = 0;
		} else {
			ins->imm = ua / ub;
		}
		break;
	case IR_INST_SMOD:
		if(sb == 0) {
			ins->imm = 0;
		} else {
			ins->imm = sa % sb;
		}
		break;
	case IR_INST_UMOD:
		if(ub == 0) {
			ins->imm = 0;
		} else {
			ins->imm = ua % ub;
		}
		break;
	case IR_INST_AND:
		ins->imm = ua & ub;
		break;
	case IR_INST_OR:
		ins->imm = ua | ub;
		break;
	case IR_INST_EOR:
		ins->imm = ua ^ ub;
		break;
	case IR_INST_SHL:
		ins->imm = ua << (ub & 63);
		break;
	case IR_INST_SHR:
		ins->imm = ua >> (ub & 63);
		break;
	case IR_INST_ASHR:
		ins->imm = sa >> (sb & 63);
		break;
	case IR_INST_EQ:
		ins->imm = ua == ub;
		break;
	case IR_INST_NE:
		ins->imm = ua != ub;
		break;
	case IR_INST_SLT:
		ins->imm = sa < sb;
		break;
	case IR_INST_SLE:
		ins->imm = sa <= sb;
		break;
	case IR_INST_SGT:
		ins->imm = sa > sb;
		break;
	case IR_INST_SGE:
		ins->imm = sa >= sb;
		break;
	case IR_INST_ULT:
		ins->imm = ua < ub;
		break;
	case IR_INST_ULE:
		ins->imm = ua <= ub;
		break;
	case IR_INST_UGT:
		ins->imm = ua > ub;
		break;
	case IR_INST_UGE:
		ins->imm = ua >= ub;
		break;

	default:
		break;
	}

	ins->type = IR_INST_IMM;
	ins->r0->insty = IR_INST_IMM;
	ins->r0->imm = ins->imm;

	return;
}

static void ir_zeroopt(ir_inst_t *inst)
{
	ir_blk_t *trueblk = inst->true_blk;
	ir_blk_t *falseblk = inst->false_blk;
	switch(inst->type) {
	case IR_INST_ADD:
	case IR_INST_SUB:
	case IR_INST_AND:
	case IR_INST_OR:
	case IR_INST_EOR:
	case IR_INST_SHL:
	case IR_INST_SHR:
	case IR_INST_ASHR:
		inst->type = IR_INST_MOV;
		break;
	case IR_INST_UMUL:
	case IR_INST_SMUL:
	case IR_INST_UDIV:
	case IR_INST_SDIV:
		inst->type = IR_INST_IMM;
		inst->imm = 0;
		break;
	case IR_INST_EQ:
		inst->type = IR_INST_NOTBOOL;
		break;
	case IR_INST_NE:
		inst->type = IR_INST_MKBOOL;
		break;
	case IR_INST_BRNE:
		inst->type = IR_INST_BR;
		break;
	case IR_INST_BREQ:
		inst->type = IR_INST_BR;
		inst->true_blk = falseblk;
		inst->false_blk = trueblk;
		break;
	default:
		break;
	}
	return;
}

static UNUSEDA int ir_fold(ir_func_t *func)
{
	int change = 0;
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			/* constant folding */
			if(ir_inst_is_foldable(ins->type)) {
				if(ins->r1 && ins->r2 && ins->r1->insty == IR_INST_IMM &&
				   ins->r2->insty == IR_INST_IMM) {
					ir_fold_ins_binop(ins);
					change = 1;
				}

				if(ir_inst_is_foldable(ins->type) && ins->r1 &&
				   ins->r1->insty == IR_INST_IMM && !ins->r2) {
					ir_fold_ins_unaryop(ins);
					change = 1;
				}
			}

			if(ins->type == IR_INST_BR && ins->r1->insty == IR_INST_IMM) {
				ins->type = IR_INST_JMP;

				ir_blk_t *trueblk = ins->true_blk;
				ir_blk_t *falseblk = ins->false_blk;
				ir_blk_t *lostblk = falseblk;

				if(!ins->r1->imm) {
					ins->true_blk = falseblk;
					lostblk = trueblk;
				}
				ins->false_blk = NULL;

				ir_remove_pred(lostblk, blk);

				change = 1;
			}

			if(ins->type != IR_INST_BR && ir_inst_is_br(ins->type) &&
			   ins->r1->insty == IR_INST_IMM && ins->r2->insty == IR_INST_IMM) {
				int cond;
				uint64_t ua = ins->r1->imm;
				uint64_t ub = ins->r2->imm;
				int64_t sa = *((int64_t *)&ins->r1->imm);
				int64_t sb = *((int64_t *)&ins->r2->imm);
				switch(ins->type) {
				case IR_INST_BREQ:
					cond = ua == ub;
					break;
				case IR_INST_BRNE:
					cond = ua != ub;
					break;
				case IR_INST_BRSLT:
					cond = sa < sb;
					break;
				case IR_INST_BRSLE:
					cond = sa <= sb;
					break;
				case IR_INST_BRSGT:
					cond = sa > sb;
					break;
				case IR_INST_BRSGE:
					cond = sa >= sb;
					break;
				case IR_INST_BRULT:
					cond = ua < ub;
					break;
				case IR_INST_BRULE:
					cond = ua <= ub;
					break;
				case IR_INST_BRUGT:
					cond = ua > ub;
					break;
				case IR_INST_BRUGE:
					cond = ua >= ub;
					break;
				default:
					cond = 0;
					break;
				}

				ir_blk_t *trueblk = ins->true_blk;
				ir_blk_t *falseblk = ins->false_blk;
				ir_blk_t *lostblk = falseblk;

				ins->type = IR_INST_JMP;
				if(!cond) {
					ins->true_blk = ins->false_blk;
					lostblk = trueblk;
				}
				ins->false_blk = NULL;

				ir_remove_pred(lostblk, blk);

				change = 1;
			}

			/* reorder
			 * fold_assoc %r0(imm), %r1(reg)
			 * ->
			 * fold_assoc %r1(reg), %r0(imm)
			 */

			if(ir_inst_is_foldable(ins->type) && ir_inst_is_assoc(ins->type) &&
			   ins->r1 && ins->r2 && ins->r1->insty == IR_INST_IMM &&
			   ins->r2->insty != IR_INST_IMM) {
				reg_t *tmp = ins->r1;
				ins->r1 = ins->r2;
				ins->r2 = tmp;
				change = 1;
			}

			/* replace
			 * %r0 = foldins %r1(reg), %r2(imm 0)
			 * into
			 * %r0 = ... whatever it evaluates to ...
			 */
			if(ir_inst_is_foldable(ins->type) && ins->r1 && ins->r2 &&
			   ins->r2->insty == IR_INST_IMM && ins->r2->imm == 0) {
				ir_zeroopt(ins);
				change = 1;
			}

			/* turn
			 * %r2 = neg %r3
			 * %r0 = add %r1, %r2
			 * ->
			 * %r0 = sub %r1, %r3
			 */
			if((ins->type == IR_INST_ADD || ins->type == IR_INST_SUB) &&
			   ins->r2->insty == IR_INST_NEG) {
				ins->type = ins->type == IR_INST_ADD ? IR_INST_SUB :
													   IR_INST_ADD;
				ins->r2 = ins->r2->lhs;
				change = 1;
			}

			/* turn
			 * %r1 = neg %r3
			 * %r0 = add %r1, %r2
			 * ->
			 * %r0 = sub %r2, %r3
			 */
			if(ins->type == IR_INST_ADD && ins->r1->insty == IR_INST_NEG) {
				ins->type = IR_INST_SUB;
				reg_t *r1 = ins->r1;
				reg_t *r2 = ins->r2;
				ins->r2 = r1->lhs;
				ins->r1 = r2;
				change = 1;
			}
		}
	}
	return change;
}

static int ir_extelim(ir_inst_t *ins)
{
	int change = 0;

	/* nothing to do */
	if(ins->r1->insty != IR_INST_ZXT && ins->r1->insty != IR_INST_SXT) {
		return 0;
	}

chk_large:
	/* larger extension after shorter extension is a mov */
	if(ins->size >= ins->r1->size) {
		ins->type = IR_INST_MOV;
		ins->size = 8;
		return 1;
	}

	/* shorter zero extension after larger zero extension: use the shorter */
	if(ins->size <= ins->r1->size) {
		ins->r1 = ins->r1->lhs;
		change = 1;
	}

	goto chk_large;

	return change;
}

static int ir_simpleopt_ins(ir_blk_t *thisblk, ir_inst_t *ins)
{
	int change = 0;

	/* extension elimination */
	if(ins->type == IR_INST_ZXT || ins->type == IR_INST_SXT) {
		change |= ir_extelim(ins);
	}

	/* %r0 = eor/sub/sdiv/udiv/smod/umod %r1, %r1
	 * ->
	 * %r0 = imm #0 */
	if((ins->type == IR_INST_EOR || ins->type == IR_INST_SUB ||
		ins->type == IR_INST_SDIV || ins->type == IR_INST_UDIV ||
		ins->type == IR_INST_SMOD || ins->type == IR_INST_UMOD) &&
	   ins->r1 == ins->r2) {
		ins->type = IR_INST_IMM;
		ins->imm = 0;
		change = 1;
	}

	/* %r0 = and/or %r1, %r1
	 * ->
	 * %r0 = %r1
	 */
	if((ins->type == IR_INST_AND || ins->type == IR_INST_OR) &&
	   ins->r1 == ins->r2) {
		ins->type = IR_INST_MOV;
		change = 1;
	}

	/* %r0 = sign_ext.i64/zero_ext.i64 %r1
	 * ->
	 * %r0 = %r1
	 */
	if((ins->type == IR_INST_SXT || ins->type == IR_INST_ZXT) &&
	   ins->size == 8) {
		ins->type = IR_INST_MOV;
		change = 1;
	}

	/* %r0 = %r0
	 * ->
	 * nop
	 */
	if(ins->type == IR_INST_MOV && ins->r0->vr == ins->r1->vr) {
		ins->type = IR_INST_NOP;
		change = 1;
	}

	/* %r0 = cmp.* %r1, %r1
	 * ->
	 * %r0 = imm #res */
	if(ir_inst_is_cmp(ins->type) && ins->r1->vr == ins->r2->vr) {
		long imm = 0;
#define CASE(v, x) \
	case v:        \
		imm = (x); \
		break
		switch(ins->type) {
			CASE(IR_INST_EQ, 1);
			CASE(IR_INST_NE, 0);
			CASE(IR_INST_SLT, 0);
			CASE(IR_INST_SLE, 1);
			CASE(IR_INST_SGT, 0);
			CASE(IR_INST_SGE, 1);
			CASE(IR_INST_ULT, 0);
			CASE(IR_INST_ULE, 1);
			CASE(IR_INST_UGT, 0);
			CASE(IR_INST_UGE, 1);
		default:
			break;
		}
#undef CASE
		ins->type = IR_INST_IMM;
		ins->imm = imm;
		change = 1;
	}

	/* br.** dontcareregs, blkA, blkA
	 * ->
	 * jmp blkA */
	if(ir_inst_is_br(ins->type) && ins->false_blk == ins->true_blk) {
		ins->type = IR_INST_JMP;
		change = 1;
	}

	/* simplify dead block jumps */
	if(ir_inst_is_br(ins->type) || ins->type == IR_INST_JMP) {
		ir_inst_t *first = ins->true_blk->insts;
		if(first->type == IR_INST_JMP && first->true_blk != ins->true_blk) {
			ir_blk_t *blk = first->true_blk;
			ir_reroute_pred(blk, ins->true_blk, thisblk);
			ins->true_blk = blk;

			change = 1;
		}
	}

	if(ir_inst_is_br(ins->type)) {
		ir_inst_t *first = ins->false_blk->insts;
		if(first->type == IR_INST_JMP && first->true_blk != ins->false_blk) {
			ir_blk_t *blk = first->true_blk;
			ir_reroute_pred(blk, ins->false_blk, thisblk);
			ins->false_blk = blk;
			change = 1;
		}
	}

	/* simplify
	 * br.cmp %r0, %r1, trueblk, falseblk (in front)
	 * to
	 * br.invcmp %r0, %r1, falseblk (in front), trueblk */
	if(ir_inst_is_br(ins->type) && ins->type != IR_INST_BR) {
		if(ins->false_blk->num == thisblk->num + 1) {
			ir_blk_t *tmpblk = ins->true_blk;
			ins->type = inverse_brcmp(ins->type);
			ins->true_blk = ins->false_blk;
			ins->false_blk = tmpblk;
			change = 1;
		}
	}

	return change;
}

/* trivial/simple optimizations */
static int ir_simpleopt(ir_func_t *func)
{
	int changed = 0;

	/* first, instruction-level opts */
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			changed |= ir_simpleopt_ins(blk, ins);
		}
	}

	return changed;
}

/* changes
 * stores #imm, %r1
 * loads %r2, #imm
 * ->
 * stores #imm, %r1
 * %r2 = %r1
 */
static int ir_stackreduce(ir_func_t *func)
{
	int changed = 0;
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		ir_inst_t *nxt = blk->insts;
		for(ir_inst_t *ins = blk->insts; ins; ins = nxt) {
			nxt = ins->next;

			if(ins && nxt && ins->type == IR_INST_STORES &&
			   nxt->type == IR_INST_LOADS && ins->imm == nxt->imm &&
			   !ins->noopt && !nxt->noopt) {
				reg_t *r1 = ins->r1;
				reg_t *r2 = nxt->r0;
				changed = 1;
				/* bingo */
				nxt->type = IR_INST_MOV;
				nxt->r0 = r2;
				nxt->r1 = r1;
			}
		}
	}
	return changed;
}

static int ir_imm_elim(ir_func_t *func)
{
	int change = 0;
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *inst = blk->insts; inst; inst = inst->next) {
			if(inst->type != IR_INST_MOV) {
				continue;
			}
			if(inst->r1->insty == IR_INST_IMM) {
				inst->type = IR_INST_IMM;
				inst->imm = inst->r1->imm;
				inst->r0->insty = IR_INST_IMM;
				inst->r0->imm = inst->imm;
				change = 1;
			}
		}
	}
	return change;
}

static bool leads_to_phi(reg_t *r)
{
	reg_t *found = r;
	while(found->insty == IR_INST_MOV) {
		if(found->lhs->insty == IR_INST_PHI) {
			return true;
		}
		found = found->lhs;
	}
	return false;
}

#define REPLACE(x)                                         \
	do {                                                   \
		if((x) && (x)->insty == IR_INST_MOV && (x)->lhs) { \
			(x) = (x)->lhs;                                \
			change = 1;                                    \
		}                                                  \
	} while(0)

/* or copy propagation, whatever you call it */
static int ir_mov_elim(ir_func_t *func)
{
	int change = 0;

	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *inst = blk->insts; inst; inst = inst->next) {
			if(inst->type != IR_INST_MOV) {
				continue;
			}

			/* do we need this? */
			if(0 && (leads_to_phi(inst->r1) || inst->r1->phi_related)) {
				inst->r1->insty = IR_INST_NOP;
				/* do we need this? */
				inst->r0->phi_related = true;
			}

			if(leads_to_phi(inst->r0) || inst->r0->phi_related) {
				inst->r0->insty = IR_INST_NOP;
			}
		}
	}

	/* do elim */
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *inst = blk->insts; inst; inst = inst->next) {
			REPLACE(inst->r0);
			REPLACE(inst->r1);
			REPLACE(inst->r2);
			if(inst->type == IR_INST_CALL) {
				for(size_t j = 0; j < list_len(inst->call_args); j++) {
					REPLACE(inst->call_args[j]->r);
				}
			}
		}
	}

	return change;
}

static int ins_proves_live(enum ins_type t)
{
	return t == IR_INST_STORE || t == IR_INST_STORES || t == IR_INST_STORESS ||
		   t == IR_INST_CALL || ir_inst_is_term(t);
}

static int ins_has_imm(enum ins_type t)
{
	return t == IR_INST_IMM || t == IR_INST_LEAS || t == IR_INST_LOADS ||
		   t == IR_INST_LOADSS || t == IR_INST_STORES || t == IR_INST_STORESS;
}

static int ins_is_same(ir_inst_t *a, ir_inst_t *b)
{
	/* general case */
	if(a == b) {
		return true;
	}

	/* trivial */
	if(a->type != b->type) {
		return false;
	}

	if(ins_proves_live(a->type) || a->type == IR_INST_PHI) {
		return false;
	}

	if(a->type == IR_INST_LEA) {
		return a->r0 == b->r0 && a->label == b->label;
	}

	bool ok = ins_has_imm(a->type) ? a->imm == b->imm : true;

	return ok && a->r0 == b->r0 && a->r1 == b->r1 && a->r2 == b->r2;
}

/* optimizes phis with all same value
 * also known as the "illusion of choice" optimization */
static int ir_phiopt(ir_func_t *func)
{
	int change = 0;

	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			if(ins->type != IR_INST_PHI) {
				continue;
			}

			ASSERT(list_len(ins->phi_args) >= 1, "invalid IR");

			ir_inst_t *first = ins->phi_args[0]->from;
			bool same = true;
			for(size_t j = 0; j < list_len(ins->phi_args); j++) {
				if(!ins_is_same(first, ins->phi_args[j]->from)) {
					same = false;
					break;
				}
			}

			if(!same) {
				continue;
			}

			/* replace phi with move */
			change = 1;
			ins->type = IR_INST_MOV;
			ins->r1 = first->r0;
			list_delete(ins->phi_args);
			list_delete(ins->phi_preds);
		}
	}

	return change;
}

/* aggressive dead code elim */
/* basically conditional constant prop but with liveness instead */
static int ir_adce(ir_func_t *func)
{
	int change = 0;
	/* set all registers to dead */
	/* all instructions w/ side effects or terminates however set to alive */
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			if(ins->r0) {
				ins->r0->alive = false;
			}
		}
	}

	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			if(ins_proves_live(ins->type)) {
				if(ins->r0) {
					ins->r0->alive = true;
				}
				if(ins->r1) {
					ins->r1->alive = true;
				}
				if(ins->r2) {
					ins->r2->alive = true;
				}
				if(ins->type == IR_INST_CALL) {
					for(size_t j = 0; j < list_len(ins->call_args); j++) {
						ins->call_args[j]->r->alive = true;
					}
				}
			}
			if(ir_inst_is_term(ins->type)) {
				break;
			}
		}
	}

#define MKALIVE(x)                 \
	do {                           \
		if((x)) {                  \
			if(!(x)->alive) {      \
				lchange = 1;       \
				(x)->alive = true; \
			}                      \
		}                          \
	} while(0)

	int lchange = 1;
	/* now:
	 * for any register that is alive, mark all registers in its def as alive.
	 * repeat until no more marks made */

	while(lchange) {
		lchange = 0;

		for(size_t i = 0; i < list_len(func->blocks); i++) {
			ir_blk_t *blk = func->blocks[i];
			for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
				if(!ins->r0) {
					continue;
				}
				if(!ins->r0->alive) {
					continue;
				}

				MKALIVE(ins->r1);
				MKALIVE(ins->r2);

				if(ins->type == IR_INST_PHI) {
					for(size_t j = 0; j < list_len(ins->phi_args); j++) {
						MKALIVE(ins->phi_args[j]);
					}
				}
				if(ins->type == IR_INST_CALL) {
					for(size_t j = 0; j < list_len(ins->call_args); j++) {
						MKALIVE(ins->call_args[j]->r);
					}
				}
			}
		}
		change |= lchange;
	}

	/* eliminate all dead instructions */
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			if(!ins->r0) {
				continue;
			}
			if(ins->r0->alive) {
				continue;
			}
			if(ins->type == IR_INST_CALL) {
				list_delete(ins->call_args);
			}

			if(ins->type == IR_INST_PHI) {
				list_delete(ins->phi_args);
				list_delete(ins->phi_preds);
			}
			ins->type = IR_INST_NOP;
		}
	}

	return change;
}

static int ir_dce(ir_func_t *func)
{
	int change = 0;

	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			if(!ins->r0) {
				continue;
			}

			if(ins->r0->phi_related) {
				continue;
			}

			/* todo: this might break in some scenarios */
			if(ins->r0->def == ins->r0->last_use) {
				change = 1;
				if(ins->type == IR_INST_CALL) {
					ins->r0 = NULL;
				} else {
					if(ins->type == IR_INST_PHI) {
						list_delete(ins->phi_args);
						list_delete(ins->phi_preds);
					}
					ins->type = IR_INST_NOP;
				}
			}
		}
	}

	return change;
}

/* optimizes an IR function */
void ir_opt(ir_func_t *func, int opt_level, enum ir_arch arch)
{
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		func->blocks[i]->tail = find_last_or_flow_ins(func->blocks[i]->insts);
	}

	int change = 0;
	int max_tolerated_change;
	switch(opt_level) {
	case 0:
		max_tolerated_change = 0;
		break;
	case 1:
		max_tolerated_change = 1;
		break;
	case 2:
		max_tolerated_change = 16;
		break;
	case 3:
		max_tolerated_change = 256; /* mimic the nature of -O3 */
		break;
	default:
		max_tolerated_change = 0;
		break;
	}

	if(debug) {
		printf("Before common opts:\n");
		ir_dump(func, 'v');
		printf("****\n");
	}

	ir_ssa_enter(func);
	ir_blk_flow(func);

	if(debug) {
		printf("After SSA constr.:\n");
		ir_dump(func, 'v');
		printf("****\n");
	}

	int left = max_tolerated_change;
	while(left) {
		change = 0;

		ir_blk_reguse(func);
		ir_blk_liveness(func);
		ir_placemarks(func);

		/* dead code elim + extras */
		{
			ir_fix(func);
			change |= ir_dce(func);
			change |= ir_adce(func);
			ir_blk_liveness(func);
			change |= ir_imm_elim(func);
			change |= ir_mov_elim(func);
			change |= ir_simpleopt(func);

			ir_fix_phis(func);
			change |= ir_fold(func);
			ir_fix_phis(func);
			change |= ir_phiopt(func);
			ir_fix_phis(func);
			ir_nopremover(func);
			ir_fix(func);
		}

		/* branch opts */
		{
			change |= ir_branchopt(func);
			// fix_phis(func);
			ir_nopremover(func);
			ir_fix(func);
		}

		/* stack-based optimizations */
		{
			change |= ir_stackopt(func);
			ir_nopremover(func);
			change |= ir_stackreduce(func);
			ir_nopremover(func);
			ir_fix(func);
		}

		/* todo: simpleopts */

		if(!change) {
			break;
		}

		left--;
		ir_blk_flow(func);
	}

	ir_nopremover(func);

	if(debug) {
		printf("Before SSA destruct.:\n");
		ir_dump(func, 'v');
		printf("****\n");
	}

	ir_ssa_exit(func);
	ir_nopremover(func);

	if(debug) {
		printf("After common opts:\n");
		ir_dump(func, 'v');
		printf("****\n");
	}

	/* apply arch specific opts */
	switch(arch) {
	case IR_ARCH_AARCH64_APPLE:
		ir_func_opt_aarch64(func, opt_level);
		break;
	case IR_ARCH_X64_SYSV:
		ir_func_opt_x64(func, opt_level);
		break;
	default:
		break;
	}

	ir_fix(func);

	if(debug) {
		printf("After arch opts:\n");
		ir_dump(func, 'v');
		printf("****\n");
	}
	return;
}
