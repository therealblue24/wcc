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

#endif /* IR_SSA_H_ */
