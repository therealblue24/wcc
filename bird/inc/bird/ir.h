/* simple three-address-code/SSA IR */
#ifndef IR_H_
#define IR_H_

#include "zz/base.h"
#include "zz/list.h"
#include "zz/set.h"
#include "zz/prof.h"

enum ins_type {
	IR_INST_NOP = 0, /* does nothing */

	/* data transfer */
	IR_INST_MOV, /* %r0 = %r1 */
	IR_INST_IMM, /* %r0 = #imm */

	/* arithmetic - binops */
	IR_INST_ADD, /* %r0 = add %r1, %r2 */
	IR_INST_SUB, /* %r0 = sub %r1, %r2 */
	IR_INST_SMUL, /* %r0 = smul %r1, %r2 */
	IR_INST_SDIV, /* %r0 = sdiv %r1, %r2 */
	IR_INST_SMOD, /* %r0 = smod %r1, %r2 */
	IR_INST_UMUL, /* %r0 = umul %r1, %r2 */
	IR_INST_UDIV, /* %r0 = udiv %r1, %r2 */
	IR_INST_UMOD, /* %r0 = umod %r1, %r2 */
	IR_INST_AND, /* %r0 = and %r1, %r2 */
	IR_INST_OR, /* %r0 = or %r1, %r2 */
	IR_INST_EOR, /* %r0 = eor %r1, %r2 */
	IR_INST_SHL, /* %r0 = shl %r1, %r2 */
	IR_INST_SHR, /* %r0 = shr %r1, %r2 */
	IR_INST_ASHR, /* %r0 = ashr %r1, %r2 */

	/* immediate ops */
	IR_INST_ANDI,
	IR_INST_ORI,
	IR_INST_EORI,
	IR_INST_ADDI,
	IR_INST_SUBI,

	IR_INST_SHLI,
	IR_INST_SHRI,
	IR_INST_ASHRI,

	/* arithmetic - unaryops */
	IR_INST_NEG, /* %r0 = neg %r1 */
	IR_INST_NOT, /* %r0 = not %r1 */
	IR_INST_MKBOOL, /* %r0 = mkbool %r1 */
	IR_INST_NOTBOOL, /* %r0 = notbool %r1 */

	/* comparisons */
	IR_INST_EQ, /* %r0 = cmp.eq %r1, %r2 */
	IR_INST_NE, /* %r0 = cmp.ne %r1, %r2 */
	IR_INST_SLT, /* %r0 = cmp.slt %r1, %r2 */
	IR_INST_SLE, /* %r0 = cmp.sle %r1, %r2 */
	IR_INST_SGT, /* %r0 = cmp.sgt %r1, %r2 */
	IR_INST_SGE, /* %r0 = cmp.sge %r1, %r2 */
	IR_INST_ULT, /* %r0 = cmp.ult %r1, %r2 */
	IR_INST_ULE, /* %r0 = cmp.ule %r1, %r2 */
	IR_INST_UGT, /* %r0 = cmp.ugt %r1, %r2 */
	IR_INST_UGE, /* %r0 = cmp.uge %r1, %r2 */

	IR_INST_EQI,
	IR_INST_NEI,
	IR_INST_SLTI,
	IR_INST_SLEI,
	IR_INST_SGTI,
	IR_INST_SGEI,
	IR_INST_ULTI,
	IR_INST_ULEI,
	IR_INST_UGTI,
	IR_INST_UGEI,

	/* compare-and-branches */
	IR_INST_BREQ, /* br.eq %r1, %r2, true-blk, false-blk */
	IR_INST_BRNE, /* br.ne %r1, %r2, true-blk, false-blk */
	IR_INST_BRSLT, /* br.slt %r1, %r2, true-blk, false-blk */
	IR_INST_BRSLE, /* br.sle %r1, %r2, true-blk, false-blk */
	IR_INST_BRSGT, /* br.sgt %r1, %r2, true-blk, false-blk */
	IR_INST_BRSGE, /* br.sge %r1, %r2, true-blk, false-blk */
	IR_INST_BRULT, /* br.ult %r1, %r2, true-blk, false-blk */
	IR_INST_BRULE, /* br.ule %r1, %r2, true-blk, false-blk */
	IR_INST_BRUGT, /* br.ugt %r1, %r2, true-blk, false-blk */
	IR_INST_BRUGE, /* br.uge %r1, %r2, true-blk, false-blk */

