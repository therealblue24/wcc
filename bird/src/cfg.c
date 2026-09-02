#include "bird.h"
#include "zz/list.h"
#include "zz/set.h"

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

int ir_phiopt(ir_func_t *func)
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

int ir_branchopt(ir_func_t *func)
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
				ins->is_32bit = cmp->is_32bit;
				changed = 1;
			}
		}
	}
	return changed;
}

/* "A Simple, Fast Dominance Algorithm" by Keith D. Cooper, Timothy J. Harvey, and Ken Kennedy. */
/* straight up copy of "Figure 3: The Engineered Algorithm" */

static ir_blk_t *intersect(ir_blk_t *b1, ir_blk_t *b2)
{
	/* remember, we have rpo calculated, so we have to flip
	 * signs as we are comparing postorder here */
	while(b1 != b2) {
		while(b1->postnum > b2->postnum) {
			b1 = b1->idom;
		}

		while(b2->postnum > b1->postnum) {
			b2 = b2->idom;
		}
	}

	return b1;
}

/* requires ir_blk_rpo, ir_blk_flow (preserves both) */
void ir_blk_dom(ir_func_t *fun)
{
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		fun->blocks[i]->idom = NULL;
		fun->blocks[i]->dom_depth = 0;
	}
	fun->blocks[0]->idom = fun->blocks[0];

	bool changed = true;
	while(changed) {
		changed = false;
		for(size_t ip = 0; ip < list_len(fun->blocks); ip++) {
			size_t i = fun->blocks[ip]->postnum;
			ir_blk_t *blk = fun->blocks[i];
			if(i == 0 || list_len(blk->pred) == 0) {
				continue;
			}

			/* select first processed predeccesor */
			ir_blk_t *sel = blk->pred[0];
			for(size_t j = 0; j < list_len(blk->pred); j++) {
				if(blk->pred[j]->idom) {
					sel = blk->pred[j];
					break;
				}
			}

			ir_blk_t *new_idom = sel;
			for(size_t j = 0; j < list_len(blk->pred); j++) {
				ir_blk_t *pred = blk->pred[j];
				if(!pred->idom || pred == sel)
					continue;
				new_idom = intersect(pred, new_idom);
			}

			if(blk->idom != new_idom) {
				blk->idom = new_idom;
				changed = true;
			}
		}
	}

	/* compute depths */
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		ir_blk_t *idom = blk->idom;
		if(idom == blk || idom == NULL) {
			blk->dom_depth = 0;
			continue;
		}
		blk->dom_depth = idom->dom_depth + 1;
	}

	return;
}

/* requires ir_blk_dom : info cannot coexist with ir_blk_domf */
void ir_blk_domtree(ir_func_t *fun)
{
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		list_hdr(blk->dom)->size = 0;
	}

	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		/* entry block moment */
		if(blk->idom == blk || blk->idom == NULL) {
			continue;
		}

		list_append(blk->idom->dom, blk);
	}

	return;
}

/* requires ir_blk_dom : info cannot coexist with ir_blk_domtree */
void ir_blk_domf(ir_func_t *fun)
{
	/* like above, straight up copy of "Figure 5: The Dominance-Frontier Algorithm" */

	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		list_hdr(blk->dom)->size = 0;
	}

	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		if(list_len(blk->pred) < 2) {
			continue;
		}

		for(size_t j = 0; j < list_len(blk->pred); j++) {
			ir_blk_t *runner = blk->pred[j];
			while(runner != blk->idom) {
				list_append(runner->dom, blk);
				runner = runner->idom;
			}
		}
	}
}

/* finds least common ancestor of b1 and b2
 * requires ir_blk_dom */
/* Global Code Motion [and] Global Value Numbering by Cliff Click
 * is where this impl comes from */
ir_blk_t *ir_blk_lca(ir_blk_t *b1, ir_blk_t *b2)
{
	if(!b1) {
		return b2;
	}
	if(!b2) {
		return b1;
	}

	while(b1->dom_depth > b2->dom_depth) {
		b1 = b1->idom;
	}
	while(b2->dom_depth > b1->dom_depth) {
		b2 = b2->idom;
	}

	while(b1 != b2) {
		b1 = b1->idom;
		b2 = b2->idom;
	}

	return b1;
}

/* does `blk` dominate `other`? */
bool ir_blk_does_dom(ir_blk_t *blk, ir_blk_t *other)
{
	if(blk == other) {
		return true;
	}

	/* climb up tree */
	while(other != other->idom) {
		other = other->idom;
		if(other == NULL) {
			return false;
		} else if(other == blk) {
			return true;
		}
	}

	return false;
}

/* marks a loop */
static void traverse(ir_blk_t *blk, ir_blk_t *hdr)
{
	if(blk == hdr || blk->visited) {
		return;
	}
	blk->visited = true;

	blk->loop_nest++;
	for(size_t i = 0; i < list_len(blk->pred); i++) {
		traverse(blk->pred[i], hdr);
	}
	return;
}

static int handleblk(ir_blk_t *blk, ir_blk_t *succ)
{
	if(ir_blk_does_dom(succ, blk)) {
		blk->loop_nest++;
		succ->loop_nest++;
		for(size_t i = 0; i < list_len(blk->pred); i++) {
			traverse(blk->pred[i], succ);
		}
		return 1;
	}
	return 0;
}

/* computes loop nesting counts */
/* requires ir_blk_flow */
void ir_blk_loopnest(ir_func_t *func)
{
	/* clear counts */
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		blk->visited = false;
		blk->loop_nest = 0;
	}

	/* find loops */
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(size_t j = 0; j < list_len(blk->succ); j++) {
			if(handleblk(blk, blk->succ[j])) {
				for(size_t k = 0; k < list_len(func->blocks); k++) {
					func->blocks[j]->visited = false;
				}
			}
		}
	}
}
