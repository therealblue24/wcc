/* based of 9cc's register allocator algorithm */
#ifndef IR_REGALLOC_H_
#define IR_REGALLOC_H_

#include "ir.h"
#include "liveness.h"
#include <stdbool.h>

/* do the register allocation for `amount` of registers */
void ir_regalloc(ir_func_t *fun, int amount);

/* do the spilling */
void ir_regalloc_spill(ir_func_t *fun);

/* do register allocation all in one */
void ir_finalize(ir_func_t *fun, int amount, int opt_level, enum ir_arch arch);

#endif /* IR_REGALLOC_H_ */
