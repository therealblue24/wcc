#include "bird.h"

void ir_placemarks(ir_func_t *func)
{
	/* initial pass - promote all regs to non-phi-related */
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *inst = blk->insts; inst; inst = inst->next) {
			if(inst->r0)
				inst->r0->phi_related = false;
		}
	}

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
				inst->r0->is_32bit = inst->is_32bit;
				inst->r0->ins_ext = inst->sign_ext;
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

	/* mark 32 bit phis */
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *inst = blk->insts; inst; inst = inst->next) {
			if(inst->type != IR_INST_PHI) {
				continue;
			}

			inst->is_32bit = true;
			for(size_t i = 0; i < list_len(inst->phi_args); i++) {
				inst->is_32bit &= inst->phi_args[i]->is_32bit;
			}
		}
	}
}
