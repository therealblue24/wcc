#include "bird.h"

static uint64_t zxt(uint64_t x, uint64_t size)
{
	uint64_t m = (1ULL << size) - 1;
	return x & m;
}

static uint64_t sxt(uint64_t x_, uint64_t size)
{
	/* thank you https://graphics.stanford.edu/~seander/bithacks.html#FixedSignExtend */
	int64_t x = x_;
	int64_t r;
	int64_t mask = 1ULL << (size - 1);
	x = x & ((1ULL << size) - 1);
	r = (x ^ mask) - mask;
	return r;
}

static uint32_t zxt32(uint32_t x, uint32_t size)
{
	uint32_t m = (1 << size) - 1;
	return x & m;
}

static uint32_t sxt32(uint32_t x_, uint32_t size)
{
	/* thank you https://graphics.stanford.edu/~seander/bithacks.html#FixedSignExtend */
	int32_t x = x_;
	int32_t r;
	int32_t mask = 1 << (size - 1);
	x = x & ((1 << size) - 1);
	r = (x ^ mask) - mask;
	return r;
}

static void ir_fold_ins_unaryop64(ir_inst_t *ins)
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

static void ir_fold_ins_unaryop32(ir_inst_t *ins)
{
	uint32_t ua = ins->r1->imm & UINT32_MAX;
	int32_t sa = *((int32_t *)&ua);
	int32_t immres = 0;

	ins->r1 = NULL;
	ins->r2 = NULL;

	switch(ins->type) {
	case IR_INST_MOV:
		immres = ua;
		break;
	case IR_INST_NEG:
		immres = -sa;
		break;
	case IR_INST_NOT:
		immres = ~ua;
		break;
	case IR_INST_MKBOOL:
		immres = (ua != 0);
		break;
	case IR_INST_NOTBOOL:
		immres = (ua == 0);
		break;
	case IR_INST_ZXT:
		immres = zxt32(ua, ins->size * 8);
		break;
	case IR_INST_SXT:
		immres = sxt32(ua, ins->size * 8);
		break;
	default:
		break;
	}

	ins->imm = (int64_t)immres;
	ins->type = IR_INST_IMM;
	ins->r0->insty = IR_INST_IMM;
	ins->r0->imm = ins->imm;
	return;
}

