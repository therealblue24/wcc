#ifndef IR_SSA_H_
#define IR_SSA_H_

#include "ir.h"

/* turn the IR into a SSA form. */
void ir_ssa_enter(ir_func_t *fun);

/* turn the IR from a SSA form into a typical 3AC IR.
 * Removes and deallocates all phis, turning them into NOPs. */
void ir_ssa_exit(ir_func_t *fun);

/* orders basic blocks in IR */
void ir_basic_block_placement(ir_func_t *fun);

/* removes a predeccesor `pred` from the block `blk` */
void ir_remove_pred(ir_blk_t *blk, ir_blk_t *pred);

/* reroutes a predeccesor `orig` to the new predeccesor `new` for the block `blk` */
void ir_reroute_pred(ir_blk_t *blk, ir_blk_t *orig, ir_blk_t *new);

/* removes block from CFG entirely */
void ir_remove_blk(ir_func_t *func, ir_blk_t *blk);

/* removes a useless `jmp` block from CFG entirely */
void ir_remove_jmpblk(ir_func_t *func, ir_blk_t *blk);

/* fixs phi nodes */
void ir_fix_phis(ir_func_t *func);

#endif /* IR_SSA_H_ */
