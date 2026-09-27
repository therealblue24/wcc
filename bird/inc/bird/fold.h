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

/* requires ir_blk_flow, ir_blk_dom and marks */
int ir_cmp_prop(ir_func_t *func);

/* eliminates 32-bit sign exts if all uses are 32 bit */
/* breaks 32 bit use information */
int ir_elim_32ext(ir_func_t *func);

#endif /* FOLD_H_ */
