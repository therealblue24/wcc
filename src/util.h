#ifndef UTIL_H_
#define UTIL_H_

#include "zz/base.h"

/* error, warning */
void compile_setsrc(char *base, char *fname);
void compile_err(char *loc, const char *fmt, ...);
void compile_warn(char *loc, const char *fmt, ...);
void compile_err_node(void *node, const char *fmt, ...);
void compile_warn_node(void *node, const char *fmt, ...);

void ast_print(void *n);
#endif /* UTIL_H_ */
