/* Implementation of the "Simple and Efficient Construction of Static Single Assignment Form" algorithm, by Matthias Braun, Sebastian Buchwald, Sebastian Hack, Roland Leißa, Christoph Mallon, and Andreas Zwinkau. */

#include "bird.h"
#include "ir.h"
#include "live.h"
#include "zz/list.h"
#include <stdint.h>

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
}

static bool has_long(LIST(long) list, long want)
{
	for(size_t i = 0; i < list_len(list); i++) {
		if(list[i] == want) {
			return true;
		}
	}
	return false;
}

static bool has_inst(LIST(ir_inst_t *) list, ir_inst_t *want)
{
	for(size_t i = 0; i < list_len(list); i++) {
		if(list[i] == want) {
			return true;
		}
	}
	return false;
}

static bool has_blk(LIST(ir_blk_t *) list, ir_blk_t *want)
{
	for(size_t i = 0; i < list_len(list); i++) {
		if(list[i] == want) {
			return true;
		}
	}
	return false;
}

static bool has_blkreg(LIST(blkreg_t *) list, ir_blk_t *blk)
{
	if(!list) {
		return false;
	}
	for(size_t i = 0; i < list_len(list); i++) {
		if(list[i]->blk == blk) {
			return true;
		}
	}
	return false;
}

static size_t find_blkreg(LIST(blkreg_t *) list, ir_blk_t *blk)
{
	for(size_t i = 0; i < list_len(list); i++) {
		if(list[i]->blk == blk) {
			return i;
		}
	}
	return -1;
}

static size_t find_blk(LIST(ir_blk_t *) list, ir_blk_t *blk)
{
	for(size_t i = 0; i < list_len(list); i++) {
		if(list[i] == blk) {
			return i;
		}
	}
	return -1;
}

/* The postorder computation was inspired from (here)[https://github.com/sampsyo/bril/blob/main/examples/dom.py#L34]. */

static void union_(LIST(long) l, long x)
{
	if(!has_long(l, x)) {
		list_append(l, x);
	}
}

static void postorder_visit(LIST(long) postorder, ir_blk_t *blk)
{
	if(blk->visited) {
		return;
	}

	blk->visited = true;

	if(blk->tail->true_blk) {
		// union_(postorder, blk->tail->true_blk->num);
		postorder_visit(postorder, blk->tail->true_blk);
	}
	if(blk->tail->false_blk) {
		// union_(postorder, blk->tail->false_blk->num);
		postorder_visit(postorder, blk->tail->false_blk);
	}

	union_(postorder, blk->num);

	return;
}

static LIST(long) postorder_get(LIST(ir_blk_t *) blocks)
{
	LIST(long) postorder = list_make(long);
	/* clear if blocks are visited */
	for(size_t i = 0; i < list_len(blocks); i++) {
		blocks[i]->visited = false;
	}

	postorder_visit(postorder, blocks[0]);

	/* clear if blocks are visited */
	for(size_t i = 0; i < list_len(blocks); i++) {
		blocks[i]->visited = false;
	}
	return postorder;
}

static blkreg_t *blkreg_make(ir_blk_t *blk, reg_t *reg)
{
	blkreg_t *r = zalloc(sizeof(blkreg_t));
	r->blk = blk;
	r->reg = reg;

	return r;
}

static reg_t *ssa_tmp(reg_t *var)
{
	reg_t *r = reg_make();
	r->ssareg = var;
	return r;
}

static reg_t *find_var(reg_t *reg)
{
	reg_t *r = reg;
	while(r->ssareg) {
		r = r->ssareg;
	}
	return r;
}

static ir_func_t *func_phi;
static LIST(ir_blk_t *) sealed_blks;

static reg_t *write_reg(ir_blk_t *blk, reg_t *reg, reg_t *val)
{
	reg_t *var = find_var(reg);

	if(!has_blkreg(var->blkregs, blk)) {
		list_append(var->blkregs, blkreg_make(blk, val));
	} else {
		var->blkregs[find_blkreg(var->blkregs, blk)]->reg = val;
	}

	return val;
}

