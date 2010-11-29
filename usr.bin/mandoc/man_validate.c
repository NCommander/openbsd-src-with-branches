/*	$Id: man_validate.c,v 1.32 2010/11/29 00:12:02 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2010 Ingo Schwarze <schwarze@openbsd.org>
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

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "libman.h"
#include "libmandoc.h"

#define	CHKARGS	  struct man *m, struct man_node *n

typedef	int	(*v_check)(CHKARGS);

struct	man_valid {
	v_check	 *pres;
	v_check	 *posts;
};

static	int	  check_bline(CHKARGS);
static	int	  check_eq0(CHKARGS);
static	int	  check_le1(CHKARGS);
static	int	  check_ge2(CHKARGS);
static	int	  check_le5(CHKARGS);
static	int	  check_ft(CHKARGS);
static	int	  check_par(CHKARGS);
static	int	  check_part(CHKARGS);
static	int	  check_root(CHKARGS);
static	int	  check_sec(CHKARGS);
static	int	  check_text(CHKARGS);
static	int	  check_title(CHKARGS);

static	v_check	  posts_eq0[] = { check_eq0, NULL };
static	v_check	  posts_ft[] = { check_ft, NULL };
static	v_check	  posts_th[] = { check_ge2, check_le5, check_title, NULL };
static	v_check	  posts_par[] = { check_par, NULL };
static	v_check	  posts_part[] = { check_part, NULL };
static	v_check	  posts_sec[] = { check_sec, NULL };
static	v_check	  posts_le1[] = { check_le1, NULL };
static	v_check	  pres_bline[] = { check_bline, NULL };

static	const struct man_valid man_valids[MAN_MAX] = {
	{ NULL, posts_eq0 }, /* br */
	{ pres_bline, posts_th }, /* TH */
	{ pres_bline, posts_sec }, /* SH */
	{ pres_bline, posts_sec }, /* SS */
	{ pres_bline, posts_par }, /* TP */
	{ pres_bline, posts_par }, /* LP */
	{ pres_bline, posts_par }, /* PP */
	{ pres_bline, posts_par }, /* P */
	{ pres_bline, posts_par }, /* IP */
	{ pres_bline, posts_par }, /* HP */
	{ NULL, NULL }, /* SM */
	{ NULL, NULL }, /* SB */
	{ NULL, NULL }, /* BI */
	{ NULL, NULL }, /* IB */
	{ NULL, NULL }, /* BR */
	{ NULL, NULL }, /* RB */
	{ NULL, NULL }, /* R */
	{ NULL, NULL }, /* B */
	{ NULL, NULL }, /* I */
	{ NULL, NULL }, /* IR */
	{ NULL, NULL }, /* RI */
	{ NULL, posts_eq0 }, /* na */ /* FIXME: should warn only. */
	{ NULL, NULL }, /* i */
	{ NULL, posts_le1 }, /* sp */ /* FIXME: should warn only. */
	{ pres_bline, posts_eq0 }, /* nf */
	{ pres_bline, posts_eq0 }, /* fi */
	{ NULL, NULL }, /* r */
	{ NULL, NULL }, /* RE */
	{ NULL, posts_part }, /* RS */
	{ NULL, NULL }, /* DT */
	{ NULL, NULL }, /* UC */
	{ NULL, NULL }, /* PD */
	{ NULL, NULL }, /* AT */
	{ NULL, NULL }, /* in */
	{ NULL, NULL }, /* TS */
	{ NULL, NULL }, /* TE */
	{ NULL, posts_ft }, /* ft */
};


int
man_valid_pre(struct man *m, struct man_node *n)
{
	v_check		*cp;

	if (MAN_TEXT == n->type)
		return(1);
	if (MAN_ROOT == n->type)
		return(1);

	if (NULL == (cp = man_valids[n->tok].pres))
		return(1);
	for ( ; *cp; cp++)
		if ( ! (*cp)(m, n)) 
			return(0);
	return(1);
}


int
man_valid_post(struct man *m)
{
	v_check		*cp;

	if (MAN_VALID & m->last->flags)
		return(1);
	m->last->flags |= MAN_VALID;

	switch (m->last->type) {
	case (MAN_TEXT): 
		return(check_text(m, m->last));
	case (MAN_ROOT):
		return(check_root(m, m->last));
	default:
		break;
	}

	if (NULL == (cp = man_valids[m->last->tok].posts))
		return(1);
	for ( ; *cp; cp++)
		if ( ! (*cp)(m, m->last))
			return(0);

	return(1);
}


static int
check_root(CHKARGS) 
{

	if (MAN_BLINE & m->flags)
		return(man_nmsg(m, n, MANDOCERR_SCOPEEXIT));
	if (MAN_ELINE & m->flags)
		return(man_nmsg(m, n, MANDOCERR_SCOPEEXIT));

	m->flags &= ~MAN_BLINE;
	m->flags &= ~MAN_ELINE;

	if (NULL == m->first->child) {
		man_nmsg(m, n, MANDOCERR_NODOCBODY);
		return(0);
	} else if (NULL == m->meta.title) {
		if ( ! man_nmsg(m, n, MANDOCERR_NOTITLE))
			return(0);
		/*
		 * If a title hasn't been set, do so now (by
		 * implication, date and section also aren't set).
		 * 
		 * FIXME: this should be in man_action.c.
		 */
	        m->meta.title = mandoc_strdup("unknown");
		m->meta.date = time(NULL);
		m->meta.msec = mandoc_strdup("1");
	}

	return(1);
}


