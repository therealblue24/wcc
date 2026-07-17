#include "sema.h"
#include "type.h"
#include "zz/base.h"

STRMAP(node_t *) goto_labels;

static void sema_visit_core(node_t *node);
static void sema_visit(node_t *node)
{
	if(!node) {
		return;
	}
	node->visited = false;

	sema_visit(node->lhs);
	sema_visit(node->rhs);
	sema_visit(node->next);
	sema_visit(node->cond);
	sema_visit(node->then);
	sema_visit(node->elze);
	sema_visit(node->init);
	sema_visit(node->inc);

	for(node_t *b = node->body; b; b = b->next) {
		sema_visit(b);
	}

	sema_visit_core(node);
	return;
}

static void map_labels(node_t *node)
{
	if(!node) {
		return;
	}

	map_labels(node->lhs);
	map_labels(node->rhs);
	map_labels(node->next);
	map_labels(node->cond);
	map_labels(node->then);
	map_labels(node->elze);
	map_labels(node->init);
	map_labels(node->inc);

	for(node_t *b = node->body; b; b = b->next) {
		map_labels(b);
	}

	if(node->kind != NODE_LABEL || node->visited) {
		return;
	}

	if(strmap_has(goto_labels, node->label)) {
		compile_err_node(node, "duplicate label '%s'", node->label);
	}

	node->visited = true;
	strmap_put(goto_labels, node->label, node);

	return;
}

static void sema_visit_core(node_t *node)
{
	switch(node->kind) {
	case NODE_LT:
	case NODE_LE:
	case NODE_GT:
	case NODE_GE:
	case NODE_EQ:
	case NODE_NE:
		if(node->lhs->type->unsignd != node->rhs->type->unsignd) {
			compile_warn_node(node, "comparison between signed and unsigned");
			/* promote to unsigned */
			node->type->unsignd = true;
		}
		break;
	case NODE_GOTO: {
		node_t **to = strmap_get(goto_labels, node->label);
		if(!to) {
			compile_err_node(node, "label '%s' doesn't exist", node->label);
		}

		if(!(*to)) {
			compile_err_node(node, "internal compiler error: '%s' is NULL",
							 node->label);
		}
		node->label_node = *to;
	}; break;
	default:
		break;
	}
	return;
}

/* does the whole semantics thing */
void sema_do(node_t *prog)
{
	goto_labels = strmap_make(node_t *);
	TIMEIT("map", { map_labels(prog); });
	TIMEIT("core", { sema_visit(prog); });
	strmap_delete(goto_labels);
	return;
}
