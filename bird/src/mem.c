#include "bird.h"

/* optimize
 * %reg = leas #off
 * ...
 * %other_reg = load %reg
 * to
 * %other_reg = loads #off
 * and then
 * %reg = leas #off
 
 */

int ir_stackopt(ir_func_t *func)
{
	int changed = 0;
	/* replace load %leas_reg with loads #off */
	/* and store %leas_reg, %reg with stores %reg, #off */
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			if(ins->type != IR_INST_LOAD && ins->type != IR_INST_STORE) {
				continue;
			}

			if(ins->type == IR_INST_LOAD && ins->r1->insty == IR_INST_LEAS) {
				ins->type = IR_INST_LOADS;
				ins->imm = -ins->r1->imm;
				changed = 1;
			}

			if(ins->type == IR_INST_STORE && ins->r1->insty == IR_INST_LEAS) {
				reg_t *r1 = ins->r1;
				ins->r1 = ins->r2;
				ins->type = IR_INST_STORES;
				ins->imm = -r1->imm;
				changed = 1;
			}
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
int ir_stackreduce(ir_func_t *func)
{
	int changed = 0;
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		ir_inst_t *nxt = blk->insts;
		for(ir_inst_t *ins = blk->insts; ins; ins = nxt) {
			nxt = ins->next;

			if(ins && nxt && ins->type == IR_INST_STORES &&
			   nxt->type == IR_INST_LOADS && ins->imm == nxt->imm &&
			   !ins->noopt && !nxt->noopt && ins->size == nxt->size &&
			   !nxt->sign_ext) {
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

static int ins_is_mem(enum ins_type t)
{
	return t == IR_INST_LOAD || t == IR_INST_LOADS || t == IR_INST_STORE ||
		   t == IR_INST_STORES;
}

static int ir_memopt_ins(ir_inst_t *ins)
{
	int change = 0;
	ir_inst_t *nxt = ins->next_mem;

	/* rewrite
	 * %r0 = load %adr
	 * store %adr, %r0
	 * ->
	 * nop
	 * store %adr, %r0
	 */
	if(nxt && ins->type == IR_INST_LOAD && nxt->type == IR_INST_STORE &&
	   ins->r0 == nxt->r2 && ins->r1 == nxt->r1 && ins->size == nxt->size) {
		ins->type = IR_INST_NOP;
		change = 1;
	}

	/* rewrite
	 * store %adr, %r0
	 * %r1 = load %adr
	 * ->
	 * store %adr, %r0
	 * %r1 = %r0
	 */
	if(nxt && ins->type == IR_INST_STORE && nxt->type == IR_INST_LOAD &&
	   ins->r1 == nxt->r1 && ins->size == nxt->size) {
		nxt->type = IR_INST_MOV;
		nxt->r1 = ins->r2;
		change = 1;
	}

	/* rewrite
	 * %r0 = loads #adr
	 * stores #adr, %r0
	 * ->
	 * nop
	 * stores #adr, %r0
	 */
	if(nxt && ins->type == IR_INST_LOADS && nxt->type == IR_INST_STORES &&
	   ins->r0 == nxt->r1 && ins->imm == nxt->imm && ins->size == nxt->size) {
		ins->type = IR_INST_NOP;
		change = 1;
	}

	/* rewrite
	 * stores #adr, %r0
	 * %r1 = loads #adr
	 * ->
	 * stores #adr, %r0
	 * %r1 = %r0
	 */
	if(nxt && ins->type == IR_INST_STORES && nxt->type == IR_INST_LOADS &&
	   ins->imm == nxt->imm && ins->size == nxt->size) {
		nxt->type = IR_INST_MOV;
		nxt->r1 = ins->r1;
		change = 1;
	}

	/* rewrite
	 * %r1 = loads #adr
	 * %r2 = loads #adr
	 * ->
	 * %r1 = loads #adr
	 * %r2 = %r1
	 */
	if(nxt && ins->type == IR_INST_LOADS && nxt->type == IR_INST_LOADS &&
	   ins->imm == nxt->imm && ins->sign_ext == nxt->sign_ext &&
	   ins->size == nxt->size) {
		nxt->type = IR_INST_MOV;
		nxt->r1 = ins->r0;
		change = 1;
	}

	/* rewrite
	 * %r1 = load %adr
	 * %r2 = load %adr
	 * ->
	 * %r1 = load %adr
	 * %r2 = %r1
	 */
	if(nxt && ins->type == IR_INST_LOAD && nxt->type == IR_INST_LOAD &&
	   ins->r1 == nxt->r1 && ins->sign_ext == nxt->sign_ext &&
	   ins->size == nxt->size) {
		nxt->type = IR_INST_MOV;
		nxt->r1 = ins->r0;
		change = 1;
	}

	/* rewrite
	 * stores #adr, %r0
	 * stores #adr, %r1
	 * ->
	 * nop
	 * stores #adr, %r1
	 */
	if(nxt && ins->type == IR_INST_STORES && nxt->type == IR_INST_STORES &&
	   ins->imm == nxt->imm && ins->size == nxt->size) {
		ins->type = IR_INST_NOP;
		change = 1;
	}

	/* rewrite
	 * store %adr, %r0
	 * store %adr, %r1
	 * ->
	 * nop
	 * store %adr, %r1
	 */
	if(nxt && ins->type == IR_INST_STORE && nxt->type == IR_INST_STORE &&
	   ins->r1 == nxt->r1 && ins->size == nxt->size) {
		ins->type = IR_INST_NOP;
		change = 1;
	}

	/* rewrite
	 * %r0 = load.(spec) %r1
	 * ..
	 * %r2 = ext.(spec) %r0
	 * ->
	 * %r0 = load.(spec) %r1
	 * %r2 = %r0
	 */
	if((ins->type == IR_INST_ZXT || ins->type == IR_INST_SXT) &&
	   (ins->r1->insty == IR_INST_LOAD || ins->r1->insty == IR_INST_LOADS)) {
		reg_t *r = ins->r1;
		if(r->is_32bit == ins->is_32bit && r->size == ins->size) {
			ins->type = IR_INST_MOV;
			change = 1;
		}
	}

	/* rewrite
	 * store(SIZE) %adr, %r0
	 * where
	 * %r0 = sign_ext/zero_ext.(SIZE) %r1
	 * ->
	 * store(SIZE) %adr, %r1
	 */
	if(ins->type == IR_INST_STORE || ins->type == IR_INST_STORES) {
		reg_t *data = ins->type == IR_INST_STORE ? ins->r2 : ins->r1;
		if((data->insty == IR_INST_SXT || data->insty == IR_INST_ZXT) &&
		   data->size == ins->size) {
			if(data->size == 8 && data->is_32bit != ins->is_32bit) {
				goto out;
			}
			data = data->lhs;
			change = 1;
		}
		if(ins->type == IR_INST_STORE) {
			ins->r2 = data;
		} else {
			ins->r1 = data;
		}
	}
out:

	return change;
}

static void build_next_mem_ins(ir_func_t *func)
{
	/* reset all next_mem */
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *inst = blk->insts; inst; inst = inst->next) {
			inst->next_mem = NULL;
		}
	}

	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		ir_inst_t *prev = NULL;
		for(ir_inst_t *inst = blk->insts; inst; inst = inst->next) {
			if(!prev && ins_is_mem(inst->type)) {
				prev = inst;
				continue;
			}

			if(ins_is_mem(inst->type)) {
				prev->next_mem = inst;
			} else {
				continue;
			}

			prev = inst;
		}
	}
}

int ir_memopt(ir_func_t *func)
{
	int change = 0;

	ir_nopremover(func);
	build_next_mem_ins(func);
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			change |= ir_memopt_ins(ins);
		}
	}
	return change;
}
