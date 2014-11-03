/*	$OpenBSD$ */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2012, 2013, 2014 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2013 Franco Fichtner <franco@lastsummer.de>
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
#include <stdlib.h>
#include <string.h>

#include "man.h"
#include "mandoc.h"
#include "libmandoc.h"
#include "libman.h"

enum	rew {
	REW_REWIND,
	REW_NOHALT,
	REW_HALT
};

static	int		 blk_close(MACRO_PROT_ARGS);
static	int		 blk_exp(MACRO_PROT_ARGS);
static	int		 blk_imp(MACRO_PROT_ARGS);
static	int		 in_line_eoln(MACRO_PROT_ARGS);
static	int		 man_args(struct man *, int,
				int *, char *, char **);

static	int		 rew_scope(enum man_type,
				struct man *, enum mant);
static	enum rew	 rew_dohalt(enum mant, enum man_type,
				const struct man_node *);
static	enum rew	 rew_block(enum mant, enum man_type,
				const struct man_node *);

const	struct man_macro __man_macros[MAN_MAX] = {
	{ in_line_eoln, MAN_NSCOPED }, /* br */
	{ in_line_eoln, MAN_BSCOPE }, /* TH */
	{ blk_imp, MAN_BSCOPE | MAN_SCOPED }, /* SH */
	{ blk_imp, MAN_BSCOPE | MAN_SCOPED }, /* SS */
	{ blk_imp, MAN_BSCOPE | MAN_SCOPED | MAN_FSCOPED }, /* TP */
	{ blk_imp, MAN_BSCOPE }, /* LP */
	{ blk_imp, MAN_BSCOPE }, /* PP */
	{ blk_imp, MAN_BSCOPE }, /* P */
	{ blk_imp, MAN_BSCOPE }, /* IP */
	{ blk_imp, MAN_BSCOPE }, /* HP */
	{ in_line_eoln, MAN_SCOPED | MAN_JOIN }, /* SM */
	{ in_line_eoln, MAN_SCOPED | MAN_JOIN }, /* SB */
	{ in_line_eoln, 0 }, /* BI */
	{ in_line_eoln, 0 }, /* IB */
	{ in_line_eoln, 0 }, /* BR */
	{ in_line_eoln, 0 }, /* RB */
	{ in_line_eoln, MAN_SCOPED | MAN_JOIN }, /* R */
	{ in_line_eoln, MAN_SCOPED | MAN_JOIN }, /* B */
	{ in_line_eoln, MAN_SCOPED | MAN_JOIN }, /* I */
	{ in_line_eoln, 0 }, /* IR */
	{ in_line_eoln, 0 }, /* RI */
	{ in_line_eoln, MAN_NSCOPED }, /* na */
	{ in_line_eoln, MAN_NSCOPED }, /* sp */
	{ in_line_eoln, MAN_BSCOPE }, /* nf */
	{ in_line_eoln, MAN_BSCOPE }, /* fi */
	{ blk_close, 0 }, /* RE */
	{ blk_exp, MAN_BSCOPE | MAN_EXPLICIT }, /* RS */
	{ in_line_eoln, 0 }, /* DT */
	{ in_line_eoln, 0 }, /* UC */
	{ in_line_eoln, 0 }, /* PD */
	{ in_line_eoln, 0 }, /* AT */
	{ in_line_eoln, 0 }, /* in */
	{ in_line_eoln, 0 }, /* ft */
	{ in_line_eoln, 0 }, /* OP */
	{ in_line_eoln, MAN_BSCOPE }, /* EX */
	{ in_line_eoln, MAN_BSCOPE }, /* EE */
	{ blk_exp, MAN_BSCOPE | MAN_EXPLICIT }, /* UR */
	{ blk_close, 0 }, /* UE */
	{ in_line_eoln, 0 }, /* ll */
};

const	struct man_macro * const man_macros = __man_macros;


int
man_unscope(struct man *man, const struct man_node *to)
{
	struct man_node	*n;

	to = to->parent;
	n = man->last;
	while (n != to) {

		/* Reached the end of the document? */

		if (to == NULL && ! (n->flags & MAN_VALID)) {
			if (man->flags & (MAN_BLINE | MAN_ELINE) &&
			    man_macros[n->tok].flags & MAN_SCOPED) {
				mandoc_vmsg(MANDOCERR_BLK_LINE,
				    man->parse, n->line, n->pos,
				    "EOF breaks %s",
				    man_macronames[n->tok]);
				if (man->flags & MAN_ELINE)
					man->flags &= ~MAN_ELINE;
				else {
					assert(n->type == MAN_HEAD);
					n = n->parent;
					man->flags &= ~MAN_BLINE;
				}
				man->last = n;
				n = n->parent;
				man_node_delete(man, man->last);
				continue;
			}
			if (n->type == MAN_BLOCK &&
			    man_macros[n->tok].flags & MAN_EXPLICIT)
				mandoc_msg(MANDOCERR_BLK_NOEND,
				    man->parse, n->line, n->pos,
				    man_macronames[n->tok]);
		}

		/*
		 * We might delete the man->last node
		 * in the post-validation phase.
		 * Save a pointer to the parent such that
		 * we know where to continue the iteration.
		 */

		man->last = n;
		n = n->parent;
		if ( ! man_valid_post(man))
			return(0);
	}

	/*
	 * If we ended up at the parent of the node we were
	 * supposed to rewind to, that means the target node
	 * got deleted, so add the next node we parse as a child
	 * of the parent instead of as a sibling of the target.
	 */

	man->next = (man->last == to) ?
	    MAN_NEXT_CHILD : MAN_NEXT_SIBLING;

	return(1);
}