static ir_inst_t *insert_phi(ir_blk_t *blk)
{
	ir_inst_t *phi = ir_inst_make(IR_INST_PHI, NULL, NULL, NULL, 0);
	phi->phi_args = list_make(reg_t *);
	phi->phi_preds = list_make(ir_blk_t *);
	for(size_t i = 0; i < list_len(blk->pred); i++) {
		list_append(phi->phi_preds, blk->pred[i]);
	}
	phi->next = blk->insts->next;
	blk->insts->next = phi;
	return phi;
}

static reg_t *read_reg(ir_blk_t *blk, reg_t *reg);

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

static reg_t *try_remove_trivial_phi(ir_inst_t *phi)
{
	if(list_len(phi->phi_args) == 1) {
		phi->type = IR_INST_MOV;
		phi->r1 = phi->phi_args[0];
		list_delete(phi->phi_args);
		list_delete(phi->phi_preds);
		return phi->r0;
	}

	reg_t *same = NULL;
	for(size_t i = 0; i < list_len(phi->phi_args); i++) {
		reg_t *op = phi->phi_args[i];
		if((same && ins_is_same(op->from, same->from)) || op == phi->r0) {
			continue;
		}

		if(!same) {
			/* phi merges at least 2 values */
			return phi->r0;
		}

		same = op;
	}

	if(!same) {
		/* DCE will take care of this. */
		return phi->r0;
	}

	/* replace all uses of `phi->r0` to `same` */
	phi->type = IR_INST_NOP;
	list_delete(phi->phi_args);
	list_delete(phi->phi_preds);

#define REPLACE(x)                       \
	do {                                 \
		if((x) && (x) == phi->r0) {      \
			if(!has_inst(users, ins)) {  \
				list_append(users, ins); \
			}                            \
			(x) = same;                  \
		}                                \
	} while(0)

	LIST(ir_inst_t *) users = list_make(ir_inst_t *);

	for(size_t i = 0; i < list_len(func_phi->blocks); i++) {
		ir_blk_t *blk = func_phi->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			REPLACE(ins->r0);
			REPLACE(ins->r1);
			REPLACE(ins->r2);
			if(ins->type == IR_INST_CALL) {
				for(size_t j = 0; j < list_len(ins->call_args); j++) {
					REPLACE(ins->call_args[j]->r);
				}
			}
			if(ins->type == IR_INST_PHI) {
				for(size_t j = 0; j < list_len(ins->phi_args); j++) {
					REPLACE(ins->phi_args[j]);
				}
			}
		}
	}

	/* try recursively remove phi users */
	for(size_t i = 0; i < list_len(users); i++) {
		if(users[i]->type != IR_INST_PHI) {
			continue;
		}
		(void)try_remove_trivial_phi(users[i]);
	}

	list_delete(users);

	return same;
}

static reg_t *add_phi_ops(ir_blk_t *blk, reg_t *var, ir_inst_t *phi)
{
	for(size_t i = 0; i < list_len(blk->pred); i++) {
		list_append(phi->phi_args, read_reg(blk->pred[i], var));
	}
	return try_remove_trivial_phi(phi);
}

static reg_t *read_var_rec(ir_blk_t *blk, reg_t *reg)
{
	reg_t *val;
	ir_inst_t *phi = NULL;
	if(!has_blk(sealed_blks, blk)) {
		phi = insert_phi(blk);
		val = phi->r0 = ssa_tmp(reg);
		phi->r0->from = phi;
		list_append(blk->incomplete_phis, phi);
	} else if(list_len(blk->pred) == 1) {
		val = read_reg(blk->pred[0], reg);
	} else {
		/* insert placeholder phi */
		phi = insert_phi(blk);
		phi->r0 = write_reg(blk, reg, ssa_tmp(reg));
		phi->r0->from = phi;
		val = add_phi_ops(blk, reg, phi);
	}
	write_reg(blk, reg, val);
	return val;
}

static reg_t *read_reg(ir_blk_t *blk, reg_t *reg)
{
	reg_t *var = find_var(reg);

	if(has_blkreg(var->blkregs, blk)) {
		return var->blkregs[find_blkreg(var->blkregs, blk)]->reg;
	}

	return read_var_rec(blk, reg);
}

