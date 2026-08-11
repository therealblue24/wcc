#ifndef DCE_H_
#define DCE_H_

#include "ir.h"

/* dead code elim */
int ir_dce(ir_func_t *fun);

/* aggressive dead code elim */
int ir_adce(ir_func_t *fun);

#endif /* DCE_H_ */
