#ifndef CFG_H_
#define CFG_H_

#include "ir.h"

/* optimizes phis with all the same value */
int ir_phiopt(ir_func_t *func);

/* optimizes compare-and-branch into single inst */
int ir_branchopt(ir_func_t *func);

/* requires ir_blk_rpo and ir_blk_flow (preserves both) */
void ir_blk_dom(ir_func_t *func);

#endif /* CFG_H_ */
