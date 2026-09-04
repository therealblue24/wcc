#ifndef PARSE_H_
#define PARSE_H_

#include "bird/bird.h"
#include "util.h"
#include "zz/strmap.h"
#include "lex.h"
#include "type.h"

/* parser */

enum node_kind {
	NODE_ADD, /* addition + */
	NODE_SUB, /* subtraction - */
	NODE_MUL, /* multiplication * */
	NODE_DIV, /* division / */
	NODE_MOD, /* modulus % */
	NODE_NUM, /* numbers 123456 */
	NODE_NEG, /* negation - */
	NODE_NOT, /* bitwise not ~ */
	NODE_LOGNEG, /* logical negation ! */
	NODE_SHL, /* shift left << */
	NODE_SHR, /* shift right >> */
	NODE_AND, /* bitwise and & */
	NODE_EOR, /* bitwise eor ^ */
	NODE_OR, /* bitwise or | */
	NODE_LOGAND, /* logical and && */
	NODE_LOGOR, /* logical or || */
	NODE_ASSIGN, /* assignment = */
	NODE_VAR, /* variable ident */
	NODE_EQ, /* equal == */
	NODE_NE, /* not equal != */
	NODE_GT, /* greater than > */
	NODE_GE, /* greater than or equal to >= */
	NODE_LT, /* less than < */
	NODE_LE, /* less than or equal to <= */
	NODE_COMMA, /* comma , */
	NODE_EXPR_STMT, /* expression statement */
	NODE_RET, /* return stmt */
	NODE_BLOCK, /* block of stmts */
	NODE_IF, /* if */
	NODE_WHILE, /* while */
	NODE_DOWHILE, /* do-while */
	NODE_FOR, /* for */
	NODE_ADDR, /* & */
	NODE_DEREF, /* * */
	NODE_FUNCALL, /* func() */
	NODE_STMT_EXPR, /* statement expression */
	NODE_BREAK, /* break */
	NODE_CONTINUE, /* continue */
	NODE_SWITCH, /* switch */
	NODE_CASE, /* case */
	NODE_DEFAULT, /* default */
	NODE_GOTO, /* goto */
	NODE_LABEL, /* label: */
	NODE_CAST, /* (type) */
	NODE_MEMBER, /* struct.member */
};

/* a variable */
struct node;
typedef struct obj {
	struct obj *next; /* linked list */
	long off; /* place on stack frame */
	char *name; /* name of variable */
	bool is_global; /* is this variable global? */
	bool is_anon; /* is this variable anonymous? */
	ir_global_t *glob; /* the global assoc with it */
	uint8_t *data; /* data for this global */
	size_t data_size; /* data size for this global */
	type_t *type; /* type of this var */
	bool addressed; /* is this variable addressed? (used for optimization) */
	bool skip; /* has this variable been turned into a register? */
	bool is_func; /* is this object a function? */
	bool is_static; /* is this object static? */
	struct node *body; /* body of the function */
	LIST(struct obj *) vars; /* variables of the function */
	struct obj *args; /* arguments to function */
	size_t stack_size; /* total size of this function's stack frame */
	reg_t *eq_reg; /* equivalent register, if used */
	long order; /* order of object */
	int eval; /* value of enum */
	bool econ; /* is this value a enum constant? */
} obj_t;

/* a struct member */
typedef struct member {
	type_t *type; /* member's type */
	token_t *ident; /* identifier */
	size_t loc; /* location relative to struct */
} member_t;

/* scope (single element, not stack) */

typedef struct scope_el {
	bool is_type; /* is this a type or var? */
	obj_t *var;
	type_t *type;
} scope_el_t;

typedef struct scope {
	STRMAP(scope_el_t *) vars;
	STRMAP(type_t *) types;
} scope_t;

/* an AST node */
typedef struct node {
	enum node_kind kind;
	/* left-, right-hand side of the tree */
	struct node *lhs, *rhs;
	struct node *next; /* next tree */
	struct node *body; /* inner block */
	type_t *type; /* type of this node */

	token_t *tok; /* first token of this node */

	/* accessing struct member */
	member_t *memb;

	/* if condition */
	struct node *cond;
	struct node *then;
	struct node *elze;

	/* for */
	struct node *init;
	struct node *inc;

	/* switch/case/default */
	ir_blk_t *case_blk;

	/* goto */
	char *label;
	struct node *label_node;
	bool visited;
	bool typed; /* is this node already typed? */

	char *fname; /* function name, for NODE_FUNCALL */
	struct node *fargs; /* function arguments, for NODE_FUNCALL */
	obj_t *var; /* for NODE_VAR */
	uint64_t num; /* for NODE_NUM */
} node_t;

extern STRMAP(obj_t *) known_funcs;

/* makes parsing arenas */
void parse_make_arenas(void);

/* deletes parsing arenas */
void parse_delete_arenas(void);

/* makes a node */
node_t *node_make(enum node_kind kind, token_t *tok);

/* deallocates a node */
void node_delete(node_t *node);

/* deallocates a whole node AST tree */
void node_delete_all(node_t *root);

/* -- node types -- */

/* make a binop node */
node_t *node_bin(enum node_kind kind, node_t *lhs, node_t *rhs, token_t *tok);

/* make a unaryop node */
node_t *node_unary(enum node_kind kind, node_t *lhs, token_t *tok);

/* make a number node */
node_t *node_num(uint64_t val, token_t *tok);

/* make a variable node */
node_t *node_var(obj_t *var, token_t *tok);

/* -- variables/objects -- */

/* create an object with a name `name` */
obj_t *obj_make(char *name, type_t *type, bool is_func);

/* delete an object */
void obj_delete(obj_t *obj);

/* deletes all objects in list */
void obj_delete_all(LIST(obj_t *) objs);

/* -- struct member -- */

/* makes a member */
member_t *member_make(type_t *type, token_t *ident, size_t loc);

typedef struct parse_res {
	LIST(obj_t *) globals;
} parse_res_t;

/* does the parsing */
parse_res_t parse_do(token_t *toks);

#endif /* PARSE_H_ */
