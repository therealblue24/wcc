#include "bird.h"

static bool ins_produces_bool(enum ins_type t)
{
	return t == IR_INST_MKBOOL || t == IR_INST_NOTBOOL || ir_inst_is_cmp(t);
}

static bool ins_is_ext(enum ins_type t)
{
	return t == IR_INST_ZXT || t == IR_INST_SXT;
}

static bool is_pow2(uint64_t v)
{
	/* thank you https://graphics.stanford.edu/~seander/bithacks.html#DetermineIfPowerOf2 */
	return v && !(v & (v - 1));
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

static uint64_t pow_log2(uint64_t v)
{
#ifdef __wcc__
	/* thank you https://graphics.stanford.edu/~seander/bithacks.html#IntegerLog */
	static const uint64_t b[6] = { 0xaaaaaaaa, 0xcccccccc, 0xf0f0f0f0,
								   0xff00ff00, 0xffff0000, 0xffffffff00000000 };
	uint64_t r = (v & b[0]) != 0;
	for(int i = 5; i > 0; i--) {
		r |= ((v & b[i]) != 0) << i;
	}
	return r;
#else
	return __builtin_ffs(v) - 1;
#endif
}

static int ir_muldiv_opt(ir_func_t *func)
{
	int change = 0;

	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		/* append NOPs */
		ir_inst_t *nop = ins_nop();
		nop->next = blk->insts;
		blk->insts = nop;
		ir_inst_t *prev = nop;
		for(ir_inst_t *inst = blk->insts->next; inst; inst = inst->next) {
			if(!inst->r2 ||
			   (inst->type != IR_INST_UDIV && inst->type != IR_INST_UMUL &&
				inst->type != IR_INST_SMUL) ||
			   inst->r2->insty != IR_INST_IMM) {
				goto next;
			}

			uint64_t imm = inst->r2->imm &
						   (inst->r2->is_32bit ? UINT32_MAX : UINT64_MAX);
			if(!is_pow2(imm)) {
				goto next;
			}

			/* %r0 = udiv/umul/smul %r1, #pow2
			 * ->
			 * %imm = #log(pow2)
			 * %r0 = shr/shl/shl %r1, %imm
			 */

			ir_inst_t *imml = ins_imm(reg_make(), pow_log2(imm));
			imml->is_32bit = inst->is_32bit;
			prev->next = imml;
			imml->next = inst;
			inst->r2 = imml->r0;
			prev = imml;
			if(inst->type == IR_INST_UDIV) {
				inst->type = IR_INST_SHR;
			} else if(inst->type == IR_INST_UMUL ||
					  inst->type == IR_INST_SMUL) {
				inst->type = IR_INST_SHL;
			}

			change = 1;

next:
			prev = inst;
		}
	}

	return change;
}

