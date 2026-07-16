#include "bird.h"
#include "ir.h"
#include <ctype.h>

void ir_func_opt_aarch64(ir_func_t *fun, int opt_level)
{
	UNUSED(opt_level);
	UNUSED(fun);
	return;
}

static void load_imm_lane(FILE *f, int reg, uint16_t i, uint16_t shift,
						  int *keep, int sz)
{
	char *kt = "zk";
	char *ss = "xw";
	if(i) {
		fprintf(f, "\tmov%c %c%d, #%hu", kt[*keep], ss[sz], reg, i);
		if(shift) {
			fprintf(f, ", lsl #%hu", shift);
		}
		fprintf(f, "\n");
		*keep = 1;
	}
	return;
}

static void load_imm_w(FILE *f, int reg, uint32_t imm_)
{
	uint32_t imm = imm_;
	int32_t imms = (int32_t)imm;

	if(imms <= 4095 && imms >= -4095) {
		fprintf(f, "\tmov w%d, #%d\n", reg, imms);
		return;
	}

	int keep = 0;

	load_imm_lane(f, reg, (imm >> 0) & 0xffff, 0, &keep, 1);
	load_imm_lane(f, reg, (imm >> 16) & 0xffff, 16, &keep, 1);
	return;
}

/* loads an immediate into `reg` */
static void load_imm(FILE *f, int reg, uint64_t imm_)
{
	uint64_t imm = imm_;
	int64_t imms = (int64_t)imm;

	if(imms <= 4095 && imms >= -4095) {
		fprintf(f, "\tmov x%d, #%lld\n", reg, imms);
		return;
	}

	/* load into w reg instead */
	if((imm & 0xffffffff) == imm) {
		load_imm_w(f, reg, imm);
		return;
	}

	int keep = 0;

	load_imm_lane(f, reg, (imm >> 0) & 0xffff, 0, &keep, 0);
	load_imm_lane(f, reg, (imm >> 16) & 0xffff, 16, &keep, 0);
	load_imm_lane(f, reg, (imm >> 32) & 0xffff, 32, &keep, 0);
	load_imm_lane(f, reg, (imm >> 48) & 0xffff, 48, &keep, 0);

	return;
}

static int load_fp_imm_x10(FILE *f, long off, bool save)
{
	if((-off) >= 65535) {
		ERROR("cannot emit code: stack size larger than 64K");
	}
	if((-off) < 255) {
		return 0;
	}

	if(save) {
		fprintf(f, "\tstr x10, [sp, #-16]!\n");
	}

	fprintf(f, "\tmovn x10, #%llu\n", (uint64_t)(-off));
	return 1;
}

static int arm_reg[10] = { 19, 20, 21, 22, 23, 24, 25, 26, 27, 28 };
static const int arm_reg_count = 10;

void ir_prog_begin_aarch64_apple(FILE *f, ir_prog_t *prog)
{
	UNUSED(prog);
	fprintf(f, "\t.p2align 4\n");
}

void ir_prog_end_aarch64_apple(FILE *f, ir_prog_t *prog)
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

void ir_glob_emit_aarch64_apple(FILE *f, ir_global_t *glob)
{
	if(!glob->is_anon) {
		fprintf(f, "\t.globl _%s\n", glob->name);
	}
	fprintf(f, "\t.data\n");
	if(!glob->has_data) {
		fprintf(f, "\t.zerofill __DATA, __common, _%s, %zu, %zu\n", glob->name,
				glob->size, glob->align);
	} else {
		fprintf(f, "_%s:\n", glob->name);
		if(glob->is_str) {
			emit_str(f, (const char *)glob->data, glob->size);
		} else {
			for(size_t i = 0; i < glob->size; i++) {
				fprintf(f, "\t.byte %hhu\n", glob->data[i]);
			}
		}
	}
	fprintf(f, "\n");
	return;
}

static INLINE void vload(FILE *f, size_t size, bool ext, int reg_to,
						 char *addr_fmt, va_list va)
{
	switch(size) {
	case 8:
		fprintf(f, "\tldr x%d, ", reg_to);
		vfprintf(f, addr_fmt, va);
		fprintf(f, "\n");
		break;
	case 4:
		fprintf(f, ext ? "\tldrsw x%d, " : "\tldr w%d, ", reg_to);
		vfprintf(f, addr_fmt, va);
		fprintf(f, "\n");
		break;
	case 2:
		fprintf(f, ext ? "\tldrsh x%d, " : "\tldrh w%d, ", reg_to);
		vfprintf(f, addr_fmt, va);
		fprintf(f, "\n");
		break;
	case 1:
		fprintf(f, ext ? "\tldrsb x%d, " : "\tldrb w%d, ", reg_to);
		vfprintf(f, addr_fmt, va);
		fprintf(f, "\n");
		break;
	default:
		break;
	}
	return;
}

