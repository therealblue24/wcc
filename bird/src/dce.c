#include "bird.h"

static int ins_proves_live(enum ins_type t)
{
	return t == IR_INST_STORE || t == IR_INST_STORES || t == IR_INST_STORESS ||
		   t == IR_INST_CALL || ir_inst_is_term(t);
}

/* aggressive dead code elim */
/* basically conditional constant prop but with liveness instead */
int ir_adce(ir_func_t *func)
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

	prof_begin("seed");
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
	prof_end();

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

	prof_begin("prop");
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
	prof_end();

	/* eliminate all dead instructions */
	prof_begin("aggelim");
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
	prof_end();

	return change;
}
#undef MKALIVE

/* dead code elimination */
int ir_dce(ir_func_t *func)
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
			/* TODO: is the above claim correct? */
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
