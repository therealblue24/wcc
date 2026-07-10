#include "bird.h"
#include "ir.h"
#include <limits.h>
#include <stdlib.h>

#define ACC(reg, cost)                   \
	do {                                 \
		if((reg)) {                      \
			(reg)->spill_cost += (cost); \
		}                                \
	} while(0)

static int ins_is_load(enum ins_type t)
{
	return t == IR_INST_LOAD || t == IR_INST_LOADS || t == IR_INST_LOADSS;
}

static int ins_is_store(enum ins_type t)
{
	return t == IR_INST_STORE || t == IR_INST_STORES || t == IR_INST_STORES;
}

/* calculates spill costs for each register */
static void calculate_spill_costs(LIST(reg_t *) allocated, ir_func_t *fun)
{
	/* reset costs */
	for(size_t i = 0; i < list_len(allocated); i++) {
		reg_t *r = allocated[i];
		int64_t range = r->last_use - r->def;
		r->spill_cost = -range;
	}

	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		/* first, make sure registers in loops are extra costly to spill */
		if(blk->loop_order) {
			/* they better shoot into the goal */
			int64_t penalty = blk->loop_order * 2500;
			for(size_t i = 0; i < list_len(blk->regs_def); i++) {
				blk->regs_def[i]->spill_cost += penalty;
			}
		}

		for(ir_inst_t *inst = blk->insts; inst; inst = inst->next) {
			/* stores should be slower than loads, so account for that */
			ACC(inst->r0, 2);
			ACC(inst->r1, 1);
			ACC(inst->r2, 1);

			if(ins_is_load(inst->type)) {
				/* we have to do a store afterwards */
				ACC(inst->r0, 4);
			}

			if(ins_is_store(inst->type)) {
				/* we have to do a load beforewards */
				ACC(inst->r1, 1);
				ACC(inst->r2, 1);
			}

			if(inst->type == IR_INST_CALL) {
				for(size_t j = 0; j < list_len(inst->call_args); j++) {
					ACC(inst->call_args[j]->r, 2);
				}
			}
		}
	}
}

#undef ACC

/* spill a register read `reg` before `ins` */
static void rewrite_load_spill(ir_inst_t *ins_prev, ir_inst_t *ins, reg_t *reg)
{
	ir_inst_t *inst = ir_inst_make(IR_INST_LOADSS, reg, NULL, NULL, reg->off);
	inst->size = 8;
	ins_prev->next = inst;
	inst->next = ins;
	return;
}

/* insert a spilled store after `ins` */
static void rewrite_store_spill(ir_inst_t *ins)
{
	ir_inst_t *inst =
		ir_inst_make(IR_INST_STORESS, NULL, ins->r0, NULL, ins->r0->off);
	inst->size = 8;
	ir_inst_t *nxt = ins->next;
	ins->next = inst;
	inst->next = nxt;
	return;
}

static reg_t *mk_spill_reg(reg_t *reg)
{
	reg_t *r = reg_make();
	r->off = reg->off;
	r->nospill = true;
	return r;
}

/* spill registers used in `ins` if needed */
static void rewrite_ins(ir_inst_t *ins_prev, ir_inst_t *ins)
{
	if(ins->type == IR_INST_LOADSS || ins->type == IR_INST_STORESS) {
		return;
	}

	if(ins->r0 && ins->r0->spilld) {
		ins->r0 = mk_spill_reg(ins->r0);
		rewrite_store_spill(ins);
	}

	if(ins->r1 && ins->r1->spilld) {
		ins->r1 = mk_spill_reg(ins->r1);
		rewrite_load_spill(ins_prev, ins, ins->r1);
		ins_prev = ins_prev->next;
	}

	if(ins->r2 && ins->r2->spilld) {
		ins->r2 = mk_spill_reg(ins->r2);
		rewrite_load_spill(ins_prev, ins, ins->r2);
		ins_prev = ins_prev->next;
	}

	return;
}

/* spill the needed registers to spill */
void ir_regalloc_spill(ir_func_t *fun)
{
	/* rewriting */
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		ir_inst_t *nop = ir_inst_make(IR_INST_NOP, NULL, NULL, NULL, 0);
		nop->next = blk->insts;
		blk->insts = nop;

		ir_inst_t *prev = blk->insts;
		ir_inst_t *cur = blk->insts->next;
		for(; cur; cur = cur->next) {
			rewrite_ins(prev, cur);
			prev = cur;

			if(ir_inst_is_term(cur->type)) {
				break;
			}
		}

		blk->insts = blk->insts->next;
		ir_inst_delete(nop);
	}
}

/* Register move coalescing */
/* https://llvm.org/ProjectsWithLLVM/2004-Fall-CS426-LS.pdf  JOIN-INTERVALS */
/* We don't have lifetime holes yet so we use a simple intersection function. */

