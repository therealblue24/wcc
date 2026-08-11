#ifndef MEM_H_
#define MEM_H_

#include "ir.h"

/* promote leas-load and store to loads(tack) and stores(tack) */
int ir_stackopt(ir_func_t *func);

/* reduce stack stores */
int ir_stackreduce(ir_func_t *func);

/* peephole memory optimizer -- simplifies trivial cases */
int ir_memopt(ir_func_t *func);

#endif /* MEM_H_ */
