#include "bird.h"
#include "ir.h"

/* track usage counts of immediates */
/* kind of dirty, but tracks usage counts in spill cost */
#define ACC(x)                                 \
	do {                                       \
		if((x) && (x)->insty == IR_INST_IMM) { \
			(x)->spill_cost++;                 \
		}                                      \
	} while(0)

void ir_immfold_analyze(ir_func_t *fun)
{
	ir_blk_reguse(fun);
	/* reset counts */
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		reg_t *r;
		set_iter(blk->regs_def, r, { r->spill_cost = 0; });
	}

	/* track usage */
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		for(ir_inst_t *inst = blk->insts; inst; inst = inst->next) {
			if(inst->type == IR_INST_IMM) {
				inst->r0->insty = IR_INST_IMM;
				inst->r0->imm = inst->imm;
			}
			ACC(inst->r1);
			ACC(inst->r2);
			if(inst->type == IR_INST_PHI) {
				for(size_t j = 0; j < list_len(inst->phi_args); j++) {
					ACC(inst->phi_args[j]);
				}
			}
			/* we dont fold immediates into calls */
		}
	}
}

#undef ACC

static bool can_fold(enum ins_type t, int64_t imm, int64_t lower_bound,
					 int64_t higher_bound, enum ir_arch arch)
{
	switch(t) {
	case IR_INST_EOR:
	case IR_INST_AND:
	case IR_INST_OR:
		/* arm in a nutshell */
		if(arch != IR_ARCH_AARCH64_APPLE) {
			goto check;
		}
		return imm >= 0 && imm <= 15;
	default:
check:;
		return imm >= lower_bound && imm <= higher_bound;
	}
	/* unreachable */
	return false;
}

/* do the folding */
void ir_immfold_do(ir_func_t *fun, int64_t lower_bound, int64_t higher_bound,
				   enum ir_arch arch)
{
	/* only fold immediates if it has less than (or equal to) 3 usages */
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		reg_t *r;
		set_iter(blk->regs_def, r, {
			if(r->spill_cost <= 3) {
				r->spill_cost = 1;
			} else {
				r->spill_cost = 0;
			}
		});
	}

	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		for(ir_inst_t *inst = blk->insts; inst; inst = inst->next) {
			if(!ir_inst_can_fold_imm(inst->type)) {
				continue;
			}

			if(inst->r2->insty != IR_INST_IMM) {
				continue;
			}

			if(!inst->r2->spill_cost) {
				continue;
			}

			int64_t immv = *(int64_t *)(&inst->r2->imm);
			if(can_fold(inst->type, immv, lower_bound, higher_bound, arch)) {
				/* fold */
				inst->imm = inst->r2->imm;
				if(inst->type == IR_INST_SHL || inst->type == IR_INST_SHR ||
				   inst->type == IR_INST_ASHR) {
					inst->imm &= 63;
				}
				inst->type = ir_inst_turn_imm(inst->type);
				inst->r2 = NULL;
			}
		}
	}
}