static void seal_block(ir_blk_t *blk)
{
	if(has_blk(sealed_blks, blk)) {
		return;
	}

	for(size_t i = 0; i < list_len(blk->incomplete_phis); i++) {
		ir_inst_t *phi = blk->incomplete_phis[i];
		add_phi_ops(blk, phi->r0, phi);
	}
	list_append(sealed_blks, blk);
}

static bool has_any_phis(ir_blk_t *blk)
{
	for(ir_inst_t *inst = blk->insts; inst; inst = inst->next) {
		if(inst->type == IR_INST_PHI) {
			return true;
		}
	}
	return false;
}

static void replace_edge(ir_blk_t *blk, ir_blk_t *orig, ir_blk_t *replace)
{
	ir_inst_t *last = blk->tail;
	if(last->true_blk && last->true_blk == orig) {
		last->true_blk = replace;
	}
	if(last->false_blk && last->false_blk == orig) {
		last->false_blk = replace;
	}
	return;
}

static void replace_phis(ir_blk_t *blk, ir_blk_t *orig, ir_blk_t *replace)
{
	for(ir_inst_t *inst = blk->insts; inst; inst = inst->next) {
		if(inst->type != IR_INST_PHI) {
			continue;
		}

		for(size_t i = 0; i < list_len(inst->phi_args); i++) {
			if(inst->phi_preds[i] == orig) {
				inst->phi_preds[i] = replace;
			}
		}
	}
	return;
}

typedef struct edge {
	ir_blk_t *from; /* pred */
	ir_blk_t *to; /* succ */
} edge_t;

static bool has_edge(LIST(edge_t) list, edge_t want)
{
	for(size_t i = 0; i < list_len(list); i++) {
		if(list[i].from == want.from && list[i].to == want.to) {
			return true;
		}
	}
	return false;
}

/* pre-step to destroying SSA form */
static void split_critical(ir_func_t *fun)
{
	/* append NOP to each block, find befores */
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		ir_inst_t *nop = ins_nop();
		nop->next = blk->insts;
		blk->insts = nop;
		find_before_last_term_ins(blk);
	}

	long counter = fun->blocks[list_len(fun->blocks) - 1]->num;

	LIST(edge_t) edges = list_make(edge_t);

	/* collect all edges */
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		if(!has_any_phis(blk)) {
			continue;
		}
		for(size_t i = 0; i < list_len(blk->pred); i++) {
			edge_t edge = { .from = blk->pred[i], .to = blk };
			if(!has_edge(edges, edge)) {
				list_append(edges, edge);
			}
		}
	}

	/* iterate over the edges */
	for(size_t i = 0; i < list_len(edges); i++) {
		edge_t edge = edges[i];

		/* split the edge if:
		 * - pred has several outgoing edges
		 * - succ has several preds
		 * - succ has any phis */
		if(list_len(edge.from->succ) > 1 && list_len(edge.to->pred) > 1) {
			ir_inst_t *jmp = ins_jmp(edge.to);
			ir_inst_t *nop = ins_nop();
			nop->next = jmp;
			ir_blk_t *critical = ir_blk_make(nop);
			critical->num = ++counter;
			find_before_last_term_ins(critical);
			list_append(fun->blocks, critical);
			replace_edge(edge.from, edge.to, critical);
			replace_phis(edge.to, edge.from, critical);
			edge.from = critical;
		}
	}

	list_delete(edges);

	return;
}

