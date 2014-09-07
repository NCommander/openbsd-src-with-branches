/*	$OpenBSD$ */
/*
 * Copyright (c) 2008-2012 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2010-2014 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2010 Joerg Sonnenberger <joerg@netbsd.org>
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

#include <sys/types.h>
#ifndef OSNAME
#include <sys/utsname.h>
#endif

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mdoc.h"
#include "mandoc.h"
#include "mandoc_aux.h"
#include "libmdoc.h"
#include "libmandoc.h"

/* FIXME: .Bl -diag can't have non-text children in HEAD. */

#define	PRE_ARGS  struct mdoc *mdoc, struct mdoc_node *n
#define	POST_ARGS struct mdoc *mdoc

enum	check_ineq {
	CHECK_LT,
	CHECK_GT,
	CHECK_EQ
};

enum	check_lvl {
	CHECK_WARN,
	CHECK_ERROR,
};

typedef	int	(*v_pre)(PRE_ARGS);
typedef	int	(*v_post)(POST_ARGS);

struct	valids {
	v_pre	 pre;
	v_post	 post;
};

static	int	 check_count(struct mdoc *, enum mdoc_type,
			enum check_lvl, enum check_ineq, int);
static	void	 check_text(struct mdoc *, int, int, char *);
static	void	 check_argv(struct mdoc *,
			struct mdoc_node *, struct mdoc_argv *);
static	void	 check_args(struct mdoc *, struct mdoc_node *);
static	enum mdoc_sec	a2sec(const char *);
static	size_t		macro2len(enum mdoct);

static	int	 ebool(POST_ARGS);
static	int	 berr_ge1(POST_ARGS);
static	int	 bwarn_ge1(POST_ARGS);
static	int	 ewarn_eq0(POST_ARGS);
static	int	 ewarn_eq1(POST_ARGS);
static	int	 ewarn_ge1(POST_ARGS);
static	int	 ewarn_le1(POST_ARGS);
static	int	 hwarn_eq0(POST_ARGS);
static	int	 hwarn_eq1(POST_ARGS);
static	int	 hwarn_ge1(POST_ARGS);

static	int	 post_an(POST_ARGS);
static	int	 post_at(POST_ARGS);
static	int	 post_bf(POST_ARGS);
static	int	 post_bk(POST_ARGS);
static	int	 post_bl(POST_ARGS);
static	int	 post_bl_block(POST_ARGS);
static	int	 post_bl_block_width(POST_ARGS);
static	int	 post_bl_block_tag(POST_ARGS);
static	int	 post_bl_head(POST_ARGS);
static	int	 post_bx(POST_ARGS);
static	int	 post_d1(POST_ARGS);
static	int	 post_defaults(POST_ARGS);
static	int	 post_dd(POST_ARGS);
static	int	 post_dt(POST_ARGS);
static	int	 post_en(POST_ARGS);
static	int	 post_es(POST_ARGS);
static	int	 post_eoln(POST_ARGS);
static	int	 post_ex(POST_ARGS);
static	int	 post_fo(POST_ARGS);
static	int	 post_hyph(POST_ARGS);
static	int	 post_hyphtext(POST_ARGS);
static	int	 post_ignpar(POST_ARGS);
static	int	 post_it(POST_ARGS);
static	int	 post_lb(POST_ARGS);
static	int	 post_literal(POST_ARGS);
static	int	 post_nd(POST_ARGS);
static	int	 post_nm(POST_ARGS);
static	int	 post_ns(POST_ARGS);
static	int	 post_os(POST_ARGS);
static	int	 post_par(POST_ARGS);
static	int	 post_root(POST_ARGS);
static	int	 post_rs(POST_ARGS);
static	int	 post_sh(POST_ARGS);
static	int	 post_sh_body(POST_ARGS);
static	int	 post_sh_head(POST_ARGS);
static	int	 post_st(POST_ARGS);
static	int	 post_vt(POST_ARGS);
static	int	 pre_an(PRE_ARGS);
static	int	 pre_bd(PRE_ARGS);
static	int	 pre_bl(PRE_ARGS);
static	int	 pre_dd(PRE_ARGS);
static	int	 pre_display(PRE_ARGS);
static	int	 pre_dt(PRE_ARGS);
static	int	 pre_literal(PRE_ARGS);
static	int	 pre_obsolete(PRE_ARGS);
static	int	 pre_os(PRE_ARGS);
static	int	 pre_par(PRE_ARGS);
static	int	 pre_std(PRE_ARGS);

static	const struct valids mdoc_valids[MDOC_MAX] = {
	{ NULL, NULL },				/* Ap */
	{ pre_dd, post_dd },			/* Dd */
	{ pre_dt, post_dt },			/* Dt */
	{ pre_os, post_os },			/* Os */
	{ NULL, post_sh },			/* Sh */
	{ NULL, post_ignpar },			/* Ss */
	{ pre_par, post_par },			/* Pp */
	{ pre_display, post_d1 },		/* D1 */
	{ pre_literal, post_literal },		/* Dl */
	{ pre_bd, post_literal },		/* Bd */
	{ NULL, NULL },				/* Ed */
	{ pre_bl, post_bl },			/* Bl */
	{ NULL, NULL },				/* El */
	{ pre_par, post_it },			/* It */
	{ NULL, NULL },				/* Ad */
	{ pre_an, post_an },			/* An */
	{ NULL, post_defaults },		/* Ar */
	{ NULL, NULL },				/* Cd */
	{ NULL, NULL },				/* Cm */
	{ NULL, NULL },				/* Dv */
	{ NULL, NULL },				/* Er */
	{ NULL, NULL },				/* Ev */
	{ pre_std, post_ex },			/* Ex */
	{ NULL, NULL },				/* Fa */
	{ NULL, ewarn_ge1 },			/* Fd */
	{ NULL, NULL },				/* Fl */
	{ NULL, NULL },				/* Fn */
	{ NULL, NULL },				/* Ft */
	{ NULL, NULL },				/* Ic */
	{ NULL, ewarn_eq1 },			/* In */
	{ NULL, post_defaults },		/* Li */
	{ NULL, post_nd },			/* Nd */
	{ NULL, post_nm },			/* Nm */
	{ NULL, NULL },				/* Op */
	{ pre_obsolete, NULL },			/* Ot */
	{ NULL, post_defaults },		/* Pa */
	{ pre_std, NULL },			/* Rv */
	{ NULL, post_st },			/* St */
	{ NULL, NULL },				/* Va */
	{ NULL, post_vt },			/* Vt */
	{ NULL, ewarn_ge1 },			/* Xr */
	{ NULL, ewarn_ge1 },			/* %A */
	{ NULL, post_hyphtext },		/* %B */ /* FIXME: can be used outside Rs/Re. */
	{ NULL, ewarn_ge1 },			/* %D */
	{ NULL, ewarn_ge1 },			/* %I */
	{ NULL, ewarn_ge1 },			/* %J */
	{ NULL, post_hyphtext },		/* %N */
	{ NULL, post_hyphtext },		/* %O */
	{ NULL, ewarn_ge1 },			/* %P */
	{ NULL, post_hyphtext },		/* %R */
	{ NULL, post_hyphtext },		/* %T */ /* FIXME: can be used outside Rs/Re. */
	{ NULL, ewarn_ge1 },			/* %V */
	{ NULL, NULL },				/* Ac */
	{ NULL, NULL },				/* Ao */
	{ NULL, NULL },				/* Aq */
	{ NULL, post_at },			/* At */
	{ NULL, NULL },				/* Bc */
	{ NULL, post_bf },			/* Bf */
	{ NULL, NULL },				/* Bo */
	{ NULL, NULL },				/* Bq */
	{ NULL, NULL },				/* Bsx */
	{ NULL, post_bx },			/* Bx */
	{ NULL, ebool },			/* Db */
	{ NULL, NULL },				/* Dc */
	{ NULL, NULL },				/* Do */
	{ NULL, NULL },				/* Dq */
	{ NULL, NULL },				/* Ec */
	{ NULL, NULL },				/* Ef */
	{ NULL, NULL },				/* Em */
	{ NULL, NULL },				/* Eo */
	{ NULL, NULL },				/* Fx */
	{ NULL, NULL },				/* Ms */
	{ NULL, ewarn_eq0 },			/* No */
	{ NULL, post_ns },			/* Ns */
	{ NULL, NULL },				/* Nx */
	{ NULL, NULL },				/* Ox */
	{ NULL, NULL },				/* Pc */
	{ NULL, ewarn_eq1 },			/* Pf */
	{ NULL, NULL },				/* Po */
	{ NULL, NULL },				/* Pq */
	{ NULL, NULL },				/* Qc */
	{ NULL, NULL },				/* Ql */
	{ NULL, NULL },				/* Qo */
	{ NULL, NULL },				/* Qq */
	{ NULL, NULL },				/* Re */
	{ NULL, post_rs },			/* Rs */
	{ NULL, NULL },				/* Sc */
	{ NULL, NULL },				/* So */
	{ NULL, NULL },				/* Sq */
	{ NULL, ebool },			/* Sm */
	{ NULL, post_hyph },			/* Sx */
	{ NULL, NULL },				/* Sy */
	{ NULL, NULL },				/* Tn */
	{ NULL, NULL },				/* Ux */
	{ NULL, NULL },				/* Xc */
	{ NULL, NULL },				/* Xo */
	{ NULL, post_fo },			/* Fo */
	{ NULL, NULL },				/* Fc */
	{ NULL, NULL },				/* Oo */
	{ NULL, NULL },				/* Oc */
	{ NULL, post_bk },			/* Bk */
	{ NULL, NULL },				/* Ek */
	{ NULL, post_eoln },			/* Bt */
	{ NULL, NULL },				/* Hf */
	{ pre_obsolete, NULL },			/* Fr */
	{ NULL, post_eoln },			/* Ud */
	{ NULL, post_lb },			/* Lb */
	{ pre_par, post_par },			/* Lp */
	{ NULL, NULL },				/* Lk */
	{ NULL, post_defaults },		/* Mt */
	{ NULL, NULL },				/* Brq */
	{ NULL, NULL },				/* Bro */
	{ NULL, NULL },				/* Brc */
	{ NULL, ewarn_ge1 },			/* %C */
	{ pre_obsolete, post_es },		/* Es */
	{ pre_obsolete, post_en },		/* En */
	{ NULL, NULL },				/* Dx */
	{ NULL, ewarn_ge1 },			/* %Q */
	{ NULL, post_par },			/* br */
	{ NULL, post_par },			/* sp */
	{ NULL, ewarn_eq1 },			/* %U */
	{ NULL, NULL },				/* Ta */
	{ NULL, NULL },				/* ll */
};

