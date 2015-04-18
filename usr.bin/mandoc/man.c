/*	$OpenBSD: man.c,v 1.100 2015/04/02 23:47:43 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2013, 2014, 2015 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2011 Joerg Sonnenberger <joerg@netbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mandoc_aux.h"
#include "mandoc.h"
#include "roff.h"
#include "man.h"
#include "libmandoc.h"
#include "libman.h"

const	char *const __man_macronames[MAN_MAX] = {
	"br",		"TH",		"SH",		"SS",
	"TP",		"LP",		"PP",		"P",
	"IP",		"HP",		"SM",		"SB",
	"BI",		"IB",		"BR",		"RB",
	"R",		"B",		"I",		"IR",
	"RI",		"sp",		"nf",
	"fi",		"RE",		"RS",		"DT",
	"UC",		"PD",		"AT",		"in",
	"ft",		"OP",		"EX",		"EE",
	"UR",		"UE",		"ll"
	};

const	char * const *man_macronames = __man_macronames;

static	void		 man_alloc1(struct roff_man *);
static	void		 man_breakscope(struct roff_man *, int);
static	void		 man_descope(struct roff_man *, int, int);
static	void		 man_free1(struct roff_man *);
static	struct roff_node *man_node_alloc(struct roff_man *, int, int,
				enum roff_type, int);
static	void		 man_node_append(struct roff_man *,
				struct roff_node *);
static	void		 man_node_free(struct roff_node *);
static	void		 man_node_unlink(struct roff_man *,
				struct roff_node *);
static	int		 man_ptext(struct roff_man *, int, char *, int);
static	int		 man_pmacro(struct roff_man *, int, char *, int);


const struct roff_node *
man_node(const struct roff_man *man)
{

	return(man->first);
}

const struct roff_meta *
man_meta(const struct roff_man *man)
{

	return(&man->meta);
}

void
man_reset(struct roff_man *man)
{

	man_free1(man);
	man_alloc1(man);
}

void
man_free(struct roff_man *man)
{

	man_free1(man);
	free(man);
}

struct roff_man *
man_alloc(struct roff *roff, struct mparse *parse,
	const char *defos, int quick)
{
	struct roff_man	*p;

	p = mandoc_calloc(1, sizeof(*p));

	man_hash_init();
	p->parse = parse;
	p->defos = defos;
	p->quick = quick;
	p->roff = roff;

	man_alloc1(p);
	return(p);
}

void
man_endparse(struct roff_man *man)
{

	man_macroend(man);
}

int
man_parseln(struct roff_man *man, int ln, char *buf, int offs)
{

	if (man->last->type != ROFFT_EQN || ln > man->last->line)
		man->flags |= MAN_NEWLINE;

	return (roff_getcontrol(man->roff, buf, &offs) ?
	    man_pmacro(man, ln, buf, offs) :
	    man_ptext(man, ln, buf, offs));
}

static void
man_free1(struct roff_man *man)
{

	if (man->first)
		man_node_delete(man, man->first);
	free(man->meta.title);
	free(man->meta.os);
	free(man->meta.date);
	free(man->meta.vol);
	free(man->meta.msec);
}

static void
man_alloc1(struct roff_man *man)
{

	memset(&man->meta, 0, sizeof(man->meta));
	man->flags = 0;
	man->last = mandoc_calloc(1, sizeof(*man->last));
	man->first = man->last;
	man->last->type = ROFFT_ROOT;
	man->last->tok = MAN_MAX;
	man->next = ROFF_NEXT_CHILD;
}


static void
man_node_append(struct roff_man *man, struct roff_node *p)
{

	assert(man->last);
	assert(man->first);
	assert(p->type != ROFFT_ROOT);

	switch (man->next) {
	case ROFF_NEXT_SIBLING:
		man->last->next = p;
		p->prev = man->last;
		p->parent = man->last->parent;
		break;
	case ROFF_NEXT_CHILD:
		man->last->child = p;
		p->parent = man->last;
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	assert(p->parent);
	p->parent->nchild++;

	switch (p->type) {
	case ROFFT_BLOCK:
		if (p->tok == MAN_SH || p->tok == MAN_SS)
			man->flags &= ~MAN_LITERAL;
		break;
	case ROFFT_HEAD:
		assert(p->parent->type == ROFFT_BLOCK);
		p->parent->head = p;
		break;
	case ROFFT_BODY:
		assert(p->parent->type == ROFFT_BLOCK);
		p->parent->body = p;
		break;
	default:
		break;
	}

	man->last = p;

	switch (p->type) {
	case ROFFT_TBL:
		/* FALLTHROUGH */
	case ROFFT_TEXT:
		man_valid_post(man);
		break;
	default:
		break;
	}
}

