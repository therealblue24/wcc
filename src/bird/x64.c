#include "bird.h"
#include <ctype.h>

/* is the instruction in form A = F(B, C) where it needs A and B to be separate? */
static bool ins_is_3source(enum ins_type t)
{
	/* ADD is not needed here because on x64 you can do lea A, [B+C] */
	return t == IR_INST_SUB || t == IR_INST_SMUL || t == IR_INST_UMUL ||
		   t == IR_INST_OR || t == IR_INST_AND || t == IR_INST_EOR ||
		   t == IR_INST_SHL || t == IR_INST_SHR || t == IR_INST_ASHR;
}

/* is the instruction in form A = F(B) where it needs A and B to be separate? */
static bool ins_is_2source(enum ins_type t)
{
	return t == IR_INST_NEG || t == IR_INST_NOT;
}

static void turn_into_x64_ins(ir_inst_t *prev, ir_inst_t *cur)
{
	if(!ins_is_3source(cur->type) && !ins_is_2source(cur->type)) {
		return;
	}

	if(ins_is_3source(cur->type)) {
		/* rewrite A = F(B, C) into A = B; A = F(A, C) */
		ir_inst_t *mov = ins_mov(cur->r0, cur->r1);
		cur->r1 = cur->r0;
		mov->next = cur;
		prev->next = mov;
		return;
	}

	if(ins_is_2source(cur->type)) {
		/* rewrite A = F(B) into A = B; A = F(A) */
		ir_inst_t *mov = ins_mov(cur->r0, cur->r1);
		cur->r1 = cur->r0;
		mov->next = cur;
		prev->next = mov;
		return;
	}
}

static void ir_turn_into_x64(ir_func_t *fun)
{
	for(size_t i = 0; i < list_len(fun->blocks); i++) {
		ir_blk_t *blk = fun->blocks[i];
		ir_inst_t *nop = ir_inst_make(IR_INST_NOP, NULL, NULL, NULL, 0);
		nop->next = blk->insts;
		blk->insts = nop;

		ir_inst_t *prev = blk->insts;
		ir_inst_t *cur = blk->insts->next;
		for(; cur; cur = cur->next) {
			turn_into_x64_ins(prev, cur);
			prev = cur;

			if(ir_inst_is_term(cur->type)) {
				break;
			}
		}

		blk->insts = blk->insts->next;
		ir_inst_delete(nop);
	}
}

void ir_func_opt_x64(ir_func_t *fun, int opt_level)
{
	UNUSED(opt_level);
	/* TODO: immediate inc/dec optimization */
	ir_turn_into_x64(fun);
	return;
}

void ir_prog_begin_x64_sysv(FILE *f, ir_prog_t *prog)
{
	UNUSED(prog);
	fprintf(f, "\t.section \".note.GNU-stack\", \"\", @progbits\n");
	fprintf(f, "\t.text\n");

	fprintf(f, "\t.align 16\n");
	fprintf(f, "\t.intel_syntax noprefix\n");
	return;
}

void ir_prog_end_x64_sysv(FILE *f, ir_prog_t *prog)
{
	UNUSED(f);
	UNUSED(prog);
	return;
}

static void print_escaped_chr(FILE *f, char c)
{
	switch(c) {
	case '\a':
		fprintf(f, "\\a");
		return;
	case '\b':
		fprintf(f, "\\b");
		return;
	case '\f':
		fprintf(f, "\\f");
		return;
	case '\n':
		fprintf(f, "\\n");
		return;
	case '\r':
		fprintf(f, "\\n");
		return;
	case '\t':
		fprintf(f, "\\t");
		return;
	case '\v':
		fprintf(f, "\\v");
		return;
	case '\\':
		fprintf(f, "\\");
		return;
	case '\'':
		fprintf(f, "\\'");
		return;
	case '\"':
		fprintf(f, "\\\"");
		return;
	case '\?':
		fprintf(f, "\\?");
		return;
	case 0:
		fprintf(f, "\\0");
		return;
	default:
		fprintf(f, "\\x%02x", c);
		return;
	}

	return;
}

static bool is_escapeprintable(int c)
{
	return c == '\\' || c == '\'' || c == '\"';
}

static void emit_str(FILE *f, const char *str, size_t len)
{
	if(str[len - 1]) {
		fprintf(f, "\t.ascii \"");
	} else {
		fprintf(f, "\t.asciz \"");
		len--;
	}

	for(size_t i = 0; i < len; i++) {
		if(isprint(str[i]) && !is_escapeprintable(str[i])) {
			fputc(str[i], f);
		} else {
			print_escaped_chr(f, str[i]);
		}
	}

	fprintf(f, "\"\n");
}

