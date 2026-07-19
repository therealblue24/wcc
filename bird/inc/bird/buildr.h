#ifndef BUILDR_H_
#define BUILDR_H_

#include "ir.h"

/* an IR builder */
typedef struct ir_buildr {
	/* block to add instructions to */
	ir_blk_t *insert_blk;
	/* current function we are building */
	ir_func_t *func;
	/* program we are maintaining */
	ir_prog_t prog;
	/* block number accumulator */
	long blknum;
} ir_buildr_t;

/* make (initialize) a builder */
void ir_buildr_make(ir_buildr_t *build);

/* makes a new, empty function with name `fname`. sets the insert block
 * to the new empty block in the function */
ir_func_t *ir_buildr_make_func(ir_buildr_t *build, char *fname);

/* end a function */
void ir_buildr_end_func(ir_buildr_t *build);

/* sets the insert block to the block provided */
void ir_buildr_set_insert_blk(ir_buildr_t *build, ir_blk_t *blk);

/* makes (emits) a new block. does not set the insert block */
ir_blk_t *ir_buildr_make_blk(ir_buildr_t *build);

/* emits instruction at current insert block */
void ir_buildr_emit_ins(ir_buildr_t *build, ir_inst_t *ins);

/* add a global to the program */
void ir_buildr_add_glob(ir_buildr_t *build, ir_global_t *glob);

/* delete a builder */
void ir_buildr_delete(ir_buildr_t *build);

/* builder instructions */

reg_t *ir_buildr_copy(ir_buildr_t *build, reg_t *reg);

reg_t *ir_buildr_creat_imm64(ir_buildr_t *build, int64_t imm);
reg_t *ir_buildr_creat_imm32(ir_buildr_t *build, int32_t imm);
reg_t *ir_buildr_creat_imm(ir_buildr_t *build, bool is_32bit, int64_t imm);

reg_t *ir_buildr_creat_add(ir_buildr_t *build, bool is_32bit, reg_t *lhs,
						   reg_t *rhs);
reg_t *ir_buildr_creat_sub(ir_buildr_t *build, bool is_32bit, reg_t *lhs,
						   reg_t *rhs);
reg_t *ir_buildr_creat_mul(ir_buildr_t *build, bool is_32bit, bool unsignd,
						   reg_t *lhs, reg_t *rhs);
reg_t *ir_buildr_creat_div(ir_buildr_t *build, bool is_32bit, bool unsignd,
						   reg_t *lhs, reg_t *rhs);
reg_t *ir_buildr_creat_mod(ir_buildr_t *build, bool is_32bit, bool unsignd,
						   reg_t *lhs, reg_t *rhs);

reg_t *ir_buildr_creat_umul(ir_buildr_t *build, bool is_32bit, reg_t *lhs,
							reg_t *rhs);
reg_t *ir_buildr_creat_udiv(ir_buildr_t *build, bool is_32bit, reg_t *lhs,
							reg_t *rhs);
reg_t *ir_buildr_creat_umod(ir_buildr_t *build, bool is_32bit, reg_t *lhs,
							reg_t *rhs);
reg_t *ir_buildr_creat_smul(ir_buildr_t *build, bool is_32bit, reg_t *lhs,
							reg_t *rhs);
reg_t *ir_buildr_creat_sdiv(ir_buildr_t *build, bool is_32bit, reg_t *lhs,
							reg_t *rhs);
reg_t *ir_buildr_creat_smod(ir_buildr_t *build, bool is_32bit, reg_t *lhs,
							reg_t *rhs);

reg_t *ir_buildr_creat_and(ir_buildr_t *build, bool is_32bit, reg_t *lhs,
						   reg_t *rhs);
reg_t *ir_buildr_creat_eor(ir_buildr_t *build, bool is_32bit, reg_t *lhs,
						   reg_t *rhs);
reg_t *ir_buildr_creat_or(ir_buildr_t *build, bool is_32bit, reg_t *lhs,
						  reg_t *rhs);

reg_t *ir_buildr_creat_shl(ir_buildr_t *build, bool is_32bit, reg_t *lhs,
						   reg_t *rhs);
reg_t *ir_buildr_creat_shr(ir_buildr_t *build, bool is_32bit, bool unsignd,
						   reg_t *lhs, reg_t *rhs);

reg_t *ir_buildr_creat_neg(ir_buildr_t *build, bool is_32bit, reg_t *lhs);
reg_t *ir_buildr_creat_not(ir_buildr_t *build, bool is_32bit, reg_t *lhs);
reg_t *ir_buildr_creat_bool(ir_buildr_t *build, bool is_32bit, reg_t *lhs);
reg_t *ir_buildr_creat_invbool(ir_buildr_t *build, bool is_32bit, reg_t *lhs);

reg_t *ir_buildr_creat_cmp_eq(ir_buildr_t *build, bool is_32bit, reg_t *lhs,
							  reg_t *rhs);
reg_t *ir_buildr_creat_cmp_ne(ir_buildr_t *build, bool is_32bit, reg_t *lhs,
							  reg_t *rhs);

reg_t *ir_buildr_creat_cmp_le(ir_buildr_t *build, bool is_32bit, bool unsignd,
							  reg_t *lhs, reg_t *rhs);
reg_t *ir_buildr_creat_cmp_lt(ir_buildr_t *build, bool is_32bit, bool unsignd,
							  reg_t *lhs, reg_t *rhs);

reg_t *ir_buildr_creat_cmp_ge(ir_buildr_t *build, bool is_32bit, bool unsignd,
							  reg_t *lhs, reg_t *rhs);
reg_t *ir_buildr_creat_cmp_gt(ir_buildr_t *build, bool is_32bit, bool unsignd,
							  reg_t *lhs, reg_t *rhs);

reg_t *ir_buildr_creat_leas(ir_buildr_t *build, int64_t off);
reg_t *ir_buildr_creat_lea(ir_buildr_t *build, ir_global_t *glob);

reg_t *ir_buildr_creat_load(ir_buildr_t *build, bool ext, size_t size,
							reg_t *addr);
void ir_buildr_creat_store(ir_buildr_t *build, size_t size, reg_t *addr,
						   reg_t *data);

reg_t *ir_buildr_creat_ext(ir_buildr_t *build, bool is_32bit, bool unsignd,
						   size_t size, reg_t *data);

void ir_buildr_creat_jmp(ir_buildr_t *build, ir_blk_t *blk);
void ir_buildr_creat_br(ir_buildr_t *build, bool is_32bit, reg_t *cond,
						ir_blk_t *true_blk, ir_blk_t *false_blk);

void ir_buildr_creat_jmp_set(ir_buildr_t *build, ir_blk_t *blk);
void ir_buildr_creat_br_set(ir_buildr_t *build, bool is_32bit, reg_t *cond,
							ir_blk_t *true_blk, ir_blk_t *false_blk,
							ir_blk_t *set_to);

void ir_buildr_creat_ret(ir_buildr_t *build, bool is_32bit, reg_t *data);

reg_t *ir_buildr_creat_call(ir_buildr_t *build, char *fname,
							LIST(callreg_t *) args);

#endif /* BUILDR_H_ */
