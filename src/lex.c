#include "lex.h"
#include "zz/base.h"
#include "type.h"
#include <stdint.h>

/* makes a token */
token_t *token_make(enum token_kind kind, char *start, char *end)
{
	token_t *tok = zalloc(sizeof(token_t));
	tok->kind = kind;
	tok->loc = start;
	tok->len = end - start;

	return tok;
}

/* deallocates a single token */
void token_delete(token_t *tok)
{
	if(tok->str) {
		free(tok->str);
	}
	free(tok);
	return;
}

/* deallocates the whole token linked list */
void token_delete_all(token_t *root)
{
	token_t *nxt;
	token_t *cur = root;
	while(cur && cur->kind != TOK_END) {
		nxt = cur->next;
		free(cur);
		cur = nxt;
	}
	free(cur);
	return;
}

/* checks if a `tok`'s content is equal to `content` */
int token_eq(token_t *tok, char *content)
{
	return tok && content && strncmp(tok->loc, content, tok->len) == 0 &&
		   content[tok->len] == 0;
}

/* skips `tok` and returns next token if `tok`'s content is equal to `content` */
token_t *token_skip(token_t *tok, char *content)
{
	if(!token_eq(tok, content)) {
		compile_err(tok->loc, "expected '%s'", content);
	}
	return tok->next;
}

/* checks if a `tok`'s content is equal to `content`. if so, skips to next token and returns 1 */
int token_eat(token_t **tok, char *content)
{
	if(token_eq(*tok, content)) {
		*tok = (*tok)->next;
		return 1;
	}
	return 0;
}

/* returns the number in `tok` if the token's kind is a number */
uint64_t token_num(token_t *tok)
{
	if(tok->kind != TOK_NUM) {
		compile_err(tok->loc, "expected a number");
	}
	return tok->num;
}

/* is this (other) character an identifier? */
int isident(int c)
{
	return isidentfirst(c) || (c >= '0' && c <= '9');
}

