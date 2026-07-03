/* based of 9cc's register allocator algorithm */
#ifndef IR_REGALLOC_H_
#define IR_REGALLOC_H_

#include "ir.h"

/* calculates register use for all blocks in `fun` */
void ir_blk_reguse(ir_func_t *fun);

/* calculates predeccesors for blocks */
void ir_blk_flow(ir_func_t *fun);

/* calculates register defs & last use for all blocks in `fun`. returns registers allocated */
LIST(reg_t *) ir_blk_reglive(ir_func_t *fun);

/* calculates register defs & last use */
void ir_blk_liveness(ir_func_t *fun);

/* needs ir_blk_reguse; defines the input registers to be zero for the entry block */
void ir_blk_fixup_entry(ir_func_t *fun);

/* do the register allocation for `amount` of registers */
void ir_regalloc(LIST(reg_t *) allocated, int amount);

/* do the spilling */
void ir_regalloc_spill(ir_func_t *fun, LIST(reg_t *) allocated);

/* do register allocation all in one */
void ir_finalize(ir_func_t *fun, int amount, int opt_level, enum ir_arch arch);

#endif /* IR_REGALLOC_H_ */