void ir_glob_emit_x64_sysv(FILE *f, ir_global_t *glob)
{
	if(!glob->is_anon) {
		fprintf(f, "\t.global %s\n", glob->name);
	}
	fprintf(f, glob->has_data ? "\t.data\n" : "\t.bss\n");
	fprintf(f, "%s:\n", glob->name);
	if(glob->has_data) {
		if(glob->is_str) {
			emit_str(f, (const char *)glob->data, glob->size);
		} else {
			for(size_t i = 0; i < glob->size; i++) {
				fprintf(f, "\t.byte %hhu\n", glob->data[i]);
			}
		}
	} else {
		fprintf(f, "\t.zero %zu\n", glob->size);
	}
	fprintf(f, "\n");
	return;
}

static const char *x64_reg[6] = { "rbx", "r12", "r13", "r14", "r15", NULL };
static const char *x64_reg8[6] = { "bl", "r12b", "r13b", "r14b", "r15b", NULL };
static const char *x64_reg16[6] = { "bx", "r12w", "r13w", "r14w", "r15w", NULL };
static const char *x64_reg32[6] = {
	"ebx", "r12d", "r13d", "r14d", "r15d", NULL
};

static const int x64_reg_count = 5;

static int64_t i64abs(int64_t v)
{
	if(v < 0) {
		return -v;
	}
	return v;
}

static INLINE void vload(FILE *f, size_t size, bool ext, int reg_to,
						 char *addr_fmt, va_list va)
{
	switch(size) {
	case 8:
	default:
		fprintf(f, "\tmov %s, ", x64_reg[reg_to]);
		vfprintf(f, addr_fmt, va);
		fprintf(f, "\n");
		break;
	case 4:
		if(ext) {
			fprintf(f, "\tmovsxd %s, dword ptr ", x64_reg[reg_to]);
		} else {
			fprintf(f, "\tmov %s, dword ptr ", x64_reg32[reg_to]);
		}
		vfprintf(f, addr_fmt, va);
		fprintf(f, "\n");
		break;
	case 2:
		if(ext) {
			fprintf(f, "\tmovsx %s, word ptr ", x64_reg[reg_to]);
		} else {
			fprintf(f, "\tmovzx %s, word ptr ", x64_reg32[reg_to]);
		}
		vfprintf(f, addr_fmt, va);
		fprintf(f, "\n");
		break;
	case 1:
		if(ext) {
			fprintf(f, "\tmovsx %s, byte ptr ", x64_reg[reg_to]);
		} else {
			fprintf(f, "\tmovzx %s, byte ptr ", x64_reg32[reg_to]);
		}
		vfprintf(f, addr_fmt, va);
		fprintf(f, "\n");
		break;
	}
	return;
}

static INLINE void vstore(FILE *f, size_t size, int reg_to, char *addr_fmt,
						  va_list va)
{
	switch(size) {
	case 8:
	default:
		fprintf(f, "\tmov ");
		vfprintf(f, addr_fmt, va);
		fprintf(f, ", %s\n", x64_reg[reg_to]);
		break;
	case 4:
		fprintf(f, "\tmov dword ptr ");
		vfprintf(f, addr_fmt, va);
		fprintf(f, ", %s\n", x64_reg32[reg_to]);
		break;
	case 2:
		fprintf(f, "\tmov word ptr ");
		vfprintf(f, addr_fmt, va);
		fprintf(f, ", %s\n", x64_reg16[reg_to]);
		break;
	case 1:
		fprintf(f, "\tmov byte ptr ");
		vfprintf(f, addr_fmt, va);
		fprintf(f, ", %s\n", x64_reg8[reg_to]);
		break;
	}
	return;
}

static void load(FILE *f, size_t size, bool ext, int reg_to, char *addr_fmt,
				 ...)
{
	va_list va;
	va_start(va, addr_fmt);
	vload(f, size, ext, reg_to, addr_fmt, va);
	va_end(va);
	return;
}

static void store(FILE *f, size_t size, int reg_to, char *addr_fmt, ...)
{
	va_list va;
	va_start(va, addr_fmt);
	vstore(f, size, reg_to, addr_fmt, va);
	va_end(va);
	return;
}

static const char *arg_reg[6] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };

