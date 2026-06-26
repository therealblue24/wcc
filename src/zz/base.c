#include "base.h"

/* aligns `v` up to multiple of `to` */
size_t align_to(size_t v, size_t to)
{
	/* https://stackoverflow.com/a/12721849 */
	size_t r = v % to;
	return r ? v + (to - r) : v;
}

/* memory stuff */

/* calloc(1, size) -- never returns NULL, always errors out */
void *zalloc(size_t size)
{
	void *mem = malloc(size);
	ENSURE(mem, "failed to allocate %zu bytes of memory", size);
	wipe(mem, size);
	return mem;
}

/* calloc() but never returns NULL, always errors out */
void *zcalloc(size_t count, size_t size)
{
	void *mem = calloc(count, size);
	ENSURE(mem, "failed to allocate %zux%zu (%zu) bytes of memory", count, size,
		   count * size);
	return mem;
}

/* realloc() but always errors out */
void *zrealloc(void *ptr, size_t size)
{
	void *mem = realloc(ptr, size);
	/* can't zero out the realloc'd memory.. sad */
	ENSURE(mem, "failed to allocate %zu bytes of memory", size);
	return mem;
}

/* strcpy() for memory */
void *memdup(void *mem, size_t size)
{
	void *newmem = malloc(size);
	ENSURE(newmem, "failed to allocate %zu bytes of memory", size);
	memcpy(newmem, mem, size);
	return newmem;
}

/* memdup() but allocates extra space */
void *memdup_extra(void *mem, size_t src_size, size_t dst_size)
{
	ASSERT(dst_size >= src_size, "destination is smaller than source");
	void *newmem = malloc(dst_size);
	ENSURE(newmem, "failed to allocate %zu bytes of memory", dst_size);
	memcpy(newmem, mem, src_size);
	wipe(newmem + src_size, dst_size - src_size);
	return newmem;
}

#define FNV_OFFSET_BASIS (14695981039346656037LLU)
#define FNV_PRIME (1099511628211LLU)

/* fnv1a hash */
uint64_t fnv1a(void *data_, size_t size)
{
	ASSERT(data_, "cannot hash NULL");
	ASSERT(size >= 0, "cannot hash nothing");
	uint8_t *data = (uint8_t *)data_;

	uint64_t hash = FNV_OFFSET_BASIS;
	for(size_t i = 0; i < size; i++) {
		hash ^= data[i];
		hash *= FNV_PRIME;
	}

	return hash;
}

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

/* strdup() but portable */
char *mystrdup(char *str)
{
	return memdup_extra(str, strlen(str), strlen(str) + 1);
}

/* strndup() but portable */
char *mystrndup(char *str, size_t n)
{
	return memdup_extra(str, strnlen(str, n), strnlen(str, n) + 1);
}

/* reads a file; turns "\r\n" -> "\n" */
char *file_reader(FILE *f)
{
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	char *prog = zalloc(size + 2);
	char *crlf_to_lf = scr_alloc(size + 1);

	long pos = 0;
	while(pos < size) {
		long readback = fread(prog + pos, 1, size - pos, f);
		ENSURE(readback, "failed to read from file");
		pos += readback;
	}

	size_t j = 0;
	/* turn CRLF -> LF because yes */
	for(size_t i = 0; i < (size_t)size; i++) {
		if(prog[i] != '\r' && prog[i] != '\n') {
			crlf_to_lf[j++] = prog[i];
			continue;
		}

		if(prog[i] == '\r' && prog[i + 1] == '\n') {
			crlf_to_lf[j++] = '\n';
			continue;
		}

		if(prog[i] == '\n') {
			crlf_to_lf[j++] = '\n';
			continue;
		}

		ERROR("how did you get here");
	}

	free(prog);

	return crlf_to_lf;
}