static void ir_fold_ins_binop64(ir_inst_t *ins)
{
	uint64_t ua = ins->r1->imm;
	uint64_t ub = ins->r2->imm;
	int64_t sa = *((int64_t *)&ins->r1->imm);
	int64_t sb = *((int64_t *)&ins->r2->imm);
	uint64_t c;

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
	case IR_INST_ROL:
		c = ub & 63;
		ins->imm = (ua << c) | (ua >> (64 - c));
		break;
	case IR_INST_ROR:
		c = ub & 63;
		ins->imm = (ua >> c) | (ua << (64 - c));
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

static void ir_fold_ins_binop32(ir_inst_t *ins)
{
	uint32_t ua = ins->r1->imm & UINT32_MAX;
	uint32_t ub = ins->r2->imm & UINT32_MAX;
	int32_t sa = *((int32_t *)&ua);
	int32_t sb = *((int32_t *)&ub);
	int32_t immres = 0;
	uint32_t c;

	ins->r1 = NULL;
	ins->r2 = NULL;

	switch(ins->type) {
	case IR_INST_ADD:
		immres = ua + ub;
		break;
	case IR_INST_SUB:
		immres = ua - ub;
		break;
	case IR_INST_SMUL:
		immres = sa * sb;
		break;
	case IR_INST_UMUL:
		immres = ua * ub;
		break;
	case IR_INST_SDIV:
		if(sb == 0) {
			immres = 0;
		} else {
			immres = sa / sb;
		}
		break;
	case IR_INST_UDIV:
		if(ub == 0) {
			immres = 0;
		} else {
			immres = ua / ub;
		}
		break;
	case IR_INST_SMOD:
		if(sb == 0) {
			immres = 0;
		} else {
			immres = sa % sb;
		}
		break;
	case IR_INST_UMOD:
		if(ub == 0) {
			immres = 0;
		} else {
			immres = ua % ub;
		}
		break;
	case IR_INST_AND:
		immres = ua & ub;
		break;
	case IR_INST_OR:
		immres = ua | ub;
		break;
	case IR_INST_EOR:
		immres = ua ^ ub;
		break;
	case IR_INST_SHL:
		immres = ua << (ub & 31);
		break;
	case IR_INST_SHR:
		immres = ua >> (ub & 31);
		break;
	case IR_INST_ASHR:
		immres = sa >> (sb & 31);
		break;
	case IR_INST_ROL:
		c = ub & 31;
		immres = (ua << c) | (ua >> (32 - c));
		break;
	case IR_INST_ROR:
		c = ub & 31;
		immres = (ua >> c) | (ua << (32 - c));
		break;
	case IR_INST_EQ:
		immres = ua == ub;
		break;
	case IR_INST_NE:
		immres = ua != ub;
		break;
	case IR_INST_SLT:
		immres = sa < sb;
		break;
	case IR_INST_SLE:
		immres = sa <= sb;
		break;
	case IR_INST_SGT:
		immres = sa > sb;
		break;
	case IR_INST_SGE:
		immres = sa >= sb;
		break;
	case IR_INST_ULT:
		immres = ua < ub;
		break;
	case IR_INST_ULE:
		immres = ua <= ub;
		break;
	case IR_INST_UGT:
		immres = ua > ub;
		break;
	case IR_INST_UGE:
		immres = ua >= ub;
		break;

	default:
		break;
	}

	ins->imm = (int64_t)immres;
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

int ir_leas_arith_opt(ir_func_t *func)
{
	int change = 0;

	/* reorder assoc ops such that immediate ops are r2 */
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			if(!ir_inst_is_assoc(ins->type)) {
				continue;
			}

			if(ins->r1->insty == IR_INST_IMM) {
				reg_t *tmp = ins->r2;
				ins->r2 = ins->r1;
				ins->r1 = tmp;
			}
		}
	}

	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			if(ins->type != IR_INST_ADD && ins->type != IR_INST_SUB &&
			   ins->type != IR_INST_UMUL && ins->type != IR_INST_SMUL &&
			   ins->type != IR_INST_UDIV && ins->type != IR_INST_SDIV) {
				continue;
			}

			if(ins->r1->insty != IR_INST_LEAS) {
				continue;
			}

			if(ins->r2->insty != IR_INST_IMM) {
				continue;
			}

			change = 1;

			ins->imm = -ins->r1->imm;

			uint64_t imm = ins->r2->imm;
			if(ins->r2->is_32bit) {
				uint32_t low = imm & UINT32_MAX;
				imm = *(int32_t *)(&low);
			}

			switch(ins->type) {
			case IR_INST_ADD:
				ins->imm += imm;
				break;
			case IR_INST_SUB:
				ins->imm -= imm;
				break;
			case IR_INST_UMUL:
			case IR_INST_SMUL:
				ins->imm *= imm;
				break;
			case IR_INST_UDIV:
			case IR_INST_SDIV:
				ins->imm /= imm;
				break;
			default:
				break;
			}
			ins->type = IR_INST_LEAS;
			ins->r2 = NULL;
			ins->imm = -ins->imm;
		}
	}

	return change;
}

int ir_fold(ir_func_t *func)
{
	int change = 0;
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			if(ins->is_32bit) {
				/* constant folding */
				if(ir_inst_is_foldable(ins->type)) {
					if(ins->r1 && ins->r2 && ins->r1->insty == IR_INST_IMM &&
					   ins->r2->insty == IR_INST_IMM) {
						ir_fold_ins_binop32(ins);
						change = 1;
					}

					if(ir_inst_is_foldable(ins->type) && ins->r1 &&
					   ins->r1->insty == IR_INST_IMM && !ins->r2) {
						ir_fold_ins_unaryop32(ins);
						change = 1;
					}
				}

			} else {
				/* constant folding */
				if(ir_inst_is_foldable(ins->type)) {
					if(ins->r1 && ins->r2 && ins->r1->insty == IR_INST_IMM &&
					   ins->r2->insty == IR_INST_IMM) {
						ir_fold_ins_binop64(ins);
						change = 1;
					}

					if(ir_inst_is_foldable(ins->type) && ins->r1 &&
					   ins->r1->insty == IR_INST_IMM && !ins->r2) {
						ir_fold_ins_unaryop64(ins);
						change = 1;
					}
				}
			}

			if(ins->type == IR_INST_BR && ins->r1->insty == IR_INST_IMM) {
				ins->type = IR_INST_JMP;

				ir_blk_t *trueblk = ins->true_blk;
				ir_blk_t *falseblk = ins->false_blk;
				ir_blk_t *lostblk = falseblk;

				uint64_t imm = ins->r1->imm;
				if(ins->is_32bit) {
					imm &= UINT32_MAX;
				}

				if(!imm) {
					ins->true_blk = falseblk;
					lostblk = trueblk;
				}
				ins->false_blk = NULL;

				ir_remove_pred(lostblk, blk);

				change = 1;
			}

			if(!ins->is_32bit && ins->type != IR_INST_BR &&
			   ir_inst_is_br(ins->type) && ins->r1->insty == IR_INST_IMM &&
			   ins->r2->insty == IR_INST_IMM) {
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

			if(ins->is_32bit && ins->type != IR_INST_BR &&
			   ir_inst_is_br(ins->type) && ins->r1->insty == IR_INST_IMM &&
			   ins->r2->insty == IR_INST_IMM) {
				int cond;
				uint32_t ua = ins->r1->imm & UINT32_MAX;
				uint32_t ub = ins->r2->imm & UINT32_MAX;
				int32_t sa = *((int32_t *)&ins->r1->imm);
				int32_t sb = *((int32_t *)&ins->r2->imm);
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
			   ins->r2->insty == IR_INST_IMM &&
			   (ins->r2->imm & (ins->is_32bit ? UINT32_MAX : UINT64_MAX)) ==
				   0) {
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
			   ins->r2->insty == IR_INST_NEG &&
			   ins->is_32bit == ins->r2->is_32bit) {
				ins->type = ins->type == IR_INST_ADD ? IR_INST_SUB :
													   IR_INST_ADD;
				ins->r2 = ins->r2->lhs;
				change = 1;
			}
		}
	}
	return change;
}