static enum rew
rew_block(enum mant ntok, enum man_type type, const struct man_node *n)
{

	if (MAN_BLOCK == type && ntok == n->parent->tok &&
	    MAN_BODY == n->parent->type)
		return(REW_REWIND);
	return(ntok == n->tok ? REW_HALT : REW_NOHALT);
}

/*
 * There are three scope levels: scoped to the root (all), scoped to the
 * section (all less sections), and scoped to subsections (all less
 * sections and subsections).
 */
static enum rew
rew_dohalt(enum mant tok, enum man_type type, const struct man_node *n)
{
	enum rew	 c;

	/* We cannot progress beyond the root ever. */
	if (MAN_ROOT == n->type)
		return(REW_HALT);

	assert(n->parent);

	/* Normal nodes shouldn't go to the level of the root. */
	if (MAN_ROOT == n->parent->type)
		return(REW_REWIND);

	/* Already-validated nodes should be closed out. */
	if (MAN_VALID & n->flags)
		return(REW_NOHALT);

	/* First: rewind to ourselves. */
	if (type == n->type && tok == n->tok) {
		if (MAN_EXPLICIT & man_macros[n->tok].flags)
			return(REW_HALT);
		else
			return(REW_REWIND);
	}

	/*
	 * Next follow the implicit scope-smashings as defined by man.7:
	 * section, sub-section, etc.
	 */

	switch (tok) {
	case MAN_SH:
		break;
	case MAN_SS:
		/* Rewind to a section, if a block. */
		if (REW_NOHALT != (c = rew_block(MAN_SH, type, n)))
			return(c);
		break;
	case MAN_RS:
		/* Preserve empty paragraphs before RS. */
		if (0 == n->nchild && (MAN_P == n->tok ||
		    MAN_PP == n->tok || MAN_LP == n->tok))
			return(REW_HALT);
		/* Rewind to a subsection, if a block. */
		if (REW_NOHALT != (c = rew_block(MAN_SS, type, n)))
			return(c);
		/* Rewind to a section, if a block. */
		if (REW_NOHALT != (c = rew_block(MAN_SH, type, n)))
			return(c);
		break;
	default:
		/* Rewind to an offsetter, if a block. */
		if (REW_NOHALT != (c = rew_block(MAN_RS, type, n)))
			return(c);
		/* Rewind to a subsection, if a block. */
		if (REW_NOHALT != (c = rew_block(MAN_SS, type, n)))
			return(c);
		/* Rewind to a section, if a block. */
		if (REW_NOHALT != (c = rew_block(MAN_SH, type, n)))
			return(c);
		break;
	}

	return(REW_NOHALT);
}

/*
 * Rewinding entails ascending the parse tree until a coherent point,
 * for example, the `SH' macro will close out any intervening `SS'
 * scopes.  When a scope is closed, it must be validated and actioned.
 */
static int
rew_scope(enum man_type type, struct man *man, enum mant tok)
{
	struct man_node	*n;
	enum rew	 c;

	for (n = man->last; n; n = n->parent) {
		/*
		 * Whether we should stop immediately (REW_HALT), stop
		 * and rewind until this point (REW_REWIND), or keep
		 * rewinding (REW_NOHALT).
		 */
		c = rew_dohalt(tok, type, n);
		if (REW_HALT == c)
			return(1);
		if (REW_REWIND == c)
			break;
	}

	/*
	 * Rewind until the current point.  Warn if we're a roff
	 * instruction that's mowing over explicit scopes.
	 */
	assert(n);

	return(man_unscope(man, n));
}


/*
 * Close out a generic explicit macro.
 */