static void ir_emit_blk_x64_sysv(FILE *f, ir_func_t *fn, ir_blk_t *blk,
								 long last_i)
{
	int can_omit = list_len(blk->pred) == 1 &&
				   blk->pred[0]->num == blk->num - 1;
	if(!can_omit) {
		fprintf(f, ".BB%ld:\n", blk->num);
	}
	for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
		int r0i = -1;
		UNUSEDA int r1i = -1;
		int r2i = -1;
		if(ins->r0 && ins->r0->rr >= 0) {
			r0i = ins->r0->rr;
		}
		if(ins->r1 && ins->r1->rr >= 0) {
			r1i = ins->r1->rr;
		}
		if(ins->r2 && ins->r2->rr >= 0) {
			r2i = ins->r2->rr;
		}
		const char *r0 = ins->r0 && ins->r0->rr >= 0 ? x64_reg[ins->r0->rr] :
													   NULL;
		const char *r1 = ins->r1 && ins->r1->rr >= 0 ? x64_reg[ins->r1->rr] :
													   NULL;
		const char *r2 = ins->r2 && ins->r2->rr >= 0 ? x64_reg[ins->r2->rr] :
													   NULL;
		const UNUSEDA char *r0b =
			ins->r0 && ins->r0->rr >= 0 ? x64_reg8[ins->r0->rr] : NULL;
		const char *r1b = ins->r1 && ins->r1->rr >= 0 ? x64_reg8[ins->r1->rr] :
														NULL;
		const UNUSEDA char *r2b =
			ins->r2 && ins->r2->rr >= 0 ? x64_reg8[ins->r2->rr] : NULL;
		const UNUSEDA char *r0w =
			ins->r0 && ins->r0->rr >= 0 ? x64_reg16[ins->r0->rr] : NULL;
		const char *r1w = ins->r1 && ins->r1->rr >= 0 ? x64_reg16[ins->r1->rr] :
														NULL;
		const UNUSEDA char *r2w =
			ins->r2 && ins->r2->rr >= 0 ? x64_reg16[ins->r2->rr] : NULL;
		const char *r0d = ins->r0 && ins->r0->rr >= 0 ? x64_reg32[ins->r0->rr] :
														NULL;
		const char *r1d = ins->r1 && ins->r1->rr >= 0 ? x64_reg32[ins->r1->rr] :
														NULL;
		const UNUSEDA char *r2d =
			ins->r2 && ins->r2->rr >= 0 ? x64_reg32[ins->r2->rr] : NULL;
		switch(ins->type) {
		case IR_INST_ZXT: {
			switch(ins->size) {
			case 8:
				fprintf(f, "\tmov %s, %s\n", r0, r1);
				break;
			case 4:
				fprintf(f, "\tmov %s, %s\n", r0d, r1d);
				break;
			case 2:
				fprintf(f, "\tmovzx %s, %s\n", r0d, r1w);
				break;
			case 1:
				fprintf(f, "\tmovzx %s, %s\n", r0d, r1b);
				break;
			}
		}; break;
		case IR_INST_SXT: {
			switch(ins->size) {
			case 8:
				fprintf(f, "\tmov %s, %s\n", r0, r1);
				break;
			case 4:
				fprintf(f, "\tmovsxd %s, %s\n", r0, r1d);
				break;
			case 2:
				fprintf(f, "\tmovsx %s, %s\n", r0, r1w);
				break;
			case 1:
				fprintf(f, "\tmovsx %s, %s\n", r0, r1b);
				break;
			}
		}; break;
		case IR_INST_CALL: {
			size_t stack_used = 0;
			for(size_t i = 0; i < list_len(ins->call_args); i++) {
				if(ins->call_args[i]->r->spilld) {
					fprintf(f, "\tmov %s, [rbp - %lld]\n",
							x64_reg[ins->call_args[i]->r->rr],
							i64abs(ins->call_args[i]->r->off));
				}
				if(i < 6) {
					fprintf(f, "\tmov %s, %s\n", arg_reg[i],
							x64_reg[ins->call_args[i]->r->rr]);
				} else {
					fprintf(f, "\tpush %s\n",
							x64_reg[ins->call_args[i]->r->rr]);
					stack_used += 8;
				}
			}
			/* set RAX to zero.
			 * needed for varadic functions
			 * where `al` is the number of float va-args
			 * we don't support varadic functions, but we do
			 * support calling them without any va-args */
			fprintf(f, "\txor eax, eax\n");
			fprintf(f, "\tcall %s\n", ins->fname);
			if(r0) {
				fprintf(f, "\tmov %s, rax\n", r0);
			}
			if(stack_used) {
				fprintf(f, "\tsub rsp, %zu\n", stack_used);
			}
		} break;

		case IR_INST_BREQ:
		case IR_INST_BRNE:
		case IR_INST_BRSLT:
		case IR_INST_BRSLE:
		case IR_INST_BRSGT:
		case IR_INST_BRSGE:
		case IR_INST_BRULT:
		case IR_INST_BRULE:
		case IR_INST_BRUGT:
		case IR_INST_BRUGE:
			fprintf(f, "\tcmp %s, %s\n", r1, r2);
			switch(ins->type) {
			default:
				break;
			case IR_INST_BREQ:
				fprintf(f, "\tjne .BB%ld\n", ins->false_blk->num);
				break;
			case IR_INST_BRNE:
				fprintf(f, "\tje .BB%ld\n", ins->false_blk->num);
				break;
			case IR_INST_BRSLT:
				fprintf(f, "\tjge .BB%ld\n", ins->false_blk->num);
				break;
			case IR_INST_BRSLE:
				fprintf(f, "\tjg .BB%ld\n", ins->false_blk->num);
				break;
			case IR_INST_BRSGT:
				fprintf(f, "\tjle .BB%ld\n", ins->false_blk->num);
				break;
			case IR_INST_BRSGE:
				fprintf(f, "\tjl .BB%ld\n", ins->false_blk->num);
				break;
			case IR_INST_BRULT:
				fprintf(f, "\tjae .BB%ld\n", ins->false_blk->num);
				break;
			case IR_INST_BRULE:
				fprintf(f, "\tja .BB%ld\n", ins->false_blk->num);
				break;
			case IR_INST_BRUGT:
				fprintf(f, "\tjbe .BB%ld\n", ins->false_blk->num);
				break;
			case IR_INST_BRUGE:
				fprintf(f, "\tjb .BB%ld\n", ins->false_blk->num);
				break;
			}
			if(ins->true_blk->num == blk->num + 1) {
				/* fallthrough */
				break;
			}
			/* dang it */
			fprintf(f, "\tjmp .BB%ld\n", ins->true_blk->num);
			break;
		case IR_INST_BR:
			fprintf(f, "\ttest %s, %s\n", r1, r1);
			fprintf(f, "\tje .BB%ld\n", ins->false_blk->num);
			/* big brain optimization */
			/* fallthrough to true block if in front of us */
			if(ins->true_blk->num == blk->num + 1) {
				break;
			}
			/* else don't */
			fprintf(f, "\tjmp .BB%ld\n", ins->true_blk->num);
			break;
		case IR_INST_JMP:
			/* big brain optimization */
			/* fallthrough if target in front of us */
			if(ins->true_blk->num == blk->num + 1) {
				break;
			}
			/* else just jump */
			fprintf(f, "\tjmp .BB%ld\n", ins->true_blk->num);
			break;
		case IR_INST_RET:
			if(r1 != NULL) {
				fprintf(f, "\tmov rax, %s\n", r1);
			}
			if(blk->num != last_i) {
				fprintf(f, "\tjmp %s_ret\n", fn->name);
			}
			break;
		case IR_INST_NOP:
			break;
		case IR_INST_MOV:
			fprintf(f, "\tmov %s, %s\n", r0, r1);
			break;
		case IR_INST_IMM:
			if(ins->imm == 0) {
				fprintf(f, "\txor %s, %s\n", r0d, r0d);
			} else {
				fprintf(f, "\tmov %s, %lld\n", r0, (int64_t)ins->imm);
			}
			break;
		case IR_INST_ADD:
			if(ins->r1->rr != ins->r0->rr) {
				fprintf(f, "\tlea %s, [%s + %s]\n", r0, r1, r2);
			} else {
				fprintf(f, "\tadd %s, %s\n", r0, r2);
			}
			break;
		case IR_INST_SUB:
			fprintf(f, "\tsub %s, %s\n", r0, r2);
			break;
		case IR_INST_SMUL:
		case IR_INST_UMUL:
			fprintf(f, "\timul %s, %s\n", r0, r2);
			break;
		case IR_INST_SDIV:
			// fprintf(f, "\tpush rdx\n");
			fprintf(f, "\tmov rax, %s\n", r1);
			fprintf(f, "\tcqo\n");
			fprintf(f, "\tidiv %s\n", r2);
			// fprintf(f, "\tpop rdx\n");
			fprintf(f, "\tmov %s, rax\n", r0);
			break;
		case IR_INST_UDIV:
			// fprintf(f, "\tpush rdx\n");
			fprintf(f, "\tmov rax, %s\n", r1);
			fprintf(f, "\txor edx, edx\n");
			fprintf(f, "\tdiv %s\n", r2);
			// fprintf(f, "\tpop rdx\n");
			fprintf(f, "\tmov %s, rax\n", r0);
			break;
		case IR_INST_AND:
			fprintf(f, "\tand %s, %s\n", r0, r2);
			break;
		case IR_INST_OR:
			fprintf(f, "\tor %s, %s\n", r0, r2);
			break;
		case IR_INST_EOR:
			fprintf(f, "\txor %s, %s\n", r0, r2);
			break;
		case IR_INST_SHL:
		case IR_INST_SHR:
		case IR_INST_ASHR:
			fprintf(f, "\tmov ecx, %s\n", r2d);
			switch(ins->type) {
			case IR_INST_SHL:
				fprintf(f, "\tshl %s, cl\n", r0);
				break;
			case IR_INST_SHR:
				fprintf(f, "\tshr %s, cl\n", r0);
				break;
			case IR_INST_ASHR:
				fprintf(f, "\tsar %s, cl\n", r0);
				break;
			default: /* wth? */
				break;
			}
			break;

		case IR_INST_LEA:
			fprintf(f, "\tlea %s, [rip + %s]\n", r0, ins->label->name);
			break;

		case IR_INST_SMOD:
			fprintf(f, "\tmov rax, %s\n", r1);
			fprintf(f, "\tcqo\n");
			fprintf(f, "\tidiv %s\n", r2);
			fprintf(f, "\tmov %s, rdx\n", r0);
			break;

		case IR_INST_UMOD:
			fprintf(f, "\tmov rax, %s\n", r1);
			fprintf(f, "\txor edx, edx\n");
			fprintf(f, "\tdiv %s\n", r2);
			fprintf(f, "\tmov %s, rdx\n", r0);
			break;
		case IR_INST_NOT:
			fprintf(f, "\tnot %s\n", r0);
			break;
		case IR_INST_MKBOOL:
			if(r0i != r1i) {
				fprintf(f, "\txor %s, %s\n", r0d, r0d);
				fprintf(f, "\ttest %s, %s\n", r1, r1);
				fprintf(f, "\tsetne %s\n", r0b);
			} else {
				fprintf(f, "\ttest %s, %s\n", r1, r1);
				fprintf(f, "\tsetne %s\n", r0b);
				fprintf(f, "\tmovzx %s, %s\n", r0d, r0b);
			}
			break;
		case IR_INST_NOTBOOL:
			if(r0i != r1i) {
				fprintf(f, "\txor %s, %s\n", r0d, r0d);
				fprintf(f, "\ttest %s, %s\n", r1, r1);
				fprintf(f, "\tsete %s\n", r0b);
			} else {
				fprintf(f, "\ttest %s, %s\n", r1, r1);
				fprintf(f, "\tsete %s\n", r0b);
				fprintf(f, "\tmovzx %s, %s\n", r0d, r0b);
			}
			break;
		case IR_INST_NEG:
			fprintf(f, "\tneg %s\n", r0);
			break;
		case IR_INST_EQ:
		case IR_INST_NE:
		case IR_INST_SLT:
		case IR_INST_SLE:
		case IR_INST_SGT:
		case IR_INST_SGE:
		case IR_INST_ULT:
		case IR_INST_ULE:
		case IR_INST_UGT:
		case IR_INST_UGE:
			if(r0i != r1i && r1i != r2i && r0i != r2i) {
				fprintf(f, "\txor %s, %s\n", r0d, r0d);
			}
			fprintf(f, "\tcmp %s, %s\n", r1, r2);

			switch(ins->type) {
			case IR_INST_EQ:
				fprintf(f, "\tsete %s\n", r0b);
				break;
			case IR_INST_NE:
				fprintf(f, "\tsetne %s\n", r0b);
				break;
			case IR_INST_SLT:
				fprintf(f, "\tsetl %s\n", r0b);
				break;
			case IR_INST_SLE:
				fprintf(f, "\tsetle %s\n", r0b);
				break;
			case IR_INST_SGT:
				fprintf(f, "\tsetg %s\n", r0b);
				break;
			case IR_INST_SGE:
				fprintf(f, "\tsetge %s\n", r0b);
				break;
			case IR_INST_ULT:
				fprintf(f, "\tsetb %s\n", r0b);
				break;
			case IR_INST_ULE:
				fprintf(f, "\tsetbe %s\n", r0b);
				break;
			case IR_INST_UGT:
				fprintf(f, "\tseta %s\n", r0b);
				break;
			case IR_INST_UGE:
				fprintf(f, "\tsetae %s\n", r0b);
				break;
			default: /* wth? */
				break;
			}
			if(!(r0i != r1i && r1i != r2i && r0i != r2i)) {
				fprintf(f, "\tmovzx %s, %s\n", r0d, r0b);
			}
			break;
		case IR_INST_LEAS:
			fprintf(f, "\tlea %s, [rbp - %lld]\n", r0, (int64_t)ins->imm);
			break;
		case IR_INST_LOAD:
			load(f, ins->size, ins->sign_ext, r0i, "[%s]", r1);
			break;
		case IR_INST_LOADS:
		case IR_INST_LOADSS:
			load(f, ins->size, ins->sign_ext, r0i, "[rbp - %lld]",
				 i64abs((int64_t)ins->imm));
			break;
			break;
		case IR_INST_STORE:
			store(f, ins->size, r2i, "[%s]", r1);
			break;
		case IR_INST_STORES:
		case IR_INST_STORESS:
			store(f, ins->size, r1i, "[rbp - %lld]", i64abs((int64_t)ins->imm));
			break;

		default:
			break;
		}

		if(ir_inst_is_term(ins->type)) {
			break;
		}
	}
	return;
}