/* isolates phi functions */
/* Algorithm 21.1 from the SSA book [https://pfalcon.github.io/ssabook/latest/book-full.pdf] */
static void isolate_phis(ir_func_t *fun)
{
	/* create parallel moves at each block, 1 at end and 1 after phis */
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		ir_inst_t *beg_pmov = blk->insts;
		beg_pmov->type = IR_INST_PMOV;
		ir_inst_t *end_pmov = ir_inst_make(IR_INST_PMOV, NULL, NULL, NULL, 0);

		beg_pmov->pmov_args = list_make(reg_pmov_t);
		end_pmov->pmov_args = list_make(reg_pmov_t);

		end_pmov->next = blk->tail;
		blk->tailprev->next = end_pmov;
		blk->tailprev = end_pmov;
	}

	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		for(ir_inst_t *inst = blk->insts; inst; inst = inst->next) {
			if(inst->type != IR_INST_PHI) {
				continue;
			}

			for(size_t j = 0; j < list_len(inst->phi_args); j++) {
				reg_t *arg = ir_find(inst->phi_args[j]);
				ir_inst_t *pmov = inst->phi_preds[j]->tailprev;
				reg_t *fresh = reg_make();
				list_append(pmov->pmov_args,
							((reg_pmov_t){ .dst = fresh, .src = arg }));

				inst->phi_args[j] = fresh;
			}

			ir_inst_t *pmov = blk->insts;

			reg_t *fresh = reg_make();

			list_append(
				pmov->pmov_args,
				((reg_pmov_t){ .dst = ir_find(inst->r0), .src = fresh }));
			inst->r0 = fresh;

			/* coalesce args, delete phi */
			for(size_t j = 0; j < list_len(inst->phi_args); j++) {
				ir_union(inst->phi_args[j], inst->r0);
			}

			list_delete(inst->phi_args);
			list_delete(inst->phi_preds);
			inst->type = IR_INST_NOP;
		}
	}

	ir_rewrite(fun);
	return;
}

static void cleanup_ins(ir_inst_t *ins, ir_blk_t *thisblk)
{
	/* simplify dead block jumps */
	if(ir_inst_is_br(ins->type) || ins->type == IR_INST_JMP) {
		ir_inst_t *first = ins->true_blk->insts;
		if(first->type == IR_INST_JMP && first->true_blk != ins->true_blk) {
			ir_blk_t *blk = first->true_blk;
			ir_reroute_pred(blk, ins->true_blk, thisblk);
			ins->true_blk = blk;
		}
	}

	if(ir_inst_is_br(ins->type)) {
		ir_inst_t *first = ins->false_blk->insts;
		if(first->type == IR_INST_JMP && first->true_blk != ins->false_blk) {
			ir_blk_t *blk = first->true_blk;
			ir_reroute_pred(blk, ins->false_blk, thisblk);
			ins->false_blk = blk;
		}
	}

	return;
}

static void cleanup_critical_jumps(ir_func_t *func)
{
	ir_nopremover(func);
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *inst = blk->insts; inst; inst = inst->next) {
			cleanup_ins(inst, blk);
		}
	}
	return;
}

/* Leroy's algorithm for sequentializing parallel moves. */
/* Couldn't find a paper/description for it, but https://github.com/tekknolagi/tekknolagi.github.com/blob/main/_posts/2025-08-13-linear-scan.md seems to have the implementation. */

enum leroy_status {
	TO_MOVE = 0,
	BEING_MOVED,
	MOVED,
};

static void move_one(reg_t **src, reg_t **dst, uint8_t *status, size_t i,
					 size_t len, LIST(reg_pmov_t) * seq, reg_t *tmp)
{
	if(src[i] == dst[i]) {
		return;
	}
	status[i] = BEING_MOVED;
	for(size_t j = 0; j < len; j++) {
		if(src[j] == dst[j]) {
			switch(status[j]) {
			case TO_MOVE:
				move_one(src, dst, status, j, len, seq, tmp);
				break;
			case BEING_MOVED: {
				list_append(*seq, ((reg_pmov_t){ .dst = tmp, .src = src[j] }));
				src[j] = tmp;
			}; break;
			default:
				break;
			}
		}
	}

	list_append(*seq, ((reg_pmov_t){ .dst = dst[i], .src = src[i] }));
	status[i] = MOVED;
	return;
}

static LIST(reg_pmov_t) deparallelize_pmov(ir_inst_t *pmov)
{
	LIST(reg_pmov_t) seq = list_make(reg_pmov_t);
	reg_t *tmp = reg_make();

	if(list_len(pmov->pmov_args) == 0) {
		return seq;
	}

	size_t len = list_len(pmov->pmov_args);
	reg_t **src = zcalloc(len, sizeof(reg_t *));
	reg_t **dst = zcalloc(len, sizeof(reg_t *));
	uint8_t *status = zcalloc(len, 1);

	for(size_t i = 0; i < list_len(pmov->pmov_args); i++) {
		src[i] = pmov->pmov_args[i].src;
		dst[i] = pmov->pmov_args[i].dst;
		status[i] = TO_MOVE;
	}

	for(size_t i = 0; i < len; i++) {
		if(status[i] == TO_MOVE) {
			move_one(src, dst, status, i, len, &seq, tmp);
		}
	}

	free(dst);
	free(src);
	free(status);

	return seq;
}