static struct roff_node *
man_node_alloc(struct roff_man *man, int line, int pos,
		enum roff_type type, int tok)
{
	struct roff_node *p;

	p = mandoc_calloc(1, sizeof(*p));
	p->line = line;
	p->pos = pos;
	p->type = type;
	p->tok = tok;

	if (man->flags & MAN_NEWLINE)
		p->flags |= MAN_LINE;
	man->flags &= ~MAN_NEWLINE;
	return(p);
}

void
man_elem_alloc(struct roff_man *man, int line, int pos, int tok)
{
	struct roff_node *p;

	p = man_node_alloc(man, line, pos, ROFFT_ELEM, tok);
	man_node_append(man, p);
	man->next = ROFF_NEXT_CHILD;
}

void
man_head_alloc(struct roff_man *man, int line, int pos, int tok)
{
	struct roff_node *p;

	p = man_node_alloc(man, line, pos, ROFFT_HEAD, tok);
	man_node_append(man, p);
	man->next = ROFF_NEXT_CHILD;
}

void
man_body_alloc(struct roff_man *man, int line, int pos, int tok)
{
	struct roff_node *p;

	p = man_node_alloc(man, line, pos, ROFFT_BODY, tok);
	man_node_append(man, p);
	man->next = ROFF_NEXT_CHILD;
}

void
man_block_alloc(struct roff_man *man, int line, int pos, int tok)
{
	struct roff_node *p;

	p = man_node_alloc(man, line, pos, ROFFT_BLOCK, tok);
	man_node_append(man, p);
	man->next = ROFF_NEXT_CHILD;
}

void
man_word_alloc(struct roff_man *man, int line, int pos, const char *word)
{
	struct roff_node *n;

	n = man_node_alloc(man, line, pos, ROFFT_TEXT, MAN_MAX);
	n->string = roff_strdup(man->roff, word);
	man_node_append(man, n);
	man->next = ROFF_NEXT_SIBLING;
}

void
man_word_append(struct roff_man *man, const char *word)
{
	struct roff_node *n;
	char		*addstr, *newstr;

	n = man->last;
	addstr = roff_strdup(man->roff, word);
	mandoc_asprintf(&newstr, "%s %s", n->string, addstr);
	free(addstr);
	free(n->string);
	n->string = newstr;
	man->next = ROFF_NEXT_SIBLING;
}

/*
 * Free all of the resources held by a node.  This does NOT unlink a
 * node from its context; for that, see man_node_unlink().
 */
static void
man_node_free(struct roff_node *p)
{

	free(p->string);
	free(p);
}

void
man_node_delete(struct roff_man *man, struct roff_node *p)
{

	while (p->child)
		man_node_delete(man, p->child);

	man_node_unlink(man, p);
	man_node_free(p);
}

void
man_addeqn(struct roff_man *man, const struct eqn *ep)
{
	struct roff_node *n;

	n = man_node_alloc(man, ep->ln, ep->pos, ROFFT_EQN, MAN_MAX);
	n->eqn = ep;
	if (ep->ln > man->last->line)
		n->flags |= MAN_LINE;
	man_node_append(man, n);
	man->next = ROFF_NEXT_SIBLING;
	man_descope(man, ep->ln, ep->pos);
}

void
man_addspan(struct roff_man *man, const struct tbl_span *sp)
{
	struct roff_node *n;

	man_breakscope(man, MAN_MAX);
	n = man_node_alloc(man, sp->line, 0, ROFFT_TBL, MAN_MAX);
	n->span = sp;
	man_node_append(man, n);
	man->next = ROFF_NEXT_SIBLING;
	man_descope(man, sp->line, 0);
}

static void
man_descope(struct roff_man *man, int line, int offs)
{
	/*
	 * Co-ordinate what happens with having a next-line scope open:
	 * first close out the element scope (if applicable), then close
	 * out the block scope (also if applicable).
	 */

	if (man->flags & MAN_ELINE) {
		man->flags &= ~MAN_ELINE;
		man_unscope(man, man->last->parent);
	}
	if ( ! (man->flags & MAN_BLINE))
		return;
	man->flags &= ~MAN_BLINE;
	man_unscope(man, man->last->parent);
	man_body_alloc(man, line, offs, man->last->tok);
}

