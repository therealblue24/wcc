#ifndef IR_OPT_H_
#define IR_OPT_H_

#include "ir.h"

/* optimizes an IR function */
void ir_opt(ir_func_t *fun, int opt_level, enum ir_arch arch);

#endif /* IR_OPT_H_ */
