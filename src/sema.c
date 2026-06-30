#include "sema.h"
#include "type.h"

static void sema_visit_core(node_t *node);
static void sema_visit(node_t *node)
{
	if(!node) {
		return;
	}

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

static void sema_visit_core(node_t *node)
{
	type_propagate(node);
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
	default:
		break;
	}
	return;
}

/* does the whole semantics thing */
void sema_do(node_t *prog)
{
	sema_visit(prog);
	type_propagate(prog);
	return;
}
