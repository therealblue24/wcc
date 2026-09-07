#include "bird.h"
#include "ir.h"

static bool inst_is_pinnable_ty(enum ins_type t)
{
	return t == IR_INST_PHI || t == IR_INST_CALL | t == IR_INST_LOAD ||
		   t == IR_INST_LOADS || t == IR_INST_STORE || t == IR_INST_STORES ||
		   ir_inst_is_term(t);
}

static bool inst_is_pinnable(ir_inst_t *ins)
{
	if(ins->noopt) {
		return true;
	}
	return inst_is_pinnable_ty(ins->type);
}

static void sched_early(ir_inst_t *ins)
{
	if(ins->visited) {
		return;
	}
	ins->visited = true;
	ins->early = ins->r0->from_blk->idom;
	/* schedule inputs */
	if(ins->r1) {
		sched_early(ins->r1->from);
		if(ins->early->dom_depth < ins->r1->from->early->dom_depth) {
			ins->early = ins->r1->from->early;
		}
	}
	if(ins->r2) {
		sched_early(ins->r2->from);
		if(ins->early->dom_depth < ins->r2->from->early->dom_depth) {
			ins->early = ins->r2->from->early;
		}
	}
}

static void sched_late(ir_inst_t *ins)
{
	if(ins->visited) {
		return;
	}
	ins->visited = true;
	if(!ins->r0) {
		return;
	}
	ir_blk_t *lca = NULL;
	for(size_t i = 0; i < list_len(ins->r0->uses); i++) {
		ir_inst_t *y = ins->r0->uses[i];
		sched_late(y);
		ir_blk_t *use = y->blk;
		if(y->type == IR_INST_PHI) {
			size_t j;
			for(j = 0; j < list_len(y->phi_args); j++) {
				if(y->phi_args[j] == ins->r0) {
					break;
				}
			}
			use = y->phi_preds[j];
		}
		lca = ir_blk_lca(lca, use);
	}

	/* find best block, in shallowest loop nest */
	ir_blk_t *best = lca;
	while(lca != ins->early) {
		if(lca->loop_nest < best->loop_nest) {
			best = lca;
		}
		lca = lca->idom;
	}

	if(lca->loop_nest < best->loop_nest) {
		best = lca;
	}

	ins->blk = best;

	return;
}

static void find_before_last_term_ins(ir_blk_t *blk)
{
	if(!blk->insts->next) {
		/* insert NOP */
		ir_inst_t *nop = ins_nop();
		nop->next = blk->insts;
		blk->insts = nop;
	}
	ir_inst_t *prev = blk->insts;
	ir_inst_t *ret = prev->next;
	for(; ret; ret = ret->next) {
		if(ir_inst_is_term(ret->type)) {
			blk->tail = ret;
			blk->tailprev = prev;
			return;
		}
		prev = ret;
	}
	ASSERT(ir_inst_is_term(ret->type), "IR is not constructed properly");
	return;
}

/* requires: flow, dom, loopnest */
int ir_gcm(ir_func_t *func)
{
	/* step 1 - pin instructions */
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			ins->pinned = inst_is_pinnable(ins);
			ins->blk = ins->early = ins->late = blk;
			ins->visited = false;
		}
	}

	printf("Before GCM:\n");
	ir_dump(func, 'v');

	/* step 2 - sched early */
	for(size_t ip = list_len(func->blocks) - 1; ip >= 0; ip--) {
		size_t i = func->blocks[ip]->postnum;
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			if(!ins->pinned) {
				continue;
			}
			ins->visited = true;
			/* handle inputs */
			if(ins->r1) {
				sched_early(ins->r1->from);
			}
			if(ins->r2) {
				sched_early(ins->r2->from);
			}
			if(ins->type == IR_INST_PHI) {
				for(size_t j = 0; j < list_len(ins->phi_args); j++) {
					sched_early(ins->phi_args[j]->from);
				}
			}
			if(ins->type == IR_INST_CALL) {
				for(size_t j = 0; j < list_len(ins->call_args); j++) {
					sched_early(ins->call_args[j]->r->from);
				}
			}
		}
		if(ip == 0) {
			break;
		}
	}

	/* clear visited */
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			ins->visited = false;
		}
	}

	/* step 3 - sched late */
	for(size_t ip = list_len(func->blocks) - 1; ip >= 0; ip--) {
		size_t i = func->blocks[ip]->postnum;
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			if(!ins->pinned) {
				continue;
			}
			ins->visited = true;
			if(ins->r0) {
				for(size_t i = 0; i < list_len(ins->r0->uses); i++) {
					sched_late(ins->r0->uses[i]);
				}
			}
		}
		if(ip == 0) {
			break;
		}
	}

	/* clear visited, mark ends, add NOPs */
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		ir_inst_t *nop = ins_nop();
		nop->next = blk->insts;
		blk->insts = nop;
		find_before_last_term_ins(blk);
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			ins->visited = false;
		}
	}

	/* move instructions */
	bool change = true;
	while(change) {
		change = false;
		for(size_t i = 0; i < list_len(func->blocks); i++) {
			ir_blk_t *blk = func->blocks[i];
			ir_inst_t *prev = blk->insts;
			ir_inst_t *nxt;
			for(ir_inst_t *ins = blk->insts->next; ins; ins = nxt) {
				nxt = ins->next;

				if(ins->pinned) {
					ins->visited = true;
					goto cont;
				}
				if(ins->blk == blk) {
					goto cont;
				}

				bool resolved1 = ins->r1 ? ins->r1->from->visited : true;
				bool resolved2 = ins->r2 ? ins->r2->from->visited : true;
				if(resolved1 && resolved2) {
					change = true;
					ins->visited = true;

					/* move it */
					ir_blk_t *target = ins->blk;
					target->tailprev->next = ins;
					ins->next = target->tail;
					ins->blk = target;
					target->tailprev = ins;
					prev->next = nxt;
					ins = prev;
				}

cont:
				prev = ins;
			}
		}
	}

	/* TODO: handle use before def errors */

	printf("After GCM:\n");
	ir_dump(func, 'v');
	/* all done */
	return 0;
}
