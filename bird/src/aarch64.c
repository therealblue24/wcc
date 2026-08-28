#include "bird.h"
#include "ir.h"
#include <ctype.h>

void ir_func_opt_aarch64(ir_func_t *fun, int opt_level)
{
	UNUSED(opt_level);
	UNUSED(fun);
	return;
}

static void load_imm_lane(FILE *f, char *reg, uint16_t i, uint16_t shift,
						  int *keep)
{
	char *kt = "zk";
	if(i) {
		fprintf(f, "\tmov%c %s, #%hu", kt[*keep], reg, i);
		if(shift) {
			fprintf(f, ", lsl #%hu", shift);
		}
		fprintf(f, "\n");
		*keep = 1;
	}
	return;
}

static void load_imm_w(FILE *f, char *reg, uint32_t imm_)
{
	uint32_t imm = imm_;
	int32_t imms = (int32_t)imm;

	if(imms <= 4095 && imms >= -4095) {
		fprintf(f, "\tmov %s, #%d\n", reg, imms);
		return;
	}

	int keep = 0;

	load_imm_lane(f, reg, (imm >> 0) & 0xffff, 0, &keep);
	load_imm_lane(f, reg, (imm >> 16) & 0xffff, 16, &keep);
	return;
}

/* loads an immediate into `reg` */
static void load_imm(FILE *f, char *reg, uint64_t imm_)
{
	uint64_t imm = imm_;
	int64_t imms = (int64_t)imm;

	if(imms <= 4095 && imms >= -4095) {
		fprintf(f, "\tmov %s, #%lld\n", reg, imms);
		return;
	}

	if(imms >= -65535 && imms < 0) {
		fprintf(f, "\tmovn %s, #%lld\n", reg, -imms);
	}

	/* load into w reg instead */
	if((imm & 0xffffffff) == imm) {
		load_imm_w(f, reg, imm);
		return;
	}

	int keep = 0;

	load_imm_lane(f, reg, (imm >> 0) & 0xffff, 0, &keep);
	load_imm_lane(f, reg, (imm >> 16) & 0xffff, 16, &keep);
	load_imm_lane(f, reg, (imm >> 32) & 0xffff, 32, &keep);
	load_imm_lane(f, reg, (imm >> 48) & 0xffff, 48, &keep);

	return;
}

static int load_fp_imm_x10(FILE *f, long off, bool save)
{
	if((-off) < 255) {
		return 0;
	}

	if(save) {
		fprintf(f, "\tstr x10, [sp, #-16]!\n");
	}

	load_imm(f, "x10", off);
	return 1;
}

static int arm_reg_mapping[20] = { 19, 20, 21, 22, 23, 24, 25, 26, 27, 28,
								   10, 0,  1,  2,  3,  4,  5,  6,  7,  8 };
static char *arm_reg[20] = { "x19", "x20", "x21", "x22", "x23", "x24", "x25",
							 "x26", "x27", "x28", "x10", "x0",	"x1",  "x2",
							 "x3",	"x4",  "x5",  "x6",	 "x7",	"x8" };
static char *arm_reg32[20] = { "w19", "w20", "w21", "w22", "w23", "w24", "w25",
							   "w26", "w27", "w28", "w10", "w0",  "w1",	 "w2",
							   "w3",  "w4",	 "w5",	"w6",  "w7",  "w8" };
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

static int common_case_glob(FILE *f, ir_global_t *glob)
{
	if(glob->size == 1) {
		fprintf(f, "\t.byte %hhu\n", glob->data[0]);
		return 0;
	} else if(glob->size == 2) {
		uint16_t *data = (uint16_t *)glob->data;
		fprintf(f, "\t.short %hu\n", data[0]);
		return 0;
	} else if(glob->size == 4) {
		uint32_t *data = (uint32_t *)glob->data;
		fprintf(f, "\t.long %u\n", data[0]);
		return 0;
	} else if(glob->size == 8) {
		uint64_t *data = (uint64_t *)glob->data;
		fprintf(f, "\t.quad %llu\n", data[0]);
		return 0;
	}

	return 1;
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
			if(common_case_glob(f, glob)) {
				for(size_t i = 0; i < glob->size; i++) {
					fprintf(f, "\t.byte %hhu\n", glob->data[i]);
				}
			}
		}
	}
	fprintf(f, "\n");
	return;
}