	IR_INST_BREQI,
	IR_INST_BRNEI,
	IR_INST_BRSLTI,
	IR_INST_BRSLEI,
	IR_INST_BRSGTI,
	IR_INST_BRSGEI,
	IR_INST_BRULTI,
	IR_INST_BRULEI,
	IR_INST_BRUGTI,
	IR_INST_BRUGEI,

	/* memory */
	IR_INST_LOAD, /* %r0 = load %r1 */
	IR_INST_STORE, /* store %r1, %r2 */
	IR_INST_LEAS, /* %r0 = leas #imm */
	IR_INST_LEA, /* %r0 = lea Label */
	/* load, store from stack */
	IR_INST_LOADS, /* %r0 = loads #imm */
	IR_INST_STORES, /* stores %r1, #imm */
	/* spilled load, store */
	IR_INST_LOADSS, /* %r0 = loadss #imm */
	IR_INST_STORESS, /* stores %r1, #imm */

	/* sign extensions */
	IR_INST_ZXT, /* %r0 = zxt %r1 */
	IR_INST_SXT, /* %r0 = sxt %r1 */

	/* basic block stuff */
	IR_INST_BR, /* br %r1, true-blk, false-blk */
	IR_INST_JMP, /* jmp blk */
	IR_INST_RET, /* ret (%r1) */
	IR_INST_CALL, /* (%r0) = call Function, %a1, %a2, ... */

	/* SSA */
	IR_INST_PHI, /* %r0 = phi [pred1, %a1], [pred2, %a1], ... */
	IR_INST_PMOV, /* parallel move */
};

struct ir_inst;
struct blkreg;
/* a "register" */
typedef struct reg {
	long vr; /* virtual register # */
	int rr; /* real register # */

	/* for register allocation: */
	long def; /* when this reg was defined */
	long last_use; /* when this reg was last used */
	bool spilld; /* is this reg spilled? */
	bool nospill; /* do not spill this reg */
	bool spilld2; /* is this reg spilled (only meant to be used for codegen) */
	int64_t spill_cost; /* cost of spilling this reg */
	uint64_t imm; /* immediate associated with this reg */
	long off; /* stack offset of register */
	/* register hinting from the Wimmer paper */
	SET(struct reg *) moveset; /* registers which are move-related */

	/* for SSA construction: */
	LIST(struct blkreg *) blkregs; /* associated block regs */
	struct reg *ssareg; /* varialbe associated with this reg */
	/* for optimization: */
	bool stack_loc; /* is this register from a leas instruction? */
	long stack_off; /* if so, it's offset */
	/* instruction register comes from */
	enum ins_type insty;
	struct reg *lhs;
	struct reg *rhs;
	struct reg *uf; /* union find set */
	struct ir_inst *from; /* NOT meant to be used for any pass except phiopt! */
	size_t size; /* instruction size of register */
	bool is_32bit;
	bool ins_ext; /* ins has ext? */

	bool phi_related; /* argument/def to phi; do NOT eliminate */
	bool alive; /* is this register not dead? */
} reg_t;

typedef struct reg_pmov {
	/* %dst = %src */
	reg_t *dst;
	reg_t *src;
} reg_pmov_t;

/* ABI argument type */
enum call_argtype {
	ARG_CLASS_INTEGER,
	ARG_CLASS_MEMORY,
	ARG_CLASS_FLOAT,
};

/* call register */
typedef struct callreg {
	reg_t *r; /* the register */
	enum call_argtype type; /* type of parameter */
	size_t size; /* size of parameter */
} callreg_t;

enum ir_arch {
	/* architecture-abi */
	IR_ARCH_AARCH64_APPLE, /* aarch64-apple */
	IR_ARCH_X64_SYSV, /* x64-sysv */
};

struct ir_blk;
struct ir_global;

