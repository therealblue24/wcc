#include "bird.h"

static ir_inst_t *find_last_or_flow_ins(ir_inst_t *root)
{
	ir_inst_t *ret = root;
	for(; ret; ret = ret->next) {
		if(ir_inst_is_term(ret->type)) {
			return ret;
		}
	}
	ASSERT(ir_inst_is_term(ret->type), "IR is not constructed properly");
	return NULL;
}

/* fill out all the defined registers in this block */
static void fill_defs(ir_blk_t *blk)
{
	for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
		if(!ins->r0) {
			continue;
		}
		set_add(&blk->regs_def, ins->r0);
	}
	return;
}

/* fill out predecessors (and also if block returns) */
static void fill_succ_pred(ir_blk_t *blk)
{
	if(!blk || blk->visited) {
		return;
	}

	blk->visited = true;

	/* find the last inst. */
	blk->tail = find_last_or_flow_ins(blk->insts);
	ir_inst_t *flow = blk->tail;
	if(flow->type == IR_INST_RET) {
		blk->returns = true;
		return;
	}

	if(flow->true_blk) {
		/* add predecessor */
		list_append(flow->true_blk->pred, blk);
		list_append(blk->succ, flow->true_blk);
		fill_succ_pred(flow->true_blk);
	}

	if(flow->false_blk) {
		/* add predecessor */
		list_append(flow->false_blk->pred, blk);
		list_append(blk->succ, flow->false_blk);
		fill_succ_pred(flow->false_blk);
	}

	return;
}

static void fill_ins_outs_reg(ir_blk_t *blk, reg_t *reg)
{
	if(!blk || !reg) {
		return;
	}

	/* want to make sure it's not a defined reg */
	if(set_has(blk->regs_def, reg)) {
		return;
	}

	/* its an input reg */
	if(!set_has(blk->regs_in, reg)) {
		set_add(&blk->regs_in, reg);
	} else {
		return;
	}

	/* to the predeccesors it's also an output reg: add it there */
	for(size_t i = 0; i < list_len(blk->pred); i++) {
		if(!set_has(blk->pred[i]->regs_out, reg)) {
			set_add(&blk->pred[i]->regs_out, reg);
			fill_ins_outs_reg(blk->pred[i], reg);
		}
	}

	return;
}

/* fill the input & output registers of block */
static void fill_ins_outs(ir_blk_t *blk)
{
	for(ir_inst_t *inst = blk->insts; inst; inst = inst->next) {
		/* r0 is never an in/out reg because it is generated in the block. */
		if(inst->r1)
			fill_ins_outs_reg(blk, inst->r1);
		if(inst->r2)
			fill_ins_outs_reg(blk, inst->r2);

		/* call arguments */
		if(inst->type == IR_INST_CALL) {
			for(size_t i = 0; i < list_len(inst->call_args); i++) {
				fill_ins_outs_reg(blk, inst->call_args[i]->r);
			}
		}

		/* special case: return */
		if(inst->type == IR_INST_RET && inst->r1) {
			set_add(&blk->regs_out, inst->r1);
		}
	}
}

static void reset_blk(ir_blk_t *blk)
{
	list_hdr(blk->pred)->size = 0;
	list_hdr(blk->succ)->size = 0;
	set_reset(blk->regs_def);
	set_reset(blk->regs_in);
	set_reset(blk->regs_out);
	blk->loop_order = 0;
}

static void reset_fun(ir_func_t *fun)
{
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		reset_blk(fun->blocks[i]);
		fun->blocks[i]->visited = false;
	}
}

static void loop_visit(ir_blk_t *blk)
{
	blk->loop_order++;
	if(blk->visited) {
		return;
	} else {
		blk->loop_order = 0;
		blk->visited = true;
	}

	ir_inst_t *flow = blk->tail;

	if(flow->true_blk) {
		loop_visit(flow->true_blk);
	}

	if(flow->false_blk) {
		loop_visit(flow->false_blk);
	}

	return;
}

void ir_blk_reguse(ir_func_t *fun)
{
	reset_fun(fun);
	size_t block_amount = list_len(fun->blocks);
	fill_succ_pred(fun->blocks[0]);
	for(size_t i = 0; i < block_amount; i++) {
		fill_defs(fun->blocks[i]);
	}
	for(size_t i = 0; i < block_amount; i++) {
		fill_ins_outs(fun->blocks[i]);
	}
	loop_visit(fun->blocks[0]);
	return;
}

void ir_blk_flow(ir_func_t *fun)
{
	reset_fun(fun);
	fill_succ_pred(fun->blocks[0]);
	return;
}