#define	RSORD_MAX 14 /* Number of `Rs' blocks. */

static	const enum mdoct rsord[RSORD_MAX] = {
	MDOC__A,
	MDOC__T,
	MDOC__B,
	MDOC__I,
	MDOC__J,
	MDOC__R,
	MDOC__N,
	MDOC__V,
	MDOC__U,
	MDOC__P,
	MDOC__Q,
	MDOC__C,
	MDOC__D,
	MDOC__O
};

static	const char * const secnames[SEC__MAX] = {
	NULL,
	"NAME",
	"LIBRARY",
	"SYNOPSIS",
	"DESCRIPTION",
	"CONTEXT",
	"IMPLEMENTATION NOTES",
	"RETURN VALUES",
	"ENVIRONMENT",
	"FILES",
	"EXIT STATUS",
	"EXAMPLES",
	"DIAGNOSTICS",
	"COMPATIBILITY",
	"ERRORS",
	"SEE ALSO",
	"STANDARDS",
	"HISTORY",
	"AUTHORS",
	"CAVEATS",
	"BUGS",
	"SECURITY CONSIDERATIONS",
	NULL
};


int
mdoc_valid_pre(struct mdoc *mdoc, struct mdoc_node *n)
{
	v_pre	 p;

	switch (n->type) {
	case MDOC_TEXT:
		check_text(mdoc, n->line, n->pos, n->string);
		/* FALLTHROUGH */
	case MDOC_TBL:
		/* FALLTHROUGH */
	case MDOC_EQN:
		/* FALLTHROUGH */
	case MDOC_ROOT:
		return(1);
	default:
		break;
	}

	check_args(mdoc, n);
	p = mdoc_valids[n->tok].pre;
	return(*p ? (*p)(mdoc, n) : 1);
}

int
mdoc_valid_post(struct mdoc *mdoc)
{
	struct mdoc_node *n;
	v_post p;

	n = mdoc->last;
	if (n->flags & MDOC_VALID)
		return(1);
	n->flags |= MDOC_VALID;

	switch (n->type) {
	case MDOC_TEXT:
		/* FALLTHROUGH */
	case MDOC_EQN:
		/* FALLTHROUGH */
	case MDOC_TBL:
		return(1);
	case MDOC_ROOT:
		return(post_root(mdoc));
	default:
		p = mdoc_valids[n->tok].post;
		return(*p ? (*p)(mdoc) : 1);
	}
}

static int
check_count(struct mdoc *mdoc, enum mdoc_type type,
		enum check_lvl lvl, enum check_ineq ineq, int val)
{
	const char	*p;
	enum mandocerr	 t;

	if (mdoc->last->type != type)
		return(1);