/* an IR instruction. is a linked list */
typedef struct ir_inst {
	struct ir_inst *next; /* next ins */
	struct ir_inst *next_mem; /* next memory instruction: used for optimizer */
	enum ins_type type; /* instruction type */
	reg_t *r0, *r1, *r2; /* instruction args */
	uint64_t imm; /* immediate, if needed */
	struct ir_blk *false_blk, *true_blk; /* for br */
	LIST(callreg_t *) call_args; /* for call */
	LIST(reg_t *) phi_args; /* for phi */
	LIST(struct ir_blk *) phi_preds; /* for phi */
	LIST(reg_pmov_t) pmov_args; /* for pmov */
	char *fname; /* for call */
	bool noopt; /* is this inst volatile? */
	bool sign_ext; /* sign extend this load? */
	size_t size; /* load/store/zero_ext/sign_ext size */
	bool is_32bit; /* is the operation 32 bit (true) or 64 bit (false)? */
	struct ir_global *label; /* for lea, the label */
} ir_inst_t;

/* IR block (collection of instructions, >= 1 entry and only <= 2 exits) */
typedef struct ir_blk {
	ir_inst_t *insts; /* instructions in this block */
	ir_inst_t *tail; /* last instruction in block */
	ir_inst_t *tailprev; /* for SSA deconstruction: ins before tail */
	bool returns; /* does this block return? */
	long num; /* this block's # */
	long postnum; /* this block's postorder num */

	/* register allocation stuff */
	bool visited;
	bool active;
	uint64_t loop_order; /* how much times this block is reached when visited */
	uint64_t loop_index; /* index of loop */
	uint64_t loop_depth; /* depth of loop */
	LIST(struct ir_blk *) succ; /* block's successors */
	LIST(struct ir_blk *) pred; /* block's predecessors */
	LIST(ir_inst_t *) incomplete_phis; /* block's incomplete phis */
	struct ir_blk *dom; /* dominator of this block */
	SET(reg_t *) regs_def; /* registers in this block */
	SET(reg_t *) regs_in; /* registers in */
	SET(reg_t *) regs_out; /* registers out */
} ir_blk_t;

/* for SSA construction */
typedef struct blkreg {
	ir_blk_t *blk;
	reg_t *reg;
} blkreg_t;

/* IR function (collection of blocks) */
typedef struct ir_func {
	char *name; /* name of this function */
	LIST(ir_blk_t *) blocks; /* the collection of blocks */
	size_t rpo_indx; /* for reverse postorder calculation */
	size_t stack_needed; /* stack space needed for this function */
	size_t align_needed; /* stack alignment needed for this function */
	bool alloc_strat; /* false = prefer caller-save first, true = prefer callee-save first */
	bool *alloc_used; /* used registers for allocation (for push-ing/pop-ing) */
	LIST(callreg_t *) args; /* arguments to this function */
	bool is_local; /* is this function local (static)? */
	bool need_frame; /* does this function need a stack frame? */
} ir_func_t;

/* global variable */
typedef struct ir_global {
	char *name; /* name of this global variable */
	size_t size; /* size of this global variable */
	size_t align; /* alignment of this global variable */
	bool has_data; /* is this global variable initalized with data? */
	bool is_str; /* is this global a string? */
	bool is_anon; /* is this global meant to be global? */
	uint8_t *data; /* if so, the data */
} ir_global_t;

/* IR program (collection of functions, variables, etc.) */
typedef struct ir_prog {
	LIST(ir_func_t *) funcs; /* the collection of functions */
	LIST(ir_global_t *) globs; /* the collection of global variables */
} ir_prog_t;

/* -- big list of instructions -- */
#define INSNAME(name) ins_##name

#define DEF_INS(name, ...) ir_inst_t *INSNAME(name)(__VA_ARGS__)