/* turns parallel moves -> sequential moves */
static void deparallelize_pmovs(ir_func_t *fun)
{
	/* append NOP to each block, find befores */
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		ir_inst_t *nop = ins_nop();
		nop->next = blk->insts;
		blk->insts = nop;
		find_before_last_term_ins(blk);
	}

	ir_inst_t **movs = NULL;
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		for(ir_inst_t *inst = blk->insts->next; inst; inst = inst->next) {
			if(inst->type != IR_INST_PMOV) {
				continue;
			}

			inst->type = IR_INST_NOP;
			if(list_len(inst->pmov_args) == 0) {
				list_delete(inst->pmov_args);
				continue;
			}

			LIST(reg_pmov_t) seq = deparallelize_pmov(inst);
			list_delete(inst->pmov_args);
			if(list_len(seq) == 0) {
				list_delete(seq);
				continue;
			}
			movs = zcalloc(sizeof(ir_inst_t *), list_len(seq));
			for(size_t i = 0; i < list_len(seq); i++) {
				reg_pmov_t pmov1 = seq[i];
				ir_inst_t *mov = ins_mov(pmov1.dst, pmov1.src);
				if(pmov1.src->is_32bit || pmov1.dst->is_32bit) {
					mov->is_32bit = true;
				}
				movs[i] = mov;
			}
			for(size_t i = 0; i < list_len(seq) - 1; i++) {
				movs[i]->next = movs[i + 1];
			}
			movs[list_len(seq) - 1]->next = inst->next;
			ir_inst_t *nxt = inst->next;
			inst->next = movs[0];
			inst = nxt;
			free(movs);
			list_delete(seq);
		}
	}
}

void ir_ssa_enter(ir_func_t *fun)
{
	prof_begin("ssa");
	reg_reset_counter();
	ir_fix(fun);
	ir_nopremover(fun);
	ir_blk_flow(fun);

	sealed_blks = list_make(ir_blk_t *);

	/* append 2 NOPs to each block, find befores */
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		ir_inst_t *nop = ins_nop();
		nop->next = blk->insts;
		blk->insts = nop;
		find_before_last_term_ins(blk);
	}

	LIST(reg_t *) allocated = list_make(reg_t *);

	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		for(ir_inst_t *inst = blk->insts; inst; inst = inst->next) {
			if(inst->r0 && !inst->r0->blkregs) {
				inst->r0->blkregs = list_make(blkreg_t *);
				list_append(allocated, inst->r0);
			}
		}
	}

	LIST(long) postorder = postorder_get(fun->blocks);

	for(size_t ip = 0; ip < list_len(postorder); ip++) {
		size_t i = postorder[ip];
		ir_blk_t *blk = fun->blocks[i];
		for(ir_inst_t *inst = blk->insts; inst; inst = inst->next) {
			if(inst->r0) {
				inst->r0 = write_reg(blk, inst->r0, ssa_tmp(inst->r0));
			}
			if(inst->r1) {
				inst->r1 = read_reg(blk, inst->r1);
			}
			if(inst->r2) {
				inst->r2 = read_reg(blk, inst->r2);
			}
			if(inst->type == IR_INST_CALL) {
				for(size_t i = 0; i < list_len(inst->call_args); i++) {
					inst->call_args[i]->r =
						read_reg(blk, inst->call_args[i]->r);
				}
			}
		}
	}

	for(size_t i = 0; i < list_len(postorder); i++) {
		seal_block(fun->blocks[i]);
	}
	ir_fix(fun);

	for(size_t i = 0; i < list_len(allocated); i++) {
		for(size_t j = 0; j < list_len(allocated[i]->blkregs); j++) {
			free(allocated[i]->blkregs[j]);
		}
		list_delete(allocated[i]->blkregs);
	}

	list_delete(sealed_blks);
	list_delete(postorder);
	list_delete(allocated);

	prof_end();
	return;
}

