#include "util.h"

/* globals representing file, file name */

char *base;
char *fname;

void compile_setsrc(char *base_, char *fname_)
{
	base = base_;
	fname = fname_;
	return;
}

static int isnewline(int c)
{
	return c == '\r' || c == '\n';
}

static char *skip_newline(char *s)
{
	char *p = s;
	while(isnewline(*p))
		p++;
	return p;
}

/* internal compiler error/warn printing mechanism */
static void verror(char *loc, const char *lbl, const char *fmt, va_list va)
{
	/* find line, column */
	size_t line = 1;
	size_t col = 1;
	char *ln = base;
	char *p = base;
	char *dfname;

	if(!loc) {
		line = 0;
		col = 0;
		goto print;
	}

	while(*p && p < loc) {
		if(isnewline(*p)) {
			line++;
			col = 1;
			p = skip_newline(p);
			ln = p;
			continue;
		}
		col++;
		p++;
	}

print:
	dfname = "<stdin>";
	if(fname) {
		dfname = fname;
	}

	if(line && col) {
		fprintf(stderr, "%s:%zu:%zu: %s: ", dfname, line, col, lbl);
	} else {
		fprintf(stderr, "%s: %s: ", dfname, lbl);
	}
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");
	if(loc) {
		fprintf(stderr, "%4zu | ", line);
		p = ln;
		while(*p && !isnewline(*p)) {
			fputc(*p++, stderr);
		}
		size_t cur_col = 1;
		fprintf(stderr, "\r\n");
		fprintf(stderr, "       ");
		while(cur_col < col) {
			char c = *ln++;
			if(c == '\t') {
				fputc('\t', stderr);
			} else {
				fputc(' ', stderr);
			}
			cur_col++;
		}
		fputc('^', stderr);
		fputc('\n', stderr);
	}
	return;
}

void compile_err(char *loc, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	verror(loc, "error", fmt, va);
	va_end(va);
	exit(EXIT_FAILURE);
}

void compile_warn(char *loc, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	verror(loc, "warning", fmt, va);
	va_end(va);
}

int starts_with(char *thing, char *with)
{
	return strncmp(thing, with, strlen(with)) == 0;
}

#include "parse.h"

void compile_err_node(void *node, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	verror(((node_t *)node)->tok->loc, "error", fmt, va);
	va_end(va);
	exit(EXIT_FAILURE);
}

void compile_warn_node(void *node, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	verror(((node_t *)node)->tok->loc, "warning", fmt, va);
	va_end(va);
}

static void indent(int tab)
{
	/* 2 space tabs here because it's an AST. It's going to nest to infinity. */
	int work = tab * 2;
	while(work--) {
		putchar(' ');
	}
	return;
}

static void ast_visit(node_t *n, int tab)
{
#define out(...)         \
	printf(__VA_ARGS__); \
	break

	indent(tab);

	switch(n->kind) {
	case NODE_ADD:
		out("Add");
	case NODE_SUB:
		out("Sub");
	case NODE_MUL:
		out("Mul");
	case NODE_DIV:
		out("Div");
	case NODE_MOD:
		out("Mod");
	case NODE_NUM:
		out("Num");
	case NODE_NEG:
		out("Neg");
	case NODE_NOT:
		out("Not");
	case NODE_LOGNEG:
		out("LogicalNeg");
	case NODE_SHL:
		out("ShiftLeft");
	case NODE_SHR:
		out("ShiftRight");
	case NODE_AND:
		out("And");
	case NODE_EOR:
		out("Eor");
	case NODE_OR:
		out("Or");
	case NODE_LOGAND:
		out("LogicalAnd");
	case NODE_LOGOR:
		out("LogicalOr");
	case NODE_ASSIGN:
		out("Assign");
	case NODE_VAR:
		out("Var");
	case NODE_EQ:
		out("Equal");
	case NODE_NE:
		out("NotEqual");
	case NODE_GT:
		out("Greater");
	case NODE_GE:
		out("GreaterEqual");
	case NODE_LT:
		out("Less");
	case NODE_LE:
		out("LessEqual");
	case NODE_COMMA:
		out("Comma");
	case NODE_EXPR_STMT:
		out("ExprStmt");
	case NODE_RET:
		out("Return");
	case NODE_IF:
		out("If");
	case NODE_WHILE:
		out("While");
	case NODE_DOWHILE:
		out("DoWhile");
	case NODE_FOR:
		out("For");
	case NODE_ADDR:
		out("Address");
	case NODE_DEREF:
		out("Deref");
	case NODE_FUNCALL:
		out("Call");
	case NODE_STMT_EXPR:
		out("StmtExpr");
	case NODE_BREAK:
		out("Break");
	case NODE_CONTINUE:
		out("Continue");
	case NODE_SWITCH:
		out("Switch");
	case NODE_CASE:
		out("Case");
	case NODE_DEFAULT:
		out("DefaultCase");
	case NODE_GOTO:
		out("Goto");
	case NODE_LABEL:
		out("Label");
	case NODE_CAST:
		out("Cast");
	case NODE_MEMBER:
		out("Member");
	default:
		break;
	}

	putchar('\n');
	if(n->kind == NODE_VAR) {
		indent(tab + 1);
		printf("%s\n", n->var->name);
	}
	if(n->kind == NODE_MEMBER) {
		indent(tab + 1);
		printf("%s\n", n->memb->ident->content);
	}
	if(n->kind == NODE_NUM) {
		indent(tab + 1);
		printf("%lld\n", n->num);
	}

	if(n->cond) {
		indent(tab + 1);
		printf("Cond\n");
		ast_visit(n->cond, tab + 2);
	}
	if(n->then) {
		indent(tab + 1);
		printf("Then\n");
		ast_visit(n->then, tab + 2);
	}
	if(n->elze) {
		indent(tab + 1);
		printf("Else\n");
		ast_visit(n->elze, tab + 2);
	}
	if(n->init) {
		indent(tab + 1);
		printf("Init\n");
		ast_visit(n->init, tab + 2);
	}
	if(n->inc) {
		indent(tab + 1);
		printf("Post\n");
		ast_visit(n->inc, tab + 2);
	}

	if(n->body) {
		indent(tab + 1);
		printf("Block\n");
		ast_visit(n->body, tab + 2);
	}

	if(n->fargs) {
		indent(tab + 1);
		printf("Args\n");
		ast_visit(n->fargs, tab + 2);
	}

	if(n->next) {
		ast_visit(n->next, tab);
	}

	if(n->lhs) {
		ast_visit(n->lhs, tab + 1);
	}
	if(n->rhs) {
		ast_visit(n->rhs, tab + 1);
	}
	return;
}

void ast_print(void *n)
{
	ast_visit((node_t *)n, 0);
	return;
}
