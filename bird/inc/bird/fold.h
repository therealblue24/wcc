#ifndef FOLD_H_
#define FOLD_H_

#include "ir.h"

/* does folding */
int ir_fold(ir_func_t *func);

/* immediate move elimination */
int ir_imm_elim(ir_func_t *func);

/* 64-bit move elimination */
int ir_mov_elim(ir_func_t *func);

/* 32-bit move/ext elimination */
int ir_mov_elim32(ir_func_t *func);

/* partial folding for leas inst. */
int ir_leas_arith_opt(ir_func_t *func);

#endif /* FOLD_H_ */
