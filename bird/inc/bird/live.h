#ifndef LIVENESS_H_
#define LIVENESS_H_

#include "ir.h"

/* calculates register use for all blocks in `fun` */
void ir_blk_reguse(ir_func_t *fun);

/* calculates predeccesors for blocks */
void ir_blk_flow(ir_func_t *fun);

/* requires liveness: coalesces non-interfering registers */
void ir_coalesce(ir_func_t *fun);

/* tries to coalesce reg `reg` with `join_with` */
int ir_try_coalesce(ir_func_t *fun, reg_t *reg, reg_t *join_with);

/* replace reg */
void ir_replace_reg(ir_func_t *fun, reg_t *from, reg_t *to);

/* do 2 register's lifetimes intersect? */
bool ir_intersect(reg_t *a, reg_t *b);

/* calculates register defs & last use for all blocks in `fun`. returns registers allocated */
LIST(reg_t *) ir_blk_reglive(ir_func_t *fun);

/* calculates register defs & last use */
void ir_blk_liveness(ir_func_t *fun);

/* needs ir_blk_reguse; defines the input registers to be zero for the entry block */
void ir_blk_fixup_entry(ir_func_t *fun);

#endif /* LIVENESS_H_ */
