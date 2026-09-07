#ifndef CFG_H_
#define CFG_H_

#include "ir.h"

/* optimizes phis with all the same value */
int ir_phiopt(ir_func_t *func);

/* optimizes compare-and-branch into single inst */
int ir_branchopt(ir_func_t *func);

/* requires ir_blk_rpo and ir_blk_flow (preserves both) */
void ir_blk_dom(ir_func_t *func);

/* requires ir_blk_dom */
void ir_blk_domtree(ir_func_t *fun);

/* requires ir_blk_dom */
void ir_blk_domf(ir_func_t *fun);

/* finds least common ancestor of b1 and b2
 * requires ir_blk_dom */
ir_blk_t *ir_blk_lca(ir_blk_t *b1, ir_blk_t *b2);

/* does `blk` dominate `other` */
bool ir_blk_does_dom(ir_blk_t *blk, ir_blk_t *other);

/* computes loop nesting counts */
/* requires ir_blk_flow */
void ir_blk_loopnest(ir_func_t *func);

#endif /* CFG_H_ */