static INLINE void vload8_16(FILE *f, size_t size, bool ext, int reg_to,
							 char *addr_fmt, bool is_32bit, va_list va)
{
	if(size == 1) {
		if(ext) {
			fprintf(f, is_32bit ? "\tldrsb w%d, " : "\tldrsb x%d, ", reg_to);
		} else {
			fprintf(f, "\tldrb w%d, ", reg_to);
		}
	} else {
		if(ext) {
			fprintf(f, is_32bit ? "\tldrsh w%d, " : "\tldrsh x%d, ", reg_to);
		} else {
			fprintf(f, "\tldrh w%d, ", reg_to);
		}
	}

	vfprintf(f, addr_fmt, va);
	fprintf(f, "\n");
}

static INLINE void vload(FILE *f, size_t size, bool ext, int reg_to_,
						 char *addr_fmt, bool is_32bit, va_list va)
{
	int reg_to = arm_reg_mapping[reg_to_];
	switch(size) {
	case 8:
		fprintf(f, "\tldr x%d, ", reg_to);
		vfprintf(f, addr_fmt, va);
		fprintf(f, "\n");
		break;
	case 4:
		if(is_32bit) {
			fprintf(f, "\tldr w%d, ", reg_to);
		} else {
			fprintf(f, ext ? "\tldrsw x%d, " : "\tldr w%d, ", reg_to);
		}
		vfprintf(f, addr_fmt, va);
		fprintf(f, "\n");
		break;
	case 2:
	case 1:
		vload8_16(f, size, ext, reg_to, addr_fmt, is_32bit, va);
		break;
	default:
		break;
	}
	return;
}