int
blk_close(MACRO_PROT_ARGS)
{
	enum mant		 ntok;
	const struct man_node	*nn;

	switch (tok) {
	case MAN_RE:
		ntok = MAN_RS;
		break;
	case MAN_UE:
		ntok = MAN_UR;
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	for (nn = man->last->parent; nn; nn = nn->parent)
		if (ntok == nn->tok && MAN_BLOCK == nn->type)
			break;

	if (NULL == nn) {
		mandoc_msg(MANDOCERR_BLK_NOTOPEN, man->parse,
		    line, ppos, man_macronames[tok]);
		if ( ! rew_scope(MAN_BLOCK, man, MAN_PP))
			return(0);
	} else
		man_unscope(man, nn);

	return(1);
}

int
blk_exp(MACRO_PROT_ARGS)
{
	struct man_node	*n;
	int		 la;
	char		*p;

	/* Close out prior implicit scopes. */

	if ( ! rew_scope(MAN_BLOCK, man, tok))
		return(0);

	if ( ! man_block_alloc(man, line, ppos, tok))
		return(0);
	if ( ! man_head_alloc(man, line, ppos, tok))
		return(0);

	for (;;) {
		la = *pos;
		if ( ! man_args(man, line, pos, buf, &p))
			break;
		if ( ! man_word_alloc(man, line, la, p))
			return(0);
	}

	assert(man);
	assert(tok != MAN_MAX);

	for (n = man->last; n; n = n->parent) {
		if (n->tok != tok)
			continue;
		assert(MAN_HEAD == n->type);
		man_unscope(man, n);
		break;
	}

	return(man_body_alloc(man, line, ppos, tok));
}

/*
 * Parse an implicit-block macro.  These contain a MAN_HEAD and a
 * MAN_BODY contained within a MAN_BLOCK.  Rules for closing out other
 * scopes, such as `SH' closing out an `SS', are defined in the rew
 * routines.
 */
int
blk_imp(MACRO_PROT_ARGS)
{
	int		 la;
	char		*p;
	struct man_node	*n;

	/* Close out prior scopes. */

	if ( ! rew_scope(MAN_BODY, man, tok))
		return(0);
	if ( ! rew_scope(MAN_BLOCK, man, tok))
		return(0);

	/* Allocate new block & head scope. */

	if ( ! man_block_alloc(man, line, ppos, tok))
		return(0);
	if ( ! man_head_alloc(man, line, ppos, tok))
		return(0);

	n = man->last;

	/* Add line arguments. */

	for (;;) {
		la = *pos;
		if ( ! man_args(man, line, pos, buf, &p))
			break;
		if ( ! man_word_alloc(man, line, la, p))
			return(0);
	}

	/* Close out head and open body (unless MAN_SCOPE). */

	if (MAN_SCOPED & man_macros[tok].flags) {
		/* If we're forcing scope (`TP'), keep it open. */
		if (MAN_FSCOPED & man_macros[tok].flags) {
			man->flags |= MAN_BLINE;
			return(1);
		} else if (n == man->last) {
			man->flags |= MAN_BLINE;
			return(1);
		}
	}

	if ( ! rew_scope(MAN_HEAD, man, tok))
		return(0);
	return(man_body_alloc(man, line, ppos, tok));
}

int
in_line_eoln(MACRO_PROT_ARGS)
{
	int		 la;
	char		*p;
	struct man_node	*n;

	if ( ! man_elem_alloc(man, line, ppos, tok))
		return(0);

	n = man->last;

	for (;;) {
		la = *pos;
		if ( ! man_args(man, line, pos, buf, &p))
			break;
		if (man_macros[tok].flags & MAN_JOIN &&
		    man->last->type == MAN_TEXT)
			man_word_append(man, p);
		else if ( ! man_word_alloc(man, line, la, p))
			return(0);
	}

	/*
	 * Append MAN_EOS in case the last snipped argument
	 * ends with a dot, e.g. `.IR syslog (3).'
	 */

	if (n != man->last &&
	    mandoc_eos(man->last->string, strlen(man->last->string)))
		man->last->flags |= MAN_EOS;

	/*
	 * If no arguments are specified and this is MAN_SCOPED (i.e.,
	 * next-line scoped), then set our mode to indicate that we're
	 * waiting for terms to load into our context.
	 */

	if (n == man->last && MAN_SCOPED & man_macros[tok].flags) {
		assert( ! (MAN_NSCOPED & man_macros[tok].flags));
		man->flags |= MAN_ELINE;
		return(1);
	}

	assert(MAN_ROOT != man->last->type);
	man->next = MAN_NEXT_SIBLING;

	/*
	 * Rewind our element scope.  Note that when TH is pruned, we'll
	 * be back at the root, so make sure that we don't clobber as
	 * its sibling.
	 */

	for ( ; man->last; man->last = man->last->parent) {
		if (man->last == n)
			break;
		if (man->last->type == MAN_ROOT)
			break;
		if ( ! man_valid_post(man))
			return(0);
	}

	assert(man->last);

	/*
	 * Same here regarding whether we're back at the root.
	 */

	if (man->last->type != MAN_ROOT && ! man_valid_post(man))
		return(0);

	return(1);
}


int
man_macroend(struct man *man)
{

	return(man_unscope(man, man->first));
}

static int
man_args(struct man *man, int line, int *pos, char *buf, char **v)
{
	char	 *start;

	assert(*pos);
	*v = start = buf + *pos;
	assert(' ' != *start);

	if ('\0' == *start)
		return(0);

	*v = mandoc_getarg(man->parse, v, line, pos);
	return(1);
}