/* is this (first) character an identifier? */
/* 6.4.2.1 nondigit */
int isidentfirst(int c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

/* returns the length of a possible punctuator */
static int punct_len(char *p)
{
	const char *puncts[] = { "<=", ">=", "==", "!=", "&&", "||",  ">>",
							 "<<", "*=", "/=", "+=", "-=", "<<=", ">>=",
							 "&=", "^=", "|=", "++", "--" };
	size_t puncts_len = sizeof(puncts) / sizeof(puncts[0]);

	for(size_t i = 0; i < puncts_len; i++) {
		if(strncmp(p, puncts[i], strlen(puncts[i])) == 0) {
			return strlen(puncts[i]);
		}
	}

	if(ispunct(*p)) {
		return 1;
	}

	return 0;
}

/* is this character whitespace? */
int iswhitespace(int c)
{
	return c == '\t' || c == ' ' || c == '\r' || c == '\n';
}

/* is this a keyword? */
static int iskeyword(char *prog, size_t plen)
{
	static const char *keywords[] = {
		"auto",		  "break",	   "case",			 "char",
		"const",	  "continue",  "default",		 "do",
		"double",	  "else",	   "enum",			 "extern",
		"float",	  "for",	   "goto",			 "if",
		"inline",	  "int",	   "long",			 "register",
		"restrict",	  "return",	   "short",			 "signed",
		"sizeof",	  "static",	   "struct",		 "switch",
		"typedef",	  "union",	   "unsigned",		 "void",
		"volatile",	  "while",	   "_Alignas",		 "_Alignof",
		"_Atomic",	  "_Bool",	   "_Complex",		 "_Generic",
		"_Imaginary", "_Noreturn", "_Static_assert", "_Thread_local"
	};
	static const size_t keywords_count = sizeof(keywords) / sizeof(keywords[0]);

	for(size_t i = 0; i < keywords_count; i++) {
		const char *kw = keywords[i];
		size_t len = strlen(kw);
		if(plen != len) {
			continue;
		}
		if(strncmp(prog, kw, len) == 0) {
			return len;
		}
	}

	return 0;
}

static uint64_t zxt(uint64_t x, uint64_t size)
{
	uint64_t m = (1ULL << size) - 1;
	return x & m;
}

static uint64_t sxt(uint64_t x, uint64_t size)
{
	/* thank you https://stackoverflow.com/a/17719010 */
	uint64_t mask = 1ULL << (size - 1);
	return (x ^ mask) - mask;
}

static bool is_dec(char c)
{
	return c >= '0' && c <= '9';
}

static bool is_oct(char c)
{
	return c >= '0' && c <= '7';
}

static bool is_bin(char c)
{
	return c == '0' || c == '1';
}

static bool is_doz_lower(char c)
{
	return c == 'a' || c == 'b';
}

static bool is_doz_upper(char c)
{
	return c == 'A' || c == 'B';
}

static bool is_hex_lower(char c)
{
	return c >= 'a' && c <= 'f';
}

static bool is_hex_upper(char c)
{
	return c >= 'A' && c <= 'F';
}

static bool is_hex(char c)
{
	return is_dec(c) || is_hex_lower(c) || is_hex_upper(c);
}

static bool is_doz(char c)
{
	return is_dec(c) || is_doz_lower(c) || is_doz_upper(c);
}

static bool is_long_suf(char c)
{
	return c == 'l' || c == 'L';
}

static bool is_unsignd_suf(char c)
{
	return c == 'u' || c == 'U';
}

static char read_hexchr(char *p, char **rest)
{
	char res = 0;
	for(int i = 0; i < 2; i++) {
		char digit = *p++;
		if(is_dec(digit)) {
			res = (16 * res) + (digit - '0');
		} else if(is_hex_lower(digit)) {
			res = (16 * res) + (10 + (digit - 'a'));
		} else if(is_hex_upper(digit)) {
			res = (16 * res) + (10 + (digit - 'A'));
		} else
			break;
	}
	*rest = p;
	return res;
}

static char read_octchr(char *p, char **rest)
{
	char res = 0;
	for(int i = 0; i < 3; i++) {
		char digit = *p++;
		if(is_oct(digit)) {
			res = (8 * res) + (digit - '0');
		} else
			break;
	}
	*rest = p;
	return res;
}

static uint64_t read_hex(char *p, char **rest)
{
	uint64_t res = 0;
	while(is_hex(*p)) {
		char digit = *p++;
		if(is_dec(digit)) {
			res = (16 * res) + (digit - '0');
		} else if(is_hex_lower(digit)) {
			res = (16 * res) + (10 + (digit - 'a'));
		} else if(is_hex_upper(digit)) {
			res = (16 * res) + (10 + (digit - 'A'));
		}
	}
	*rest = p;
	return res;
}

static uint64_t read_dec(char *p, char **rest)
{
	uint64_t res = 0;
	while(is_dec(*p)) {
		char digit = *p++;
		res = (10 * res) + (digit - '0');
	}
	*rest = p;
	return res;
}

static uint64_t read_doz(char *p, char **rest)
{
	uint64_t res = 0;
	while(is_doz(*p)) {
		char digit = *p++;
		if(is_dec(digit)) {
			res = (12 * res) + (digit - '0');
		} else if(is_doz_lower(digit)) {
			res = (12 * res) + (digit - 'a');
		} else if(is_doz_upper(digit)) {
			res = (12 * res) + (digit - 'A');
		}
	}
	*rest = p;
	return res;
}

static uint64_t read_oct(char *p, char **rest)
{
	uint64_t res = 0;
	while(is_oct(*p)) {
		char digit = *p++;
		res = (8 * res) + (digit - '0');
	}
	*rest = p;
	return res;
}

static uint64_t read_bin(char *p, char **rest)
{
	uint64_t res = 0;
	while(is_bin(*p)) {
		char digit = *p++;
		res = (2 * res) + (digit - '0');
	}
	*rest = p;
	return res;
}

static int read_prefix(char *p, char **rest)
{
	/* all prefixes start with '0' */
	if(*p != '0') {
		return 10;
	}
	p++; /* advance */

	switch(*p) {
	case 'b':
		*rest = p + 1;
		return 2; /* binary */
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
		*rest = p;
		return 8; /* octal (implicit prefix) */
	case 'o':
		*rest = p + 1;
		return 8; /* octal (explicit prefix) */
	case 'z':
		/* Yes, there is dozenal number support. This is completely
		 * useless and of basically no value but it is funny.
		 * This idea was taken straight from Odin. */
		*rest = p + 1;
		return 12;
	case 'x':
		*rest = p + 1;
		return 16; /* hex */
	case '8':
	case '9': /* special error for invalid octal */
		compile_err(p, "invalid octal digit '%c'", *p);
	default:
		return 10;
	}

	/* unreachable */
	return 10;
}

static uint64_t read_number(int base, char *p, char **rest)
{
	switch(base) {
	case 2:
		return read_bin(p, rest);
	case 8:
		return read_oct(p, rest);
	case 10:
		return read_dec(p, rest);
	case 12:
		return read_doz(p, rest);
	case 16:
		return read_hex(p, rest);
	/* unreachable */
	default:
		return -1;
	}
	/* unreachable */
	return -1;
}

static type_t *read_suffix(char *p, char **rest)
{
	char *start = p;
	int lcount = 0;
	int unsignd = 0;
	while(is_long_suf(*p) || is_unsignd_suf(*p)) {
		char suf = *p++;
		lcount += is_long_suf(suf);
		unsignd += is_unsignd_suf(suf);
	}

	if(lcount > 2 || unsignd > 1) {
		compile_err(start, "invalid suffix '%.*s'", p - start, p);
	}

	type_t *base = lcount ? TY_LONG : TY_INT;
	type_t *ty = type_clone(base);
	ty->unsignd = unsignd;

	*rest = p;
	return ty;
}

static void read_full_num(uint64_t *num_, type_t **ty_, char *p, char **rest)
{
	char *start = p;
	int base = read_prefix(p, &p);
	uint64_t num = read_number(base, p, &p);
	type_t *ty = read_suffix(p, rest);
	if(ty->size == 4) {
		if(ty->unsignd) {
			if(num > UINT32_MAX) {
				compile_warn(start, "number is too big for target type");
			}
		} else {
			int64_t n = num;
			if(n < INT32_MIN || n > INT32_MAX) {
				compile_warn(start, "number is too big for target type");
			}
		}
		num = ty->unsignd ? zxt(num, 32) : sxt(num, 32);
	}
	*num_ = num;
	*ty_ = ty;
	return;
}

/* read a single (possibly escaped) character */
static char read_chr(char *prog, char **rest)
{
	char *p = prog;
	if(*p != '\\') {
		*rest = p + 1;
		return *p;
	}

	p++;
	char res = 0;
	switch(*p) {
#define CASE(c, v) \
	case c:        \
		res = (v); \
		p++;       \
		break
		CASE('a', 0x07);
		CASE('b', 0x08);
		CASE('e', 0x1b); /* GNU ext. */
		CASE('f', 0x0c);
		CASE('n', 0x0a);
		CASE('r', 0x0d);
		CASE('t', 0x09);
		CASE('v', 0x0b);
		CASE('\\', 0x5c);
		CASE('\'', 0x27);
		CASE('\"', 0x22);
		CASE('?', 0x3f);
#undef CASE

	/* octal byte */
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
		res = read_octchr(p, &p);
		break;

	/* hex byte */
	case 'x':
		res = read_hexchr(p + 1, &p);
		break;

	default:
		compile_err(p, "unknown escape character '%s'", *p);
	}

	*rest = p;
	return res;
}

/* read a string */
static strb_t read_str(char *prog, char **rest)
{
	strb_t str = strb_make(NULL, 0);
	char *p = prog;
	while(*p != '\"') {
		strb_add_chr(&str, read_chr(p, &p));
	}
	*rest = p;
	return str;
}

/* does the lexing */
token_t *lex_do(char *prog, token_t **end)
{
	token_t start;
	token_t *tok = &start;

	int start_line = 1;
	while(*prog) {
		/* skip over whitespace */
		while(iswhitespace(*prog)) {
			if(*prog == '\n' || *prog == '\r') {
				start_line = 1;
			}
			prog++;
		}

		if(*prog == 0) {
			break;
		}

		/* if we encounter a //, it is a single line comment.
		 * go to next newline */
		if(*prog == '/' && *(prog + 1) == '/') {
			start_line = 0;
			while(*prog != '\n') {
				prog++;
			}
			continue;
		}

		/* if we see a /\*, it is a multi-line comment.
		 * skip all characters until we see a */
		if(*prog == '/' && *(prog + 1) == '*') {
			start_line = 0;
			char *end = strstr(prog, "*/");
			if(!end) {
				compile_err(prog, "unclosed multi-line comment");
			}
			prog = end + 2;
			continue;
		}

		/* tokenize number */
		if(isdigit(*prog)) {
			char *num = prog;
			uint64_t intlit;
			type_t *ty;
			read_full_num(&intlit, &ty, prog, &prog);
			token_t *numb = token_make(TOK_NUM, num, prog);
			numb->num = intlit;
			numb->type = ty;
			numb->start_line = start_line;
			start_line = 0;
			tok->next = numb;
			tok = tok->next;
			continue;
		}

		/* tokenize identifiers */
		if(isidentfirst(*prog)) {
			char *start = prog;
			while(isident(*prog)) {
				prog++;
			}

			/* tokenize keywords */
			int type = TOK_IDENT;
			if(iskeyword(start, prog - start)) {
				type = TOK_KEYWORD;
			}

			token_t *ident = token_make(type, start, prog);
			tok->next = ident;
			ident->start_line = start;
			start_line = 0;
			tok = tok->next;
			continue;
		}

		/* tokenize strings */
		if(*prog == '"') {
			char *begin = prog + 1;
			/* this is absolutely not correct but I will fix it later */
			strb_t strb = read_str(begin, &prog);
			char *string = strb_build(&strb);
			if(*prog != '\"') {
				compile_err(prog, "unclosed string");
			}
			token_t *str = token_make(TOK_STR, begin, prog);
			str->str = string;
			str->type = type_arr_to(TY_CHAR, strb.len + 1);
			str->start_line = start_line;
			start_line = 0;
			prog++;
			tok->next = str;
			tok = tok->next;
			continue;
		}

		if(*prog == '\'') {
			char *chr = prog + 1;

			char content = read_chr(chr, &chr);
			char *str = zalloc(2);
			str[0] = content;
			if(*chr != '\'') {
				compile_err(prog, "unclosed character literal");
			}

			token_t *chrlit = token_make(TOK_STR, chr, chr);
			chrlit->str = str;
			chrlit->type = TY_CHAR;
			chrlit->start_line = start_line;
			start_line = 0;
			prog = chr + 1;
			tok->next = chrlit;
			tok = tok->next;
			continue;
		}

		/* tokenize puncts */
		int plen = punct_len(prog);
		if(plen) {
			token_t *punct = token_make(TOK_PUNCT, prog, prog + plen);
			prog += plen;
			tok->next = punct;
			punct->start_line = start_line;
			start_line = 0;
			tok = tok->next;
			continue;
		}

		compile_err(prog, "unknown expression '%d'", *prog);
	}

	if(end) {
		*end = tok;
	} else {
		tok = tok->next = token_make(TOK_END, prog, prog);
	}

	return start.next;
}
