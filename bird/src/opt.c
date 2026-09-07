#include "bird.h"
#include "cfg.h"
#include "info.h"
#include "ir.h"
#include "ssa.h"
#include <stdint.h>

extern int debug;

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

/* removes useless blocks */
static int ir_remblks(ir_func_t *func)
{
	int change = 0;
	ir_nopremover(func);
	ir_blk_flow(func);
	/* first block is always entry, dont remove */
	for(size_t i = 1; i < list_len(func->blocks);) {
		ir_blk_t *blk = func->blocks[i];
		if(list_len(blk->pred) == 0) {
			ir_remove_blk(func, blk);
			change = 1;
		} else {
			i++;
		}
	}

	return change;
}

/* optimizes an IR function */
void ir_opt(ir_func_t *func, int opt_level, enum ir_arch arch)
{
	prof_begin("opt");
	for(size_t i = 0; i < list_len(func->blocks); i++) {
		func->blocks[i]->tail = find_last_or_flow_ins(func->blocks[i]->insts);
	}

	int change = 1;
	int max_tolerated_change = 1;
	switch(opt_level) {
	case 0:
		max_tolerated_change = 0;
		break;
	case 1:
		max_tolerated_change = 1;
		break;
	case 2:
		max_tolerated_change = 16;
		break;
	case 3:
		max_tolerated_change = 64; /* mimic the nature of -O3 */
		break;
	}

	if(debug) {
		printf("Before common opts:\n");
		ir_dump(func, 'v');
		printf("****\n");
	}

	ir_ssa_enter(func);
	ir_blk_flow(func);
	ir_simpleopt(func);

	if(debug) {
		printf("After SSA constr.:\n");
		ir_dump(func, 'v');
		printf("****\n");
	}

	int left = max_tolerated_change;
	while(left && change) {
		change = 0;

		TIMEIT("info", {
			/* currently unused */
			TIMEIT("use", { ir_blk_reguse(func); });
			TIMEIT("live", { ir_blk_liveness(func); });
			TIMEIT("mark", { ir_placemarks(func); });
		});

		/* dead code elim */
		TIMEIT("dce", {
			ir_fix(func);
			TIMEIT("agg", { change |= ir_adce(func); });
			TIMEIT("norm", {
				ir_blk_liveness(func);
				change |= ir_dce(func);
			});
			ir_nopremover(func);
		});

		ir_placemarks(func);
		/* GVN */
		TIMEIT("gvn", {
			ir_fix(func);
			ir_nopremover(func);
			ir_blk_flow(func);
			ir_blk_rpo(func);
			ir_blk_dom(func);
			ir_blk_domtree(func);
			change |= ir_gvn(func);
			ir_fix_phis(func);
		});

		/* GCM */
		/* TODO: Make this work!!! */
		// TIMEIT("gcm", {
		// 	ir_fix(func);
		// 	ir_nopremover(func);
		// 	ir_blk_flow(func);
		// 	ir_blk_rpo(func);
		// 	ir_blk_dom(func);
		// 	ir_blk_loopnest(func);
		// 	ir_placemarks(func);
		// 	ir_fill_use(func);
		// 	change |= ir_gcm(func);

		// 	ir_del_use(func);
		// });

		/* SSA-related opts */
		TIMEIT("phi", {
			change |= ir_phiopt(func);
			ir_fix_phis(func);
			ir_nopremover(func);
			ir_fix(func);
		});

		ir_placemarks(func);

		TIMEIT("movelim", {
			ir_blk_dom(func);
			ir_blk_loopnest(func);
			change |= ir_mov_elim(func);
			change |= ir_imm_elim(func);
		});

		ir_placemarks(func);

		/* peephole opts */
		TIMEIT("peep", {
			change |= ir_simpleopt(func);
			ir_fix_phis(func);
		});

		ir_placemarks(func);

		/* memory optimization */
		TIMEIT("mem", {
			change |= ir_memopt(func);
			ir_nopremover(func);
		});

		/* folding */
		TIMEIT("fold", {
			change |= ir_fold(func);
			ir_placemarks(func);
			/* TODO: this doesn't work */
			change |= ir_leas_arith_opt(func);
			ir_placemarks(func);
			ir_fix_phis(func);
		});

		/* branch opts */
		TIMEIT("bropt", {
			ir_placemarks(func);
			change |= ir_branchopt(func);
			ir_nopremover(func);
			ir_fix(func);
		});

		/* stack-based optimizations */
		TIMEIT("stkopt", {
			ir_placemarks(func);
			change |= ir_stackopt(func);
			ir_nopremover(func);
			change |= ir_stackreduce(func);
			ir_nopremover(func);
			ir_fix(func);
		});

		if(!change) {
			break;
		}

		left--;
		ir_blk_flow(func);
	}

	ir_nopremover(func);

	if(debug) {
		printf("Before SSA destruct.:\n");
		ir_dump(func, 'v');
		printf("****\n");
	}

	TIMEIT("immfold", {
		ir_immfold_analyze(func);
		switch(arch) {
		case IR_ARCH_AARCH64_APPLE:
			ir_immfold_do(func, 0, 4095, arch);
			break;
		case IR_ARCH_X64_SYSV:
			ir_immfold_do(func, INT32_MIN, INT32_MAX, arch);
			break;
		default:
			break;
		}
		ir_adce(func);
		ir_blk_liveness(func);
		ir_dce(func);
	});

	TIMEIT("exit", {
		ir_placemarks(func);
		ir_ssa_exit(func);
		ir_remblks(func);
		ir_nopremover(func);
	});

	if(debug) {
		printf("After common opts:\n");
		ir_dump(func, 'v');
		printf("****\n");
	}

	/* apply arch specific opts */
	TIMEIT("archopt", {
		switch(arch) {
		case IR_ARCH_AARCH64_APPLE:
			ir_func_opt_aarch64(func, opt_level);
			break;
		case IR_ARCH_X64_SYSV:
			ir_func_opt_x64(func, opt_level);
			break;
		default:
			break;
		}
	});

	ir_fix(func);

	if(debug) {
		printf("After arch opts:\n");
		ir_dump(func, 'v');
		printf("****\n");
	}
	prof_end();
	return;
}
