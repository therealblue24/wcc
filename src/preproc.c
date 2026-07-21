#include "preproc.h"
#include "lex.h"
#include "zz/base.h"
#include "zz/strmap.h"

STRMAP(token_t *) defines;

static void alloc_stuff()
{
	defines = strmap_make(token_t *);
}

static void dealloc_stuff()
{
	UNUSEDA char *key;
	token_t *chain;
	strmap_iter(defines, key, chain, { token_delete_all(chain); });
	strmap_delete(defines);
}

static void add_define(char *name, token_t *toks)
{
	if(strmap_has(defines, name)) {
		token_delete_all(*strmap_get(defines, name));
		strmap_del(defines, name);
	}

	strmap_put(defines, name, toks);
	return;
}

static token_t *preproc_driver(token_t *toks);

static int change = 1;

/* does the preprocessing */
token_t *preproc_do(token_t *toks_in)
{
	alloc_stuff();

	token_t *toks = toks_in;
	while(change) {
		change = 0;
		toks = preproc_driver(toks);
	}

	dealloc_stuff();
	return toks;
}

static token_t *preproc_handle_directive(token_t **prev, token_t *dir)
{
	token_t *nxt = dir->next;
	if(token_eat(&nxt, "include")) {
		if(nxt->kind != TOK_STR) {
			compile_err(nxt->loc, "expected filename");
		}
		FILE *f = fopen(nxt->str, "r");
		ENSURE(f, "failed to read file '%s'", nxt->str);
		char *src = file_reader(f);
		fclose(f);
		token_t *end;
		token_t *new = lex_do(src, &end);
		(*prev)->next = new;
		end->next = nxt->next;

		token_delete(dir->next->next);
		token_delete(dir->next);
		token_delete(dir);

		return end;
	}
	if(token_eat(&nxt, "define")) {
		token_t *save;
		/* currently implementing */
		/* #define ident toks? \n */

		if(nxt->kind != TOK_IDENT) {
			compile_err(nxt->loc, "expected an identifier");
		}

		char *name = scr_strdup(nxt->content);
		nxt = nxt->next;

		token_delete(dir->next->next); /* nameident */
		token_delete(dir->next); /* define */
		token_delete(dir); /* # */

		/* get token chain */
		token_t *chain_start = NULL;
		token_t *chain = NULL;
		while(!nxt->start_line) {
			token_t *copy = token_make(TOK_END, NULL, NULL);
			memcpy(copy, nxt, sizeof(token_t));
			if(copy->str)
				copy->str = strdup(copy->str);
			copy->content = strdup(copy->content);
			copy->next = NULL;

			if(!chain) {
				chain = copy;
				chain_start = chain;
			} else {
				chain->next = copy;
				chain = copy;
			}

			save = nxt;
			nxt = nxt->next;
			token_delete(save);
		}

		if(chain) {
			chain->next = token_make(TOK_END, NULL, NULL);
		} else {
			chain = token_make(TOK_END, NULL, NULL);
		}

		/* add it to defines */
		add_define(name, chain_start);

		/* reroute to here */
		(*prev)->next = nxt;
		return nxt;
	}

	return dir;
}

static token_t *duplicate(token_t *tok, token_t **last)
{
	token_t *chain = NULL;
	token_t *chain_start = NULL;
	while(tok->kind != TOK_END) {
		token_t *copy = token_make(TOK_END, NULL, NULL);
		memcpy(copy, tok, sizeof(token_t));
		if(copy->str)
			copy->str = strdup(copy->str);
		copy->content = strdup(copy->content);
		copy->next = NULL;

		if(!chain) {
			chain = copy;
			chain_start = chain;
		} else {
			chain->next = copy;
			chain = copy;
		}

		tok = tok->next;
	}

	*last = chain;
	return chain_start;
}

static token_t *macro_substitute(token_t **prev, token_t *replace,
								 token_t *with)
{
	/* duplicate chain */
	token_t *last;
	token_t *insert = duplicate(with, &last);
	(*prev)->next = insert;
	last->next = replace->next;
	token_delete(replace);
	return (*prev)->next;
}

static token_t *preproc_driver(token_t *toks_in)
{
	token_t *tmp = token_make(TOK_START, NULL, NULL);
	tmp->next = toks_in;
	token_t *toks = tmp;

	token_t *prev = toks;
	for(token_t *iter = toks->next; iter;) {
		if(iter->start_line && token_eq(iter, "#")) {
			preproc_handle_directive(&prev, iter);
			change = 1;
			iter = prev;
		}

		if(iter->kind == TOK_IDENT && strmap_has(defines, iter->content)) {
			/* replace with content */
			change = 1;
			iter = macro_substitute(&prev, iter,
									*strmap_get(defines, iter->content));
		}

		prev = iter;
		iter = iter->next;
	}

	token_t *nxt = tmp->next;
	token_delete(tmp);
	return nxt;
}