/* alg
 * visit(blk):
 *     if blk->visited return
 *     order = order U blk
 *     order = order U blk->true
 *     order = order U blk->false
 *     blk->visited = true
 *     visit order->true
 *     visit order->false
 */

static long cur_postnum = 0;

#define unionB(b)                         \
	do {                                  \
		if((b)->postnum == -1) {          \
			(b)->postnum = cur_postnum++; \
		}                                 \
	} while(0)

static void order_visit(ir_blk_t *blk)
{
	if(blk->visited) {
		return;
	}
	blk->visited = true;

	unionB(blk);

	if(blk->tail->true_blk) {
		unionB(blk->tail->true_blk);
		order_visit(blk->tail->true_blk);
	}

	if(blk->tail->false_blk) {
		unionB(blk->tail->false_blk);
		order_visit(blk->tail->false_blk);
	}

	return;
}

static int blocks_cmp(const void *a, const void *b)
{
	ir_blk_t **blk1 = (ir_blk_t **)a;
	ir_blk_t **blk2 = (ir_blk_t **)b;
	int64_t ord1 = (*blk1)->postnum;
	int64_t ord2 = (*blk2)->postnum;
	int64_t cmp = ord1 - ord2;
	if(cmp < 0) {
		return -1;
	}
	if(cmp > 0) {
		return +1;
	}
	return 0;
}

static void order_blocks(ir_func_t *fun)
{
	cur_postnum = 0;
	/* reset visited */
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		find_before_last_term_ins(fun->blocks[i]);
		fun->blocks[i]->visited = false;
		fun->blocks[i]->postnum = -1;
	}

	/* visit blocks */
	order_visit(fun->blocks[0]);

	/* mark dead blocks at end */
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		if(fun->blocks[i]->postnum == -1) {
			fun->blocks[i]->postnum = INT64_MAX - 1;
		}
	}

	/* sort blocks */
	qsort(fun->blocks, list_len(fun->blocks), sizeof(ir_blk_t *), blocks_cmp);
}

static void renumber_blocks(ir_func_t *fun, long base)
{
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		fun->blocks[i]->num = i + base;
	}
	return;
}

void ir_basic_block_placement(ir_func_t *fun)
{
	ir_fix(fun);
	long base = fun->blocks[0]->num;
	order_blocks(fun);

	size_t oldsize = list_len(fun->blocks);
	/* remove all dead blocks */
	for(size_t i = list_len(fun->blocks) - 1; i >= 0; i--) {
		if(i == 0) {
			break;
		}

		if(fun->blocks[i]->postnum == (INT64_MAX - 1)) {
			list_hdr(fun->blocks)->size = i;
			continue;
		}

		break;
	}

	for(size_t i = list_len(fun->blocks); i < oldsize; i++) {
		ir_blk_t *blk = fun->blocks[i];
		ir_inst_t *nxt = NULL;
		for(ir_inst_t *ins = blk->insts; ins; ins = nxt) {
			nxt = ins->next;
			ir_inst_delete(ins);
		}
		ir_blk_delete(blk);
	}

	renumber_blocks(fun, base);

	return;
}

/* turn the IR from an SSA form into a typical 3AC IR.
 * Removes and deallocates all phis, turning them into NOPs. */
void ir_ssa_exit(ir_func_t *fun)
{
	ir_nopremover(fun);
	ir_fix(fun);
	ir_blk_flow(fun);
	split_critical(fun);
	isolate_phis(fun);
	deparallelize_pmovs(fun);
	cleanup_critical_jumps(fun);
	ir_blk_flow(fun);
	ir_basic_block_placement(fun);
	ir_nopremover(fun);
	ir_fix(fun);

	return;
}

static void unordered_remove_blk(LIST(ir_blk_t *) list, size_t indx)
{
	list_hdr(list)->size--;

	if(list_len(list) == 0 || indx == list_len(list)) {
		/* also nothing to do */
		return;
	}

	ir_blk_t *tmp = list[indx];
	list[indx] = list[list_len(list)];
	list[list_len(list)] = tmp;

	return;
}

