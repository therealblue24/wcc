#ifndef LIVENESS_H_
#define LIVENESS_H_

#include "ir.h"

/* calculates register use for all blocks in `fun` */
void ir_blk_reguse(ir_func_t *fun);

/* calculates predeccesors for blocks */
void ir_blk_flow(ir_func_t *fun);

/* requires liveness: coalesces non-interfering registers */
/* mode can be either:
 * 'c' - Original coalescing alg -- coalesce EACH oppourtunity
 * 'b' - Briggs coalescing
 * 'g' - George coalescing
 */
int ir_coalesce(ir_func_t *fun, int mode, int amount);

/* do 2 register's lifetimes intersect? */
bool ir_intersect(reg_t *a, reg_t *b);

/* calculates register defs & last use for all blocks in `fun`. returns registers allocated */
LIST(reg_t *) ir_blk_reglive(ir_func_t *fun);

/* calculates register defs & last use */
void ir_blk_liveness(ir_func_t *fun);

/* needs ir_blk_reguse; defines the input registers to be zero for the entry block */
void ir_blk_fixup_entry(ir_func_t *fun);

/* proper liveness analysis */
void ir_proper_liveness(ir_func_t *func);
/* reset interference graph */
void ir_reset_inter_graph(ir_func_t *func);
/* delete interference graph */
void ir_delete_inter_graph(ir_func_t *func);
/* print interference graph */
void ir_print_inter_graph(ir_func_t *func);
/* yeah you know */
void ir_build_inter_graph(ir_func_t *func);

#endif /* LIVENESS_H_ */