static int
check_title(CHKARGS) 
{
	const char	*p;

	assert(n->child);
	/* FIXME: is this sufficient? */
	if ('\0' == *n->child->string) {
		man_nmsg(m, n, MANDOCERR_SYNTARGCOUNT);
		return(0);
	}

	for (p = n->child->string; '\0' != *p; p++)
		if (isalpha((u_char)*p) && ! isupper((u_char)*p))
			if ( ! man_nmsg(m, n, MANDOCERR_UPPERCASE))
				return(0);

	return(1);
}


static int
check_text(CHKARGS) 
{
	char		*p;
	int		 pos, c;
	size_t		 sz;

	for (p = n->string, pos = n->pos + 1; *p; p++, pos++) {
		sz = strcspn(p, "\t\\");
		p += (int)sz;

		if ('\0' == *p)
			break;

		pos += (int)sz;

		if ('\t' == *p) {
			if (MAN_LITERAL & m->flags)
				continue;
			if (man_pmsg(m, n->line, pos, MANDOCERR_BADTAB))
				continue;
			return(0);
		}

		/* Check the special character. */

		c = mandoc_special(p);
		if (c) {
			p += c - 1;
			pos += c - 1;
		} else
			man_pmsg(m, n->line, pos, MANDOCERR_BADESCAPE);
	}

	return(1);
}


#define	INEQ_DEFINE(x, ineq, name) \
static int \
check_##name(CHKARGS) \
{ \
	if (n->nchild ineq (x)) \
		return(1); \
	man_vmsg(m, MANDOCERR_SYNTARGCOUNT, n->line, n->pos, \
			"line arguments %s %d (have %d)", \
			#ineq, (x), n->nchild); \
	return(0); \
}

INEQ_DEFINE(0, ==, eq0)
INEQ_DEFINE(1, <=, le1)
INEQ_DEFINE(2, >=, ge2)
INEQ_DEFINE(5, <=, le5)


static int
check_ft(CHKARGS)
{
	char	*cp;
	int	 ok;

	if (0 == n->nchild)
		return(1);

	ok = 0;
	cp = n->child->string;
	switch (*cp) {
	case ('1'):
		/* FALLTHROUGH */
	case ('2'):
		/* FALLTHROUGH */
	case ('3'):
		/* FALLTHROUGH */
	case ('4'):
		/* FALLTHROUGH */
	case ('I'):
		/* FALLTHROUGH */
	case ('P'):
		/* FALLTHROUGH */
	case ('R'):
		if ('\0' == cp[1])
			ok = 1;
		break;
	case ('B'):
		if ('\0' == cp[1] || ('I' == cp[1] && '\0' == cp[2]))
			ok = 1;
		break;
	case ('C'):
		if ('W' == cp[1] && '\0' == cp[2])
			ok = 1;
		break;
	default:
		break;
	}

	if (0 == ok) {
		man_vmsg(m, MANDOCERR_BADFONT, n->line, n->pos, "%s", cp);
		*cp = '\0';
	}

	if (1 < n->nchild)
		man_vmsg(m, MANDOCERR_ARGCOUNT, n->line, n->pos,
		    "want one child (have %d)", n->nchild);

	return(1);
}


static int
check_sec(CHKARGS)
{

	if (MAN_HEAD == n->type && 0 == n->nchild) {
		man_nmsg(m, n, MANDOCERR_SYNTARGCOUNT);
		return(0);
	} else if (MAN_BODY == n->type && 0 == n->nchild)
		return(man_nmsg(m, n, MANDOCERR_NOBODY));

	return(1);
}


static int
check_part(CHKARGS)
{

	if (MAN_BODY == n->type && 0 == n->nchild)
		return(man_nmsg(m, n, MANDOCERR_NOBODY));
	return(1);
}


static int
check_par(CHKARGS)
{

	if (MAN_BODY == n->type) 
		switch (n->tok) {
		case (MAN_IP):
			/* FALLTHROUGH */
		case (MAN_HP):
			/* FALLTHROUGH */
		case (MAN_TP):
			/* Body-less lists are ok. */
			break;
		default:
			if (n->nchild)
				break;
			return(man_nmsg(m, n, MANDOCERR_NOBODY));
		}
	if (MAN_HEAD == n->type)
		switch (n->tok) {
		case (MAN_PP):
			/* FALLTHROUGH */
		case (MAN_P):
			/* FALLTHROUGH */
		case (MAN_LP):
			if (0 == n->nchild)
				break;
			return(man_nmsg(m, n, MANDOCERR_ARGSLOST));
		default:
			break;
		}

	return(1);
}


static int
check_bline(CHKARGS)
{

	assert( ! (MAN_ELINE & m->flags));
	if (MAN_BLINE & m->flags) {
		man_nmsg(m, n, MANDOCERR_SYNTLINESCOPE);
		return(0);
	}

	return(1);
}

