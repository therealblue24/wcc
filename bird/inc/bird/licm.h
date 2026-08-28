#ifndef LICM_H_
#define LICM_H_

#include "ir.h"

/* do loop invariant code motion */
int ir_licm(ir_func_t *fun);

#endif /* LICM_H_ */