#define REPLACE(x, fr, to) \
	do {                   \
		if((x) == (fr)) {  \
			(x) = (to);    \
		}                  \
	} while(0)

static void replace_reg(ir_func_t *fun, reg_t *from, reg_t *to)
{
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			REPLACE(ins->r0, from, to);
			REPLACE(ins->r1, from, to);
			REPLACE(ins->r2, from, to);
			if(ins->type == IR_INST_CALL) {
				for(size_t i = 0; i < list_len(ins->call_args); i++) {
					REPLACE(ins->call_args[i]->r, from, to);
				}
			}
		}
	}
}

#undef REPLACE

static bool intersect(reg_t *a, reg_t *b)
{
	/* thx https://stackoverflow.com/a/1558990 */
	return !((a->last_use < b->def) || (b->last_use < a->def));
}

static long long_min(long a, long b)
{
	return a < b ? a : b;
}

static long long_max(long a, long b)
{
	return a > b ? a : b;
}

static void join_intervals(reg_t *reg, reg_t *join_with)
{
	long def = long_min(reg->def, join_with->def);
	long use = long_max(reg->last_use, join_with->last_use);
	reg->def = def;
	reg->last_use = use;
	return;
}

static void coalesce_block(ir_func_t *fun, ir_blk_t *blk)
{
	for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
		if(ins->noopt) {
			continue;
		}

		if(ins->type != IR_INST_MOV) {
			continue;
		}

		if(intersect(ins->r0, ins->r1)) {
			continue;
		}

		/* replace r0 with r1, join intervals */
		join_intervals(ins->r1, ins->r0);

		replace_reg(fun, ins->r0, ins->r1);
		ins->type = IR_INST_NOP;
	}
	return;
}

static void dfs_visit(ir_func_t *fun, ir_blk_t *blk)
{
	if(blk->visited) {
		return;
	}
	blk->visited = true;
	ir_inst_t *flow = blk->tail;
	coalesce_block(fun, blk);
	if(flow->true_blk) {
		dfs_visit(fun, flow->true_blk);
	}
	if(flow->false_blk) {
		dfs_visit(fun, flow->false_blk);
	}
	return;
}

void ir_coalesce(ir_func_t *fun)
{
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		fun->blocks[i]->visited = false;
	}

	dfs_visit(fun, fun->blocks[0]);

	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		fun->blocks[i]->visited = false;
	}
}

/* 1:1 copy of Poletto 1999 linear scan algorithm + improvments from https://llvm.org/ProjectsWithLLVM/2004-Fall-CS426-LS.pdf (aka "iterative" register allocation)  */

static long get_cost(reg_t *r)
{
	return r ? r->spill_cost : -1;
}

static reg_t *regalloc_try(LIST(reg_t *) allocated, size_t amount)
{
	/* real registers */
	reg_t **regs = zcalloc((size_t)amount, sizeof(reg_t *));
	int free_reg;

	for(size_t i = 0; i < list_len(allocated); i++) {
		free_reg = -1;
		reg_t *r = allocated[i];
		int tospill = 0;

		/* expire intervals */
		for(size_t i = 0; i < amount; i++) {
			if(regs[i] && r->def >= regs[i]->last_use) {
				regs[i] = NULL;
			}

			/* fast way to select free register */
			if(!regs[i] && free_reg == -1) {
				free_reg = i;
			}

			/* select register to spill, just in case */
			if(regs[i] && !regs[i]->nospill &&
			   get_cost(regs[tospill]) > get_cost(regs[i])) {
				tospill = i;
			}
		}

		/* if we have a free register, allocate it.
		 * else, spill */
		if(free_reg != -1) {
			r->rr = free_reg;
			regs[free_reg] = r;
		} else {
			/* oops */
			reg_t *r = regs[tospill];
			free(regs);
			return r;
		}
	}

	free(regs);

	return NULL;
}

/* TODO: model lifetime holes */
void ir_regalloc(ir_func_t *fun, int amount_)
{
	LIST(reg_t *) allocd;
	size_t amount = amount_;

	for(;;) {
		allocd = ir_blk_reglive(fun);
		ir_dce(fun);
		reg_t *spill = regalloc_try(allocd, amount);
		if(!spill) {
			/* we achieved an allocation */
			list_delete(allocd);
			break;
		}

		/* we didnt, spill the candidate and retry allocation */

		spill->spilld = true;
		spill->spilld2 = true;
		spill->nospill = true;

		long off = -(long)(fun->stack_needed);
		long base = off - 8;
		fun->stack_needed += 8;
		spill->off = base;

		ir_regalloc_spill(fun);
		spill->spilld = false;

		list_delete(allocd);
	}
	return;
}

