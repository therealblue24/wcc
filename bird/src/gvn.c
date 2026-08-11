#include "bird.h"
#include "ir.h"
#include "zz/list.h"

static uint64_t hashd(uint64_t x)
{
	return fnv1a(&x, sizeof(uint64_t));
}

static uint64_t hashk(uint64_t x, uint64_t k)
{
	uint64_t v;
	v = hashd(x << ((k ^ x) & 3));
	v ^= k;
	return fnv1a(&v, sizeof(uint64_t));
}

static uint64_t hashp(void *x)
{
	return fnv1a(&x, sizeof(uint64_t));
}

static int ins_has_imm(enum ins_type t)
{
	return t == IR_INST_IMM || t == IR_INST_LEAS || t == IR_INST_LOADS ||
		   t == IR_INST_LOADSS || t == IR_INST_STORES || t == IR_INST_STORESS;
}

static bool do_not_gvn_type(enum ins_type t)
{
	return t == IR_INST_LOAD || t == IR_INST_LOADS || t == IR_INST_STORE ||
		   t == IR_INST_STORES || t == IR_INST_CALL || t == IR_INST_PHI ||
		   ir_inst_is_term(t);
}

static bool do_not_gvn(ir_inst_t *i)
{
	if(!i->r0 || do_not_gvn_type(i->type) || i->noopt) {
		return true;
	}

	if(i->r0->phi_related) {
		return true;
	}

	return false;
}

/* excludes ins->r0; assumes instruction is GVN-able */
static uint64_t hash_ins(ir_inst_t *ins)
{
	uint64_t hash = hashd(ins->type);

	// clang-format off

	/* I had some fun with these constants. They could be
	 * anything random enough, really. */
	hash ^= hashk(ins->sign_ext, 0x1618033988749894);
	hash ^= hashk(ins->size,     0x2718281828459045);
	hash ^= hashk(ins->is_32bit, 0x3141592653589793);
	
	if(ins_has_imm(ins->type)) hash ^= hashd(ins->imm);
	if(ir_find(ins->r1))	hash ^= hashp(ir_find(ins->r1));
	if(ir_find(ins->r2))	hash ^= hashp(ir_find(ins->r2));
	if(ins->type == IR_INST_LEA) hash ^= hashp(ins->label);
	// clang-format on
	return hash;
}

static bool ins_are_same(ir_inst_t *a, ir_inst_t *b)
{
	if(!a || !b) {
		return false;
	}

	if(a == b) {
		return true;
	}

	if(a->type != b->type) {
		return false;
	}

	if(a->sign_ext != b->sign_ext || a->size != b->size ||
	   a->is_32bit != b->is_32bit || ir_find(a->r1) != ir_find(b->r1) ||
	   ir_find(a->r2) != ir_find(b->r2)) {
		return false;
	}

	if(ins_has_imm(a->type) && a->imm != b->imm) {
		return false;
	}

	if(a->type == IR_INST_LEA && a->label != b->label) {
		return false;
	}

	return true;
}

/* GVN hashmap */
static ir_inst_t **map = NULL; /* val is map->r0 */
static size_t keys; /* keys in the map */
static size_t cap; /* capacity of map */

static void reset_map(void)
{
	if(!map) {
		map = zcalloc(256, sizeof(ir_inst_t *));
		cap = 256;
	}
	wipe(map, cap * sizeof(ir_inst_t *));
	keys = 0;
	return;
}

typedef struct lookup {
	size_t loc;
	bool exists;
} lookup_t;

static lookup_t lookup_map(ir_inst_t **map2, size_t cap2, ir_inst_t *ins)
{
	uint64_t hash = hash_ins(ins);
	uint64_t mask = cap2 - 1;
	hash &= mask;
	uint64_t step = 1; /* TODO: improve */
	for(uint64_t i = hash - step;;) {
		i = (i + step) & mask;

		if(map2[i] == NULL) {
			return (lookup_t){ .loc = i, .exists = false };
		}

		/* check if it's really it */
		if(ins_are_same(ins, map2[i])) {
			/* yup */
			return (lookup_t){ .loc = i, .exists = true };
		}
	}

	/* you don't get here */
	return (lookup_t){ .loc = -1, .exists = false };
}

static void rebuild_map(void);

static void insert_map(ir_inst_t **map2, size_t cap2, ir_inst_t *ins)
{
	lookup_t l = lookup_map(map2, cap2, ins);
	if(!l.exists)
		keys++;

	if(keys >= (3 * cap2) / 4) {
		rebuild_map();
		map2 = map;
	}

	map2[l.loc] = ins;
	return;
}

static ir_inst_t *find(ir_inst_t **map2, size_t cap2, ir_inst_t *ins)
{
	lookup_t l = lookup_map(map2, cap2, ins);
	if(l.exists) {
		return map2[l.loc];
	}
	return NULL;
}

static void rebuild_map(void)
{
	ir_inst_t **new = zcalloc(cap * 2, sizeof(ir_inst_t *));

	keys = 0;
	for(size_t i = 0; i < cap; i++) {
		if(!map[i]) {
			continue;
		}

		insert_map(new, cap * 2, map[i]);
	}

	free(map);
	map = new;
	cap *= 2;

	return;
}

static int gvn_rewrite(ir_inst_t *ins)
{
	/* assumes ins is GVN-able */
	ir_inst_t *found = find(map, cap, ins);
	if(found) {
		ins->type = IR_INST_NOP;
		ir_union(ins->r0, found->r0);
		return 1;
	} else {
		/* welcome to GVN */
		insert_map(map, cap, ins);
		return 0;
	}

	return -1;
}

int ir_gvn(ir_func_t *func)
{
	int change = 0;

	for(size_t i = 0; i < list_len(func->blocks); i++) {
		reset_map(); /* TODO: make global. requires dominator computation however */
		ir_blk_t *blk = func->blocks[i];
		for(ir_inst_t *inst = blk->insts; inst; inst = inst->next) {
			if(!inst->r0) {
				continue;
			}
			if(do_not_gvn(inst)) {
				continue;
			}
			change |= gvn_rewrite(inst);
		}
	}

	ir_rewrite(func);

	return change;
}