DEF_INS(nop, void);
DEF_INS(mov, reg_t *r0, reg_t *r1);
DEF_INS(imm, reg_t *r0, uint64_t imm);
DEF_INS(add, reg_t *r0, reg_t *r2, reg_t *r3);
DEF_INS(sub, reg_t *r0, reg_t *r2, reg_t *r3);
DEF_INS(shl, reg_t *r0, reg_t *r2, reg_t *r3);
DEF_INS(shr, reg_t *r0, reg_t *r2, reg_t *r3);
DEF_INS(ashr, reg_t *r0, reg_t *r2, reg_t *r3);
DEF_INS(and, reg_t *r0, reg_t *r2, reg_t *r3);
DEF_INS(or, reg_t *r0, reg_t *r2, reg_t *r3);
DEF_INS(eor, reg_t *r0, reg_t *r2, reg_t *r3);
DEF_INS(smul, reg_t *r0, reg_t *r2, reg_t *r3);
DEF_INS(sdiv, reg_t *r0, reg_t *r2, reg_t *r3);
DEF_INS(smod, reg_t *r0, reg_t *r2, reg_t *r3);
DEF_INS(umul, reg_t *r0, reg_t *r2, reg_t *r3);
DEF_INS(udiv, reg_t *r0, reg_t *r2, reg_t *r3);
DEF_INS(umod, reg_t *r0, reg_t *r2, reg_t *r3);
DEF_INS(eq, reg_t *r0, reg_t *r2, reg_t *r3);
DEF_INS(ne, reg_t *r0, reg_t *r2, reg_t *r3);
DEF_INS(slt, reg_t *r0, reg_t *r2, reg_t *r3);
DEF_INS(sle, reg_t *r0, reg_t *r2, reg_t *r3);
DEF_INS(sgt, reg_t *r0, reg_t *r2, reg_t *r3);
DEF_INS(sge, reg_t *r0, reg_t *r2, reg_t *r3);
DEF_INS(ult, reg_t *r0, reg_t *r2, reg_t *r3);
DEF_INS(ule, reg_t *r0, reg_t *r2, reg_t *r3);
DEF_INS(ugt, reg_t *r0, reg_t *r2, reg_t *r3);
DEF_INS(uge, reg_t *r0, reg_t *r2, reg_t *r3);
DEF_INS(neg, reg_t *r0, reg_t *r1);
DEF_INS(not, reg_t *r0, reg_t *r1);
DEF_INS(mkbool, reg_t *r0, reg_t *r1);
DEF_INS(notbool, reg_t *r0, reg_t *r1);
DEF_INS(leas, reg_t *r0, long imm);
DEF_INS(lea, reg_t *r0, struct ir_global *lbl);
DEF_INS(load, reg_t *r0, reg_t *r1);
DEF_INS(loads, reg_t *r0, long imm);
DEF_INS(store, reg_t *r0, reg_t *r1);
DEF_INS(stores, reg_t *r0, long imm);
DEF_INS(loadl, reg_t *r0, reg_t *r1);
DEF_INS(loadsl, reg_t *r0, long imm);
DEF_INS(storel, reg_t *r0, reg_t *r1);
DEF_INS(storesl, reg_t *r0, long imm);
DEF_INS(loadb, reg_t *r0, reg_t *r1);
DEF_INS(loadsb, reg_t *r0, long imm);
DEF_INS(storeb, reg_t *r0, reg_t *r1);
DEF_INS(storesb, reg_t *r0, long imm);
DEF_INS(loadw, reg_t *r0, reg_t *r1);
DEF_INS(loadsw, reg_t *r0, long imm);
DEF_INS(storew, reg_t *r0, reg_t *r1);
DEF_INS(storesw, reg_t *r0, long imm);
DEF_INS(br, reg_t *r1, ir_blk_t *falseblk, ir_blk_t *trueblk);
DEF_INS(breq, reg_t *r1, reg_t *r2, ir_blk_t *falseblk, ir_blk_t *trueblk);
DEF_INS(brne, reg_t *r1, reg_t *r2, ir_blk_t *falseblk, ir_blk_t *trueblk);
DEF_INS(brslt, reg_t *r1, reg_t *r2, ir_blk_t *falseblk, ir_blk_t *trueblk);
DEF_INS(brsle, reg_t *r1, reg_t *r2, ir_blk_t *falseblk, ir_blk_t *trueblk);
DEF_INS(brsgt, reg_t *r1, reg_t *r2, ir_blk_t *falseblk, ir_blk_t *trueblk);
DEF_INS(brsge, reg_t *r1, reg_t *r2, ir_blk_t *falseblk, ir_blk_t *trueblk);
DEF_INS(brult, reg_t *r1, reg_t *r2, ir_blk_t *falseblk, ir_blk_t *trueblk);
DEF_INS(brule, reg_t *r1, reg_t *r2, ir_blk_t *falseblk, ir_blk_t *trueblk);
DEF_INS(brugt, reg_t *r1, reg_t *r2, ir_blk_t *falseblk, ir_blk_t *trueblk);
DEF_INS(bruge, reg_t *r1, reg_t *r2, ir_blk_t *falseblk, ir_blk_t *trueblk);

