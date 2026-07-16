/* immediate folding (into ins) */
#ifndef IMMFOLD_H_
#define IMMFOLD_H_

#include "ir.h"

/* track usage counts of immediates */
void ir_immfold_analyze(ir_func_t *fun);

/* do the folding */
void ir_immfold_do(ir_func_t *fun, int64_t lower_bound, int64_t higher_bound,
				   enum ir_arch arch);

#endif /* IMMFOLD_H_ */