static int
man_ptext(struct roff_man *man, int line, char *buf, int offs)
{
	int		 i;

	/* Literal free-form text whitespace is preserved. */

	if (man->flags & MAN_LITERAL) {
		man_word_alloc(man, line, offs, buf + offs);
		man_descope(man, line, offs);
		return(1);
	}

	for (i = offs; buf[i] == ' '; i++)
		/* Skip leading whitespace. */ ;

	/*
	 * Blank lines are ignored right after headings
	 * but add a single vertical space elsewhere.
	 */

	if (buf[i] == '\0') {
		/* Allocate a blank entry. */
		if (man->last->tok != MAN_SH &&
		    man->last->tok != MAN_SS) {
			man_elem_alloc(man, line, offs, MAN_sp);
			man->next = ROFF_NEXT_SIBLING;
		}
		return(1);
	}

	/*
	 * Warn if the last un-escaped character is whitespace. Then
	 * strip away the remaining spaces (tabs stay!).
	 */

	i = (int)strlen(buf);
	assert(i);

	if (' ' == buf[i - 1] || '\t' == buf[i - 1]) {
		if (i > 1 && '\\' != buf[i - 2])
			mandoc_msg(MANDOCERR_SPACE_EOL, man->parse,
			    line, i - 1, NULL);

		for (--i; i && ' ' == buf[i]; i--)
			/* Spin back to non-space. */ ;

		/* Jump ahead of escaped whitespace. */
		i += '\\' == buf[i] ? 2 : 1;

		buf[i] = '\0';
	}
	man_word_alloc(man, line, offs, buf + offs);

	/*
	 * End-of-sentence check.  If the last character is an unescaped
	 * EOS character, then flag the node as being the end of a
	 * sentence.  The front-end will know how to interpret this.
	 */

	assert(i);
	if (mandoc_eos(buf, (size_t)i))
		man->last->flags |= MAN_EOS;

	man_descope(man, line, offs);
	return(1);
}

static int
man_pmacro(struct roff_man *man, int ln, char *buf, int offs)
{
	struct roff_node *n;
	const char	*cp;
	int		 tok;
	int		 i, ppos;
	int		 bline;
	char		 mac[5];

	ppos = offs;

	/*
	 * Copy the first word into a nil-terminated buffer.
	 * Stop when a space, tab, escape, or eoln is encountered.
	 */

	i = 0;
	while (i < 4 && strchr(" \t\\", buf[offs]) == NULL)
		mac[i++] = buf[offs++];

	mac[i] = '\0';

	tok = (i > 0 && i < 4) ? man_hash_find(mac) : MAN_MAX;

	if (tok == MAN_MAX) {
		mandoc_msg(MANDOCERR_MACRO, man->parse,
		    ln, ppos, buf + ppos - 1);
		return(1);
	}

	/* Skip a leading escape sequence or tab. */

	switch (buf[offs]) {
	case '\\':
		cp = buf + offs + 1;
		mandoc_escape(&cp, NULL, NULL);
		offs = cp - buf;
		break;
	case '\t':
		offs++;
		break;
	default:
		break;
	}

	/* Jump to the next non-whitespace word. */

	while (buf[offs] && buf[offs] == ' ')
		offs++;

	/*
	 * Trailing whitespace.  Note that tabs are allowed to be passed
	 * into the parser as "text", so we only warn about spaces here.
	 */

	if (buf[offs] == '\0' && buf[offs - 1] == ' ')
		mandoc_msg(MANDOCERR_SPACE_EOL, man->parse,
		    ln, offs - 1, NULL);

	/*
	 * Some macros break next-line scopes; otherwise, remember
	 * whether we are in next-line scope for a block head.
	 */

	man_breakscope(man, tok);
	bline = man->flags & MAN_BLINE;

	/* Call to handler... */

	assert(man_macros[tok].fp);
	(*man_macros[tok].fp)(man, tok, ln, ppos, &offs, buf);

	/* In quick mode (for mandocdb), abort after the NAME section. */

	if (man->quick && tok == MAN_SH) {
		n = man->last;
		if (n->type == ROFFT_BODY &&
		    strcmp(n->prev->child->string, "NAME"))
			return(2);
	}

	/*
	 * If we are in a next-line scope for a block head,
	 * close it out now and switch to the body,
	 * unless the next-line scope is allowed to continue.
	 */

	if ( ! bline || man->flags & MAN_ELINE ||
	    man_macros[tok].flags & MAN_NSCOPED)
		return(1);

	assert(man->flags & MAN_BLINE);
	man->flags &= ~MAN_BLINE;

	man_unscope(man, man->last->parent);
	man_body_alloc(man, ln, ppos, man->last->tok);
	return(1);
}