static long ir_eval_strat_cost(ir_func_t *fun, int amount, int callee_cost,
							   int caller_cost, bool strat, bool *used)
{
	long cost = 0;

	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			if(ins->r0) {
				used[ins->r0->rr] = 1;
			}
			if(ins->r1) {
				used[ins->r1->rr] = 1;
			}
			if(ins->r2) {
				used[ins->r2->rr] = 1;
			}

			if(ins->type == IR_INST_CALL) {
				size_t args = list_len(ins->call_args);
				for(size_t i = 0; i < args; i++) {
					if(ins->call_args[i]) {
						used[ins->call_args[i]->r->rr] = 1;
					}
				}
			}
		}
	}

	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			if(ins->type != IR_INST_CALL) {
				continue;
			}

			/* count # of registers used */
			size_t used_count = 0;
			size_t leftovers = 0;
			for(size_t i = 0; i < (size_t)amount; i++) {
				used_count += used[i];
			}
			if(used_count > (size_t)caller_cost) {
				leftovers = used_count - caller_cost;
				used_count = caller_cost;
			}

			if(!strat) {
				cost += used_count;
			} else {
				cost += leftovers;
			}
		}
	}

	size_t used_count = 0;
	size_t leftovers = 0;
	for(size_t i = 0; i < (size_t)amount; i++) {
		used_count += used[i];
	}
	if(used_count > (size_t)callee_cost) {
		leftovers = used_count - callee_cost;
		used_count = callee_cost;
	}

	if(strat) {
		cost += used_count;
	} else {
		cost += leftovers;
	}

	return cost;
}

/* chooses a register allocation strat */
/* callee_cost = # of registers that are callee-save */
/* caller_cost = # of registers that are caller-save */
static bool ir_choose_alloc_strat(ir_func_t *fun, int amount, int callee_cost,
								  int caller_cost)
{
	bool *used = zcalloc(amount, sizeof(bool));
	fun->alloc_used = used;
	long cost_caller =
		ir_eval_strat_cost(fun, amount, callee_cost, caller_cost, false, used);
	long cost_callee =
		ir_eval_strat_cost(fun, amount, callee_cost, caller_cost, true, used);
	if(cost_callee == cost_caller) {
		/* callee is easier to implement */
		return true;
	}

	if(cost_callee < cost_caller) {
		return true; /* callee-save */
	} else if(cost_caller < cost_callee) {
		return false; /* caller-save */
	}

	return true; /* how did you get here? */
}

static bool ins_has_side_effects(enum ins_type t)
{
	return t == IR_INST_STORE || t == IR_INST_STORES || t == IR_INST_STORESS ||
		   t == IR_INST_CALL;
}

static bool ins_has_reg(ir_inst_t *inst, reg_t *r)
{
	if(inst->r0 && inst->r0->rr == r->rr) {
		return true;
	}
	if(inst->r1 && inst->r1->rr == r->rr) {
		return true;
	}
	if(inst->r2 && inst->r2->rr == r->rr) {
		return true;
	}
	if(inst->type == IR_INST_CALL) {
		for(size_t i = 0; i < list_len(inst->call_args); i++) {
			if(inst->call_args[i]->r->rr == r->rr) {
				return true;
			}
		}
	}
	return false;
}