/* needs ir_blk_reguse; defines the input registers to be zero for the entry block */
void ir_blk_fixup_entry(ir_func_t *fun)
{
	if(list_len(fun->blocks) == 0) {
		return;
	}
	ir_blk_t *entry = fun->blocks[0];
	ir_inst_t *nop = ir_inst_make(IR_INST_NOP, NULL, NULL, NULL, 0);
	nop->next = entry->insts;
	entry->insts = nop;

	ir_inst_t *prev = entry->insts;
	ir_inst_t *cur = entry->insts->next;

	reg_t *reg;
	set_iter(entry->regs_in, reg, {
		ir_inst_t *def = ir_inst_make(IR_INST_IMM, reg, NULL, NULL, 0);
		def->next = cur;
		prev->next = def;
	});

	entry->insts = entry->insts->next;
	ir_inst_delete(nop);

	/* recompute defs */
	ir_blk_reguse(fun);

	return;
}

/* assumes the register is either r1 or r2 */
static void reg_update_counter(reg_t *reg, long ins_counter)
{
	if(!reg)
		return;
	if(reg->last_use < ins_counter) {
		reg->last_use = ins_counter;
	}
	return;
}

#define RESET(x)                          \
	do {                                  \
		if((x)) {                         \
			(x)->def = (x)->last_use = 0; \
		}                                 \
	} while(0)

static void reset_liveness(ir_func_t *fun)
{
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			RESET(ins->r0);
		}
	}
	return;
}

void ir_blk_liveness(ir_func_t *fun)
{
	reset_liveness(fun);
	long ins_count = 1;
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			reg_update_counter(ins->r0, ins_count);
			if(ins->r0 && ins->r0->def == 0) {
				ins->r0->def = ins_count;
			}
			reg_update_counter(ins->r1, ins_count);
			reg_update_counter(ins->r2, ins_count);

			if(ins->type == IR_INST_CALL) {
				for(size_t j = 0; j < list_len(ins->call_args); j++) {
					reg_t *reg = ins->call_args[j]->r;
					reg_update_counter(reg, ins_count);
				}
			}

			if(ins->type == IR_INST_PHI) {
				for(size_t j = 0; j < list_len(ins->phi_args); j++) {
					reg_t *reg = ins->phi_args[j];
					reg_update_counter(reg, ins_count);
				}
			}

			ins_count++;
		}
		reg_t *r;
		set_iter(blk->regs_out, r, { reg_update_counter(r, ins_count); });
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

void ir_replace_reg(ir_func_t *fun, reg_t *from, reg_t *to)
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
			if(ins->type == IR_INST_PHI) {
				for(size_t i = 0; i < list_len(ins->phi_args); i++) {
					REPLACE(ins->phi_args[i], from, to);
				}
			}
			if(ins->type == IR_INST_PMOV) {
				for(size_t i = 0; i < list_len(ins->pmov_args); i++) {
					REPLACE(ins->pmov_args[i].dst, from, to);
					REPLACE(ins->pmov_args[i].src, from, to);
				}
			}
		}
	}
}

#undef REPLACE

#define USE(x) ((x) * 2)
#define DEF(x) (USE(x) + 1)

#define intersect ir_intersect
bool ir_intersect(reg_t *a, reg_t *b)
{
	if(a == b) {
		return true;
	}
	/* thx https://stackoverflow.com/a/1558990 */
	return !((USE(a->last_use) < DEF(b->def)) ||
			 (USE(b->last_use) < DEF(a->def)));
}

#undef USE
#undef DEF

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

int ir_try_coalesce(ir_func_t *fun, reg_t *reg, reg_t *join_with)
{
	if(intersect(join_with, reg)) {
		return 0;
	}

	/* replace r0 with r1, join intervals */
	join_intervals(reg, join_with);

	ir_replace_reg(fun, join_with, reg);
	return 1;
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

		if(ir_try_coalesce(fun, ins->r1, ins->r0)) {
			ins->type = IR_INST_NOP;
		}
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
	return;
}

/* calculates register defs & last use for all blocks in `fun` */
LIST(reg_t *) ir_blk_reglive(ir_func_t *fun)
{
	reset_liveness(fun);
	LIST(reg_t *) allocated = list_make(reg_t *);
	/* the algorithm here is quite simple. basically,
	 * we assume the blocks are laid out in order,
	 * and then for each instruction we increment a counter
	 * and then that is the "Program Counter". We assign
	 * register definitions & last uses based on that number.
	 * We also do these for the output registers.
	 * This will be used for the eventual register allocator.
	 */

	/* instruction counter, starts at 1 because 0 is undef'd */
	long ins_count = 1;
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
			reg_update_counter(ins->r0, ins_count);
			if(ins->r0 && ins->r0->def == 0) {
				ins->r0->def = ins_count;
				ins->r0->from = ins;
				list_append(allocated, ins->r0);
			}
			reg_update_counter(ins->r1, ins_count);
			reg_update_counter(ins->r2, ins_count);

			if(ins->type == IR_INST_CALL) {
				for(size_t j = 0; j < list_len(ins->call_args); j++) {
					reg_t *reg = ins->call_args[j]->r;
					reg_update_counter(reg, ins_count);
				}
			}

			ins_count++;
		}
		reg_t *r;
		set_iter(blk->regs_out, r, { reg_update_counter(r, ins_count); });
	}
	return allocated;
}
