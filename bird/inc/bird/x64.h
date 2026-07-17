#ifndef IR_X64_H_
#define IR_X64_H_

#include "ir.h"
#include "regalloc.h"

void ir_prog_begin_x64_sysv(FILE *f, ir_prog_t *prog);
void ir_prog_end_x64_sysv(FILE *f, ir_prog_t *prog);
void ir_func_opt_x64(ir_func_t *fun, int opt_level);
void ir_func_emit_x64_sysv(FILE *f, ir_func_t *fun);
void ir_glob_emit_x64_sysv(FILE *f, ir_global_t *glob);

#endif /* IR_X64_H_ */