	switch (ineq) {
	case CHECK_LT:
		p = "less than ";
		if (mdoc->last->nchild < val)
			return(1);
		break;
	case CHECK_GT:
		p = "more than ";
		if (mdoc->last->nchild > val)
			return(1);
		break;
	case CHECK_EQ:
		p = "";
		if (val == mdoc->last->nchild)
			return(1);
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	t = lvl == CHECK_WARN ? MANDOCERR_ARGCWARN : MANDOCERR_ARGCOUNT;
	mandoc_vmsg(t, mdoc->parse, mdoc->last->line,
	    mdoc->last->pos, "want %s%d children (have %d)",
	    p, val, mdoc->last->nchild);
	return(1);
}

static int
berr_ge1(POST_ARGS)
{

	return(check_count(mdoc, MDOC_BODY, CHECK_ERROR, CHECK_GT, 0));
}

static int
bwarn_ge1(POST_ARGS)
{
	return(check_count(mdoc, MDOC_BODY, CHECK_WARN, CHECK_GT, 0));
}

static int
ewarn_eq0(POST_ARGS)
{
	return(check_count(mdoc, MDOC_ELEM, CHECK_WARN, CHECK_EQ, 0));
}

static int
ewarn_eq1(POST_ARGS)
{
	return(check_count(mdoc, MDOC_ELEM, CHECK_WARN, CHECK_EQ, 1));
}

static int
ewarn_ge1(POST_ARGS)
{
	return(check_count(mdoc, MDOC_ELEM, CHECK_WARN, CHECK_GT, 0));
}

static int
ewarn_le1(POST_ARGS)
{
	return(check_count(mdoc, MDOC_ELEM, CHECK_WARN, CHECK_LT, 2));
}

static int
hwarn_eq0(POST_ARGS)
{
	return(check_count(mdoc, MDOC_HEAD, CHECK_WARN, CHECK_EQ, 0));
}

static int
hwarn_eq1(POST_ARGS)
{
	return(check_count(mdoc, MDOC_HEAD, CHECK_WARN, CHECK_EQ, 1));
}

static int
hwarn_ge1(POST_ARGS)
{
	return(check_count(mdoc, MDOC_HEAD, CHECK_WARN, CHECK_GT, 0));
}

static void
check_args(struct mdoc *mdoc, struct mdoc_node *n)
{
	int		 i;

	if (NULL == n->args)
		return;

	assert(n->args->argc);
	for (i = 0; i < (int)n->args->argc; i++)
		check_argv(mdoc, n, &n->args->argv[i]);
}

static void
check_argv(struct mdoc *mdoc, struct mdoc_node *n, struct mdoc_argv *v)
{
	int		 i;

	for (i = 0; i < (int)v->sz; i++)
		check_text(mdoc, v->line, v->pos, v->value[i]);
}

static void
check_text(struct mdoc *mdoc, int ln, int pos, char *p)
{
	char		*cp;

	if (MDOC_LITERAL & mdoc->flags)
		return;

	for (cp = p; NULL != (p = strchr(p, '\t')); p++)
		mandoc_msg(MANDOCERR_FI_TAB, mdoc->parse,
		    ln, pos + (int)(p - cp), NULL);
}

static int
pre_display(PRE_ARGS)
{
	struct mdoc_node *node;

	if (MDOC_BLOCK != n->type)
		return(1);

	for (node = mdoc->last->parent; node; node = node->parent)
		if (MDOC_BLOCK == node->type)
			if (MDOC_Bd == node->tok)
				break;

	if (node)
		mandoc_vmsg(MANDOCERR_BD_NEST,
		    mdoc->parse, n->line, n->pos,
		    "%s in Bd", mdoc_macronames[n->tok]);

	return(1);
}

static int
pre_bl(PRE_ARGS)
{
	struct mdoc_node *np;
	struct mdoc_argv *argv, *wa;
	int		  i;
	enum mdocargt	  mdoclt;
	enum mdoc_list	  lt;

	if (MDOC_BLOCK != n->type) {
		if (ENDBODY_NOT != n->end) {
			assert(n->pending);
			np = n->pending->parent;
		} else
			np = n->parent;

		assert(np);
		assert(MDOC_BLOCK == np->type);
		assert(MDOC_Bl == np->tok);
		return(1);
	}

	/*
	 * First figure out which kind of list to use: bind ourselves to
	 * the first mentioned list type and warn about any remaining
	 * ones.  If we find no list type, we default to LIST_item.
	 */

	wa = (n->args == NULL) ? NULL : n->args->argv;
	mdoclt = MDOC_ARG_MAX;
	for (i = 0; n->args && i < (int)n->args->argc; i++) {
		argv = n->args->argv + i;
		lt = LIST__NONE;
		switch (argv->arg) {
		/* Set list types. */
		case MDOC_Bullet:
			lt = LIST_bullet;
			break;
		case MDOC_Dash:
			lt = LIST_dash;
			break;
		case MDOC_Enum:
			lt = LIST_enum;
			break;
		case MDOC_Hyphen:
			lt = LIST_hyphen;
			break;
		case MDOC_Item:
			lt = LIST_item;
			break;
		case MDOC_Tag:
			lt = LIST_tag;
			break;
		case MDOC_Diag:
			lt = LIST_diag;
			break;
		case MDOC_Hang:
			lt = LIST_hang;
			break;
		case MDOC_Ohang:
			lt = LIST_ohang;
			break;
		case MDOC_Inset:
			lt = LIST_inset;
			break;
		case MDOC_Column:
			lt = LIST_column;
			break;
		/* Set list arguments. */
		case MDOC_Compact:
			if (n->norm->Bl.comp)
				mandoc_msg(MANDOCERR_ARG_REP,
				    mdoc->parse, argv->line,
				    argv->pos, "Bl -compact");
			n->norm->Bl.comp = 1;
			break;
		case MDOC_Width:
			wa = argv;
			if (0 == argv->sz) {
				mandoc_msg(MANDOCERR_ARG_EMPTY,
				    mdoc->parse, argv->line,
				    argv->pos, "Bl -width");
				n->norm->Bl.width = "0n";
				break;
			}
			if (NULL != n->norm->Bl.width)
				mandoc_vmsg(MANDOCERR_ARG_REP,
				    mdoc->parse, argv->line,
				    argv->pos, "Bl -width %s",
				    argv->value[0]);
			n->norm->Bl.width = argv->value[0];
			break;
		case MDOC_Offset:
			if (0 == argv->sz) {
				mandoc_msg(MANDOCERR_ARG_EMPTY,
				    mdoc->parse, argv->line,
				    argv->pos, "Bl -offset");
				break;
			}
			if (NULL != n->norm->Bl.offs)
				mandoc_vmsg(MANDOCERR_ARG_REP,
				    mdoc->parse, argv->line,
				    argv->pos, "Bl -offset %s",
				    argv->value[0]);
			n->norm->Bl.offs = argv->value[0];
			break;
		default:
			continue;
		}
		if (LIST__NONE == lt)
			continue;
		mdoclt = argv->arg;

		/* Check: multiple list types. */

		if (LIST__NONE != n->norm->Bl.type) {
			mandoc_vmsg(MANDOCERR_BL_REP,
			    mdoc->parse, n->line, n->pos,
			    "Bl -%s", mdoc_argnames[argv->arg]);
			continue;
		}

		/* The list type should come first. */

		if (n->norm->Bl.width ||
		    n->norm->Bl.offs ||
		    n->norm->Bl.comp)
			mandoc_vmsg(MANDOCERR_BL_LATETYPE,
			    mdoc->parse, n->line, n->pos, "Bl -%s",
			    mdoc_argnames[n->args->argv[0].arg]);

		n->norm->Bl.type = lt;
		if (LIST_column == lt) {
			n->norm->Bl.ncols = argv->sz;
			n->norm->Bl.cols = (void *)argv->value;
		}
	}

	/* Allow lists to default to LIST_item. */

	if (LIST__NONE == n->norm->Bl.type) {
		mandoc_msg(MANDOCERR_BL_NOTYPE, mdoc->parse,
		    n->line, n->pos, "Bl");
		n->norm->Bl.type = LIST_item;
	}

	/*
	 * Validate the width field.  Some list types don't need width
	 * types and should be warned about them.  Others should have it
	 * and must also be warned.  Yet others have a default and need
	 * no warning.
	 */

	switch (n->norm->Bl.type) {
	case LIST_tag:
		if (NULL == n->norm->Bl.width)
			mandoc_msg(MANDOCERR_BL_NOWIDTH, mdoc->parse,
			    n->line, n->pos, "Bl -tag");
		break;
	case LIST_column:
		/* FALLTHROUGH */
	case LIST_diag:
		/* FALLTHROUGH */
	case LIST_ohang:
		/* FALLTHROUGH */
	case LIST_inset:
		/* FALLTHROUGH */
	case LIST_item:
		if (n->norm->Bl.width)
			mandoc_vmsg(MANDOCERR_BL_SKIPW, mdoc->parse,
			    wa->line, wa->pos, "Bl -%s",
			    mdoc_argnames[mdoclt]);
		break;
	case LIST_bullet:
		/* FALLTHROUGH */
	case LIST_dash:
		/* FALLTHROUGH */
	case LIST_hyphen:
		if (NULL == n->norm->Bl.width)
			n->norm->Bl.width = "2n";
		break;
	case LIST_enum:
		if (NULL == n->norm->Bl.width)
			n->norm->Bl.width = "3n";
		break;
	default:
		break;
	}

	return(pre_par(mdoc, n));
}

static int
pre_bd(PRE_ARGS)
{
	struct mdoc_node *np;
	struct mdoc_argv *argv;
	int		  i;
	enum mdoc_disp	  dt;

	pre_literal(mdoc, n);

	if (MDOC_BLOCK != n->type) {
		if (ENDBODY_NOT != n->end) {
			assert(n->pending);
			np = n->pending->parent;
		} else
			np = n->parent;

		assert(np);
		assert(MDOC_BLOCK == np->type);
		assert(MDOC_Bd == np->tok);
		return(1);
	}

	for (i = 0; n->args && i < (int)n->args->argc; i++) {
		argv = n->args->argv + i;
		dt = DISP__NONE;

		switch (argv->arg) {
		case MDOC_Centred:
			dt = DISP_centered;
			break;
		case MDOC_Ragged:
			dt = DISP_ragged;
			break;
		case MDOC_Unfilled:
			dt = DISP_unfilled;
			break;
		case MDOC_Filled:
			dt = DISP_filled;
			break;
		case MDOC_Literal:
			dt = DISP_literal;
			break;
		case MDOC_File:
			mandoc_msg(MANDOCERR_BD_FILE, mdoc->parse,
			    n->line, n->pos, NULL);
			return(0);
		case MDOC_Offset:
			if (0 == argv->sz) {
				mandoc_msg(MANDOCERR_ARG_EMPTY,
				    mdoc->parse, argv->line,
				    argv->pos, "Bd -offset");
				break;
			}
			if (NULL != n->norm->Bd.offs)
				mandoc_vmsg(MANDOCERR_ARG_REP,
				    mdoc->parse, argv->line,
				    argv->pos, "Bd -offset %s",
				    argv->value[0]);
			n->norm->Bd.offs = argv->value[0];
			break;
		case MDOC_Compact:
			if (n->norm->Bd.comp)
				mandoc_msg(MANDOCERR_ARG_REP,
				    mdoc->parse, argv->line,
				    argv->pos, "Bd -compact");
			n->norm->Bd.comp = 1;
			break;
		default:
			abort();
			/* NOTREACHED */
		}
		if (DISP__NONE == dt)
			continue;

		if (DISP__NONE == n->norm->Bd.type)
			n->norm->Bd.type = dt;
		else
			mandoc_vmsg(MANDOCERR_BD_REP,
			    mdoc->parse, n->line, n->pos,
			    "Bd -%s", mdoc_argnames[argv->arg]);
	}

	if (DISP__NONE == n->norm->Bd.type) {
		mandoc_msg(MANDOCERR_BD_NOTYPE, mdoc->parse,
		    n->line, n->pos, "Bd");
		n->norm->Bd.type = DISP_ragged;
	}

	return(pre_par(mdoc, n));
}

static int
pre_an(PRE_ARGS)
{
	struct mdoc_argv *argv;
	size_t	 i;

	if (n->args == NULL)
		return(1);

	for (i = 1; i < n->args->argc; i++) {
		argv = n->args->argv + i;
		mandoc_vmsg(MANDOCERR_AN_REP,
		    mdoc->parse, argv->line, argv->pos,
		    "An -%s", mdoc_argnames[argv->arg]);
	}

	argv = n->args->argv;
	if (argv->arg == MDOC_Split)
		n->norm->An.auth = AUTH_split;
	else if (argv->arg == MDOC_Nosplit)
		n->norm->An.auth = AUTH_nosplit;
	else
		abort();

	return(1);
}

static int
pre_std(PRE_ARGS)
{

	if (n->args && 1 == n->args->argc)
		if (MDOC_Std == n->args->argv[0].arg)
			return(1);

	mandoc_msg(MANDOCERR_ARG_STD, mdoc->parse,
	    n->line, n->pos, mdoc_macronames[n->tok]);
	return(1);
}

static int
pre_obsolete(PRE_ARGS)
{

	if (MDOC_ELEM == n->type || MDOC_BLOCK == n->type)
		mandoc_msg(MANDOCERR_MACRO_OBS, mdoc->parse,
		    n->line, n->pos, mdoc_macronames[n->tok]);
	return(1);
}

static int
pre_dt(PRE_ARGS)
{

	if (mdoc->meta.title != NULL)
		mandoc_msg(MANDOCERR_PROLOG_REP, mdoc->parse,
		    n->line, n->pos, "Dt");
	else if (mdoc->meta.os != NULL)
		mandoc_msg(MANDOCERR_PROLOG_ORDER, mdoc->parse,
		    n->line, n->pos, "Dt after Os");
	return(1);
}

static int
pre_os(PRE_ARGS)
{

	if (mdoc->meta.os != NULL)
		mandoc_msg(MANDOCERR_PROLOG_REP, mdoc->parse,
		    n->line, n->pos, "Os");
	else if (mdoc->flags & MDOC_PBODY)
		mandoc_msg(MANDOCERR_PROLOG_LATE, mdoc->parse,
		    n->line, n->pos, "Os");
	return(1);
}

static int
pre_dd(PRE_ARGS)
{

	if (mdoc->meta.date != NULL)
		mandoc_msg(MANDOCERR_PROLOG_REP, mdoc->parse,
		    n->line, n->pos, "Dd");
	else if (mdoc->flags & MDOC_PBODY)
		mandoc_msg(MANDOCERR_PROLOG_LATE, mdoc->parse,
		    n->line, n->pos, "Dd");
	else if (mdoc->meta.title != NULL)
		mandoc_msg(MANDOCERR_PROLOG_ORDER, mdoc->parse,
		    n->line, n->pos, "Dd after Dt");
	else if (mdoc->meta.os != NULL)
		mandoc_msg(MANDOCERR_PROLOG_ORDER, mdoc->parse,
		    n->line, n->pos, "Dd after Os");
	return(1);
}

static int
post_bf(POST_ARGS)
{
	struct mdoc_node *np, *nch;
	enum mdocargt	  arg;

	/*
	 * Unlike other data pointers, these are "housed" by the HEAD
	 * element, which contains the goods.
	 */

	if (MDOC_HEAD != mdoc->last->type) {
		if (ENDBODY_NOT != mdoc->last->end) {
			assert(mdoc->last->pending);
			np = mdoc->last->pending->parent->head;
		} else if (MDOC_BLOCK != mdoc->last->type) {
			np = mdoc->last->parent->head;
		} else
			np = mdoc->last->head;

		assert(np);
		assert(MDOC_HEAD == np->type);
		assert(MDOC_Bf == np->tok);
		return(1);
	}

	np = mdoc->last;
	assert(MDOC_BLOCK == np->parent->type);
	assert(MDOC_Bf == np->parent->tok);

	/* Check the number of arguments. */

	nch = np->child;
	if (NULL == np->parent->args) {
		if (NULL == nch) {
			mandoc_msg(MANDOCERR_BF_NOFONT, mdoc->parse,
			    np->line, np->pos, "Bf");
			return(1);
		}
		nch = nch->next;
	}
	if (NULL != nch)
		mandoc_vmsg(MANDOCERR_ARG_EXCESS, mdoc->parse,
		    nch->line, nch->pos, "Bf ... %s", nch->string);

	/* Extract argument into data. */

	if (np->parent->args) {
		arg = np->parent->args->argv[0].arg;
		if (MDOC_Emphasis == arg)
			np->norm->Bf.font = FONT_Em;
		else if (MDOC_Literal == arg)
			np->norm->Bf.font = FONT_Li;
		else if (MDOC_Symbolic == arg)
			np->norm->Bf.font = FONT_Sy;
		else
			abort();
		return(1);
	}

	/* Extract parameter into data. */

	if (0 == strcmp(np->child->string, "Em"))
		np->norm->Bf.font = FONT_Em;
	else if (0 == strcmp(np->child->string, "Li"))
		np->norm->Bf.font = FONT_Li;
	else if (0 == strcmp(np->child->string, "Sy"))
		np->norm->Bf.font = FONT_Sy;
	else
		mandoc_vmsg(MANDOCERR_BF_BADFONT, mdoc->parse,
		    np->child->line, np->child->pos,
		    "Bf %s", np->child->string);

	return(1);
}

static int
post_lb(POST_ARGS)
{
	struct mdoc_node	*n;
	const char		*stdlibname;
	char			*libname;

	check_count(mdoc, MDOC_ELEM, CHECK_WARN, CHECK_EQ, 1);

	n = mdoc->last->child;

	assert(n);
	assert(MDOC_TEXT == n->type);

	if (NULL == (stdlibname = mdoc_a2lib(n->string)))
		mandoc_asprintf(&libname,
		    "library \\(lq%s\\(rq", n->string);
	else
		libname = mandoc_strdup(stdlibname);

	free(n->string);
	n->string = libname;
	return(1);
}

static int
post_eoln(POST_ARGS)
{
	const struct mdoc_node *n;

	n = mdoc->last;
	if (n->child)
		mandoc_vmsg(MANDOCERR_ARG_SKIP,
		    mdoc->parse, n->line, n->pos,
		    "%s %s", mdoc_macronames[n->tok],
		    n->child->string);
	return(1);
}

static int
post_fo(POST_ARGS)
{

	hwarn_eq1(mdoc);
	bwarn_ge1(mdoc);
	return(1);
}

static int
post_vt(POST_ARGS)
{
	const struct mdoc_node *n;

	/*
	 * The Vt macro comes in both ELEM and BLOCK form, both of which
	 * have different syntaxes (yet more context-sensitive
	 * behaviour).  ELEM types must have a child, which is already
	 * guaranteed by the in_line parsing routine; BLOCK types,
	 * specifically the BODY, should only have TEXT children.
	 */

	if (MDOC_BODY != mdoc->last->type)
		return(1);

	for (n = mdoc->last->child; n; n = n->next)
		if (MDOC_TEXT != n->type)
			mandoc_msg(MANDOCERR_VT_CHILD, mdoc->parse,
			    n->line, n->pos, mdoc_macronames[n->tok]);

	return(1);
}

static int
post_nm(POST_ARGS)
{

	if (NULL != mdoc->meta.name)
		return(1);

	mdoc_deroff(&mdoc->meta.name, mdoc->last);

	if (NULL == mdoc->meta.name)
		mandoc_msg(MANDOCERR_NM_NONAME, mdoc->parse,
		    mdoc->last->line, mdoc->last->pos, "Nm");
	return(1);
}

static int
post_nd(POST_ARGS)
{

	berr_ge1(mdoc);
	return(post_hyph(mdoc));
}

static int
post_d1(POST_ARGS)
{

	bwarn_ge1(mdoc);
	return(post_hyph(mdoc));
}

static int
post_literal(POST_ARGS)
{

	if (mdoc->last->tok == MDOC_Bd)
		hwarn_eq0(mdoc);
	bwarn_ge1(mdoc);

	/*
	 * The `Dl' (note "el" not "one") and `Bd' macros unset the
	 * MDOC_LITERAL flag as they leave.  Note that `Bd' only sets
	 * this in literal mode, but it doesn't hurt to just switch it
	 * off in general since displays can't be nested.
	 */

	if (MDOC_BODY == mdoc->last->type)
		mdoc->flags &= ~MDOC_LITERAL;

	return(1);
}

static int
post_defaults(POST_ARGS)
{
	struct mdoc_node *nn;

	/*
	 * The `Ar' defaults to "file ..." if no value is provided as an
	 * argument; the `Mt' and `Pa' macros use "~"; the `Li' just
	 * gets an empty string.
	 */

	if (mdoc->last->child)
		return(1);

	nn = mdoc->last;
	mdoc->next = MDOC_NEXT_CHILD;

	switch (nn->tok) {
	case MDOC_Ar:
		if ( ! mdoc_word_alloc(mdoc, nn->line, nn->pos, "file"))
			return(0);
		if ( ! mdoc_word_alloc(mdoc, nn->line, nn->pos, "..."))
			return(0);
		break;
	case MDOC_Li:
		if ( ! mdoc_word_alloc(mdoc, nn->line, nn->pos, ""))
			return(0);
		break;
	case MDOC_Pa:
		/* FALLTHROUGH */
	case MDOC_Mt:
		if ( ! mdoc_word_alloc(mdoc, nn->line, nn->pos, "~"))
			return(0);
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	mdoc->last = nn;
	return(1);
}

static int
post_at(POST_ARGS)
{
	struct mdoc_node	*n;
	const char		*std_att;
	char			*att;

	n = mdoc->last;
	if (n->child == NULL) {
		mdoc->next = MDOC_NEXT_CHILD;
		if ( ! mdoc_word_alloc(mdoc, n->line, n->pos, "AT&T UNIX"))
			return(0);
		mdoc->last = n;
		return(1);
	}

	/*
	 * If we have a child, look it up in the standard keys.  If a
	 * key exist, use that instead of the child; if it doesn't,
	 * prefix "AT&T UNIX " to the existing data.
	 */

	n = n->child;
	assert(MDOC_TEXT == n->type);
	if (NULL == (std_att = mdoc_a2att(n->string))) {
		mandoc_vmsg(MANDOCERR_AT_BAD, mdoc->parse,
		    n->line, n->pos, "At %s", n->string);
		mandoc_asprintf(&att, "AT&T UNIX %s", n->string);
	} else
		att = mandoc_strdup(std_att);

	free(n->string);
	n->string = att;
	return(1);
}

static int
post_an(POST_ARGS)
{
	struct mdoc_node *np;

	np = mdoc->last;
	if (AUTH__NONE == np->norm->An.auth) {
		if (0 == np->child)
			check_count(mdoc, MDOC_ELEM, CHECK_WARN, CHECK_GT, 0);
	} else if (np->child)
		check_count(mdoc, MDOC_ELEM, CHECK_WARN, CHECK_EQ, 0);

	return(1);
}

static int
post_en(POST_ARGS)
{

	if (MDOC_BLOCK == mdoc->last->type)
		mdoc->last->norm->Es = mdoc->last_es;
	return(1);
}

static int
post_es(POST_ARGS)
{

	mdoc->last_es = mdoc->last;
	return(1);
}

static int
post_it(POST_ARGS)
{
	int		  i, cols;
	enum mdoc_list	  lt;
	struct mdoc_node *nbl, *nit, *nch;

	nit = mdoc->last;
	if (MDOC_BLOCK != nit->type)
		return(1);

	nbl = nit->parent->parent;
	lt = nbl->norm->Bl.type;

	switch (lt) {
	case LIST_tag:
		/* FALLTHROUGH */
	case LIST_hang:
		/* FALLTHROUGH */
	case LIST_ohang:
		/* FALLTHROUGH */
	case LIST_inset:
		/* FALLTHROUGH */
	case LIST_diag:
		if (NULL == nit->head->child)
			mandoc_vmsg(MANDOCERR_IT_NOHEAD,
			    mdoc->parse, nit->line, nit->pos,
			    "Bl -%s It",
			    mdoc_argnames[nbl->args->argv[0].arg]);
		break;
	case LIST_bullet:
		/* FALLTHROUGH */
	case LIST_dash:
		/* FALLTHROUGH */
	case LIST_enum:
		/* FALLTHROUGH */
	case LIST_hyphen:
		if (NULL == nit->body->child)
			mandoc_vmsg(MANDOCERR_IT_NOBODY,
			    mdoc->parse, nit->line, nit->pos,
			    "Bl -%s It",
			    mdoc_argnames[nbl->args->argv[0].arg]);
		/* FALLTHROUGH */
	case LIST_item:
		if (NULL != nit->head->child)
			mandoc_vmsg(MANDOCERR_ARG_SKIP,
			    mdoc->parse, nit->line, nit->pos,
			    "It %s", nit->head->child->string);
		break;
	case LIST_column:
		cols = (int)nbl->norm->Bl.ncols;

		assert(NULL == nit->head->child);

		for (i = 0, nch = nit->child; nch; nch = nch->next)
			if (MDOC_BODY == nch->type)
				i++;

		if (i < cols || i > cols + 1)
			mandoc_vmsg(MANDOCERR_ARGCOUNT,
			    mdoc->parse, nit->line, nit->pos,
			    "columns == %d (have %d)", cols, i);
		break;
	default:
		abort();
	}

	return(1);
}

static int
post_bl_block(POST_ARGS)
{
	struct mdoc_node *n, *ni, *nc;

	/*
	 * These are fairly complicated, so we've broken them into two
	 * functions.  post_bl_block_tag() is called when a -tag is
	 * specified, but no -width (it must be guessed).  The second
	 * when a -width is specified (macro indicators must be
	 * rewritten into real lengths).
	 */

	n = mdoc->last;

	if (LIST_tag == n->norm->Bl.type &&
	    NULL == n->norm->Bl.width) {
		if ( ! post_bl_block_tag(mdoc))
			return(0);
		assert(n->norm->Bl.width);
	} else if (NULL != n->norm->Bl.width) {
		if ( ! post_bl_block_width(mdoc))
			return(0);
		assert(n->norm->Bl.width);
	}

	for (ni = n->body->child; ni; ni = ni->next) {
		if (NULL == ni->body)
			continue;
		nc = ni->body->last;
		while (NULL != nc) {
			switch (nc->tok) {
			case MDOC_Pp:
				/* FALLTHROUGH */
			case MDOC_Lp:
				/* FALLTHROUGH */
			case MDOC_br:
				break;
			default:
				nc = NULL;
				continue;
			}
			if (NULL == ni->next) {
				mandoc_msg(MANDOCERR_PAR_MOVE,
				    mdoc->parse, nc->line, nc->pos,
				    mdoc_macronames[nc->tok]);
				if ( ! mdoc_node_relink(mdoc, nc))
					return(0);
			} else if (0 == n->norm->Bl.comp &&
			    LIST_column != n->norm->Bl.type) {
				mandoc_vmsg(MANDOCERR_PAR_SKIP,
				    mdoc->parse, nc->line, nc->pos,
				    "%s before It",
				    mdoc_macronames[nc->tok]);
				mdoc_node_delete(mdoc, nc);
			} else
				break;
			nc = ni->body->last;
		}
	}
	return(1);
}

static int
post_bl_block_width(POST_ARGS)
{
	size_t		  width;
	int		  i;
	enum mdoct	  tok;
	struct mdoc_node *n;
	char		  buf[24];

	n = mdoc->last;

	/*
	 * Calculate the real width of a list from the -width string,
	 * which may contain a macro (with a known default width), a
	 * literal string, or a scaling width.
	 *
	 * If the value to -width is a macro, then we re-write it to be
	 * the macro's width as set in share/tmac/mdoc/doc-common.
	 */

	if (0 == strcmp(n->norm->Bl.width, "Ds"))
		width = 6;
	else if (MDOC_MAX == (tok = mdoc_hash_find(n->norm->Bl.width)))
		return(1);
	else
		width = macro2len(tok);

	/* The value already exists: free and reallocate it. */

	assert(n->args);

	for (i = 0; i < (int)n->args->argc; i++)
		if (MDOC_Width == n->args->argv[i].arg)
			break;

	assert(i < (int)n->args->argc);

	(void)snprintf(buf, sizeof(buf), "%un", (unsigned int)width);
	free(n->args->argv[i].value[0]);
	n->args->argv[i].value[0] = mandoc_strdup(buf);

	/* Set our width! */
	n->norm->Bl.width = n->args->argv[i].value[0];
	return(1);
}

static int
post_bl_block_tag(POST_ARGS)
{
	struct mdoc_node *n, *nn;
	size_t		  sz, ssz;
	int		  i;
	char		  buf[24];

	/*
	 * Calculate the -width for a `Bl -tag' list if it hasn't been
	 * provided.  Uses the first head macro.  NOTE AGAIN: this is
	 * ONLY if the -width argument has NOT been provided.  See
	 * post_bl_block_width() for converting the -width string.
	 */

	sz = 10;
	n = mdoc->last;

	for (nn = n->body->child; nn; nn = nn->next) {
		if (MDOC_It != nn->tok)
			continue;

		assert(MDOC_BLOCK == nn->type);
		nn = nn->head->child;

		if (nn == NULL)
			break;

		if (MDOC_TEXT == nn->type) {
			sz = strlen(nn->string) + 1;
			break;
		}

		if (0 != (ssz = macro2len(nn->tok)))
			sz = ssz;

		break;
	}

	/* Defaults to ten ens. */

	(void)snprintf(buf, sizeof(buf), "%un", (unsigned int)sz);

	/*
	 * We have to dynamically add this to the macro's argument list.
	 * We're guaranteed that a MDOC_Width doesn't already exist.
	 */

	assert(n->args);
	i = (int)(n->args->argc)++;

	n->args->argv = mandoc_reallocarray(n->args->argv,
	    n->args->argc, sizeof(struct mdoc_argv));

	n->args->argv[i].arg = MDOC_Width;
	n->args->argv[i].line = n->line;
	n->args->argv[i].pos = n->pos;
	n->args->argv[i].sz = 1;
	n->args->argv[i].value = mandoc_malloc(sizeof(char *));
	n->args->argv[i].value[0] = mandoc_strdup(buf);

	/* Set our width! */
	n->norm->Bl.width = n->args->argv[i].value[0];
	return(1);
}

static int
post_bl_head(POST_ARGS)
{
	struct mdoc_node *np, *nn, *nnp;
	struct mdoc_argv *argv;
	int		  i, j;

	if (LIST_column != mdoc->last->norm->Bl.type)
		/* FIXME: this should be ERROR class... */
		return(hwarn_eq0(mdoc));

	/*
	 * Append old-style lists, where the column width specifiers
	 * trail as macro parameters, to the new-style ("normal-form")
	 * lists where they're argument values following -column.
	 */

	if (mdoc->last->child == NULL)
		return(1);

	np = mdoc->last->parent;
	assert(np->args);

	for (j = 0; j < (int)np->args->argc; j++)
		if (MDOC_Column == np->args->argv[j].arg)
			break;

	assert(j < (int)np->args->argc);

	/*
	 * Accommodate for new-style groff column syntax.  Shuffle the
	 * child nodes, all of which must be TEXT, as arguments for the
	 * column field.  Then, delete the head children.
	 */

	argv = np->args->argv + j;
	i = argv->sz;
	argv->sz += mdoc->last->nchild;
	argv->value = mandoc_reallocarray(argv->value,
	    argv->sz, sizeof(char *));

	mdoc->last->norm->Bl.ncols = argv->sz;
	mdoc->last->norm->Bl.cols = (void *)argv->value;

	for (nn = mdoc->last->child; nn; i++) {
		argv->value[i] = nn->string;
		nn->string = NULL;
		nnp = nn;
		nn = nn->next;
		mdoc_node_delete(NULL, nnp);
	}

	mdoc->last->nchild = 0;
	mdoc->last->child = NULL;

	return(1);
}

static int
post_bl(POST_ARGS)
{
	struct mdoc_node	*nparent, *nprev; /* of the Bl block */
	struct mdoc_node	*nblock, *nbody;  /* of the Bl */
	struct mdoc_node	*nchild, *nnext;  /* of the Bl body */

	nbody = mdoc->last;
	switch (nbody->type) {
	case MDOC_BLOCK:
		return(post_bl_block(mdoc));
	case MDOC_HEAD:
		return(post_bl_head(mdoc));
	case MDOC_BODY:
		break;
	default:
		return(1);
	}

	bwarn_ge1(mdoc);

	nchild = nbody->child;
	while (NULL != nchild) {
		if (MDOC_It == nchild->tok || MDOC_Sm == nchild->tok) {
			nchild = nchild->next;
			continue;
		}

		mandoc_msg(MANDOCERR_BL_MOVE, mdoc->parse,
		    nchild->line, nchild->pos,
		    mdoc_macronames[nchild->tok]);

		/*
		 * Move the node out of the Bl block.
		 * First, collect all required node pointers.
		 */

		nblock  = nbody->parent;
		nprev   = nblock->prev;
		nparent = nblock->parent;
		nnext   = nchild->next;

		/*
		 * Unlink this child.
		 */

		assert(NULL == nchild->prev);
		if (0 == --nbody->nchild) {
			nbody->child = NULL;
			nbody->last  = NULL;
			assert(NULL == nnext);
		} else {
			nbody->child = nnext;
			nnext->prev = NULL;
		}

		/*
		 * Relink this child.
		 */

		nchild->parent = nparent;
		nchild->prev   = nprev;
		nchild->next   = nblock;

		nblock->prev = nchild;
		nparent->nchild++;
		if (NULL == nprev)
			nparent->child = nchild;
		else
			nprev->next = nchild;

		nchild = nnext;
	}

	return(1);
}

static int
post_bk(POST_ARGS)
{

	hwarn_eq0(mdoc);
	bwarn_ge1(mdoc);
	return(1);
}

static int
ebool(struct mdoc *mdoc)
{
	struct mdoc_node	*nch;
	enum mdoct		 tok;

	tok = mdoc->last->tok;
	nch = mdoc->last->child;

	if (NULL == nch) {
		if (MDOC_Sm == tok)
			mdoc->flags ^= MDOC_SMOFF;
		return(1);
	}

	check_count(mdoc, MDOC_ELEM, CHECK_WARN, CHECK_LT, 2);

	assert(MDOC_TEXT == nch->type);

	if (0 == strcmp(nch->string, "on")) {
		if (MDOC_Sm == tok)
			mdoc->flags &= ~MDOC_SMOFF;
		return(1);
	}
	if (0 == strcmp(nch->string, "off")) {
		if (MDOC_Sm == tok)
			mdoc->flags |= MDOC_SMOFF;
		return(1);
	}

	mandoc_vmsg(MANDOCERR_SM_BAD,
	    mdoc->parse, nch->line, nch->pos,
	    "%s %s", mdoc_macronames[tok], nch->string);
	return(mdoc_node_relink(mdoc, nch));
}

static int
post_root(POST_ARGS)
{
	struct mdoc_node *n;

	/* Add missing prologue data. */

	if (mdoc->meta.date == NULL)
		mdoc->meta.date = mdoc->quick ?
		    mandoc_strdup("") :
		    mandoc_normdate(mdoc->parse, NULL, 0, 0);

	if (mdoc->meta.title == NULL) {
		mandoc_msg(MANDOCERR_DT_NOTITLE,
		    mdoc->parse, 0, 0, "EOF");
		mdoc->meta.title = mandoc_strdup("UNTITLED");
	}

	if (mdoc->meta.vol == NULL)
		mdoc->meta.vol = mandoc_strdup("LOCAL");

	if (mdoc->meta.os == NULL) {
		mandoc_msg(MANDOCERR_OS_MISSING,
		    mdoc->parse, 0, 0, NULL);
		mdoc->meta.os = mandoc_strdup("");
	}

	/* Check that we begin with a proper `Sh'. */

	n = mdoc->first->child;
	while (n != NULL && mdoc_macros[n->tok].flags & MDOC_PROLOGUE)
		n = n->next;

	if (n == NULL)
		mandoc_msg(MANDOCERR_DOC_EMPTY, mdoc->parse, 0, 0, NULL);
	else if (n->tok != MDOC_Sh)
		mandoc_msg(MANDOCERR_SEC_BEFORE, mdoc->parse,
		    n->line, n->pos, mdoc_macronames[n->tok]);

	return(1);
}

static int
post_st(POST_ARGS)
{
	struct mdoc_node	 *n, *nch;
	const char		 *p;

	n = mdoc->last;
	nch = n->child;

	if (NULL == nch) {
		mandoc_msg(MANDOCERR_MACRO_EMPTY, mdoc->parse,
		    n->line, n->pos, mdoc_macronames[n->tok]);
		mdoc_node_delete(mdoc, n);
		return(1);
	}

	assert(MDOC_TEXT == nch->type);

	if (NULL == (p = mdoc_a2st(nch->string))) {
		mandoc_vmsg(MANDOCERR_ST_BAD, mdoc->parse,
		    nch->line, nch->pos, "St %s", nch->string);
		mdoc_node_delete(mdoc, n);
	} else {
		free(nch->string);
		nch->string = mandoc_strdup(p);
	}

	return(1);
}

static int
post_rs(POST_ARGS)
{
	struct mdoc_node *nn, *next, *prev;
	int		  i, j;

	switch (mdoc->last->type) {
	case MDOC_HEAD:
		check_count(mdoc, MDOC_HEAD, CHECK_WARN, CHECK_EQ, 0);
		return(1);
	case MDOC_BODY:
		if (mdoc->last->child)
			break;
		check_count(mdoc, MDOC_BODY, CHECK_WARN, CHECK_GT, 0);
		return(1);
	default:
		return(1);
	}

	/*
	 * The full `Rs' block needs special handling to order the
	 * sub-elements according to `rsord'.  Pick through each element
	 * and correctly order it.  This is an insertion sort.
	 */

	next = NULL;
	for (nn = mdoc->last->child->next; nn; nn = next) {
		/* Determine order of `nn'. */
		for (i = 0; i < RSORD_MAX; i++)
			if (rsord[i] == nn->tok)
				break;

		if (i == RSORD_MAX) {
			mandoc_msg(MANDOCERR_RS_BAD,
			    mdoc->parse, nn->line, nn->pos,
			    mdoc_macronames[nn->tok]);
			i = -1;
		} else if (MDOC__J == nn->tok || MDOC__B == nn->tok)
			mdoc->last->norm->Rs.quote_T++;

		/*
		 * Remove `nn' from the chain.  This somewhat
		 * repeats mdoc_node_unlink(), but since we're
		 * just re-ordering, there's no need for the
		 * full unlink process.
		 */

		if (NULL != (next = nn->next))
			next->prev = nn->prev;

		if (NULL != (prev = nn->prev))
			prev->next = nn->next;

		nn->prev = nn->next = NULL;

		/*
		 * Scan back until we reach a node that's
		 * ordered before `nn'.
		 */

		for ( ; prev ; prev = prev->prev) {
			/* Determine order of `prev'. */
			for (j = 0; j < RSORD_MAX; j++)
				if (rsord[j] == prev->tok)
					break;
			if (j == RSORD_MAX)
				j = -1;

			if (j <= i)
				break;
		}

		/*
		 * Set `nn' back into its correct place in front
		 * of the `prev' node.
		 */

		nn->prev = prev;

		if (prev) {
			if (prev->next)
				prev->next->prev = nn;
			nn->next = prev->next;
			prev->next = nn;
		} else {
			mdoc->last->child->prev = nn;
			nn->next = mdoc->last->child;
			mdoc->last->child = nn;
		}
	}

	return(1);
}

/*
 * For some arguments of some macros,
 * convert all breakable hyphens into ASCII_HYPH.
 */
static int
post_hyph(POST_ARGS)
{
	struct mdoc_node	*n, *nch;
	char			*cp;

	n = mdoc->last;
	switch (n->type) {
	case MDOC_HEAD:
		if (MDOC_Sh == n->tok || MDOC_Ss == n->tok)
			break;
		return(1);
	case MDOC_BODY:
		if (MDOC_D1 == n->tok || MDOC_Nd == n->tok)
			break;
		return(1);
	case MDOC_ELEM:
		break;
	default:
		return(1);
	}

	for (nch = n->child; nch; nch = nch->next) {
		if (MDOC_TEXT != nch->type)
			continue;
		cp = nch->string;
		if ('\0' == *cp)
			continue;
		while ('\0' != *(++cp))
			if ('-' == *cp &&
			    isalpha((unsigned char)cp[-1]) &&
			    isalpha((unsigned char)cp[1]))
				*cp = ASCII_HYPH;
	}
	return(1);
}

static int
post_hyphtext(POST_ARGS)
{

	ewarn_ge1(mdoc);
	return(post_hyph(mdoc));
}

static int
post_ns(POST_ARGS)
{

	if (MDOC_LINE & mdoc->last->flags)
		mandoc_msg(MANDOCERR_NS_SKIP, mdoc->parse,
		    mdoc->last->line, mdoc->last->pos, NULL);
	return(1);
}

static int
post_sh(POST_ARGS)
{

	post_ignpar(mdoc);

	if (MDOC_HEAD == mdoc->last->type)
		return(post_sh_head(mdoc));
	if (MDOC_BODY == mdoc->last->type)
		return(post_sh_body(mdoc));

	return(1);
}

static int
post_sh_body(POST_ARGS)
{
	struct mdoc_node *n;

	if (SEC_NAME != mdoc->lastsec)
		return(1);

	/*
	 * Warn if the NAME section doesn't contain the `Nm' and `Nd'
	 * macros (can have multiple `Nm' and one `Nd').  Note that the
	 * children of the BODY declaration can also be "text".
	 */

	if (NULL == (n = mdoc->last->child)) {
		mandoc_msg(MANDOCERR_NAMESEC_BAD, mdoc->parse,
		    mdoc->last->line, mdoc->last->pos, "empty");
		return(1);
	}

	for ( ; n && n->next; n = n->next) {
		if (MDOC_ELEM == n->type && MDOC_Nm == n->tok)
			continue;
		if (MDOC_TEXT == n->type)
			continue;
		mandoc_msg(MANDOCERR_NAMESEC_BAD, mdoc->parse,
		    n->line, n->pos, mdoc_macronames[n->tok]);
	}

	assert(n);
	if (MDOC_BLOCK == n->type && MDOC_Nd == n->tok)
		return(1);

	mandoc_msg(MANDOCERR_NAMESEC_BAD, mdoc->parse,
	    n->line, n->pos, mdoc_macronames[n->tok]);
	return(1);
}

static int
post_sh_head(POST_ARGS)
{
	struct mdoc_node *n;
	const char	*goodsec;
	char		*secname;
	enum mdoc_sec	 sec;

	/*
	 * Process a new section.  Sections are either "named" or
	 * "custom".  Custom sections are user-defined, while named ones
	 * follow a conventional order and may only appear in certain
	 * manual sections.
	 */

	secname = NULL;
	sec = SEC_CUSTOM;
	mdoc_deroff(&secname, mdoc->last);
	sec = NULL == secname ? SEC_CUSTOM : a2sec(secname);

	/* The NAME should be first. */

	if (SEC_NAME != sec && SEC_NONE == mdoc->lastnamed)
		mandoc_vmsg(MANDOCERR_NAMESEC_FIRST, mdoc->parse,
		    mdoc->last->line, mdoc->last->pos,
		    "Sh %s", secname);

	/* The SYNOPSIS gets special attention in other areas. */

	if (SEC_SYNOPSIS == sec) {
		roff_setreg(mdoc->roff, "nS", 1, '=');
		mdoc->flags |= MDOC_SYNOPSIS;
	} else {
		roff_setreg(mdoc->roff, "nS", 0, '=');
		mdoc->flags &= ~MDOC_SYNOPSIS;
	}

	/* Mark our last section. */

	mdoc->lastsec = sec;

	/*
	 * Set the section attribute for the current HEAD, for its
	 * parent BLOCK, and for the HEAD children; the latter can
	 * only be TEXT nodes, so no recursion is needed.
	 * For other blocks and elements, including .Sh BODY, this is
	 * done when allocating the node data structures, but for .Sh
	 * BLOCK and HEAD, the section is still unknown at that time.
	 */

	mdoc->last->parent->sec = sec;
	mdoc->last->sec = sec;
	for (n = mdoc->last->child; n; n = n->next)
		n->sec = sec;

	/* We don't care about custom sections after this. */

	if (SEC_CUSTOM == sec) {
		free(secname);
		return(1);
	}

	/*
	 * Check whether our non-custom section is being repeated or is
	 * out of order.
	 */

	if (sec == mdoc->lastnamed)
		mandoc_vmsg(MANDOCERR_SEC_REP, mdoc->parse,
		    mdoc->last->line, mdoc->last->pos,
		    "Sh %s", secname);

	if (sec < mdoc->lastnamed)
		mandoc_vmsg(MANDOCERR_SEC_ORDER, mdoc->parse,
		    mdoc->last->line, mdoc->last->pos,
		    "Sh %s", secname);

	/* Mark the last named section. */

	mdoc->lastnamed = sec;

	/* Check particular section/manual conventions. */

	if (mdoc->meta.msec == NULL) {
		free(secname);
		return(1);
	}

	goodsec = NULL;
	switch (sec) {
	case SEC_ERRORS:
		if (*mdoc->meta.msec == '4')
			break;
		goodsec = "2, 3, 4, 9";
		/* FALLTHROUGH */
	case SEC_RETURN_VALUES:
		/* FALLTHROUGH */
	case SEC_LIBRARY:
		if (*mdoc->meta.msec == '2')
			break;
		if (*mdoc->meta.msec == '3')
			break;
		if (NULL == goodsec)
			goodsec = "2, 3, 9";
		/* FALLTHROUGH */
	case SEC_CONTEXT:
		if (*mdoc->meta.msec == '9')
			break;
		if (NULL == goodsec)
			goodsec = "9";
		mandoc_vmsg(MANDOCERR_SEC_MSEC, mdoc->parse,
		    mdoc->last->line, mdoc->last->pos,
		    "Sh %s for %s only", secname, goodsec);
		break;
	default:
		break;
	}

	free(secname);
	return(1);
}

static int
post_ignpar(POST_ARGS)
{
	struct mdoc_node *np;

	hwarn_ge1(mdoc);
	post_hyph(mdoc);

	if (MDOC_BODY != mdoc->last->type)
		return(1);

	if (NULL != (np = mdoc->last->child))
		if (MDOC_Pp == np->tok || MDOC_Lp == np->tok) {
			mandoc_vmsg(MANDOCERR_PAR_SKIP,
			    mdoc->parse, np->line, np->pos,
			    "%s after %s", mdoc_macronames[np->tok],
			    mdoc_macronames[mdoc->last->tok]);
			mdoc_node_delete(mdoc, np);
		}

	if (NULL != (np = mdoc->last->last))
		if (MDOC_Pp == np->tok || MDOC_Lp == np->tok) {
			mandoc_vmsg(MANDOCERR_PAR_SKIP, mdoc->parse,
			    np->line, np->pos, "%s at the end of %s",
			    mdoc_macronames[np->tok],
			    mdoc_macronames[mdoc->last->tok]);
			mdoc_node_delete(mdoc, np);
		}

	return(1);
}

static int
pre_par(PRE_ARGS)
{

	if (NULL == mdoc->last)
		return(1);
	if (MDOC_ELEM != n->type && MDOC_BLOCK != n->type)
		return(1);

	/*
	 * Don't allow prior `Lp' or `Pp' prior to a paragraph-type
	 * block:  `Lp', `Pp', or non-compact `Bd' or `Bl'.
	 */

	if (MDOC_Pp != mdoc->last->tok &&
	    MDOC_Lp != mdoc->last->tok &&
	    MDOC_br != mdoc->last->tok)
		return(1);
	if (MDOC_Bl == n->tok && n->norm->Bl.comp)
		return(1);
	if (MDOC_Bd == n->tok && n->norm->Bd.comp)
		return(1);
	if (MDOC_It == n->tok && n->parent->norm->Bl.comp)
		return(1);

	mandoc_vmsg(MANDOCERR_PAR_SKIP, mdoc->parse,
	    mdoc->last->line, mdoc->last->pos,
	    "%s before %s", mdoc_macronames[mdoc->last->tok],
	    mdoc_macronames[n->tok]);
	mdoc_node_delete(mdoc, mdoc->last);
	return(1);
}

static int
post_par(POST_ARGS)
{
	struct mdoc_node *np;

	if (mdoc->last->tok == MDOC_sp)
		ewarn_le1(mdoc);
	else
		ewarn_eq0(mdoc);

	if (MDOC_ELEM != mdoc->last->type &&
	    MDOC_BLOCK != mdoc->last->type)
		return(1);

	if (NULL == (np = mdoc->last->prev)) {
		np = mdoc->last->parent;
		if (MDOC_Sh != np->tok && MDOC_Ss != np->tok)
			return(1);
	} else {
		if (MDOC_Pp != np->tok && MDOC_Lp != np->tok &&
		    (MDOC_br != mdoc->last->tok ||
		     (MDOC_sp != np->tok && MDOC_br != np->tok)))
			return(1);
	}

	mandoc_vmsg(MANDOCERR_PAR_SKIP, mdoc->parse,
	    mdoc->last->line, mdoc->last->pos,
	    "%s after %s", mdoc_macronames[mdoc->last->tok],
	    mdoc_macronames[np->tok]);
	mdoc_node_delete(mdoc, mdoc->last);
	return(1);
}

static int
pre_literal(PRE_ARGS)
{

	pre_display(mdoc, n);

	if (MDOC_BODY != n->type)
		return(1);

	/*
	 * The `Dl' (note "el" not "one") and `Bd -literal' and `Bd
	 * -unfilled' macros set MDOC_LITERAL on entrance to the body.
	 */

	switch (n->tok) {
	case MDOC_Dl:
		mdoc->flags |= MDOC_LITERAL;
		break;
	case MDOC_Bd:
		if (DISP_literal == n->norm->Bd.type)
			mdoc->flags |= MDOC_LITERAL;
		if (DISP_unfilled == n->norm->Bd.type)
			mdoc->flags |= MDOC_LITERAL;
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	return(1);
}

static int
post_dd(POST_ARGS)
{
	struct mdoc_node *n;
	char		 *datestr;

	if (mdoc->meta.date)
		free(mdoc->meta.date);

	n = mdoc->last;
	if (NULL == n->child || '\0' == n->child->string[0]) {
		mdoc->meta.date = mdoc->quick ? mandoc_strdup("") :
		    mandoc_normdate(mdoc->parse, NULL, n->line, n->pos);
		goto out;
	}

	datestr = NULL;
	mdoc_deroff(&datestr, n);
	if (mdoc->quick)
		mdoc->meta.date = datestr;
	else {
		mdoc->meta.date = mandoc_normdate(mdoc->parse,
		    datestr, n->line, n->pos);
		free(datestr);
	}
out:
	mdoc_node_delete(mdoc, n);
	return(1);
}

static int
post_dt(POST_ARGS)
{
	struct mdoc_node *nn, *n;
	const char	 *cp;
	char		 *p;

	n = mdoc->last;

	free(mdoc->meta.title);
	free(mdoc->meta.msec);
	free(mdoc->meta.vol);
	free(mdoc->meta.arch);

	mdoc->meta.title = NULL;
	mdoc->meta.msec = NULL;
	mdoc->meta.vol = NULL;
	mdoc->meta.arch = NULL;

	/* First check that all characters are uppercase. */

	if (NULL != (nn = n->child))
		for (p = nn->string; *p; p++) {
			if (toupper((unsigned char)*p) == *p)
				continue;
			mandoc_vmsg(MANDOCERR_TITLE_CASE,
			    mdoc->parse, nn->line,
			    nn->pos + (p - nn->string),
			    "Dt %s", nn->string);
			break;
		}

	/* No argument: msec and arch remain NULL. */

	if (NULL == (nn = n->child)) {
		mandoc_msg(MANDOCERR_DT_NOTITLE,
		    mdoc->parse, n->line, n->pos, "Dt");
		mdoc->meta.title = mandoc_strdup("UNTITLED");
		mdoc->meta.vol = mandoc_strdup("LOCAL");
		goto out;
	}

	/* One argument: msec and arch remain NULL. */

	mdoc->meta.title = mandoc_strdup(
	    '\0' == nn->string[0] ? "UNTITLED" : nn->string);

	if (NULL == (nn = nn->next)) {
		mandoc_vmsg(MANDOCERR_MSEC_MISSING,
		    mdoc->parse, n->line, n->pos,
		    "Dt %s", mdoc->meta.title);
		mdoc->meta.vol = mandoc_strdup("LOCAL");
		goto out;
	}

	/* Handles: `.Dt TITLE SEC'
	 * title = TITLE,
	 * volume = SEC is msec ? format(msec) : SEC,
	 * msec = SEC is msec ? atoi(msec) : 0,
	 * arch = NULL
	 */

	cp = mandoc_a2msec(nn->string);
	if (cp) {
		mdoc->meta.vol = mandoc_strdup(cp);
		mdoc->meta.msec = mandoc_strdup(nn->string);
	} else {
		mandoc_vmsg(MANDOCERR_MSEC_BAD, mdoc->parse,
		    nn->line, nn->pos, "Dt ... %s", nn->string);
		mdoc->meta.vol = mandoc_strdup(nn->string);
		mdoc->meta.msec = mandoc_strdup(nn->string);
	}

	if (NULL == (nn = nn->next))
		goto out;

	/* Handles: `.Dt TITLE SEC VOL'
	 * title = TITLE,
	 * volume = VOL is vol ? format(VOL) :
	 *	    VOL is arch ? format(arch) :
	 *	    VOL
	 */

	cp = mdoc_a2vol(nn->string);
	if (cp) {
		free(mdoc->meta.vol);
		mdoc->meta.vol = mandoc_strdup(cp);
	} else {
		cp = mdoc_a2arch(nn->string);
		if (NULL == cp) {
			mandoc_vmsg(MANDOCERR_ARCH_BAD, mdoc->parse,
			    nn->line, nn->pos, "Dt ... %s", nn->string);
			free(mdoc->meta.vol);
			mdoc->meta.vol = mandoc_strdup(nn->string);
		} else
			mdoc->meta.arch = mandoc_strdup(cp);
	}

	/* Ignore any subsequent parameters... */
	/* FIXME: warn about subsequent parameters. */
out:
	mdoc_node_delete(mdoc, n);
	return(1);
}

static int
post_bx(POST_ARGS)
{
	struct mdoc_node	*n;

	/*
	 * Make `Bx's second argument always start with an uppercase
	 * letter.  Groff checks if it's an "accepted" term, but we just
	 * uppercase blindly.
	 */

	n = mdoc->last->child;
	if (n && NULL != (n = n->next))
		*n->string = (char)toupper((unsigned char)*n->string);

	return(1);
}

static int
post_os(POST_ARGS)
{
#ifndef OSNAME
	struct utsname	  utsname;
	static char	 *defbuf;
#endif
	struct mdoc_node *n;

	n = mdoc->last;

	/*
	 * Set the operating system by way of the `Os' macro.
	 * The order of precedence is:
	 * 1. the argument of the `Os' macro, unless empty
	 * 2. the -Ios=foo command line argument, if provided
	 * 3. -DOSNAME="\"foo\"", if provided during compilation
	 * 4. "sysname release" from uname(3)
	 */

	free(mdoc->meta.os);
	mdoc->meta.os = NULL;
	mdoc_deroff(&mdoc->meta.os, n);
	if (mdoc->meta.os)
		goto out;

	if (mdoc->defos) {
		mdoc->meta.os = mandoc_strdup(mdoc->defos);
		goto out;
	}

#ifdef OSNAME
	mdoc->meta.os = mandoc_strdup(OSNAME);
#else /*!OSNAME */
	if (NULL == defbuf) {
		if (-1 == uname(&utsname)) {
			mandoc_msg(MANDOCERR_OS_UNAME, mdoc->parse,
			    n->line, n->pos, "Os");
			defbuf = mandoc_strdup("UNKNOWN");
		} else
			mandoc_asprintf(&defbuf, "%s %s",
			    utsname.sysname, utsname.release);
	}
	mdoc->meta.os = mandoc_strdup(defbuf);
#endif /*!OSNAME*/

out:
	mdoc_node_delete(mdoc, n);
	return(1);
}

/*
 * If no argument is provided,
 * fill in the name of the current manual page.
 */
static int
post_ex(POST_ARGS)
{
	struct mdoc_node *n;

	n = mdoc->last;

	if (n->child)
		return(1);

	if (mdoc->meta.name == NULL) {
		mandoc_msg(MANDOCERR_EX_NONAME, mdoc->parse,
		    n->line, n->pos, "Ex");
		return(1);
	}

	mdoc->next = MDOC_NEXT_CHILD;

	if ( ! mdoc_word_alloc(mdoc, n->line, n->pos, mdoc->meta.name))
		return(0);

	mdoc->last = n;
	return(1);
}

static enum mdoc_sec
a2sec(const char *p)
{
	int		 i;

	for (i = 0; i < (int)SEC__MAX; i++)
		if (secnames[i] && 0 == strcmp(p, secnames[i]))
			return((enum mdoc_sec)i);

	return(SEC_CUSTOM);
}

static size_t
macro2len(enum mdoct macro)
{

	switch (macro) {
	case MDOC_Ad:
		return(12);
	case MDOC_Ao:
		return(12);
	case MDOC_An:
		return(12);
	case MDOC_Aq:
		return(12);
	case MDOC_Ar:
		return(12);
	case MDOC_Bo:
		return(12);
	case MDOC_Bq:
		return(12);
	case MDOC_Cd:
		return(12);
	case MDOC_Cm:
		return(10);
	case MDOC_Do:
		return(10);
	case MDOC_Dq:
		return(12);
	case MDOC_Dv:
		return(12);
	case MDOC_Eo:
		return(12);
	case MDOC_Em:
		return(10);
	case MDOC_Er:
		return(17);
	case MDOC_Ev:
		return(15);
	case MDOC_Fa:
		return(12);
	case MDOC_Fl:
		return(10);
	case MDOC_Fo:
		return(16);
	case MDOC_Fn:
		return(16);
	case MDOC_Ic:
		return(10);
	case MDOC_Li:
		return(16);
	case MDOC_Ms:
		return(6);
	case MDOC_Nm:
		return(10);
	case MDOC_No:
		return(12);
	case MDOC_Oo:
		return(10);
	case MDOC_Op:
		return(14);
	case MDOC_Pa:
		return(32);
	case MDOC_Pf:
		return(12);
	case MDOC_Po:
		return(12);
	case MDOC_Pq:
		return(12);
	case MDOC_Ql:
		return(16);
	case MDOC_Qo:
		return(12);
	case MDOC_So:
		return(12);
	case MDOC_Sq:
		return(12);
	case MDOC_Sy:
		return(6);
	case MDOC_Sx:
		return(16);
	case MDOC_Tn:
		return(10);
	case MDOC_Va:
		return(12);
	case MDOC_Vt:
		return(12);
	case MDOC_Xr:
		return(10);
	default:
		break;
	};
	return(0);
}
