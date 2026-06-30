/* semantics */
/* type checking, resolving, etc. */
#ifndef SEMA_H_
#define SEMA_H_

#include "parse.h"
#include "type.h"

/* does the whole semantics thing */
void sema_do(node_t *prog);

#endif /* SEMA_H_ */
