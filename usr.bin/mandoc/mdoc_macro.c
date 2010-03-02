/*	$Id: mdoc_macro.c,v 1.29 2010/02/26 12:12:24 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009 Kristaps Dzonsons <kristaps@kth.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "libmdoc.h"

#define	REWIND_REWIND	(1 << 0)
#define	REWIND_NOHALT	(1 << 1)
#define	REWIND_HALT	(1 << 2)

static	int	  ctx_synopsis(MACRO_PROT_ARGS);
static	int	  obsolete(MACRO_PROT_ARGS);
static	int	  blk_part_exp(MACRO_PROT_ARGS);
static	int	  in_line_eoln(MACRO_PROT_ARGS);
static	int	  in_line_argn(MACRO_PROT_ARGS);
static	int	  in_line(MACRO_PROT_ARGS);
static	int	  blk_full(MACRO_PROT_ARGS);
static	int	  blk_exp_close(MACRO_PROT_ARGS);
static	int	  blk_part_imp(MACRO_PROT_ARGS);

static	int	  phrase(struct mdoc *, int, int, char *);
static	int	  rew_dohalt(int, enum mdoc_type, 
			const struct mdoc_node *);
static	int	  rew_alt(int);
static	int	  rew_dobreak(int, const struct mdoc_node *);
static	int	  rew_elem(struct mdoc *, int);
static	int	  rew_sub(enum mdoc_type, struct mdoc *, 
			int, int, int);
static	int	  rew_last(struct mdoc *, 
			const struct mdoc_node *);
static	int	  append_delims(struct mdoc *, int, int *, char *);
static	int	  lookup(int, const char *);
static	int	  lookup_raw(const char *);
static	int	  swarn(struct mdoc *, enum mdoc_type, int, int, 
			const struct mdoc_node *);

/* Central table of library: who gets parsed how. */

const	struct mdoc_macro __mdoc_macros[MDOC_MAX] = {
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Ap */
	{ in_line_eoln, MDOC_PROLOGUE }, /* Dd */
	{ in_line_eoln, MDOC_PROLOGUE }, /* Dt */
	{ in_line_eoln, MDOC_PROLOGUE }, /* Os */
	{ blk_full, 0 }, /* Sh */
	{ blk_full, 0 }, /* Ss */ 
	{ in_line_eoln, 0 }, /* Pp */ 
	{ blk_part_imp, MDOC_PARSED }, /* D1 */
	{ blk_part_imp, MDOC_PARSED }, /* Dl */
	{ blk_full, MDOC_EXPLICIT }, /* Bd */
	{ blk_exp_close, MDOC_EXPLICIT }, /* Ed */
	{ blk_full, MDOC_EXPLICIT }, /* Bl */
	{ blk_exp_close, MDOC_EXPLICIT }, /* El */
	{ blk_full, MDOC_PARSED }, /* It */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Ad */ 
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* An */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Ar */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Cd */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Cm */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Dv */ 
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Er */ 
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Ev */ 
	{ in_line_eoln, 0 }, /* Ex */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Fa */ 
	{ in_line_eoln, 0 }, /* Fd */ 
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Fl */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Fn */ 
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Ft */ 
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Ic */ 
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* In */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Li */
	{ blk_full, 0 }, /* Nd */ 
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Nm */ 
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Op */
	{ obsolete, 0 }, /* Ot */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Pa */
	{ in_line_eoln, 0 }, /* Rv */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* St */ 
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Va */
	{ ctx_synopsis, MDOC_CALLABLE | MDOC_PARSED }, /* Vt */ 
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Xr */
	{ in_line_eoln, 0 }, /* %A */
	{ in_line_eoln, 0 }, /* %B */
	{ in_line_eoln, 0 }, /* %D */
	{ in_line_eoln, 0 }, /* %I */
	{ in_line_eoln, 0 }, /* %J */
	{ in_line_eoln, 0 }, /* %N */
	{ in_line_eoln, 0 }, /* %O */
	{ in_line_eoln, 0 }, /* %P */
	{ in_line_eoln, 0 }, /* %R */
	{ in_line_eoln, 0 }, /* %T */
	{ in_line_eoln, 0 }, /* %V */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Ac */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Ao */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Aq */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* At */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Bc */
	{ blk_full, MDOC_EXPLICIT }, /* Bf */ 
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Bo */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Bq */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Bsx */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Bx */
	{ in_line_eoln, 0 }, /* Db */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Dc */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Do */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Dq */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Ec */
	{ blk_exp_close, MDOC_EXPLICIT }, /* Ef */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Em */ 
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Eo */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Fx */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Ms */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* No */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Ns */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Nx */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Ox */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Pc */
	{ in_line_argn, MDOC_PARSED | MDOC_IGNDELIM }, /* Pf */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Po */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Pq */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Qc */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Ql */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Qo */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Qq */
	{ blk_exp_close, MDOC_EXPLICIT }, /* Re */
	{ blk_full, MDOC_EXPLICIT }, /* Rs */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Sc */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* So */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Sq */
	{ in_line_eoln, 0 }, /* Sm */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Sx */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Sy */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Tn */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Ux */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Xc */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Xo */
	{ blk_full, MDOC_EXPLICIT | MDOC_CALLABLE }, /* Fo */ 
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Fc */ 
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Oo */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Oc */
	{ blk_full, MDOC_EXPLICIT }, /* Bk */
	{ blk_exp_close, MDOC_EXPLICIT }, /* Ek */
	{ in_line_eoln, 0 }, /* Bt */
	{ in_line_eoln, 0 }, /* Hf */
	{ obsolete, 0 }, /* Fr */
	{ in_line_eoln, 0 }, /* Ud */
	{ in_line_eoln, 0 }, /* Lb */
	{ in_line_eoln, 0 }, /* Lp */ 
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Lk */ 
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Mt */ 
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Brq */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Bro */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Brc */
	{ in_line_eoln, 0 }, /* %C */
	{ obsolete, 0 }, /* Es */
	{ obsolete, 0 }, /* En */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Dx */
	{ in_line_eoln, 0 }, /* %Q */
	{ in_line_eoln, 0 }, /* br */
	{ in_line_eoln, 0 }, /* sp */
	{ in_line_eoln, 0 }, /* %U */
};