static INLINE void vstore(FILE *f, size_t size, int reg_to_, char *addr_fmt,
						  bool is_32bit, va_list va)
{
	UNUSED(is_32bit);
	int reg_to = arm_reg_mapping[reg_to_];
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

static void load(FILE *f, size_t size, bool ext, int reg_to, bool is_32bit,
				 char *addr_fmt, ...)
{
	va_list va;
	va_start(va, addr_fmt);
	vload(f, size, ext, reg_to, addr_fmt, is_32bit, va);
	va_end(va);
	return;
}

static void store(FILE *f, size_t size, int reg_to, bool is_32bit,
				  char *addr_fmt, ...)
{
	va_list va;
	va_start(va, addr_fmt);
	vstore(f, size, reg_to, addr_fmt, is_32bit, va);
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
		space_needed = align_to(space_needed, 16);
		fprintf(f, "\tsub sp, sp, #%zu\n", space_needed);
	}

	for(size_t i = 0; i < alen; i++) {
		callreg_t *ca = args[i];
		reg_t *reg = ca->r;
		int arg = reg->rr;

		if(reg->spilld2) {
			arg = 10;
			if(load_fp_imm_x10(f, reg->off, false)) {
				fprintf(f, "\tldr x%d, [fp, x10]\n", arg);
			} else {
				fprintf(f, "\tldr x%d, [fp, #%ld]\n", arg, reg->off);
			}
		}

		if(i <= 7) {
			switch(ca->size) {
			case 8:
			default:
				fprintf(f, "\tmov x%zu, %s\n", i, arm_reg[arg]);
				break;
			case 4:
				fprintf(f, "\tmov w%zu, %s\n", i, arm_reg32[arg]);
				break;
			case 2:
				fprintf(f, "\tuxth w%zu, %s\n", i, arm_reg32[arg]);
				break;
			case 1:
				fprintf(f, "\tuxtb w%zu, %s\n", i, arm_reg32[arg]);
				break;
			}
		} else {
			ENSURE(stack_indx >= 0, "negative stack index, somehow");

			store(f, ca->size, arg, ca->size <= 4, "[sp, #%zu]", stack_indx);
			stack_indx -= args[i]->size;
		}
	}

	fprintf(f, "\tbl _%s\n", ins->fname);
	if(ins->r0) {
		if(ins->is_32bit) {
			fprintf(f, "\tmov %s, w0\n", arm_reg32[r0]);
		} else {
			fprintf(f, "\tmov %s, x0\n", arm_reg[r0]);
		}
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
		int r0i = ins->r0 && ins->r0->rr >= 0 ? ins->r0->rr : -1;
		int r1i = ins->r1 && ins->r1->rr >= 0 ? ins->r1->rr : -1;
		int r2i = ins->r2 && ins->r2->rr >= 0 ? ins->r2->rr : -1;
		char *r0x = r0i == -1 ? NULL : arm_reg[r0i];
		char *r1x = r1i == -1 ? NULL : arm_reg[r1i];
		char *r2x = r2i == -1 ? NULL : arm_reg[r2i];
		char *r0w = r0i == -1 ? NULL : arm_reg32[r0i];
		char *r1w = r1i == -1 ? NULL : arm_reg32[r1i];
		char *r2w = r2i == -1 ? NULL : arm_reg32[r2i];
		char *r0 = ins->is_32bit ? r0w : r0x;
		char *r1 = ins->is_32bit ? r1w : r1x;
		char *r2 = ins->is_32bit ? r2w : r2x;
		size_t sz = ins->size;
		bool is_32bit = ins->is_32bit;
		int64_t imm = (int64_t)ins->imm;
		switch(ins->type) {
		case IR_INST_ZXT:
			switch(ins->size) {
			case 1:
				fprintf(f, "\tuxtb %s, %s\n", r0w, r1w);
				break;
			case 2:
				fprintf(f, "\tuxth %s, %s\n", r0w, r1w);
				break;
			case 4:
				/* this uses w registers don't misread it */
				fprintf(f, "\tmov %s, %s\n", r0w, r1w);
				break;
			case 8:
			default:
				if(r0 != r1) {
					fprintf(f, "\tmov %s, %s\n", r0, r1);
				}
				break;
			}
			break;
		case IR_INST_SXT:
			switch(ins->size) {
			case 1:
				fprintf(f, "\tsxtb %s, %s\n", r0, r1);
				break;
			case 2:
				fprintf(f, "\tsxth %s, %s\n", r0, r1);
				break;
			case 4:
				if(is_32bit) {
					fprintf(f, "\tmov %s, %s\n", r0, r1);
				} else {
					fprintf(f, "\tsxtw %s, %s\n", r0, r1);
				}
				break;
			case 8:
			default:
				if(r0 != r1) {
					fprintf(f, "\tmov %s, %s\n", r0, r1);
				}
				break;
			}
			break;
		case IR_INST_CALL:
			ir_ins_abi_call_aarch64(f, fn, blk, ins, ins->call_args, r0i, r1i,
									r2i);
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
			fprintf(f, "\tcmp %s, #%lld\n", r1, imm);
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
			fprintf(f, "\tcmp %s, %s\n", r1, r2);
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
			fprintf(f, "\tcbz %s, .BB%ld\n", r1, ins->false_blk->num);
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
			if(r1i != -1) {
				if(is_32bit) {
					fprintf(f, "\tmov w0, %s\n", r1);
				} else {
					fprintf(f, "\tmov x0, %s\n", r1);
				}
			}
			if(blk->num != last_i) {
				fprintf(f, "\tb .L%s_ret\n", fn->name);
			}
			break;
		case IR_INST_RETI:
			if(is_32bit) {
				load_imm_w(f, "w0", ins->imm);
			} else {
				load_imm(f, "x0", ins->imm);
			}
			if(blk->num != last_i) {
				fprintf(f, "\tb .L%s_ret\n", fn->name);
			}
			break;
		case IR_INST_NOP:
			break;
		case IR_INST_MOV:
			fprintf(f, "\tmov %s, %s\n", r0, r1);
			break;
		case IR_INST_IMM:
			if(is_32bit) {
				load_imm_w(f, r0, ins->imm & UINT32_MAX);
			} else {
				load_imm(f, r0, ins->imm);
			}
			break;
		case IR_INST_ADD:
			fprintf(f, "\tadd %s, %s, %s\n", r0, r1, r2);
			break;
		case IR_INST_SUB:
			fprintf(f, "\tsub %s, %s, %s\n", r0, r1, r2);
			break;
		case IR_INST_ADDI:
			fprintf(f, "\tadd %s, %s, #%lld\n", r0, r1, imm);
			break;
		case IR_INST_SUBI:
			fprintf(f, "\tsub %s, %s, #%lld\n", r0, r1, imm);
			break;
		case IR_INST_AND:
			fprintf(f, "\tand %s, %s, %s\n", r0, r1, r2);
			break;
		case IR_INST_OR:
			fprintf(f, "\torr %s, %s, %s\n", r0, r1, r2);
			break;
		case IR_INST_EOR:
			fprintf(f, "\teor %s, %s, %s\n", r0, r1, r2);
			break;
		case IR_INST_ANDI:
			fprintf(f, "\tand %s, %s, #%lld\n", r0, r1, imm);
			break;
		case IR_INST_ORI:
			fprintf(f, "\torr %s, %s, #%lld\n", r0, r1, imm);
			break;
		case IR_INST_EORI:
			fprintf(f, "\teor %s, %s, #%lld\n", r0, r1, imm);
			break;
		case IR_INST_SHL:
			fprintf(f, "\tlsl %s, %s, %s\n", r0, r1, r2);
			break;
		case IR_INST_SHR:
			fprintf(f, "\tlsr %s, %s, %s\n", r0, r1, r2);
			break;
		case IR_INST_ASHR:
			fprintf(f, "\tasr %s, %s, %s\n", r0, r1, r2);
			break;
		case IR_INST_SHLI:
			fprintf(f, "\tlsl %s, %s, #%lld\n", r0, r1, imm);
			break;
		case IR_INST_SHRI:
			fprintf(f, "\tlsr %s, %s, #%lld\n", r0, r1, imm);
			break;
		case IR_INST_ASHRI:
			fprintf(f, "\tasr %s, %s, #%lld\n", r0, r1, imm);
			break;
		case IR_INST_SMUL:
		case IR_INST_UMUL:
			fprintf(f, "\tmul %s, %s, %s\n", r0, r1, r2);
			break;
		case IR_INST_SDIV:
			fprintf(f, "\tsdiv %s, %s, %s\n", r0, r1, r2);
			break;
		case IR_INST_SMOD: {
			char *r10 = is_32bit ? "w10" : "x10";
			fprintf(f, "\tsdiv %s, %s, %s\n", r10, r1, r2);
			fprintf(f, "\tmsub %s, %s, %s, %s\n", r0, r10, r2, r1);
		}; break;
		case IR_INST_UDIV:
			fprintf(f, "\tudiv %s, %s, %s\n", r0, r1, r2);
			break;
		case IR_INST_UMOD: {
			char *r10 = is_32bit ? "w10" : "x10";
			fprintf(f, "\tudiv %s, %s, %s\n", r10, r1, r2);
			fprintf(f, "\tmsub %s, %s, %s, %s\n", r0, r10, r2, r1);
		}; break;
		case IR_INST_NOT:
			fprintf(f, "\tmvn %s, %s\n", r0, r1);
			break;
		case IR_INST_MKBOOL:
			fprintf(f, "\ttst %s, %s\n", r1, r1);
			fprintf(f, "\tcset %s, ne\n", r0w);
			break;
		case IR_INST_NOTBOOL:
			fprintf(f, "\ttst %s, %s\n", r1, r1);
			fprintf(f, "\tcset %s, eq\n", r0w);
			break;
		case IR_INST_NEG:
			fprintf(f, "\tneg %s, %s\n", r0, r1);
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
			fprintf(f, "\tcmp %s, #%lld\n", r1, imm);
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
			fprintf(f, "\tcmp %s, %s\n", r1, r2);
set_cond:
			switch(ins->type) {
			case IR_INST_EQ:
			case IR_INST_EQI:
				fprintf(f, "\tcset %s, eq\n", r0w);
				break;
			case IR_INST_NE:
			case IR_INST_NEI:
				fprintf(f, "\tcset %s, ne\n", r0w);
				break;
			case IR_INST_SLT:
			case IR_INST_SLTI:
				fprintf(f, "\tcset %s, lt\n", r0w);
				break;
			case IR_INST_SLE:
			case IR_INST_SLEI:
				fprintf(f, "\tcset %s, le\n", r0w);
				break;
			case IR_INST_SGT:
			case IR_INST_SGTI:
				fprintf(f, "\tcset %s, gt\n", r0w);
				break;
			case IR_INST_SGE:
			case IR_INST_SGEI:
				fprintf(f, "\tcset %s, ge\n", r0w);
				break;
			case IR_INST_ULT:
			case IR_INST_ULTI:
				fprintf(f, "\tcset %s, lo\n", r0w);
				break;
			case IR_INST_ULE:
			case IR_INST_ULEI:
				fprintf(f, "\tcset %s, ls\n", r0w);
				break;
			case IR_INST_UGT:
			case IR_INST_UGTI:
				fprintf(f, "\tcset %s, hi\n", r0w);
				break;
			case IR_INST_UGE:
			case IR_INST_UGEI:
				fprintf(f, "\tcset %s, hs\n", r0w);
				break;
			default: /* wth? */
				break;
			}
			break;
		case IR_INST_LEAS:
			if(load_fp_imm_x10(f, -imm, false)) {
				fprintf(f, "\tadd %s, fp, x10\n", r0x);
			} else {
				fprintf(f, "\tsub %s, fp, #%lld\n", r0x, imm);
			}
			break;
		case IR_INST_LEA:
			fprintf(f, "\tadrp %s, _%s@PAGE\n", r0x, ins->label->name);
			fprintf(f, "\tadd %s, %s, _%s@PAGEOFF\n", r0x, r0x,
					ins->label->name);
			break;
		case IR_INST_LOAD:
			load(f, sz, ins->sign_ext, r0i, is_32bit, "[%s]", r1x);
			break;
		case IR_INST_LOADS:
			if(load_fp_imm_x10(f, imm, false)) {
				load(f, sz, ins->sign_ext, r0i, is_32bit, "[fp, x10]");
			} else {
				load(f, sz, ins->sign_ext, r0i, is_32bit, "[fp, #%lld]", imm);
			}
			break;
		case IR_INST_LOADSS:
			if(load_fp_imm_x10(f, imm, false)) {
				load(f, sz, ins->sign_ext, r0i, false,
					 "[fp, x10]\t ; spilled load");
			} else {
				load(f, sz, ins->sign_ext, r0i, false,
					 "[fp, #%lld]\t ; spilled load", imm);
			}
			break;

		case IR_INST_STORE:
			store(f, sz, r2i, is_32bit, "[%s]", r1x);
			break;
		case IR_INST_STORES:
			if(load_fp_imm_x10(f, imm, false)) {
				store(f, sz, r1i, is_32bit, "[fp, x10]");
			} else {
				store(f, sz, r1i, is_32bit, "[fp, #%lld]", imm);
			}
			break;
		case IR_INST_STORESS:
			if(load_fp_imm_x10(f, imm, false)) {
				store(f, sz, r1i, false, "[fp, x10]\t ; spilled store");
			} else {
				store(f, sz, r1i, false, "[fp, #%lld]\t ; spilled store", imm);
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
				fprintf(f, "\tstp %s, %s, [sp, #-16]!\n", arm_reg[reg1],
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
			fprintf(f, "\tstr %s, [sp, #-16]!\n", arm_reg[i]);
		}
	}

	free(used_copy);

	return ret;
}

static void ir_func_restore_regs_callee(FILE *f, ir_func_t *fun, int save)
{
	if(save != -1) {
		fprintf(f, "\tldr %s, [sp], #16\n", arm_reg[save]);
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
				fprintf(f, "\tldp %s, %s, [sp], #16\n", arm_reg[reg1],
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
	if(!fun->is_local) {
		fprintf(f, "\t.globl _%s\n", fun->name);
	}
	fprintf(f, "\t.text\n");
	fprintf(f, "_%s:\n", fun->name);
	/* enter stack frame */
	size_t alignd = align_to(fun->stack_needed, 16);
	if(fun->need_frame) {
		fprintf(f, "\tstp fp, lr, [sp, #-16]!\n");
	}
	int save = ir_func_save_regs_callee(f, fun);
	if(fun->need_frame) {
		fprintf(f, "\tmov fp, sp\n");
	}

	size_t alen = list_len(fun->args);
	size_t stack_indx = 0;
	size_t space_needed = 0;
	size_t stack_disp = 16;
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
				store(f, arg->size, i + 11, false, "[fp, x10]");
			} else {
				store(f, arg->size, i + 11, false, "[fp, #%lld]",
					  (int64_t)arg->r->off);
			}
		} else {
			ENSURE(stack_indx >= 0, "negative stack index, somehow");

			if(load_fp_imm_x10(f, -(int64_t)(stack_indx + stack_disp), false)) {
				load(f, arg->size, false, 10, false, "[sp, x10]");
			} else {
				load(f, arg->size, false, 10, false, "[sp, #%zu]",
					 stack_indx + stack_disp);
			}

			if(load_fp_imm_x10(f, arg->r->off, false)) {
				store(f, arg->size, 10, false, "[fp, x10]");
			} else {
				store(f, arg->size, 10, false, "[fp, #%lld]",
					  (int64_t)arg->r->off);
			}

			stack_indx -= arg->size;
		}
	}

	if(alignd && fun->need_frame) {
		size_t alignd2 = alignd;
		int count = 0;
		while(alignd2) {
			fprintf(f, "\tsub sp, sp, #%zu", alignd2 & 4095);
			if(count) {
				fprintf(f, ", lsl #%d", count);
			}
			fprintf(f, "\n");
			count += 12;
			alignd2 >>= 12;
		}
	}

	size_t len = list_len(fun->blocks);
	for(size_t i = 0; i < len; i++) {
		ir_emit_blk_aarch64_apple(f, fun, fun->blocks[i],
								  (len - 1) + fun->blocks[0]->num);
	}

	/* leave stack frame */
	fprintf(f, ".L%s_ret:\n", fun->name);
	if(fun->need_frame) {
		fprintf(f, "\tmov sp, fp\n");
	}
	ir_func_restore_regs_callee(f, fun, save);
	if(fun->need_frame) {
		fprintf(f, "\tldp fp, lr, [sp], #16\n");
	}
	fprintf(f, "\tret\n\n");

	return;
}
