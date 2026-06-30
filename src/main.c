#include "bird/ir.h"
#include <stdio.h>
#include <stdlib.h>
#include "zz/base.h"
#include "zz/arena.h"
#include "zz/strmap.h"
#include <ctype.h>
#include <stdarg.h>
#include "lex.h"
#include "preproc.h"
#include "parse.h"
#include "type.h"
#include "sema.h"
#include "codegen.h"

int debug = 0;
int opt_level = 0;

char *next_arg(int max, int *argc, char *argv[])
{
	if(*argc >= max) {
		ERROR("not enough args supplied");
	}
	char *arg = argv[*argc];
	(*argc)++;
	return arg;
}

static void version(char *pname)
{
	UNUSED(pname);
	printf("wcc version 0.0.2 build %s\n", __DATE__);
	return;
}

static void help(char *pname)
{
	version(pname);
	printf(
		"Usage: %s <input file> [-o <output asm file>] [-t <arch>-<abi>] [-d] [-?/--help]\n",
		pname);
	printf(
		"  -o <output>:\t\tfile to output assembly to (stdout is default)\n");
	printf("  -t <arch>-<abi>:\ttarget architecture, abi\n");
	printf("                  \tonly aarch64-apple, x64-sysv are supported.\n");
	printf("  -d:\t\t\tenable debug IR printing\n");
	printf("  -O0/1/2/3:\t\toptimization level (default: 0)\n");
	printf("  -?, --help:\t\tthis page\n");
	return;
}

static char *vfmt(char *fmt, va_list va)
{
	char *res = NULL;
	vasprintf(&res, fmt, va);
	return res;
}

static char *fmt(char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	char *res = vfmt(fmt, va);
	va_end(va);
	return res;
}

static void test_strmap(void)
{
	STRMAP(int) map = strmap_make(int);
	/* populate with keys */
	for(int i = 0; i < 1000; i++) {
		char *s = fmt("big key %d", i);
		strmap_put(map, s, i);
		if(*strmap_get(map, s) != i) {
			printf("uh oh\n");
			exit(1);
		}
		if(*strmap_get(map, "big key 0") != 0) {
			printf("uh oh\n");
			exit(1);
		}
	}

	/* check */
	for(int i = 0; i < 1000; i++) {
		char *s = fmt("big key %d", i);
		int j = *strmap_get(map, s);
		if(j != i) {
			printf("uh oh\n");
			exit(1);
		}
	}

	/* delete all odds */
	for(int i = 0; i < 1000; i++) {
		if(!(i & 1)) {
			continue;
		}
		char *s = fmt("big key %d", i);
		strmap_del(map, s);
	}

	/* check */
	for(int i = 0; i < 1000; i++) {
		char *s = fmt("big key %d", i);
		if(i & 1) {
			if(strmap_has(map, s)) {
				printf("uh oh\n");
				exit(1);
			}
		} else {
			int j = *strmap_get(map, s);
			if(j != i) {
				printf("uh oh\n");
				exit(1);
			}
		}
	}

	/* check iter */
	UNUSEDA char *key;
	int val;
	strmap_iter(map, key, val, {
		if(val & 1) {
			printf("uh oh\n");
			exit(1);
		}
		if(!starts_with(key, "big key")) {
			printf("uh oh\n");
			exit(1);
		}
	});

	strmap_delete(map);
}

int main(int argc, char *argv[])
{
	ENSURE(sizeof(char) == 1 && sizeof(short) == 2 && sizeof(int) == 4,
		   "invalid runtime platform");

	ENSURE(sizeof(long) == sizeof(long long) && sizeof(long) == 8,
		   "Look, I'll add Windows support later, not now.");

	if(scr_init()) {
		ERROR("failed to allocate scratch allocator");
		return 1;
	}

	if(argc == 1) {
		help(argv[0]);
		return 1;
	}

	enum ir_arch arch = DEFAULT_BACKEND;

	FILE *read_from = NULL;
	char *read_from_name = NULL;
	FILE *emit_to = NULL;

	int argc2 = 1;
	while(argc2 < argc) {
		const char *arg = argv[argc2++];
		if(strcmp(arg, "-o") == 0) {
			arg = next_arg(argc, &argc2, argv);
			if(!emit_to) {
				emit_to = fopen(arg, "w");
				ENSURE(emit_to, "failed to open file '%s'", arg);
			}
			continue;
		}
		if(strcmp(arg, "-t") == 0) {
			arg = next_arg(argc, &argc2, argv);
			if(strcmp(arg, "aarch64-apple") == 0) {
				arch = IR_ARCH_AARCH64_APPLE;
			}
			if(strcmp(arg, "x64-sysv") == 0) {
				arch = IR_ARCH_X64_SYSV;
			}
			continue;
		}

		if(strcmp(arg, "-d") == 0) {
			debug = 1;
			continue;
		}

		if(strcmp(arg, "--internal-test-strmap") == 0) {
			test_strmap();
			printf("OK\n");
			exit(0);
		}

		if(strcmp(arg, "--help") == 0 || strcmp(arg, "-?") == 0) {
			help(argv[0]);
			return 1;
		}
		if(strstr(arg, ".c")) {
			if(!read_from) {
				read_from = fopen(arg, "r");
				read_from_name = (char *)arg;
				ENSURE(read_from, "failed to open file '%s'", arg);
			} else {
				ENSURE(2 + 2 == 3, "TODO: Multiple file compilation");
			}
			continue;
		}
		if(starts_with((char *)arg, "-O")) {
			char *num = (char *)arg + 2;
			if(num && *num >= '0' && *num <= '3') {
				opt_level = *num - '0';
			} else {
				WARN("unknown optimization level '%s', defaulting to 0", arg);
				opt_level = 0;
			}
			continue;
		}

		ERROR("unknown argument '%s'", arg);
	}

	if(!read_from) {
		ERROR("need an input file");
	}

	if(!emit_to) {
		emit_to = stdout;
	}

	char *prog = file_reader(read_from);
	fclose(read_from);
	compile_setsrc(prog, read_from_name);

	token_t *head = lex_do(prog, NULL);
	head = preproc_do(head);

	token_t *cur = head;
	parse_res_t res = parse_do(cur);
	for(size_t i = 0; i < list_len(res.globals); i++) {
		obj_t *glob = res.globals[i];
		if(glob->is_func) {
			sema_do(glob->body);
		}
	}

	LIST(obj_t *) globals = res.globals;
	codegen_func(emit_to, globals, opt_level, arch);

	fclose(emit_to);

	token_delete_all(head);
	for(size_t i = 0; i < list_len(globals); i++) {
		node_t *curnode = globals[i]->body;
		node_t *nxtnode = NULL;
		while(curnode) {
			nxtnode = curnode->next;
			node_delete_all(curnode);
			curnode = nxtnode;
		}
	}

	for(size_t i = 0; i < list_len(globals); i++) {
		if(globals[i]) {
			obj_delete_all(globals[i]->vars);
		}
	}

	list_delete(globals);

	scr_cleanup();
	return 0;
}
