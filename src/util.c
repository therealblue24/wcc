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