static int ir_simpleopt_ins(ir_blk_t *thisblk, ir_inst_t *ins)
{
	int change = 0;

	/* extension elim */

	if((ins->type == IR_INST_ZXT || ins->type == IR_INST_SXT) &&
	   ins->r1->insty == ins->type && ins->size <= ins->r1->size) {
		ins->type = IR_INST_MOV;
		change = 1;
	}

	/* elim of exts into 32-bit op */
	if(ins->is_32bit && ins->r2 && ins_is_ext(ins->r2->insty) &&
	   ins->r2->size == 4) {
		ins->r2 = ins->r2->lhs;
		change = 1;
	}

	if(ins->is_32bit && ins->r1 && ins_is_ext(ins->r1->insty) &&
	   ins->r1->size == 4) {
		ins->r1 = ins->r1->lhs;
		change = 1;
	}

	/* elim of ext out of 32-bit op */
	if(ins_is_ext(ins->type) && ins->r1->is_32bit && ins->size == 4) {
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

	/* %r0 = and %r1, #power-2-minus-1
	 * ->
	 * %r0 = zero_ext.(log) %r1
	 */
	if(ins->type == IR_INST_AND) {
		int r1_imm = ins->r1->insty == IR_INST_IMM;
		int r2_imm = ins->r2->insty == IR_INST_IMM;
		if((r1_imm + r2_imm) == 1) {
			/* reorder so that imm is in r2 */
			if(r1_imm) {
				reg_t *tmp = ins->r2;
				ins->r2 = ins->r1;
				ins->r1 = tmp;
			}

			/* if imm is 0, replace ins with imm #0 */
			if(ins->r2->imm == 0) {
				ins->type = IR_INST_IMM;
				ins->imm = 0;
				change = 1;
				goto exit;
			}

			/* options to extend:
			 * all 64 bits set: this is just a move
			 * low 32 bits set: zero_ext.i32
			 * low 16 bits set: zero_ext.i16
			 * low  8 bits set: zero_ext.i8
			 */
			uint64_t imm = ins->r2->imm &
						   (ins->is_32bit ? UINT32_MAX : UINT64_MAX);
			if(imm == UINT64_MAX) {
				ins->type = IR_INST_MOV;
				change = 1;
			} else if(imm == UINT32_MAX) {
				ins->type = IR_INST_ZXT;
				ins->size = 4;
				change = 1;
			} else if(imm == UINT16_MAX) {
				ins->type = IR_INST_ZXT;
				ins->size = 2;
				change = 1;
			} else if(imm == UINT8_MAX) {
				ins->type = IR_INST_ZXT;
				ins->size = 1;
				change = 1;
			}
		}
	}
exit:

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

	/* %r0 = i64 imm #x (x fits in 32 bits)
	 * ->
	 * %r0 = i32 imm #x
	 */
	if(ins->type == IR_INST_IMM && (ins->imm & UINT32_MAX) == ins->imm) {
		ins->is_32bit = true;
		change = 1;
	}

	/* %r1 = ins_produces_bool ...
	 * %r2 = mkbool %r1
	 * ->
	 * %r1 = ins_produces_bool ...
	 * %r2 = %r1
	 */

	if(ins->type == IR_INST_MKBOOL && ins_produces_bool(ins->r1->insty)) {
		ins->type = IR_INST_MOV;
		change = 1;
	}

	/* %r1 = mkbool %r0
	 * %r2 = mkbool %r1
	 * ->
	 * %r1 = mkbool %r0
	 * %r2 = %r1
	 */
	if(ins->type == IR_INST_MKBOOL && ins->r1->insty == IR_INST_MKBOOL) {
		ins->type = IR_INST_MOV;
		change = 1;
	}

	/* %r1 = mkbool %r0
	 * %r2 = notbool %r1
	 * ->
	 * %r1 = mkbool %r0
	 * %r2 = notbool %r0
	 */
	if(ins->type == IR_INST_NOTBOOL && ins->r1->insty == IR_INST_MKBOOL) {
		ins->r1 = ins->r1->lhs;
		change = 1;
	}

	/* %r1 = notbool %r0
	 * %r2 = mkbool %r1
	 * ->
	 * %r1 = notbool %r0
	 * %r2 = %r1
	 */
	if(ins->type == IR_INST_MKBOOL && ins->r1->insty == IR_INST_NOTBOOL) {
		ins->type = IR_INST_MOV;
		change = 1;
	}

	/* %r1 = notbool %r0
	 * %r2 = notbool %r1
	 * ->
	 * %r1 = notbool %r0
	 * %r2 = mkbool %r0
	 */
	if(ins->type == IR_INST_NOTBOOL && ins->r1->insty == IR_INST_NOTBOOL) {
		ins->type = IR_INST_MKBOOL;
		ins->r1 = ins->r1->lhs;
		change = 1;
	}

	/* %r0 = %r0
	 * ->
	 * nop
	 */
	if(ins->type == IR_INST_MOV && ins->r0 == ins->r1) {
		ins->type = IR_INST_NOP;
		change = 1;
	}

	/* %r0 = cmp.* %r1, %r1
	 * ->
	 * %r0 = imm #res */
	if(ir_inst_is_cmp(ins->type) && ins->r1 == ins->r2) {
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

static int ir_simpleopt_ins_alg(ir_inst_t *ins)
{
	int change = 0;

	/* %r2 = sub %r0, %r1
	 * %r3 = add %r2, %r1
	 * ->
	 * %r2 = sub %r0, %r1
	 * %r3 = %r0
	 */
	if(ins->type == IR_INST_ADD && ins->r1->insty == IR_INST_SUB &&
	   ins->r1->rhs == ins->r2) {
		ins->type = IR_INST_MOV;
		ins->r2 = NULL;
		ins->r1 = ins->r1->lhs;
		change = 1;
	}

	/* %r2 = add %r0, %r1
	 * %r3 = sub %r2, %r1
	 * ->
	 * %r2 = add %r0, %r1
	 * %r3 = %r0
	 */
	if(ins->type == IR_INST_SUB && ins->r1->insty == IR_INST_ADD &&
	   ins->r1->rhs == ins->r2) {
		ins->type = IR_INST_MOV;
		ins->r2 = NULL;
		ins->r1 = ins->r1->lhs;
		change = 1;
	}

	/* TODO: think of more rewritings */

	return change;
}

/* trivial/simple optimizations */
int ir_simpleopt(ir_func_t *func)
{
	int changed = 0;

	/* first, instruction-level opts */
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			changed |= ir_simpleopt_ins(blk, ins);
			changed |= ir_simpleopt_ins_alg(ins);
		}
	}

	changed |= ir_muldiv_opt(func);

	ir_nopremover(func);
	ir_placemarks(func);
	return changed;
}
