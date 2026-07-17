#ifndef IR_AARCH64_H_
#define IR_AARCH64_H_

#include "ir.h"

void ir_prog_begin_aarch64_apple(FILE *f, ir_prog_t *prog);
void ir_prog_end_aarch64_apple(FILE *f, ir_prog_t *prog);
void ir_func_opt_aarch64(ir_func_t *fun, int opt_level);
void ir_func_emit_aarch64_apple(FILE *f, ir_func_t *fun);
void ir_glob_emit_aarch64_apple(FILE *f, ir_global_t *glob);

#endif /* IR_AARCH64_H_ */
