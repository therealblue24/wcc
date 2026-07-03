#ifndef CODEGEN_H_
#define CODEGEN_H_

#include "zz/base.h"
#include "lex.h"
#include "bird.h"
#include "parse.h"

/* generates code for a function */
void codegen_func(FILE *f, LIST(obj_t *) globals, int opt_level,
				  enum ir_arch backend);

#endif /* CODEGEN_H_ */
