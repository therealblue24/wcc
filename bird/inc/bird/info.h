#ifndef INFO_H_
#define INFO_H_

#include "ir.h"

/* fill info for each register the instruction it comes from */
/* used for various optimzations */
void ir_placemarks(ir_func_t *func);

/* fill uses for each register */
void ir_fill_use(ir_func_t *func);

/* delete uses for each register */
void ir_del_use(ir_func_t *func);

#endif /* INFO_H_ */