const	struct mdoc_macro * const mdoc_macros = __mdoc_macros;


static int
swarn(struct mdoc *mdoc, enum mdoc_type type, 
		int line, int pos, const struct mdoc_node *p)
{
	const char	*n, *t, *tt;

	n = t = "<root>";
	tt = "block";

	switch (type) {
	case (MDOC_BODY):
		tt = "multi-line";
		break;
	case (MDOC_HEAD):
		tt = "line";
		break;
	default:
		break;
	}

	switch (p->type) {
	case (MDOC_BLOCK):
		n = mdoc_macronames[p->tok];
		t = "block";
		break;
	case (MDOC_BODY):
		n = mdoc_macronames[p->tok];
		t = "multi-line";
		break;
	case (MDOC_HEAD):
		n = mdoc_macronames[p->tok];
		t = "line";
		break;
	default:
		break;
	}

	if ( ! (MDOC_IGN_SCOPE & mdoc->pflags))
		return(mdoc_verr(mdoc, line, pos, 
				"%s scope breaks %s scope of %s", 
				tt, t, n));
	return(mdoc_vwarn(mdoc, line, pos, 
				"%s scope breaks %s scope of %s", 
				tt, t, n));
}


/*
 * This is called at the end of parsing.  It must traverse up the tree,
 * closing out open [implicit] scopes.  Obviously, open explicit scopes
 * are errors.
 */
int
mdoc_macroend(struct mdoc *m)
{
	struct mdoc_node *n;

	/* Scan for open explicit scopes. */

	n = MDOC_VALID & m->last->flags ?  m->last->parent : m->last;

	for ( ; n; n = n->parent) {
		if (MDOC_BLOCK != n->type)
			continue;
		if ( ! (MDOC_EXPLICIT & mdoc_macros[n->tok].flags))
			continue;
		return(mdoc_nerr(m, n, EOPEN));
	}

	/* Rewind to the first. */

	return(rew_last(m, m->first));
}


/*
 * Look up a macro from within a subsequent context.
 */
static int
lookup(int from, const char *p)
{
	/* FIXME: make -diag lists be un-PARSED. */

	if ( ! (MDOC_PARSED & mdoc_macros[from].flags))
		return(MDOC_MAX);
	return(lookup_raw(p));
}


/*
 * Lookup a macro following the initial line macro.
 */
static int
lookup_raw(const char *p)
{
	int		 res;

	if (MDOC_MAX == (res = mdoc_hash_find(p)))
		return(MDOC_MAX);
	if (MDOC_CALLABLE & mdoc_macros[res].flags)
		return(res);
	return(MDOC_MAX);
}


static int
rew_last(struct mdoc *mdoc, const struct mdoc_node *to)
{

	assert(to);
	mdoc->next = MDOC_NEXT_SIBLING;

	/* LINTED */
	while (mdoc->last != to) {
		if ( ! mdoc_valid_post(mdoc))
			return(0);
		if ( ! mdoc_action_post(mdoc))
			return(0);
		mdoc->last = mdoc->last->parent;
		assert(mdoc->last);
	}

	if ( ! mdoc_valid_post(mdoc))
		return(0);
	return(mdoc_action_post(mdoc));
}


/*
 * Return the opening macro of a closing one, e.g., `Ec' has `Eo' as its
 * matching pair.
 */