int ir_imm_elim(ir_func_t *func)
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

#define REPLACE(x)                                         \
	do {                                                   \
		if((x) && (x)->insty == IR_INST_MOV && (x)->lhs) { \
			(x) = (x)->lhs;                                \
			change = 1;                                    \
		}                                                  \
	} while(0)

/* or copy propagation, whatever you call it */
int ir_mov_elim(ir_func_t *func)
{
	int change = 0;

	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *inst = blk->insts; inst; inst = inst->next) {
			if(inst->type != IR_INST_PHI) {
				continue;
			}

			/* do not move elim backedge phis */
			if(blk->loop_nest) {
				for(size_t j = 0; j < list_len(inst->phi_args); j++) {
					reg_t *arg = inst->phi_args[j];
					arg->insty = IR_INST_NOP;
				}
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
			if(inst->type == IR_INST_PHI) {
				for(size_t j = 0; j < list_len(inst->phi_args); j++) {
					REPLACE(inst->phi_args[j]);
				}
			}
			if(inst->type == IR_INST_MOV && inst->r0 == inst->r1) {
				inst->type = IR_INST_NOP;
			}
		}
	}

	ir_placemarks(func);
	change |= ir_mov_elim32(func);
	return change;
}
#undef REPLACE

static int possible_to_elim32(reg_t *r)
{
	return r->is_32bit && r->size == 4 &&
		   (r->insty == IR_INST_ZXT || r->insty == IR_INST_SXT);
}

/* 32 bit move elimination - does move elim for
 * %r0 = i32 sign_ext/zero_ext.i32 %r1
 */
int ir_mov_elim32(ir_func_t *func)
{
	int change = 0;

	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *inst = blk->insts; inst; inst = inst->next) {
			if(!inst->r0 || (inst->r0 && !possible_to_elim32(inst->r0))) {
				continue;
			}
			if(inst->type != IR_INST_PHI) {
				continue;
			}

			/* do not move elim backedge phis */
			if(blk->loop_nest) {
				for(size_t j = 0; j < list_len(inst->phi_args); j++) {
					reg_t *arg = inst->phi_args[j];
					arg->insty = IR_INST_NOP;
				}
			}
		}
	}

#define REPLACE(x)                       \
	if((x) && possible_to_elim32((x))) { \
		(x) = (x)->lhs;                  \
	}

	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *inst = blk->insts; inst; inst = inst->next) {
			REPLACE(inst->r1);
			REPLACE(inst->r2);

			if(inst->type == IR_INST_CALL) {
				for(size_t j = 0; j < list_len(inst->call_args); j++) {
					REPLACE(inst->call_args[j]->r);
				}
			}
			if(inst->type == IR_INST_PHI) {
				for(size_t j = 0; j < list_len(inst->phi_args); j++) {
					REPLACE(inst->phi_args[j]);
				}
			}
			if(inst->type == IR_INST_MOV && inst->r0 == inst->r1) {
				inst->type = IR_INST_NOP;
			}
		}
	}

#undef REPLACE

	return change;
}