static int ir_simplify(ir_func_t *fun, int amount, enum ir_arch arch)
{
	UNUSED(amount);
	int change = 0;
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];

		ir_inst_t *nxt = blk->insts;
		for(ir_inst_t *ins = blk->insts; ins; ins = nxt) {
			nxt = ins->next;

			/* simplify useless moves where the source
			 * and destination have same real register */
			if(ins->type == IR_INST_MOV && ins->r0->rr != -1 &&
			   ins->r0->rr == ins->r1->rr && !ins->noopt) {
				ins->type = IR_INST_NOP;
				change = 1;
			}

			/* simplify
			 * %r0 = spill_load #loc
			 * spill_store #loc, %r0
			 * ->
			 * %r0 = spill_load #loc
			 * nop
			 */
			if(ins->type == IR_INST_LOADSS && nxt->type == IR_INST_STORESS &&
			   ins->imm == nxt->imm) {
				nxt->type = IR_INST_NOP;
				change = 1;
			}

			/* simplify
			 * spill_store #loc, %r0
			 * %r1 = spill_load #loc
			 * ->
			 * spill_store #loc, %r0
			 * IF(%r0 == %r1): nop ELSE %r1 = %r0
			 */
			if(ins->type == IR_INST_STORESS && nxt->type == IR_INST_LOADSS &&
			   ins->imm == nxt->imm) {
				if(nxt->r0->rr == ins->r1->rr) {
					nxt->type = IR_INST_NOP;
				} else {
					nxt->type = IR_INST_MOV;
					nxt->r1 = ins->r1;
				}
				change = 1;
			}

			/* simplify
			 * %r0 = %r1
			 * %r1 = %r0
			 * into
			 * %r0 = %r1
			 * nop */
			if(nxt && ins->type == IR_INST_MOV && nxt->type == IR_INST_MOV &&
			   ins->r0->rr == nxt->r1->rr && ins->r1->rr == nxt->r0->rr &&
			   !ins->noopt && !nxt->noopt) {
				nxt->type = IR_INST_NOP;
				change = 1;
			}

			/* simplify
			 * %r0 = assoc %r1, %r0
			 * into
			 * %r0 = assoc %r0, %r1
			 */
			if(ir_inst_is_assoc(ins->type) && ins->r0->rr == ins->r2->rr) {
				reg_t *tmp = ins->r1;
				ins->r1 = ins->r2;
				ins->r2 = tmp;
				change = 1;
			}

			/* x64 opt:
			 * according to Intel's optimization manual (Intel® 64 and IA-32 Architectures Optimization Reference Manual):
			 * "For LEA instructions with three source operands and some specific situations, instruction latency has increased to 3 cycles, and must dispatch via port 1: [...] --- LEA that uses base and index registers where the base is EBP, RBP, or R13."
			 * R13 is the x64 backend is "real" register r2, so
			 * if we encounter add %r2, %r?  then reorder it to add %r?, %r2
			 * if the dest is not %r2. */
			if(arch == IR_ARCH_X64_SYSV && ins->type == IR_INST_ADD &&
			   ins->r0->rr != ins->r1->rr && ins->r1->rr == 2) {
				reg_t *tmp = ins->r1;
				ins->r1 = ins->r2;
				ins->r2 = tmp;
				change = 1;
			}

			/* remove useless insts where thing stored is never used
			 * beyond this inst */
			if(ins->r0 && ins->r0->def == ins->r0->last_use &&
			   ins->type != IR_INST_CALL && !ins->noopt) {
				ins->type = IR_INST_NOP;
				change = 1;
			}

			/* if call ins, and the val is not used at all, remove
			 * the storing */
			if(ins->r0 && ins->r0->def == ins->r0->last_use &&
			   ins->type == IR_INST_CALL) {
				ins->r0 = NULL;
				change = 1;
			}

			/* rewrite
			 * %r0 = imm #x
			 * %r0 = op
			 * to just
			 * nop (useless imm load)
			 * %r0 = op
			 */
			if(nxt && ins->r0 && nxt->r0 && ins->type == IR_INST_IMM &&
			   nxt->r0->rr == ins->r0->rr && !ins_has_reg(nxt, ins->r0)) {
				ins->type = IR_INST_NOP;
				change = 1;
			}

			/* rewrite
			 * %r0 = pure_op
			 * %r0 = other_op
			 * to just
			 * nop
			 * %r0 = other_op
			 */
			if(nxt && ins->r0 && nxt->r0 && !ins_has_side_effects(ins->type) &&
			   !ins_has_reg(nxt, ins->r0) && ins->r0->rr == nxt->r0->rr) {
				ins->type = IR_INST_NOP;
				change = 1;
			}
		}

		ir_nopremover(fun);
	}

	return change;
}

extern int debug;

/* do register allocation all in one */
void ir_finalize(ir_func_t *fun, int amount, int opt_level, enum ir_arch arch)
{
	ir_blk_reguse(fun);
	ir_blk_fixup_entry(fun);
	ir_coalesce(fun);

	if(debug) {
		printf("After register coalescing\n");
		ir_dump(fun, 'v');
	}

	LIST(reg_t *) allocated = ir_blk_reglive(fun);
	calculate_spill_costs(allocated, fun);
	list_delete(allocated);
	ir_regalloc(fun, amount);
	ir_fix(fun);
	int tolerance = (opt_level * 8) + 1;
	int change = 1;
	ir_fix(fun);
	while(change && tolerance) {
		tolerance--;
		change = ir_simplify(fun, amount, arch);
		ir_fix(fun);
	}

	int callee_cost;
	int caller_cost;

	switch(arch) {
	case IR_ARCH_AARCH64_APPLE:
		callee_cost = 10; /* r19 .. r28 */
		caller_cost = 14; /* r1 .. r9, r11 .. r15 */
		break;
	case IR_ARCH_X64_SYSV:
		callee_cost = 5; /* rbx, r12 .. r15 */
		caller_cost = 6; /* rdi, rsi, r8, r9, r10, r11 */
		break;

	default:
		ERROR("unknown backend");
		break;
	}

	(void)ir_choose_alloc_strat(fun, amount, callee_cost, caller_cost);

	return;
}