static void ir_func_save_regs(FILE *f, ir_func_t *fun)
{
	/* luckily x86_64 is CISC, ez */
	for(size_t i = 0; i < x64_reg_count; i++) {
		if(fun->alloc_used[i] && i != 5 && i != 6) {
			fprintf(f, "\tpush %s\n", x64_reg[i]);
		}
	}
	return;
}

static void ir_func_restore_regs(FILE *f, ir_func_t *fun)
{
	for(size_t i = x64_reg_count - 1; i >= 0; i--) {
		if(fun->alloc_used[i] && i != 5 && i != 6) {
			fprintf(f, "\tpop %s\n", x64_reg[i]);
		}

		if(i == 0) {
			break;
		}
	}
	return;
}

void ir_func_emit_x64_sysv(FILE *f, ir_func_t *fun)
{
	char *name = fun->name;
	/* correct syntax */
	fprintf(f, "\t.global %s\n", name);
	fprintf(f, "\t.text\n");
	fprintf(f, "%s:\n", name);

	/* enter stack frame */
	size_t alignd = align_to(fun->stack_needed, 16);
	fprintf(f, "\tpush rbp\n");
	ir_func_save_regs(f, fun);

	fprintf(f, "\tmov rbp, rsp\n");
	size_t alen = list_len(fun->args);
	size_t stack_indx = 0;
	size_t space_needed = 0;
	size_t stack_disp = 16;
	for(size_t i = 0; i < x64_reg_count; i++) {
		if(fun->alloc_used[i]) {
			stack_disp += 8;
		}
	}
	if(alen > 6) {
		for(size_t i = 6; i < alen; i++) {
			stack_indx = space_needed;
			space_needed += fun->args[i]->size;
		}
	}
	/* setup function frame */

	for(size_t i = 0; i < list_len(fun->args); i++) {
		callreg_t *arg = fun->args[i];
		if(i < 6) {
			fprintf(f, "\tmov [rbp - %lld], %s\n", i64abs((int64_t)arg->r->off),
					arg_reg[i]);
		} else {
			ENSURE(stack_indx >= 0, "negative stack index, somehow");
			fprintf(f, "\tmov rax, [rsp + %zu]\n", stack_indx + stack_disp);
			fprintf(f, "\tmov [rbp - %lld], rax\n",
					i64abs((int64_t)arg->r->off));

			stack_indx -= arg->size;
		}
	}

	if(alignd) {
		fprintf(f, "\tsub rsp, %zu\n", alignd);
	}

	size_t len = list_len(fun->blocks);
	for(size_t i = 0; i < len; i++) {
		ir_emit_blk_x64_sysv(f, fun, fun->blocks[i],
							 (len - 1) + fun->blocks[0]->num);
	}

	/* leave stack frame */
	fprintf(f, "%s_ret:\n", name);

	fprintf(f, "\tmov rsp, rbp\n");
	ir_func_restore_regs(f, fun);
	fprintf(f, "\tpop rbp\n");
	fprintf(f, "\tret\n");
	fprintf(f, "\n");

	return;
}