static int
rew_alt(int tok)
{
	switch (tok) {
	case (MDOC_Ac):
		return(MDOC_Ao);
	case (MDOC_Bc):
		return(MDOC_Bo);
	case (MDOC_Brc):
		return(MDOC_Bro);
	case (MDOC_Dc):
		return(MDOC_Do);
	case (MDOC_Ec):
		return(MDOC_Eo);
	case (MDOC_Ed):
		return(MDOC_Bd);
	case (MDOC_Ef):
		return(MDOC_Bf);
	case (MDOC_Ek):
		return(MDOC_Bk);
	case (MDOC_El):
		return(MDOC_Bl);
	case (MDOC_Fc):
		return(MDOC_Fo);
	case (MDOC_Oc):
		return(MDOC_Oo);
	case (MDOC_Pc):
		return(MDOC_Po);
	case (MDOC_Qc):
		return(MDOC_Qo);
	case (MDOC_Re):
		return(MDOC_Rs);
	case (MDOC_Sc):
		return(MDOC_So);
	case (MDOC_Xc):
		return(MDOC_Xo);
	default:
		break;
	}
	abort();
	/* NOTREACHED */
}


/* 
 * Rewind rules.  This indicates whether to stop rewinding
 * (REWIND_HALT) without touching our current scope, stop rewinding and
 * close our current scope (REWIND_REWIND), or continue (REWIND_NOHALT).
 * The scope-closing and so on occurs in the various rew_* routines.
 */
