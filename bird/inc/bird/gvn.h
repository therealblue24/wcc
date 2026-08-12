#ifndef GVN_H_
#define GVN_H_

#include "ir.h"

#define GVN_INIT_SIZE (64)

/* global value numbering */
int ir_gvn(ir_func_t *func);

#endif /* GVN_H_ */