void
man_breakscope(struct roff_man *man, int tok)
{
	struct roff_node *n;

	/*
	 * An element next line scope is open,
	 * and the new macro is not allowed inside elements.
	 * Delete the element that is being broken.
	 */

	if (man->flags & MAN_ELINE && (tok == MAN_MAX ||
	    ! (man_macros[tok].flags & MAN_NSCOPED))) {
		n = man->last;
		assert(n->type != ROFFT_TEXT);
		if (man_macros[n->tok].flags & MAN_NSCOPED)
			n = n->parent;

		mandoc_vmsg(MANDOCERR_BLK_LINE, man->parse,
		    n->line, n->pos, "%s breaks %s",
		    tok == MAN_MAX ? "TS" : man_macronames[tok],
		    man_macronames[n->tok]);

		man_node_delete(man, n);
		man->flags &= ~MAN_ELINE;
	}

	/*
	 * A block header next line scope is open,
	 * and the new macro is not allowed inside block headers.
	 * Delete the block that is being broken.
	 */

	if (man->flags & MAN_BLINE && (tok == MAN_MAX ||
	    man_macros[tok].flags & MAN_BSCOPE)) {
		n = man->last;
		if (n->type == ROFFT_TEXT)
			n = n->parent;
		if ( ! (man_macros[n->tok].flags & MAN_BSCOPE))
			n = n->parent;

		assert(n->type == ROFFT_HEAD);
		n = n->parent;
		assert(n->type == ROFFT_BLOCK);
		assert(man_macros[n->tok].flags & MAN_SCOPED);

		mandoc_vmsg(MANDOCERR_BLK_LINE, man->parse,
		    n->line, n->pos, "%s breaks %s",
		    tok == MAN_MAX ? "TS" : man_macronames[tok],
		    man_macronames[n->tok]);

		man_node_delete(man, n);
		man->flags &= ~MAN_BLINE;
	}
}

/*
 * Unlink a node from its context.  If "man" is provided, the last parse
 * point will also be adjusted accordingly.
 */
static void
man_node_unlink(struct roff_man *man, struct roff_node *n)
{

	/* Adjust siblings. */

	if (n->prev)
		n->prev->next = n->next;
	if (n->next)
		n->next->prev = n->prev;

	/* Adjust parent. */

	if (n->parent) {
		n->parent->nchild--;
		if (n->parent->child == n)
			n->parent->child = n->prev ? n->prev : n->next;
	}

	/* Adjust parse point, if applicable. */

	if (man && man->last == n) {
		/*XXX: this can occur when bailing from validation. */
		/*assert(NULL == n->next);*/
		if (n->prev) {
			man->last = n->prev;
			man->next = ROFF_NEXT_SIBLING;
		} else {
			man->last = n->parent;
			man->next = ROFF_NEXT_CHILD;
		}
	}

	if (man && man->first == n)
		man->first = NULL;
}

const struct mparse *
man_mparse(const struct roff_man *man)
{

	assert(man && man->parse);
	return(man->parse);
}

void
man_deroff(char **dest, const struct roff_node *n)
{
	char	*cp;
	size_t	 sz;

	if (n->type != ROFFT_TEXT) {
		for (n = n->child; n; n = n->next)
			man_deroff(dest, n);
		return;
	}

	/* Skip leading whitespace and escape sequences. */

	cp = n->string;
	while ('\0' != *cp) {
		if ('\\' == *cp) {
			cp++;
			mandoc_escape((const char **)&cp, NULL, NULL);
		} else if (isspace((unsigned char)*cp))
			cp++;
		else
			break;
	}

	/* Skip trailing whitespace. */

	for (sz = strlen(cp); sz; sz--)
		if (0 == isspace((unsigned char)cp[sz-1]))
			break;

	/* Skip empty strings. */

	if (0 == sz)
		return;

	if (NULL == *dest) {
		*dest = mandoc_strndup(cp, sz);
		return;
	}

	mandoc_asprintf(&cp, "%s %*s", *dest, (int)sz, cp);
	free(*dest);
	*dest = cp;
}