static int 
rew_dohalt(int tok, enum mdoc_type type, const struct mdoc_node *p)
{

	if (MDOC_ROOT == p->type)
		return(REWIND_HALT);
	if (MDOC_VALID & p->flags)
		return(REWIND_NOHALT);

	switch (tok) {
	case (MDOC_Aq):
		/* FALLTHROUGH */
	case (MDOC_Bq):
		/* FALLTHROUGH */
	case (MDOC_Brq):
		/* FALLTHROUGH */
	case (MDOC_D1):
		/* FALLTHROUGH */
	case (MDOC_Dl):
		/* FALLTHROUGH */
	case (MDOC_Dq):
		/* FALLTHROUGH */
	case (MDOC_Op):
		/* FALLTHROUGH */
	case (MDOC_Pq):
		/* FALLTHROUGH */
	case (MDOC_Ql):
		/* FALLTHROUGH */
	case (MDOC_Qq):
		/* FALLTHROUGH */
	case (MDOC_Sq):
		/* FALLTHROUGH */
	case (MDOC_Vt):
		assert(MDOC_TAIL != type);
		if (type == p->type && tok == p->tok)
			return(REWIND_REWIND);
		break;
	case (MDOC_It):
		assert(MDOC_TAIL != type);
		if (type == p->type && tok == p->tok)
			return(REWIND_REWIND);
		if (MDOC_BODY == p->type && MDOC_Bl == p->tok)
			return(REWIND_HALT);
		break;
	case (MDOC_Sh):
		if (type == p->type && tok == p->tok)
			return(REWIND_REWIND);
		break;
	case (MDOC_Nd):
		/* FALLTHROUGH */
	case (MDOC_Ss):
		assert(MDOC_TAIL != type);
		if (type == p->type && tok == p->tok)
			return(REWIND_REWIND);
		if (MDOC_BODY == p->type && MDOC_Sh == p->tok)
			return(REWIND_HALT);
		break;
	case (MDOC_Ao):
		/* FALLTHROUGH */
	case (MDOC_Bd):
		/* FALLTHROUGH */
	case (MDOC_Bf):
		/* FALLTHROUGH */
	case (MDOC_Bk):
		/* FALLTHROUGH */
	case (MDOC_Bl):
		/* FALLTHROUGH */
	case (MDOC_Bo):
		/* FALLTHROUGH */
	case (MDOC_Bro):
		/* FALLTHROUGH */
	case (MDOC_Do):
		/* FALLTHROUGH */
	case (MDOC_Eo):
		/* FALLTHROUGH */
	case (MDOC_Fo):
		/* FALLTHROUGH */
	case (MDOC_Oo):
		/* FALLTHROUGH */
	case (MDOC_Po):
		/* FALLTHROUGH */
	case (MDOC_Qo):
		/* FALLTHROUGH */
	case (MDOC_Rs):
		/* FALLTHROUGH */
	case (MDOC_So):
		/* FALLTHROUGH */
	case (MDOC_Xo):
		if (type == p->type && tok == p->tok)
			return(REWIND_REWIND);
		break;
	/* Multi-line explicit scope close. */
	case (MDOC_Ac):
		/* FALLTHROUGH */
	case (MDOC_Bc):
		/* FALLTHROUGH */
	case (MDOC_Brc):
		/* FALLTHROUGH */
	case (MDOC_Dc):
		/* FALLTHROUGH */
	case (MDOC_Ec):
		/* FALLTHROUGH */
	case (MDOC_Ed):
		/* FALLTHROUGH */
	case (MDOC_Ek):
		/* FALLTHROUGH */
	case (MDOC_El):
		/* FALLTHROUGH */
	case (MDOC_Fc):
		/* FALLTHROUGH */
	case (MDOC_Ef):
		/* FALLTHROUGH */
	case (MDOC_Oc):
		/* FALLTHROUGH */
	case (MDOC_Pc):
		/* FALLTHROUGH */
	case (MDOC_Qc):
		/* FALLTHROUGH */
	case (MDOC_Re):
		/* FALLTHROUGH */
	case (MDOC_Sc):
		/* FALLTHROUGH */
	case (MDOC_Xc):
		if (type == p->type && rew_alt(tok) == p->tok)
			return(REWIND_REWIND);
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	return(REWIND_NOHALT);
}


/*
 * See if we can break an encountered scope (the rew_dohalt has returned
 * REWIND_NOHALT). 
 */
static int
rew_dobreak(int tok, const struct mdoc_node *p)
{

	assert(MDOC_ROOT != p->type);
	if (MDOC_ELEM == p->type)
		return(1);
	if (MDOC_TEXT == p->type)
		return(1);
	if (MDOC_VALID & p->flags)
		return(1);

	switch (tok) {
	case (MDOC_It):
		return(MDOC_It == p->tok);
	case (MDOC_Nd):
		return(MDOC_Nd == p->tok);
	case (MDOC_Ss):
		return(MDOC_Ss == p->tok);
	case (MDOC_Sh):
		if (MDOC_Nd == p->tok)
			return(1);
		if (MDOC_Ss == p->tok)
			return(1);
		return(MDOC_Sh == p->tok);
	case (MDOC_El):
		if (MDOC_It == p->tok)
			return(1);
		break;
	case (MDOC_Oc):
		/* XXX - experimental! */
		if (MDOC_Op == p->tok)
			return(1);
		break;
	default:
		break;
	}

	if (MDOC_EXPLICIT & mdoc_macros[tok].flags) 
		return(p->tok == rew_alt(tok));
	else if (MDOC_BLOCK == p->type)
		return(1);

	return(tok == p->tok);
}


static int
rew_elem(struct mdoc *mdoc, int tok)
{
	struct mdoc_node *n;

	n = mdoc->last;
	if (MDOC_ELEM != n->type)
		n = n->parent;
	assert(MDOC_ELEM == n->type);
	assert(tok == n->tok);

	return(rew_last(mdoc, n));
}


static int
rew_sub(enum mdoc_type t, struct mdoc *m, 
		int tok, int line, int ppos)
{
	struct mdoc_node *n;
	int		  c;

	/* LINTED */
	for (n = m->last; n; n = n->parent) {
		c = rew_dohalt(tok, t, n);
		if (REWIND_HALT == c) {
			if (MDOC_BLOCK != t)
				return(1);
			if ( ! (MDOC_EXPLICIT & mdoc_macros[tok].flags))
				return(1);
			return(mdoc_perr(m, line, ppos, ENOCTX));
		}
		if (REWIND_REWIND == c)
			break;
		else if (rew_dobreak(tok, n))
			continue;
		if ( ! swarn(m, t, line, ppos, n))
			return(0);
	}

	assert(n);
	if ( ! rew_last(m, n))
		return(0);

	/*
	 * The current block extends an enclosing block beyond a line break.
	 * Now that the current block ends, close the enclosing block, too.
	 */
	if ((n = n->pending) != NULL) {
		assert(MDOC_HEAD == n->type);
		if ( ! rew_last(m, n))
			return(0);
		if ( ! mdoc_body_alloc(m, n->line, n->pos, n->tok))
			return(0);
	}
	return(1);
}


static int
append_delims(struct mdoc *mdoc, int line, int *pos, char *buf)
{
	int		 c, lastarg;
	char		*p;

	if (0 == buf[*pos])
		return(1);

	for (;;) {
		lastarg = *pos;
		c = mdoc_zargs(mdoc, line, pos, buf, ARGS_NOWARN, &p);
		assert(ARGS_PHRASE != c);

		if (ARGS_ERROR == c)
			return(0);
		else if (ARGS_EOLN == c)
			break;
		assert(mdoc_isdelim(p));
		if ( ! mdoc_word_alloc(mdoc, line, lastarg, p))
			return(0);
	}

	return(1);
}


/*
 * Close out block partial/full explicit.  
 */
static int
blk_exp_close(MACRO_PROT_ARGS)
{
	int	 	 j, c, lastarg, maxargs, flushed;
	char		*p;

	switch (tok) {
	case (MDOC_Ec):
		maxargs = 1;
		break;
	default:
		maxargs = 0;
		break;
	}

	if ( ! (MDOC_CALLABLE & mdoc_macros[tok].flags)) {
		if (buf[*pos]) 
			if ( ! mdoc_pwarn(m, line, ppos, ENOLINE))
				return(0);

		if ( ! rew_sub(MDOC_BODY, m, tok, line, ppos))
			return(0);
		return(rew_sub(MDOC_BLOCK, m, tok, line, ppos));
	}

	if ( ! rew_sub(MDOC_BODY, m, tok, line, ppos))
		return(0);

	if (maxargs > 0) 
		if ( ! mdoc_tail_alloc(m, line, ppos, rew_alt(tok)))
			return(0);

	for (flushed = j = 0; ; j++) {
		lastarg = *pos;

		if (j == maxargs && ! flushed) {
			if ( ! rew_sub(MDOC_BLOCK, m, tok, line, ppos))
				return(0);
			flushed = 1;
		}

		c = mdoc_args(m, line, pos, buf, tok, &p);

		if (ARGS_ERROR == c)
			return(0);
		if (ARGS_PUNCT == c)
			break;
		if (ARGS_EOLN == c)
			break;

		if (MDOC_MAX != (c = lookup(tok, p))) {
			if ( ! flushed) {
				if ( ! rew_sub(MDOC_BLOCK, m, tok, line, ppos))
					return(0);
				flushed = 1;
			}
			if ( ! mdoc_macro(m, c, line, lastarg, pos, buf))
				return(0);
			break;
		} 

		if ( ! mdoc_word_alloc(m, line, lastarg, p))
			return(0);
	}

	if ( ! flushed && ! rew_sub(MDOC_BLOCK, m, tok, line, ppos))
		return(0);

	if (ppos > 1)
		return(1);
	return(append_delims(m, line, pos, buf));
}


static int
in_line(MACRO_PROT_ARGS)
{
	int		  la, lastpunct, c, w, cnt, d, nc;
	struct mdoc_arg	 *arg;
	char		 *p;

	/*
	 * Whether we allow ignored elements (those without content,
	 * usually because of reserved words) to squeak by.
	 */
	switch (tok) {
	case (MDOC_An):
		/* FALLTHROUGH */
	case (MDOC_Ar):
		/* FALLTHROUGH */
	case (MDOC_Fl):
		/* FALLTHROUGH */
	case (MDOC_Lk):
		/* FALLTHROUGH */
	case (MDOC_Nm):
		/* FALLTHROUGH */
	case (MDOC_Pa):
		nc = 1;
		break;
	default:
		nc = 0;
		break;
	}

	for (arg = NULL;; ) {
		la = *pos;
		c = mdoc_argv(m, line, tok, &arg, pos, buf);

		if (ARGV_WORD == c) {
			*pos = la;
			break;
		} 
		if (ARGV_EOLN == c)
			break;
		if (ARGV_ARG == c)
			continue;

		mdoc_argv_free(arg);
		return(0);
	}

	for (cnt = 0, lastpunct = 1;; ) {
		la = *pos;
		w = mdoc_args(m, line, pos, buf, tok, &p);

		if (ARGS_ERROR == w)
			return(0);
		if (ARGS_EOLN == w)
			break;
		if (ARGS_PUNCT == w)
			break;

		/* Quoted words shouldn't be looked-up. */

		c = ARGS_QWORD == w ? MDOC_MAX : lookup(tok, p);

		/* 
		 * In this case, we've located a submacro and must
		 * execute it.  Close out scope, if open.  If no
		 * elements have been generated, either create one (nc)
		 * or raise a warning.
		 */

		if (MDOC_MAX != c) {
			if (0 == lastpunct && ! rew_elem(m, tok))
				return(0);
			if (nc && 0 == cnt) {
				if ( ! mdoc_elem_alloc(m, line, ppos, tok, arg))
					return(0);
				if ( ! rew_last(m, m->last))
					return(0);
			} else if ( ! nc && 0 == cnt) {
				mdoc_argv_free(arg);
				if ( ! mdoc_pwarn(m, line, ppos, EIGNE))
					return(0);
			}
			c = mdoc_macro(m, c, line, la, pos, buf);
			if (0 == c)
				return(0);
			if (ppos > 1)
				return(1);
			return(append_delims(m, line, pos, buf));
		} 

		/* 
		 * Non-quote-enclosed punctuation.  Set up our scope, if
		 * a word; rewind the scope, if a delimiter; then append
		 * the word. 
		 */

		d = mdoc_isdelim(p);

		if (ARGS_QWORD != w && d) {
			if (0 == lastpunct && ! rew_elem(m, tok))
				return(0);
			lastpunct = 1;
		} else if (lastpunct) {
			if ( ! mdoc_elem_alloc(m, line, ppos, tok, arg))
				return(0);
			lastpunct = 0;
		}

		if ( ! d)
			cnt++;
		if ( ! mdoc_word_alloc(m, line, la, p))
			return(0);

		/*
		 * `Fl' macros have their scope re-opened with each new
		 * word so that the `-' can be added to each one without
		 * having to parse out spaces.
		 */
		if (0 == lastpunct && MDOC_Fl == tok) {
			if ( ! rew_elem(m, tok))
				return(0);
			lastpunct = 1;
		}
	}

	if (0 == lastpunct && ! rew_elem(m, tok))
		return(0);

	/*
	 * If no elements have been collected and we're allowed to have
	 * empties (nc), open a scope and close it out.  Otherwise,
	 * raise a warning.
	 *
	 */
	if (nc && 0 == cnt) {
		if ( ! mdoc_elem_alloc(m, line, ppos, tok, arg))
			return(0);
		if ( ! rew_last(m, m->last))
			return(0);
	} else if ( ! nc && 0 == cnt) {
		mdoc_argv_free(arg);
		if ( ! mdoc_pwarn(m, line, ppos, EIGNE))
			return(0);
	}

	if (ppos > 1)
		return(1);
	return(append_delims(m, line, pos, buf));
}


static int
blk_full(MACRO_PROT_ARGS)
{
	int		  c, lastarg, reopen, dohead;
	struct mdoc_arg	 *arg;
	struct mdoc_node *head, *n;
	char		 *p;

	/* 
	 * Whether to process a block-head section.  If this is
	 * non-zero, then a head will be opened for all line arguments.
	 * If not, then the head will always be empty and only a body
	 * will be opened, which will stay open at the eoln.
	 */

	switch (tok) {
	case (MDOC_Nd):
		dohead = 0;
		break;
	default:
		dohead = 1;
		break;
	}

	if ( ! (MDOC_EXPLICIT & mdoc_macros[tok].flags)) {
		if ( ! rew_sub(MDOC_BODY, m, tok, line, ppos))
			return(0);
		if ( ! rew_sub(MDOC_BLOCK, m, tok, line, ppos))
			return(0);
	}

	for (arg = NULL;; ) {
		lastarg = *pos;
		c = mdoc_argv(m, line, tok, &arg, pos, buf);

		if (ARGV_WORD == c) {
			*pos = lastarg;
			break;
		} 

		if (ARGV_EOLN == c)
			break;
		if (ARGV_ARG == c)
			continue;

		mdoc_argv_free(arg);
		return(0);
	}

	if ( ! mdoc_block_alloc(m, line, ppos, tok, arg))
		return(0);
	if ( ! mdoc_head_alloc(m, line, ppos, tok))
		return(0);
	head = m->last;

	if (0 == buf[*pos]) {
		if ( ! rew_sub(MDOC_HEAD, m, tok, line, ppos))
			return(0);
		if ( ! mdoc_body_alloc(m, line, ppos, tok))
			return(0);
		return(1);
	}

	/* Immediately close out head and enter body, if applicable. */

	if (0 == dohead) {
		if ( ! rew_sub(MDOC_HEAD, m, tok, line, ppos))
			return(0);
		if ( ! mdoc_body_alloc(m, line, ppos, tok))
			return(0);
	} 

	for (reopen = 0;; ) {
		lastarg = *pos;
		c = mdoc_args(m, line, pos, buf, tok, &p);

		if (ARGS_ERROR == c)
			return(0);
		if (ARGS_EOLN == c)
			break;
		if (ARGS_PHRASE == c) {
			assert(dohead);
			if (reopen && ! mdoc_head_alloc(m, line, ppos, tok))
				return(0);
			head = m->last;
			/*
			 * Phrases are self-contained macro phrases used
			 * in the columnar output of a macro. They need
			 * special handling.
			 */
			if ( ! phrase(m, line, lastarg, buf))
				return(0);
			if ( ! rew_sub(MDOC_HEAD, m, tok, line, ppos))
				return(0);

			reopen = 1;
			continue;
		}

		if (MDOC_MAX == (c = lookup(tok, p))) {
			if ( ! mdoc_word_alloc(m, line, lastarg, p))
				return(0);
			continue;
		} 

		if ( ! mdoc_macro(m, c, line, lastarg, pos, buf))
			return(0);
		break;
	}
	
	if (1 == ppos && ! append_delims(m, line, pos, buf))
		return(0);

	/* If the body's already open, then just return. */
	if (0 == dohead) 
		return(1);

	/*
	 * If there is an open sub-block requiring explicit close-out,
	 * postpone switching the current block from head to body
	 * until the rew_sub() call closing out that sub-block.
	 */
	for (n = m->last; n && n != head; n = n->parent) {
		if (MDOC_BLOCK == n->type &&
		    MDOC_EXPLICIT & mdoc_macros[n->tok].flags) {
			n->pending = head;
			return(1);
		}
	}

	if ( ! rew_sub(MDOC_HEAD, m, tok, line, ppos))
		return(0);
	if ( ! mdoc_body_alloc(m, line, ppos, tok))
		return(0);

	return(1);
}


static int
blk_part_imp(MACRO_PROT_ARGS)
{
	int		  la, c;
	char		 *p;
	struct mdoc_node *blk, *body, *n;

	/* If applicable, close out prior scopes. */

	if ( ! mdoc_block_alloc(m, line, ppos, tok, NULL))
		return(0);
	/* Saved for later close-out. */
	blk = m->last;
	if ( ! mdoc_head_alloc(m, line, ppos, tok))
		return(0);
	if ( ! rew_sub(MDOC_HEAD, m, tok, line, ppos))
		return(0);
	if ( ! mdoc_body_alloc(m, line, ppos, tok))
		return(0);
	/* Saved for later close-out. */
	body = m->last;

	/* Body argument processing. */

	for (;;) {
		la = *pos;
		c = mdoc_args(m, line, pos, buf, tok, &p);
		assert(ARGS_PHRASE != c);

		if (ARGS_ERROR == c)
			return(0);
		if (ARGS_PUNCT == c)
			break;
		if (ARGS_EOLN == c)
			break;

		if (MDOC_MAX == (c = lookup(tok, p))) {
			if ( ! mdoc_word_alloc(m, line, la, p))
				return(0);
			continue;
		} 

		if ( ! mdoc_macro(m, c, line, la, pos, buf))
			return(0);
		break;
	}

	/* 
	 * If we can't rewind to our body, then our scope has already
	 * been closed by another macro (like `Oc' closing `Op').  This
	 * is ugly behaviour nodding its head to OpenBSD's overwhelming
	 * crufty use of `Op' breakage--XXX, deprecate in time.
	 */
	for (n = m->last; n; n = n->parent)
		if (body == n)
			break;
	if (NULL == n && ! mdoc_nwarn(m, body, EIMPBRK))
		return(0);
	if (n && ! rew_last(m, body))
		return(0);

	/* Standard appending of delimiters. */

	if (1 == ppos && ! append_delims(m, line, pos, buf))
		return(0);

	/* Rewind scope, if applicable. */

	if (n && ! rew_last(m, blk))
		return(0);

	return(1);
}


static int
blk_part_exp(MACRO_PROT_ARGS)
{
	int		  la, flushed, j, c, maxargs;
	char		 *p;

	/* Number of head arguments.  Only `Eo' has these, */

	switch (tok) {
	case (MDOC_Eo):
		maxargs = 1;
		break;
	default:
		maxargs = 0;
		break;
	}

	/* Begin the block scope. */

	if ( ! mdoc_block_alloc(m, line, ppos, tok, NULL))
		return(0); 

	/* 
	 * If no head arguments, open and then close out a head, noting
	 * that we've flushed our terms.  `flushed' means that we've
	 * flushed out the head and the body is open.
	 */

	if (0 == maxargs) {
		if ( ! mdoc_head_alloc(m, line, ppos, tok))
			return(0);
		if ( ! rew_sub(MDOC_HEAD, m, tok, line, ppos))
			return(0);
		if ( ! mdoc_body_alloc(m, line, ppos, tok))
			return(0);
		flushed = 1;
	} else {
		if ( ! mdoc_head_alloc(m, line, ppos, tok))
			return(0);
		flushed = 0;
	}

	/* Process the head/head+body line arguments. */

	for (j = 0; ; j++) {
		la = *pos;
		if (j == maxargs && ! flushed) {
			if ( ! rew_sub(MDOC_HEAD, m, tok, line, ppos))
				return(0);
			flushed = 1;
			if ( ! mdoc_body_alloc(m, line, ppos, tok))
				return(0);
		}

		c = mdoc_args(m, line, pos, buf, tok, &p);
		assert(ARGS_PHRASE != c);

		if (ARGS_ERROR == c)
			return(0);
		if (ARGS_PUNCT == c)
			break;
		if (ARGS_EOLN == c)
			break;

		if (MDOC_MAX != (c = lookup(tok, p))) {
			if ( ! flushed) {
				if ( ! rew_sub(MDOC_HEAD, m, tok, line, ppos))
					return(0);
				flushed = 1;
				if ( ! mdoc_body_alloc(m, line, ppos, tok))
					return(0);
			}
			if ( ! mdoc_macro(m, c, line, la, pos, buf))
				return(0);
			break;
		}

		if ( ! flushed && mdoc_isdelim(p) > 1) {
			if ( ! rew_sub(MDOC_HEAD, m, tok, line, ppos))
				return(0);
			flushed = 1;
			if ( ! mdoc_body_alloc(m, line, ppos, tok))
				return(0);
		}
	
		if ( ! mdoc_word_alloc(m, line, la, p))
			return(0);
	}

	/* Close the head and open the body, if applicable. */

	if ( ! flushed) {
		if ( ! rew_sub(MDOC_HEAD, m, tok, line, ppos))
			return(0);
		if ( ! mdoc_body_alloc(m, line, ppos, tok))
			return(0);
	}

	/* Standard appending of delimiters. */

	if (ppos > 1)
		return(1);
	return(append_delims(m, line, pos, buf));
}


static int
in_line_argn(MACRO_PROT_ARGS)
{
	int		  la, flushed, j, c, maxargs;
	struct mdoc_arg	 *arg;
	char		 *p;

	/* Fixed maximum arguments per macro, if applicable. */

	switch (tok) {
	case (MDOC_Ap):
		/* FALLTHROUGH */
	case (MDOC_No):
		/* FALLTHROUGH */
	case (MDOC_Ns):
		/* FALLTHROUGH */
	case (MDOC_Ux):
		maxargs = 0;
		break;
	case (MDOC_Xr):
		maxargs = 2;
		break;
	default:
		maxargs = 1;
		break;
	}

	/* Macro argument processing. */

	for (arg = NULL;; ) {
		la = *pos;
		c = mdoc_argv(m, line, tok, &arg, pos, buf);

		if (ARGV_WORD == c) {
			*pos = la;
			break;
		} 

		if (ARGV_EOLN == c)
			break;
		if (ARGV_ARG == c)
			continue;

		mdoc_argv_free(arg);
		return(0);
	}

	/* Open the element scope. */

	if ( ! mdoc_elem_alloc(m, line, ppos, tok, arg))
		return(0);

	/* Process element arguments. */

	for (flushed = j = 0; ; j++) {
		la = *pos;

		if (j == maxargs && ! flushed) {
			if ( ! rew_elem(m, tok))
				return(0);
			flushed = 1;
		}

		c = mdoc_args(m, line, pos, buf, tok, &p);

		if (ARGS_ERROR == c)
			return(0);
		if (ARGS_PUNCT == c)
			break;
		if (ARGS_EOLN == c)
			break;

		if (MDOC_MAX != (c = lookup(tok, p))) {
			if ( ! flushed && ! rew_elem(m, tok))
				return(0);
			flushed = 1;
			if ( ! mdoc_macro(m, c, line, la, pos, buf))
				return(0);
			break;
		}

		if ( ! (MDOC_IGNDELIM & mdoc_macros[tok].flags) &&
				! flushed && mdoc_isdelim(p)) {
			if ( ! rew_elem(m, tok))
				return(0);
			flushed = 1;
		}

		/* 
		 * XXX: this is a hack to work around groff's ugliness
		 * as regards `Xr' and extraneous arguments.  It should
		 * ideally be deprecated behaviour, but because this is
		 * code is no here, it's unlikely to be removed.
		 */
		if (MDOC_Xr == tok && j == maxargs) {
			if ( ! mdoc_elem_alloc(m, line, ppos, MDOC_Ns, NULL))
				return(0);
			if ( ! rew_elem(m, MDOC_Ns))
				return(0);
		}

		if ( ! mdoc_word_alloc(m, line, la, p))
			return(0);
	}

	/* Close out and append delimiters. */

	if ( ! flushed && ! rew_elem(m, tok))
		return(0);

	if (ppos > 1)
		return(1);
	return(append_delims(m, line, pos, buf));
}


static int
in_line_eoln(MACRO_PROT_ARGS)
{
	int		  c, w, la;
	struct mdoc_arg	 *arg;
	char		 *p;

	assert( ! (MDOC_PARSED & mdoc_macros[tok].flags));

	/* Parse macro arguments. */

	for (arg = NULL; ; ) {
		la = *pos;
		c = mdoc_argv(m, line, tok, &arg, pos, buf);

		if (ARGV_WORD == c) {
			*pos = la;
			break;
		}
		if (ARGV_EOLN == c) 
			break;
		if (ARGV_ARG == c)
			continue;

		mdoc_argv_free(arg);
		return(0);
	}

	/* Open element scope. */

	if ( ! mdoc_elem_alloc(m, line, ppos, tok, arg))
		return(0);

	/* Parse argument terms. */

	for (;;) {
		la = *pos;
		w = mdoc_args(m, line, pos, buf, tok, &p);

		if (ARGS_ERROR == w)
			return(0);
		if (ARGS_EOLN == w)
			break;

		c = ARGS_QWORD == w ? MDOC_MAX : lookup(tok, p);

		if (MDOC_MAX != c) {
			if ( ! rew_elem(m, tok))
				return(0);
			return(mdoc_macro(m, c, line, la, pos, buf));
		} 

		if ( ! mdoc_word_alloc(m, line, la, p))
			return(0);
	}

	/* Close out (no delimiters). */

	return(rew_elem(m, tok));
}


/* ARGSUSED */
static int
ctx_synopsis(MACRO_PROT_ARGS)
{

	/* If we're not in the SYNOPSIS, go straight to in-line. */
	if (SEC_SYNOPSIS != m->lastsec)
		return(in_line(m, tok, line, ppos, pos, buf));

	/* If we're a nested call, same place. */
	if (ppos > 1)
		return(in_line(m, tok, line, ppos, pos, buf));

	/*
	 * XXX: this will open a block scope; however, if later we end
	 * up formatting the block scope, then child nodes will inherit
	 * the formatting.  Be careful.
	 */

	return(blk_part_imp(m, tok, line, ppos, pos, buf));
}


/* ARGSUSED */
static int
obsolete(MACRO_PROT_ARGS)
{

	return(mdoc_pwarn(m, line, ppos, EOBS));
}


/*
 * Phrases occur within `Bl -column' entries, separated by `Ta' or tabs.
 * They're unusual because they're basically free-form text until a
 * macro is encountered.
 */
static int
phrase(struct mdoc *m, int line, int ppos, char *buf)
{
	int		  c, w, la, pos;
	char		 *p;

	for (pos = ppos; ; ) {
		la = pos;

		/* Note: no calling context! */
		w = mdoc_zargs(m, line, &pos, buf, 0, &p);

		if (ARGS_ERROR == w)
			return(0);
		if (ARGS_EOLN == w)
			break;

		c = ARGS_QWORD == w ? MDOC_MAX : lookup_raw(p);

		if (MDOC_MAX != c) {
			if ( ! mdoc_macro(m, c, line, la, &pos, buf))
				return(0);
			return(append_delims(m, line, &pos, buf));
		} 

		if ( ! mdoc_word_alloc(m, line, la, p))
			return(0);
	}

	return(1);
}