DEF_INS(jmp, ir_blk_t *blk);
DEF_INS(ret, reg_t *r1);
DEF_INS(call, reg_t *res, char *fname, LIST(callreg_t *) args);

DEF_INS(zxtb, reg_t *r0, reg_t *r1);
DEF_INS(sxtb, reg_t *r0, reg_t *r1);
DEF_INS(zxtw, reg_t *r0, reg_t *r1);
DEF_INS(sxtw, reg_t *r0, reg_t *r1);
DEF_INS(zxtl, reg_t *r0, reg_t *r1);
DEF_INS(sxtl, reg_t *r0, reg_t *r1);

#undef DEF_INS
#undef INSNAME

/* does this instruction terminate a block? */
int ir_inst_is_term(enum ins_type type);

/* is this instruction foldable? */
int ir_inst_is_foldable(enum ins_type type);

/* is this instruction a branch? */
int ir_inst_is_br(enum ins_type type);

/* is this instruction a comparison? */
int ir_inst_is_cmp(enum ins_type type);

/* is this instruction associative? (F(B, C) == F(C, B)) */
int ir_inst_is_assoc(enum ins_type type);

/* can the instruction be immediate folded? */
int ir_inst_can_fold_imm(enum ins_type type);

/* turn instruction type -> immediate instruction type */
int ir_inst_turn_imm(enum ins_type type);

/* make a (new) register */
reg_t *reg_make(void);

/* make a (new) call register */
callreg_t *callreg_make(reg_t *reg, enum call_argtype class, size_t size);

/* delete a register */
void reg_delete(reg_t *reg);

/* reset register counter */
void reg_reset_counter(void);

/* make an IR instruction */
ir_inst_t *ir_inst_make(enum ins_type type, reg_t *r0, reg_t *r1, reg_t *r2,
						uint64_t imm);

/* delete an IR instruction */
void ir_inst_delete(ir_inst_t *ins);

/* make an IR block */
ir_blk_t *ir_blk_make(ir_inst_t *insts);

/* delete an IR block */
void ir_blk_delete(ir_blk_t *blk);

/* delete an IR block and instructions */
void ir_blk_delete_all(ir_blk_t *blk);

/* make an IR function */
ir_func_t *ir_func_make(char *name);

/* delete an IR function (aka all blocks, extras) */
void ir_func_delete(ir_func_t *fun);

/* print IR instruction */
void ir_print_inst(ir_inst_t *ins, int mode);

/* dump IR */
void ir_dump(ir_func_t *fun, int mode);

/* removes nops */
void ir_nopremover(ir_func_t *fun);

/* codegen an IR function */
/* assumes function has been finalized */
void ir_func_emit(FILE *f, ir_func_t *fun, enum ir_arch arch);

/* generates code for an IR program */
/* handles all the function finalization stuff */
void ir_prog_compile(FILE *f, ir_prog_t *prog, enum ir_arch arch, int opt);

/* add IR instruction to IR block */
void ir_blk_add(ir_blk_t *blk, ir_inst_t *inst);

/* fixes IR function */
void ir_fix(ir_func_t *func);

/* make a global variable */
ir_global_t *ir_glob_make(char *name, size_t size, size_t align, uint8_t *data);

/* delete a global variable */
void ir_glob_delete(ir_global_t *glob);

/* -- union find mechanism -- */
/* dst = src; */
void ir_union(reg_t *dst, reg_t *src);

/* return (val of src); */
reg_t *ir_find(reg_t *src);

/* rewrites whole function via ir_find */
void ir_rewrite(ir_func_t *fun);

/* computes reverse postorder of block */
void ir_blk_rpo(ir_func_t *fun);

#endif /* IR_H_ */