static void unordered_remove_reg(LIST(reg_t *) list, size_t indx)
{
	list_hdr(list)->size--;

	if(list_len(list) == 0 || indx == list_len(list)) {
		/* also nothing to do */
		return;
	}

	reg_t *tmp = list[indx];
	list[indx] = list[list_len(list)];
	list[list_len(list)] = tmp;

	return;
}

static UNUSEDA void ordered_remove_blk(LIST(ir_blk_t *) list, size_t indx)
{
	list_hdr(list)->size--;
	if(list_len(list) == 0 || indx == list_len(list)) {
		/* nothing to do */
		return;
	}

	for(size_t i = indx; i < list_len(list); i++) {
		list[i] = list[i + 1];
	}
	return;
}

/* removes a predeccesor `pred` from the block `blk` */
void ir_remove_pred(ir_blk_t *blk, ir_blk_t *pred)
{
	size_t indx = find_blk(blk->pred, pred);
	if(indx == (size_t)-1) {
		/* nothing to do.. */
		return;
	}

	/* remove from predeccesors */
	unordered_remove_blk(blk->pred, indx);

	/* remove uses in phi nodes */
	for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
		if(ins->type != IR_INST_PHI) {
			continue;
		}

		size_t indx = find_blk(ins->phi_preds, pred);
		if(indx == (size_t)-1) {
			continue;
		}

		unordered_remove_blk(ins->phi_preds, indx);
		unordered_remove_reg(ins->phi_args, indx);

		/* simplify 1-arg phis */
		if(list_len(ins->phi_preds) == 1) {
			ins->phi_args[0]->phi_related = false;
			ins->type = IR_INST_MOV;
			ins->r1 = ins->phi_args[0];
			list_delete(ins->phi_args);
			list_delete(ins->phi_preds);
		}
	}

	return;
}

/* reroutes a predeccesor `orig` to the new predeccesor `new` for the block `blk` */
void ir_reroute_pred(ir_blk_t *blk, ir_blk_t *orig, ir_blk_t *new)
{
	size_t indx = find_blk(blk->pred, orig);
	if(indx == (size_t)-1) {
		return;
	}

	blk->pred[indx] = new;

	/* replace uses in phi nodes */
	for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
		if(ins->type != IR_INST_PHI) {
			continue;
		}

		size_t indx = find_blk(ins->phi_preds, orig);
		if(indx == (size_t)-1) {
			continue;
		}
		ins->phi_preds[indx] = new;
	}

	return;
}

/* removes block from CFG entirely */
void ir_remove_blk(ir_func_t *func, ir_blk_t *blk)
{
	size_t indx = find_blk(func->blocks, blk);
	if(indx == (size_t)-1) {
		return;
	}

	/* remove from successors's predeccesors */
	for(size_t i = 0; i < list_len(blk->succ); i++) {
		ir_remove_pred(blk->succ[i], blk);
	}

	ir_blk_delete(func->blocks[indx]);
	ordered_remove_blk(func->blocks, indx);

	return;
}

static void fix_single_phi(ir_inst_t *phi, ir_blk_t *blk)
{
	LIST(ir_blk_t *) pred = blk->pred;
	size_t len = list_len(pred);
	for(size_t i = 0; i < len;) {
		size_t indx = find_blk(pred, phi->phi_preds[i]);
		if(indx != (size_t)-1) {
			i++;
			continue;
		}

		unordered_remove_blk(phi->phi_preds, i);
		unordered_remove_reg(phi->phi_args, i);

		/* simplify 1-arg phis */
		if(list_len(phi->phi_preds) == 1) {
			phi->phi_args[0]->phi_related = false;
			phi->type = IR_INST_MOV;
			phi->r1 = phi->phi_args[0];
			list_delete(phi->phi_args);
			list_delete(phi->phi_preds);
			break;
		}
		len--;
	}
	return;
}

/* fixs phi nodes */
void ir_fix_phis(ir_func_t *func)
{
	ir_blk_flow(func);
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			if(ins->type != IR_INST_PHI) {
				continue;
			}

			fix_single_phi(ins, blk);
		}
	}

	return;
}
