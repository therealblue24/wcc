#include "bird.h"
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
			b1 = b1->dom;
		}

		while(b2->postnum > b1->postnum) {
			b2 = b2->dom;
		}
	}

	return b1;
}

/* requires ir_blk_rpo, ir_blk_flow (preserves both) */
void ir_blk_dom(ir_func_t *fun)
{
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		fun->blocks[i]->dom = NULL;
	}
	fun->blocks[0]->dom = fun->blocks[0];

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
				if(blk->pred[j]->dom) {
					sel = blk->pred[j];
					break;
				}
			}

			ir_blk_t *new_idom = sel;
			for(size_t j = 0; j < list_len(blk->pred); j++) {
				ir_blk_t *pred = blk->pred[j];
				if(!pred->dom || pred == sel)
					continue;
				new_idom = intersect(pred, new_idom);
			}

			if(blk->dom != new_idom) {
				blk->dom = new_idom;
				changed = true;
			}
		}
	}

	return;
}