static INLINE void vstore(FILE *f, size_t size, int reg_to, char *addr_fmt,
						  va_list va)
{
	switch(size) {
	case 8:
		fprintf(f, "\tstr x%d, ", reg_to);
		vfprintf(f, addr_fmt, va);
		fprintf(f, "\n");
		break;
	case 4:
		fprintf(f, "\tstr w%d, ", reg_to);
		vfprintf(f, addr_fmt, va);
		fprintf(f, "\n");
		break;
	case 2:
		fprintf(f, "\tstrh w%d, ", reg_to);
		vfprintf(f, addr_fmt, va);
		fprintf(f, "\n");
		break;
	case 1:
		fprintf(f, "\tstrb w%d, ", reg_to);
		vfprintf(f, addr_fmt, va);
		fprintf(f, "\n");
		break;
	default:
		break;
	}
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

static void ir_ins_abi_call_aarch64(FILE *f, ir_func_t *func, ir_blk_t *blk,
									ir_inst_t *ins, LIST(callreg_t *) args,
									int r0, int r1, int r2)
{
	UNUSED(func);
	UNUSED(blk);
	UNUSED(r1);
	UNUSED(r2);

	size_t alen = list_len(args);
	size_t stack_indx = 0;
	size_t space_needed = 0;
	if(alen > 8) {
		for(size_t i = 8; i < alen; i++) {
			stack_indx = space_needed;
			space_needed += args[i]->size;
		}
		fprintf(f, "\tsub sp, sp, #%zu\n", space_needed);
	}

	for(size_t i = 0; i < alen; i++) {
		reg_t *reg = args[i]->r;
		int arg = arm_reg[reg->rr];
		if(reg->spilld2) {
			arg = 10;
			if(load_fp_imm_x10(f, reg->off, false)) {
				fprintf(f, "\tldr x%d, [fp, x10]\n", arg);
			} else {
				fprintf(f, "\tldr x%d, [fp, #%ld]\n", arg, reg->off);
			}
		}

		if(i <= 7) {
			fprintf(f, "\tmov x%zu, x%d\n", i, arg);
		} else {
			ENSURE(stack_indx >= 0, "negative stack index, somehow");
			fprintf(f, "\tstr x%d, [sp, #%zu]\n", arg, stack_indx);
			stack_indx -= args[i]->size;
		}
	}
	fprintf(f, "\tbl _%s\n", ins->fname);
	if(ins->r0) {
		fprintf(f, "\tmov x%d, x0\n", r0);
	}
	if(space_needed) {
		fprintf(f, "\tadd sp, sp, #%zu\n", space_needed);
	}

	return;
}

static void ir_emit_blk_aarch64_apple(FILE *f, ir_func_t *fn, ir_blk_t *blk,
									  long last_i)
{
	/* todo: smarter basic block placement */
	int can_omit = list_len(blk->pred) == 1 &&
				   blk->pred[0]->num == blk->num - 1;
	if(!can_omit) {
		fprintf(f, ".BB%ld: \t\t; preds = ", blk->num);
		for(size_t i = 0; i < list_len(blk->pred); i++) {
			fprintf(f, ".BB%ld, ", blk->pred[i]->num);
		}
		fprintf(f, "\n");
	}
	for(ir_inst_t *ins = blk->insts; ins; ins = ins->next) {
		int r0 = ins->r0 && ins->r0->rr >= 0 ? arm_reg[ins->r0->rr] : -1;
		int r1 = ins->r1 && ins->r1->rr >= 0 ? arm_reg[ins->r1->rr] : -1;
		int r2 = ins->r2 && ins->r2->rr >= 0 ? arm_reg[ins->r2->rr] : -1;
		size_t sz = ins->size;
		int64_t imm = (int64_t)ins->imm;
		switch(ins->type) {
		case IR_INST_ZXT:
			switch(ins->size) {
			case 1:
				fprintf(f, "\tuxtb w%d, w%d\n", r0, r1);
				break;
			case 2:
				fprintf(f, "\tuxth w%d, w%d\n", r0, r1);
				break;
			case 4:
				/* this uses w registers don't misread it */
				fprintf(f, "\tmov w%d, w%d\n", r0, r1);
				break;
			case 8:
			default:
				if(r0 != r1) {
					fprintf(f, "\tmov x%d, x%d\n", r0, r1);
				}
				break;
			}
			break;
		case IR_INST_SXT:
			switch(ins->size) {
			case 1:
				fprintf(f, "\tsxtb x%d, x%d\n", r0, r1);
				break;
			case 2:
				fprintf(f, "\tsxth x%d, x%d\n", r0, r1);
				break;
			case 4:
				fprintf(f, "\tsxtw x%d, x%d\n", r0, r1);
				break;
			case 8:
			default:
				if(r0 != r1) {
					fprintf(f, "\tmov x%d, x%d\n", r0, r1);
				}
				break;
			}
			break;
		case IR_INST_CALL:
			ir_ins_abi_call_aarch64(f, fn, blk, ins, ins->call_args, r0, r1,
									r2);
			break;
		case IR_INST_BREQI:
		case IR_INST_BRNEI:
		case IR_INST_BRSLTI:
		case IR_INST_BRSLEI:
		case IR_INST_BRSGTI:
		case IR_INST_BRSGEI:
		case IR_INST_BRULTI:
		case IR_INST_BRULEI:
		case IR_INST_BRUGTI:
		case IR_INST_BRUGEI:
			fprintf(f, "\tcmp x%d, #%lld\n", r1, imm);
			goto branch_cond;
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
			fprintf(f, "\tcmp x%d, x%d\n", r1, r2);
branch_cond:
			switch(ins->type) {
			default:
				break;
			/* remember: this is for false condition
			 * so invert the specified condition */
			case IR_INST_BREQ:
			case IR_INST_BREQI:
				fprintf(f, "\tbne .BB%ld\n", ins->false_blk->num);
				break;
			case IR_INST_BRNE:
			case IR_INST_BRNEI:
				fprintf(f, "\tbeq .BB%ld\n", ins->false_blk->num);
				break;
			case IR_INST_BRSLT:
			case IR_INST_BRSLTI:
				fprintf(f, "\tbge .BB%ld\n", ins->false_blk->num);
				break;
			case IR_INST_BRSLE:
			case IR_INST_BRSLEI:
				fprintf(f, "\tbgt .BB%ld\n", ins->false_blk->num);
				break;
			case IR_INST_BRSGT:
			case IR_INST_BRSGTI:
				fprintf(f, "\tble .BB%ld\n", ins->false_blk->num);
				break;
			case IR_INST_BRSGE:
			case IR_INST_BRSGEI:
				fprintf(f, "\tblt .BB%ld\n", ins->false_blk->num);
				break;
			case IR_INST_BRULT:
			case IR_INST_BRULTI:
				fprintf(f, "\tbhs .BB%ld\n", ins->false_blk->num);
				break;
			case IR_INST_BRULE:
			case IR_INST_BRULEI:
				fprintf(f, "\tbhi .BB%ld\n", ins->false_blk->num);
				break;
			case IR_INST_BRUGT:
			case IR_INST_BRUGTI:
				fprintf(f, "\tbls .BB%ld\n", ins->false_blk->num);
				break;
			case IR_INST_BRUGE:
			case IR_INST_BRUGEI:
				fprintf(f, "\tblo .BB%ld\n", ins->false_blk->num);
				break;
			}
			if(ins->true_blk->num == blk->num + 1) {
				/* fallthrough */
				break;
			}
			/* dang it */
			fprintf(f, "\tb .BB%ld\n", ins->true_blk->num);
			break;
		case IR_INST_BR:
			fprintf(f, "\tcbz x%d, .BB%ld\n", r1, ins->false_blk->num);
			/* big brain optimization */
			/* fallthrough to true block if in front of us */
			if(ins->true_blk->num == blk->num + 1) {
				break;
			}
			/* else don't */
			fprintf(f, "\tb .BB%ld\n", ins->true_blk->num);
			break;
		case IR_INST_JMP:
			/* big brain optimization */
			/* fallthrough if target in front of us */
			if(ins->true_blk->num == blk->num + 1) {
				break;
			}
			/* else just jump */
			fprintf(f, "\tb .BB%ld\n", ins->true_blk->num);
			break;
		case IR_INST_RET:
			if(r1 != -1) {
				fprintf(f, "\tmov x0, x%d\n", r1);
			}
			if(blk->num != last_i) {
				fprintf(f, "\tb .L%s_ret\n", fn->name);
			}
			break;
		case IR_INST_NOP:
			break;
		case IR_INST_MOV:
			fprintf(f, "\tmov x%d, x%d\n", r0, r1);
			break;
		case IR_INST_IMM:
			load_imm(f, r0, ins->imm);
			break;
		case IR_INST_ADD:
			fprintf(f, "\tadd x%d, x%d, x%d\n", r0, r1, r2);
			break;
		case IR_INST_SUB:
			fprintf(f, "\tsub x%d, x%d, x%d\n", r0, r1, r2);
			break;
		case IR_INST_ADDI:
			fprintf(f, "\tadd x%d, x%d, #%lld\n", r0, r1, imm);
			break;
		case IR_INST_SUBI:
			fprintf(f, "\tsub x%d, x%d, #%lld\n", r0, r1, imm);
			break;
		case IR_INST_AND:
			fprintf(f, "\tand x%d, x%d, x%d\n", r0, r1, r2);
			break;
		case IR_INST_OR:
			fprintf(f, "\torr x%d, x%d, x%d\n", r0, r1, r2);
			break;
		case IR_INST_EOR:
			fprintf(f, "\teor x%d, x%d, x%d\n", r0, r1, r2);
			break;
		case IR_INST_ANDI:
			fprintf(f, "\tand x%d, x%d, #%lld\n", r0, r1, imm);
			break;
		case IR_INST_ORI:
			fprintf(f, "\torr x%d, x%d, #%lld\n", r0, r1, imm);
			break;
		case IR_INST_EORI:
			fprintf(f, "\teor x%d, x%d, #%lld\n", r0, r1, imm);
			break;
		case IR_INST_SHL:
			fprintf(f, "\tlsl x%d, x%d, x%d\n", r0, r1, r2);
			break;
		case IR_INST_SHR:
			fprintf(f, "\tlsr x%d, x%d, x%d\n", r0, r1, r2);
			break;
		case IR_INST_ASHR:
			fprintf(f, "\tasr x%d, x%d, x%d\n", r0, r1, r2);
			break;
		case IR_INST_SHLI:
			fprintf(f, "\tlsl x%d, x%d, #%lld\n", r0, r1, imm);
			break;
		case IR_INST_SHRI:
			fprintf(f, "\tlsr x%d, x%d, #%lld\n", r0, r1, imm);
			break;
		case IR_INST_ASHRI:
			fprintf(f, "\tasr x%d, x%d, #%lld\n", r0, r1, imm);
			break;
		case IR_INST_SMUL:
		case IR_INST_UMUL:
			fprintf(f, "\tmul x%d, x%d, x%d\n", r0, r1, r2);
			break;
		case IR_INST_SDIV:
			fprintf(f, "\tsdiv x%d, x%d, x%d\n", r0, r1, r2);
			break;
		case IR_INST_SMOD:
			fprintf(f, "\tsdiv x10, x%d, x%d\n", r1, r2);
			fprintf(f, "\tmsub x%d, x10, x%d, x%d\n", r0, r2, r1);
			break;
		case IR_INST_UDIV:
			fprintf(f, "\tudiv x%d, x%d, x%d\n", r0, r1, r2);
			break;
		case IR_INST_UMOD:
			fprintf(f, "\tudiv x10, x%d, x%d\n", r1, r2);
			fprintf(f, "\tmsub x%d, x10, x%d, x%d\n", r0, r2, r1);
			break;
		case IR_INST_NOT:
			fprintf(f, "\tmvn x%d, x%d\n", r0, r1);
			break;
		case IR_INST_MKBOOL:
			fprintf(f, "\ttst x%d, x%d\n", r1, r1);
			fprintf(f, "\tcset x%d, ne\n", r0);
			break;
		case IR_INST_NOTBOOL:
			fprintf(f, "\ttst x%d, x%d\n", r1, r1);
			fprintf(f, "\tcset x%d, eq\n", r0);
			break;
		case IR_INST_NEG:
			fprintf(f, "\tneg x%d, x%d\n", r0, r1);
			break;
		case IR_INST_EQI:
		case IR_INST_NEI:
		case IR_INST_SLTI:
		case IR_INST_SLEI:
		case IR_INST_SGTI:
		case IR_INST_SGEI:
		case IR_INST_ULTI:
		case IR_INST_ULEI:
		case IR_INST_UGTI:
		case IR_INST_UGEI:
			fprintf(f, "\tcmp x%d, #%lld\n", r1, imm);
			goto set_cond;
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
			fprintf(f, "\tcmp x%d, x%d\n", r1, r2);
set_cond:
			switch(ins->type) {
			case IR_INST_EQ:
			case IR_INST_EQI:
				fprintf(f, "\tcset x%d, eq\n", r0);
				break;
			case IR_INST_NE:
			case IR_INST_NEI:
				fprintf(f, "\tcset x%d, ne\n", r0);
				break;
			case IR_INST_SLT:
			case IR_INST_SLTI:
				fprintf(f, "\tcset x%d, lt\n", r0);
				break;
			case IR_INST_SLE:
			case IR_INST_SLEI:
				fprintf(f, "\tcset x%d, le\n", r0);
				break;
			case IR_INST_SGT:
			case IR_INST_SGTI:
				fprintf(f, "\tcset x%d, gt\n", r0);
				break;
			case IR_INST_SGE:
			case IR_INST_SGEI:
				fprintf(f, "\tcset x%d, ge\n", r0);
				break;
			case IR_INST_ULT:
			case IR_INST_ULTI:
				fprintf(f, "\tcset x%d, lo\n", r0);
				break;
			case IR_INST_ULE:
			case IR_INST_ULEI:
				fprintf(f, "\tcset x%d, ls\n", r0);
				break;
			case IR_INST_UGT:
			case IR_INST_UGTI:
				fprintf(f, "\tcset x%d, hi\n", r0);
				break;
			case IR_INST_UGE:
			case IR_INST_UGEI:
				fprintf(f, "\tcset x%d, hs\n", r0);
				break;
			default: /* wth? */
				break;
			}
			break;
		case IR_INST_LEAS:
			if(load_fp_imm_x10(f, imm, false)) {
				fprintf(f, "\tadd x%d, fp, x10\n", r0);
			} else {
				fprintf(f, "\tsub x%d, fp, #%lld\n", r0, imm);
			}
			break;
		case IR_INST_LEA:
			fprintf(f, "\tadrp x%d, _%s@PAGE\n", r0, ins->label->name);
			fprintf(f, "\tadd x%d, x%d, _%s@PAGEOFF\n", r0, r0,
					ins->label->name);
			break;
		case IR_INST_LOAD:
			load(f, sz, ins->sign_ext, r0, "[x%d]", r1);
			break;
		case IR_INST_LOADS:
			if(load_fp_imm_x10(f, imm, false)) {
				load(f, sz, ins->sign_ext, r0, "[fp, x10]");
			} else {
				load(f, sz, ins->sign_ext, r0, "[fp, #%lld]", imm);
			}
			break;
		case IR_INST_LOADSS:
			if(load_fp_imm_x10(f, imm, false)) {
				load(f, sz, ins->sign_ext, r0, "[fp, x10]\t ; spilled load");
			} else {
				load(f, sz, ins->sign_ext, r0, "[fp, #%lld]\t ; spilled load",
					 imm);
			}
			break;

		case IR_INST_STORE:
			store(f, sz, r2, "[x%d]", r1);
			break;
		case IR_INST_STORES:
			if(load_fp_imm_x10(f, imm, false)) {
				store(f, sz, r1, "[fp, x10]");
			} else {
				store(f, sz, r1, "[fp, #%lld]", imm);
			}
			break;
		case IR_INST_STORESS:
			if(load_fp_imm_x10(f, imm, false)) {
				store(f, sz, r1, "[fp, x10]\t ; spilled store");
			} else {
				store(f, sz, r1, "[fp, #%lld]\t ; spilled store", imm);
			}
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

static int ir_func_save_regs_callee(FILE *f, ir_func_t *fun)
{
	bool *used_copy = zcalloc(arm_reg_count, sizeof(bool));
	memcpy(used_copy, fun->alloc_used, arm_reg_count * sizeof(bool));

	/* save all registers needing to be saved */
	int reg1 = -1;
	int reg2 = -1;
	/* batch up 2 regs */
	for(size_t i = 0; i < arm_reg_count; i++) {
		if(used_copy[i]) {
			if(reg1 == -1) {
				reg1 = (int)i;
			} else if(reg2 == -1) {
				reg2 = (int)i;
				fprintf(f, "\tstp x%d, x%d, [sp, #-16]!\n", arm_reg[reg1],
						arm_reg[reg2]);
				used_copy[reg1] = used_copy[reg2] = 0;
				reg1 = reg2 = -1;
			}
		}
	}

	int ret = -1;

	/* if any remain emit */
	for(size_t i = 0; i < arm_reg_count; i++) {
		if(used_copy[i]) {
			ret = i;
			fprintf(f, "\tstr x%d, [sp, #-16]!\n", arm_reg[i]);
		}
	}

	free(used_copy);

	return ret;
}

static void ir_func_restore_regs_callee(FILE *f, ir_func_t *fun, int save)
{
	if(save != -1) {
		fprintf(f, "\tldr x%d, [sp], #16\n", arm_reg[save]);
		fun->alloc_used[save] = 0;
	}

	/* save all registers needing to be saved */
	int reg1 = -1;
	int reg2 = -1;
	/* batch up 2 regs */
	for(size_t i = arm_reg_count - 1; i >= 0; i--) {
		if(fun->alloc_used[i]) {
			if(reg2 == -1) {
				reg2 = (int)i;
			} else if(reg1 == -1) {
				reg1 = (int)i;
				fprintf(f, "\tldp x%d, x%d, [sp], #16\n", arm_reg[reg1],
						arm_reg[reg2]);
				fun->alloc_used[reg1] = fun->alloc_used[reg2] = 0;
				reg1 = reg2 = -1;
			}
		}

		if(i == 0) {
			break;
		}
	}
	return;
}

void ir_func_emit_aarch64_apple(FILE *f, ir_func_t *fun)
{
	fprintf(f, "\t.globl _%s\n", fun->name);
	fprintf(f, "\t.text\n");
	fprintf(f, "_%s:\n", fun->name);
	/* enter stack frame */
	size_t alignd = align_to(fun->stack_needed, 16);
	fprintf(f, "\tstp fp, lr, [sp, #-16]!\n");
	int save = ir_func_save_regs_callee(f, fun);
	/* No clue why this is needed, but it is needed. Why? */
	fprintf(f, "\tsub sp, sp, #16\n");
	fprintf(f, "\tmov fp, sp\n");

	size_t alen = list_len(fun->args);
	size_t stack_indx = 0;
	size_t space_needed = 0;
	size_t stack_disp = 32;
	for(size_t i = 0; i < arm_reg_count; i++) {
		if(fun->alloc_used[i]) {
			stack_disp += 8;
		}
	}
	if(alen > 8) {
		for(size_t i = 8; i < alen; i++) {
			stack_indx = space_needed;
			space_needed += fun->args[i]->size;
		}
	}

	/* setup function frame */
	for(size_t i = 0; i < list_len(fun->args); i++) {
		callreg_t *arg = fun->args[i];
		if(i < 8) {
			if(load_fp_imm_x10(f, arg->r->off, false)) {
				fprintf(f, "\tstr x%d, [fp, x10]\n", (int)i);
			} else {
				fprintf(f, "\tstr x%d, [fp, #%lld]\n", (int)i,
						(int64_t)arg->r->off);
			}
		} else {
			ENSURE(stack_indx >= 0, "negative stack index, somehow");

			if(load_fp_imm_x10(f, -(int64_t)(stack_indx + stack_disp), false)) {
				fprintf(f, "\tldr x10, [sp, x10]\n");
			} else {
				fprintf(f, "\tldr x10, [sp, #%zu]\n", stack_indx + stack_disp);
			}

			if(load_fp_imm_x10(f, arg->r->off, false)) {
				fprintf(f, "\tstr x10, [fp, x10]\n");
			} else {
				fprintf(f, "\tstr x10, [fp, #%lld]\n", (int64_t)arg->r->off);
			}

			stack_indx -= arg->size;
		}
	}

	if(alignd) {
		fprintf(f, "\tsub sp, sp, #%zu\n", alignd);
	}

	size_t len = list_len(fun->blocks);
	for(size_t i = 0; i < len; i++) {
		ir_emit_blk_aarch64_apple(f, fun, fun->blocks[i],
								  (len - 1) + fun->blocks[0]->num);
	}

	/* leave stack frame */
	fprintf(f, ".L%s_ret:\n", fun->name);
	fprintf(f, "\tadd sp, fp, #16\n");
	ir_func_restore_regs_callee(f, fun, save);
	fprintf(f, "\tldp fp, lr, [sp], #16\n");
	fprintf(f, "\tret\n");
	fprintf(f, "\n");

	return;
}
