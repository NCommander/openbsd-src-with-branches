/*	$Id: mdoc_html.c,v 1.31 2010/09/26 18:23:54 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mandoc.h"
#include "out.h"
#include "html.h"
#include "mdoc.h"
#include "main.h"

#define	INDENT		 5
#define	HALFINDENT	 3

#define	MDOC_ARGS	  const struct mdoc_meta *m, \
			  const struct mdoc_node *n, \
			  struct html *h

#ifndef MIN
#define	MIN(a,b)	((/*CONSTCOND*/(a)<(b))?(a):(b))
#endif

struct	htmlmdoc {
	int		(*pre)(MDOC_ARGS);
	void		(*post)(MDOC_ARGS);
};

static	void		  print_mdoc(MDOC_ARGS);
static	void		  print_mdoc_head(MDOC_ARGS);
static	void		  print_mdoc_node(MDOC_ARGS);
static	void		  print_mdoc_nodelist(MDOC_ARGS);
static	void	  	  synopsis_pre(struct html *, 
				const struct mdoc_node *);

static	void		  a2width(const char *, struct roffsu *);
static	void		  a2offs(const char *, struct roffsu *);

static	void		  mdoc_root_post(MDOC_ARGS);
static	int		  mdoc_root_pre(MDOC_ARGS);

static	void		  mdoc__x_post(MDOC_ARGS);
static	int		  mdoc__x_pre(MDOC_ARGS);
static	int		  mdoc_ad_pre(MDOC_ARGS);
static	int		  mdoc_an_pre(MDOC_ARGS);
static	int		  mdoc_ap_pre(MDOC_ARGS);
static	int		  mdoc_ar_pre(MDOC_ARGS);
static	int		  mdoc_bd_pre(MDOC_ARGS);
static	int		  mdoc_bf_pre(MDOC_ARGS);
static	void		  mdoc_bk_post(MDOC_ARGS);
static	int		  mdoc_bk_pre(MDOC_ARGS);
static	void		  mdoc_bl_post(MDOC_ARGS);
static	int		  mdoc_bl_pre(MDOC_ARGS);
static	int		  mdoc_bt_pre(MDOC_ARGS);
static	int		  mdoc_bx_pre(MDOC_ARGS);
static	int		  mdoc_cd_pre(MDOC_ARGS);
static	int		  mdoc_d1_pre(MDOC_ARGS);
static	int		  mdoc_dv_pre(MDOC_ARGS);
static	int		  mdoc_fa_pre(MDOC_ARGS);
static	int		  mdoc_fd_pre(MDOC_ARGS);
static	int		  mdoc_fl_pre(MDOC_ARGS);
static	int		  mdoc_fn_pre(MDOC_ARGS);
static	int		  mdoc_ft_pre(MDOC_ARGS);
static	int		  mdoc_em_pre(MDOC_ARGS);
static	int		  mdoc_er_pre(MDOC_ARGS);
static	int		  mdoc_ev_pre(MDOC_ARGS);
static	int		  mdoc_ex_pre(MDOC_ARGS);
static	void		  mdoc_fo_post(MDOC_ARGS);
static	int		  mdoc_fo_pre(MDOC_ARGS);
static	int		  mdoc_ic_pre(MDOC_ARGS);
static	int		  mdoc_in_pre(MDOC_ARGS);
static	int		  mdoc_it_block_pre(MDOC_ARGS, enum mdoc_list,
				int, struct roffsu *, struct roffsu *);
static	int		  mdoc_it_head_pre(MDOC_ARGS, enum mdoc_list, 
				struct roffsu *);
static	int		  mdoc_it_body_pre(MDOC_ARGS, enum mdoc_list,
				struct roffsu *);
static	int		  mdoc_it_pre(MDOC_ARGS);
static	int		  mdoc_lb_pre(MDOC_ARGS);
static	int		  mdoc_li_pre(MDOC_ARGS);
static	int		  mdoc_lk_pre(MDOC_ARGS);
static	int		  mdoc_mt_pre(MDOC_ARGS);
static	int		  mdoc_ms_pre(MDOC_ARGS);
static	int		  mdoc_nd_pre(MDOC_ARGS);
static	int		  mdoc_nm_pre(MDOC_ARGS);
static	int		  mdoc_ns_pre(MDOC_ARGS);
static	int		  mdoc_pa_pre(MDOC_ARGS);
static	void		  mdoc_pf_post(MDOC_ARGS);
static	int		  mdoc_pf_pre(MDOC_ARGS);
static	void		  mdoc_quote_post(MDOC_ARGS);
static	int		  mdoc_quote_pre(MDOC_ARGS);
static	int		  mdoc_rs_pre(MDOC_ARGS);
static	int		  mdoc_rv_pre(MDOC_ARGS);
static	int		  mdoc_sh_pre(MDOC_ARGS);
static	int		  mdoc_sm_pre(MDOC_ARGS);
static	int		  mdoc_sp_pre(MDOC_ARGS);
static	int		  mdoc_ss_pre(MDOC_ARGS);
static	int		  mdoc_sx_pre(MDOC_ARGS);
static	int		  mdoc_sy_pre(MDOC_ARGS);
static	int		  mdoc_ud_pre(MDOC_ARGS);
static	int		  mdoc_va_pre(MDOC_ARGS);
static	int		  mdoc_vt_pre(MDOC_ARGS);
static	int		  mdoc_xr_pre(MDOC_ARGS);
static	int		  mdoc_xx_pre(MDOC_ARGS);

static	const struct htmlmdoc mdocs[MDOC_MAX] = {
	{mdoc_ap_pre, NULL}, /* Ap */
	{NULL, NULL}, /* Dd */
	{NULL, NULL}, /* Dt */
	{NULL, NULL}, /* Os */
	{mdoc_sh_pre, NULL }, /* Sh */
	{mdoc_ss_pre, NULL }, /* Ss */ 
	{mdoc_sp_pre, NULL}, /* Pp */ 
	{mdoc_d1_pre, NULL}, /* D1 */
	{mdoc_d1_pre, NULL}, /* Dl */
	{mdoc_bd_pre, NULL}, /* Bd */
	{NULL, NULL}, /* Ed */
	{mdoc_bl_pre, mdoc_bl_post}, /* Bl */
	{NULL, NULL}, /* El */
	{mdoc_it_pre, NULL}, /* It */
	{mdoc_ad_pre, NULL}, /* Ad */ 
	{mdoc_an_pre, NULL}, /* An */
	{mdoc_ar_pre, NULL}, /* Ar */
	{mdoc_cd_pre, NULL}, /* Cd */
	{mdoc_fl_pre, NULL}, /* Cm */
	{mdoc_dv_pre, NULL}, /* Dv */ 
	{mdoc_er_pre, NULL}, /* Er */ 
	{mdoc_ev_pre, NULL}, /* Ev */ 
	{mdoc_ex_pre, NULL}, /* Ex */
	{mdoc_fa_pre, NULL}, /* Fa */ 
	{mdoc_fd_pre, NULL}, /* Fd */ 
	{mdoc_fl_pre, NULL}, /* Fl */
	{mdoc_fn_pre, NULL}, /* Fn */ 
	{mdoc_ft_pre, NULL}, /* Ft */ 
	{mdoc_ic_pre, NULL}, /* Ic */ 
	{mdoc_in_pre, NULL}, /* In */ 
	{mdoc_li_pre, NULL}, /* Li */
	{mdoc_nd_pre, NULL}, /* Nd */ 
	{mdoc_nm_pre, NULL}, /* Nm */ 
	{mdoc_quote_pre, mdoc_quote_post}, /* Op */
	{NULL, NULL}, /* Ot */
	{mdoc_pa_pre, NULL}, /* Pa */
	{mdoc_rv_pre, NULL}, /* Rv */
	{NULL, NULL}, /* St */ 
	{mdoc_va_pre, NULL}, /* Va */
	{mdoc_vt_pre, NULL}, /* Vt */ 
	{mdoc_xr_pre, NULL}, /* Xr */
	{mdoc__x_pre, mdoc__x_post}, /* %A */
	{mdoc__x_pre, mdoc__x_post}, /* %B */
	{mdoc__x_pre, mdoc__x_post}, /* %D */
	{mdoc__x_pre, mdoc__x_post}, /* %I */
	{mdoc__x_pre, mdoc__x_post}, /* %J */
	{mdoc__x_pre, mdoc__x_post}, /* %N */
	{mdoc__x_pre, mdoc__x_post}, /* %O */
	{mdoc__x_pre, mdoc__x_post}, /* %P */
	{mdoc__x_pre, mdoc__x_post}, /* %R */
	{mdoc__x_pre, mdoc__x_post}, /* %T */
	{mdoc__x_pre, mdoc__x_post}, /* %V */
	{NULL, NULL}, /* Ac */
	{mdoc_quote_pre, mdoc_quote_post}, /* Ao */
	{mdoc_quote_pre, mdoc_quote_post}, /* Aq */
	{NULL, NULL}, /* At */
	{NULL, NULL}, /* Bc */
	{mdoc_bf_pre, NULL}, /* Bf */ 
	{mdoc_quote_pre, mdoc_quote_post}, /* Bo */
	{mdoc_quote_pre, mdoc_quote_post}, /* Bq */
	{mdoc_xx_pre, NULL}, /* Bsx */
	{mdoc_bx_pre, NULL}, /* Bx */
	{NULL, NULL}, /* Db */
	{NULL, NULL}, /* Dc */
	{mdoc_quote_pre, mdoc_quote_post}, /* Do */
	{mdoc_quote_pre, mdoc_quote_post}, /* Dq */
	{NULL, NULL}, /* Ec */ /* FIXME: no space */
	{NULL, NULL}, /* Ef */
	{mdoc_em_pre, NULL}, /* Em */ 
	{NULL, NULL}, /* Eo */
	{mdoc_xx_pre, NULL}, /* Fx */
	{mdoc_ms_pre, NULL}, /* Ms */
	{NULL, NULL}, /* No */
	{mdoc_ns_pre, NULL}, /* Ns */
	{mdoc_xx_pre, NULL}, /* Nx */
	{mdoc_xx_pre, NULL}, /* Ox */
	{NULL, NULL}, /* Pc */
	{mdoc_pf_pre, mdoc_pf_post}, /* Pf */
	{mdoc_quote_pre, mdoc_quote_post}, /* Po */
	{mdoc_quote_pre, mdoc_quote_post}, /* Pq */
	{NULL, NULL}, /* Qc */
	{mdoc_quote_pre, mdoc_quote_post}, /* Ql */
	{mdoc_quote_pre, mdoc_quote_post}, /* Qo */
	{mdoc_quote_pre, mdoc_quote_post}, /* Qq */
	{NULL, NULL}, /* Re */
	{mdoc_rs_pre, NULL}, /* Rs */
	{NULL, NULL}, /* Sc */
	{mdoc_quote_pre, mdoc_quote_post}, /* So */
	{mdoc_quote_pre, mdoc_quote_post}, /* Sq */
	{mdoc_sm_pre, NULL}, /* Sm */ 
	{mdoc_sx_pre, NULL}, /* Sx */
	{mdoc_sy_pre, NULL}, /* Sy */
	{NULL, NULL}, /* Tn */
	{mdoc_xx_pre, NULL}, /* Ux */
	{NULL, NULL}, /* Xc */
	{NULL, NULL}, /* Xo */
	{mdoc_fo_pre, mdoc_fo_post}, /* Fo */ 
	{NULL, NULL}, /* Fc */ 
	{mdoc_quote_pre, mdoc_quote_post}, /* Oo */
	{NULL, NULL}, /* Oc */
	{mdoc_bk_pre, mdoc_bk_post}, /* Bk */
	{NULL, NULL}, /* Ek */
	{mdoc_bt_pre, NULL}, /* Bt */
	{NULL, NULL}, /* Hf */
	{NULL, NULL}, /* Fr */
	{mdoc_ud_pre, NULL}, /* Ud */
	{mdoc_lb_pre, NULL}, /* Lb */
	{mdoc_sp_pre, NULL}, /* Lp */ 
	{mdoc_lk_pre, NULL}, /* Lk */ 
	{mdoc_mt_pre, NULL}, /* Mt */ 
	{mdoc_quote_pre, mdoc_quote_post}, /* Brq */ 
	{mdoc_quote_pre, mdoc_quote_post}, /* Bro */ 
	{NULL, NULL}, /* Brc */ 
	{mdoc__x_pre, mdoc__x_post}, /* %C */ 
	{NULL, NULL}, /* Es */  /* TODO */
	{NULL, NULL}, /* En */  /* TODO */
	{mdoc_xx_pre, NULL}, /* Dx */ 
	{mdoc__x_pre, mdoc__x_post}, /* %Q */ 
	{mdoc_sp_pre, NULL}, /* br */
	{mdoc_sp_pre, NULL}, /* sp */ 
	{mdoc__x_pre, mdoc__x_post}, /* %U */ 
	{NULL, NULL}, /* Ta */ 
};


void
html_mdoc(void *arg, const struct mdoc *m)
{
	struct html 	*h;
	struct tag	*t;

	h = (struct html *)arg;

	print_gen_decls(h);
	t = print_otag(h, TAG_HTML, 0, NULL);
	print_mdoc(mdoc_meta(m), mdoc_node(m), h);
	print_tagq(h, t);

	printf("\n");
}


/*
 * Calculate the scaling unit passed in a `-width' argument.  This uses
 * either a native scaling unit (e.g., 1i, 2m) or the string length of
 * the value.
 */
static void
a2width(const char *p, struct roffsu *su)
{

	if ( ! a2roffsu(p, su, SCALE_MAX)) {
		su->unit = SCALE_EM;
		su->scale = (int)strlen(p);
	}
}


/*
 * See the same function in mdoc_term.c for documentation.
 */
static void
synopsis_pre(struct html *h, const struct mdoc_node *n)
{
	struct roffsu	 su;
	struct htmlpair	 tag;

	if (NULL == n->prev || ! (MDOC_SYNPRETTY & n->flags))
		return;

	SCALE_VS_INIT(&su, 1);
	bufcat_su(h, "margin-top", &su);
	PAIR_STYLE_INIT(&tag, h);

	if (n->prev->tok == n->tok && 
			MDOC_Fo != n->tok && 
			MDOC_Ft != n->tok && 
			MDOC_Fn != n->tok) {
		print_otag(h, TAG_DIV, 0, NULL);
		return;
	}

	switch (n->prev->tok) {
	case (MDOC_Fd):
		/* FALLTHROUGH */
	case (MDOC_Fn):
		/* FALLTHROUGH */
	case (MDOC_Fo):
		/* FALLTHROUGH */
	case (MDOC_In):
		/* FALLTHROUGH */
	case (MDOC_Vt):
		print_otag(h, TAG_DIV, 1, &tag);
		break;
	case (MDOC_Ft):
		if (MDOC_Fn != n->tok && MDOC_Fo != n->tok) {
			print_otag(h, TAG_DIV, 1, &tag);
			break;
		}
		/* FALLTHROUGH */
	default:
		print_otag(h, TAG_DIV, 0, NULL);
		break;
	}
}


/*
 * Calculate the scaling unit passed in an `-offset' argument.  This
 * uses either a native scaling unit (e.g., 1i, 2m), one of a set of
 * predefined strings (indent, etc.), or the string length of the value.
 */
static void
a2offs(const char *p, struct roffsu *su)
{

	/* FIXME: "right"? */

	if (0 == strcmp(p, "left"))
		SCALE_HS_INIT(su, 0);
	else if (0 == strcmp(p, "indent"))
		SCALE_HS_INIT(su, INDENT);
	else if (0 == strcmp(p, "indent-two"))
		SCALE_HS_INIT(su, INDENT * 2);
	else if ( ! a2roffsu(p, su, SCALE_MAX)) {
		su->unit = SCALE_EM;
		su->scale = (int)strlen(p);
	}
}


static void
print_mdoc(MDOC_ARGS)
{
	struct tag	*t;
	struct htmlpair	 tag;

	t = print_otag(h, TAG_HEAD, 0, NULL);
	print_mdoc_head(m, n, h);
	print_tagq(h, t);

	t = print_otag(h, TAG_BODY, 0, NULL);

	tag.key = ATTR_CLASS;
	tag.val = "body";
	print_otag(h, TAG_DIV, 1, &tag);

	print_mdoc_nodelist(m, n, h);
	print_tagq(h, t);
}


/* ARGSUSED */
static void
print_mdoc_head(MDOC_ARGS)
{

	print_gen_head(h);
	bufinit(h);
	buffmt(h, "%s(%s)", m->title, m->msec);

	if (m->arch) {
		bufcat(h, " (");
		bufcat(h, m->arch);
		bufcat(h, ")");
	}

	print_otag(h, TAG_TITLE, 0, NULL);
	print_text(h, h->buf);
}


static void
print_mdoc_nodelist(MDOC_ARGS)
{

	print_mdoc_node(m, n, h);
	if (n->next)
		print_mdoc_nodelist(m, n->next, h);
}


static void
print_mdoc_node(MDOC_ARGS)
{
	int		 child;
	struct tag	*t;

	child = 1;
	t = h->tags.head;

	bufinit(h);
	switch (n->type) {
	case (MDOC_ROOT):
		child = mdoc_root_pre(m, n, h);
		break;
	case (MDOC_TEXT):
		print_text(h, n->string);
		return;
	default:
		if (mdocs[n->tok].pre && ENDBODY_NOT == n->end)
			child = (*mdocs[n->tok].pre)(m, n, h);
		break;
	}

	if (HTML_KEEP & h->flags) {
		if (n->prev && n->prev->line != n->line) {
			h->flags &= ~HTML_KEEP;
			h->flags |= HTML_PREKEEP;
		} else if (NULL == n->prev) {
			if (n->parent && n->parent->line != n->line) {
				h->flags &= ~HTML_KEEP;
				h->flags |= HTML_PREKEEP;
			}
		}
	}

	if (child && n->child)
		print_mdoc_nodelist(m, n->child, h);

	print_stagq(h, t);

	bufinit(h);
	switch (n->type) {
	case (MDOC_ROOT):
		mdoc_root_post(m, n, h);
		break;
	default:
		if (mdocs[n->tok].post && ENDBODY_NOT == n->end)
			(*mdocs[n->tok].post)(m, n, h);
		break;
	}
}


/* ARGSUSED */
static void
mdoc_root_post(MDOC_ARGS)
{
	struct htmlpair	 tag[3];
	struct tag	*t, *tt;
	char		 b[DATESIZ];

	time2a(m->date, b, DATESIZ);

	/*
	 * XXX: this should use divs, but in Firefox, divs with nested
	 * divs for some reason puke when trying to put a border line
	 * below.  So I use tables, instead.
	 */

	PAIR_CLASS_INIT(&tag[0], "footer");
	bufcat_style(h, "width", "100%");
	PAIR_STYLE_INIT(&tag[1], h);
	PAIR_SUMMARY_INIT(&tag[2], "footer");

	t = print_otag(h, TAG_TABLE, 3, tag);
	tt = print_otag(h, TAG_TR, 0, NULL);

	bufinit(h);
	bufcat_style(h, "width", "50%");
	PAIR_STYLE_INIT(&tag[0], h);
	print_otag(h, TAG_TD, 1, tag);
	print_text(h, b);
	print_stagq(h, tt);

	bufinit(h);
	bufcat_style(h, "width", "50%");
	bufcat_style(h, "text-align", "right");
	PAIR_STYLE_INIT(&tag[0], h);
	print_otag(h, TAG_TD, 1, tag);
	print_text(h, m->os);
	print_tagq(h, t);
}


/* ARGSUSED */
static int
mdoc_root_pre(MDOC_ARGS)
{
	struct htmlpair	 tag[3];
	struct tag	*t, *tt;
	char		 b[BUFSIZ], title[BUFSIZ];

	(void)strlcpy(b, m->vol, BUFSIZ);

	if (m->arch) {
		(void)strlcat(b, " (", BUFSIZ);
		(void)strlcat(b, m->arch, BUFSIZ);
		(void)strlcat(b, ")", BUFSIZ);
	}

	(void)snprintf(title, BUFSIZ - 1, 
			"%s(%s)", m->title, m->msec);

	/* XXX: see note in mdoc_root_post() about divs. */

	PAIR_CLASS_INIT(&tag[0], "header");
	bufcat_style(h, "width", "100%");
	PAIR_STYLE_INIT(&tag[1], h);
	PAIR_SUMMARY_INIT(&tag[2], "header");

	t = print_otag(h, TAG_TABLE, 3, tag);

	tt = print_otag(h, TAG_TR, 0, NULL);

	bufinit(h);
	bufcat_style(h, "width", "10%");
	PAIR_STYLE_INIT(&tag[0], h);
	print_otag(h, TAG_TD, 1, tag);
	print_text(h, title);
	print_stagq(h, tt);

	bufinit(h);
	bufcat_style(h, "text-align", "center");
	bufcat_style(h, "white-space", "nowrap");
	bufcat_style(h, "width", "80%");
	PAIR_STYLE_INIT(&tag[0], h);
	print_otag(h, TAG_TD, 1, tag);
	print_text(h, b);
	print_stagq(h, tt);

	bufinit(h);
	bufcat_style(h, "text-align", "right");
	bufcat_style(h, "width", "10%");
	PAIR_STYLE_INIT(&tag[0], h);
	print_otag(h, TAG_TD, 1, tag);
	print_text(h, title);
	print_tagq(h, t);
	return(1);
}


/* ARGSUSED */
static int
mdoc_sh_pre(MDOC_ARGS)
{
	struct htmlpair		 tag[2];
	const struct mdoc_node	*nn;
	char			 buf[BUFSIZ];
	struct roffsu		 su;

	if (MDOC_BODY == n->type) {
		SCALE_HS_INIT(&su, INDENT);
		bufcat_su(h, "margin-left", &su);
		PAIR_CLASS_INIT(&tag[0], "sec-body");
		PAIR_STYLE_INIT(&tag[1], h);
		print_otag(h, TAG_DIV, 2, tag);
		return(1);
	} else if (MDOC_BLOCK == n->type) {
		PAIR_CLASS_INIT(&tag[0], "sec-block");
		if (n->prev && NULL == n->prev->body->child) {
			print_otag(h, TAG_DIV, 1, tag);
			return(1);
		}
		
		SCALE_VS_INIT(&su, 1);
		bufcat_su(h, "margin-top", &su);
		if (NULL == n->next)
			bufcat_su(h, "margin-bottom", &su);

		PAIR_STYLE_INIT(&tag[1], h);
		print_otag(h, TAG_DIV, 2, tag);
		return(1);
	}

	buf[0] = '\0';
	for (nn = n->child; nn; nn = nn->next) {
		html_idcat(buf, nn->string, BUFSIZ);
		if (nn->next)
			html_idcat(buf, " ", BUFSIZ);
	}

	PAIR_CLASS_INIT(&tag[0], "sec-head");
	PAIR_ID_INIT(&tag[1], buf);

	print_otag(h, TAG_DIV, 2, tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_ss_pre(MDOC_ARGS)
{
	struct htmlpair	 	 tag[3];
	const struct mdoc_node	*nn;
	char			 buf[BUFSIZ];
	struct roffsu		 su;

	SCALE_VS_INIT(&su, 1);

	if (MDOC_BODY == n->type) {
		PAIR_CLASS_INIT(&tag[0], "ssec-body");
		if (n->parent->next && n->child) {
			bufcat_su(h, "margin-bottom", &su);
			PAIR_STYLE_INIT(&tag[1], h);
			print_otag(h, TAG_DIV, 2, tag);
		} else
			print_otag(h, TAG_DIV, 1, tag);
		return(1);
	} else if (MDOC_BLOCK == n->type) {
		PAIR_CLASS_INIT(&tag[0], "ssec-block");
		if (n->prev) {
			bufcat_su(h, "margin-top", &su);
			PAIR_STYLE_INIT(&tag[1], h);
			print_otag(h, TAG_DIV, 2, tag);
		} else
			print_otag(h, TAG_DIV, 1, tag);
		return(1);
	}

	/* TODO: see note in mdoc_sh_pre() about duplicates. */

	buf[0] = '\0';
	for (nn = n->child; nn; nn = nn->next) {
		html_idcat(buf, nn->string, BUFSIZ);
		if (nn->next)
			html_idcat(buf, " ", BUFSIZ);
	}

	SCALE_HS_INIT(&su, INDENT - HALFINDENT);
	su.scale = -su.scale;
	bufcat_su(h, "margin-left", &su);

	PAIR_CLASS_INIT(&tag[0], "ssec-head");
	PAIR_STYLE_INIT(&tag[1], h);
	PAIR_ID_INIT(&tag[2], buf);

	print_otag(h, TAG_DIV, 3, tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_fl_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;

	PAIR_CLASS_INIT(&tag, "flag");
	print_otag(h, TAG_SPAN, 1, &tag);

	/* `Cm' has no leading hyphen. */

	if (MDOC_Cm == n->tok)
		return(1);

	print_text(h, "\\-");

	if (n->child)
		h->flags |= HTML_NOSPACE;
	else if (n->next && n->next->line == n->line)
		h->flags |= HTML_NOSPACE;

	return(1);
}


/* ARGSUSED */
static int
mdoc_nd_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;

	if (MDOC_BODY != n->type)
		return(1);

	/* XXX: this tag in theory can contain block elements. */

	print_text(h, "\\(em");
	PAIR_CLASS_INIT(&tag, "desc-body");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


static int
mdoc_nm_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;
	struct roffsu	 su;
	const char	*cp;

	/*
	 * Accomodate for `Nm' being both an element (which may have
	 * NULL children AND no m->name) and a block.
	 */

	cp = NULL;

	if (MDOC_ELEM == n->type) {
		if (NULL == n->child && NULL == m->name)
			return(1);
		synopsis_pre(h, n);
		PAIR_CLASS_INIT(&tag, "name");
		print_otag(h, TAG_SPAN, 1, &tag);
		if (NULL == n->child)
			print_text(h, m->name);
	} else if (MDOC_BLOCK == n->type) {
		synopsis_pre(h, n);

		bufcat_style(h, "clear", "both");
		if (n->head->child || m->name) {
			if (n->head->child && MDOC_TEXT == 
					n->head->child->type)
				cp = n->head->child->string;
			if (NULL == cp || '\0' == *cp)
				cp = m->name;

			SCALE_HS_INIT(&su, (double)strlen(cp));
			bufcat_su(h, "padding-left", &su);
		}

		PAIR_STYLE_INIT(&tag, h);
		print_otag(h, TAG_DIV, 1, &tag);
	} else if (MDOC_HEAD == n->type) { 
		if (NULL == n->child && NULL == m->name)
			return(1);

		if (n->child && MDOC_TEXT == n->child->type)
			cp = n->child->string;
		if (NULL == cp || '\0' == *cp)
			cp = m->name;

		SCALE_HS_INIT(&su, (double)strlen(cp));

		bufcat_style(h, "float", "left");
		bufcat_su(h, "min-width", &su);
		SCALE_INVERT(&su);
		bufcat_su(h, "margin-left", &su);

		PAIR_STYLE_INIT(&tag, h);
		print_otag(h, TAG_DIV, 1, &tag);

		if (NULL == n->child)
			print_text(h, m->name);
	} else if (MDOC_BODY == n->type) {
		SCALE_HS_INIT(&su, 2);
		bufcat_su(h, "margin-left", &su);
		PAIR_STYLE_INIT(&tag, h);
		print_otag(h, TAG_DIV, 1, &tag);
	}

	return(1);
}


/* ARGSUSED */
static int
mdoc_xr_pre(MDOC_ARGS)
{
	struct htmlpair	 	 tag[2];
	const struct mdoc_node	*nn;

	if (NULL == n->child)
		return(0);

	PAIR_CLASS_INIT(&tag[0], "link-man");

	if (h->base_man) {
		buffmt_man(h, n->child->string, 
				n->child->next ? 
				n->child->next->string : NULL);
		PAIR_HREF_INIT(&tag[1], h->buf);
		print_otag(h, TAG_A, 2, tag);
	} else
		print_otag(h, TAG_A, 1, tag);

	nn = n->child;
	print_text(h, nn->string);

	if (NULL == (nn = nn->next))
		return(0);

	h->flags |= HTML_NOSPACE;
	print_text(h, "(");
	h->flags |= HTML_NOSPACE;
	print_text(h, nn->string);
	h->flags |= HTML_NOSPACE;
	print_text(h, ")");
	return(0);
}


/* ARGSUSED */
static int
mdoc_ns_pre(MDOC_ARGS)
{

	h->flags |= HTML_NOSPACE;
	return(1);
}


/* ARGSUSED */
static int
mdoc_ar_pre(MDOC_ARGS)
{
	struct htmlpair tag;

	PAIR_CLASS_INIT(&tag, "arg");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_xx_pre(MDOC_ARGS)
{
	const char	*pp;
	struct htmlpair	 tag;

	switch (n->tok) {
	case (MDOC_Bsx):
		pp = "BSDI BSD/OS";
		break;
	case (MDOC_Dx):
		pp = "DragonFly";
		break;
	case (MDOC_Fx):
		pp = "FreeBSD";
		break;
	case (MDOC_Nx):
		pp = "NetBSD";
		break;
	case (MDOC_Ox):
		pp = "OpenBSD";
		break;
	case (MDOC_Ux):
		pp = "UNIX";
		break;
	default:
		return(1);
	}

	PAIR_CLASS_INIT(&tag, "unix");
	print_otag(h, TAG_SPAN, 1, &tag);
	print_text(h, pp);
	return(1);
}


/* ARGSUSED */
static int
mdoc_bx_pre(MDOC_ARGS)
{
	const struct mdoc_node	*nn;
	struct htmlpair		 tag;

	PAIR_CLASS_INIT(&tag, "unix");
	print_otag(h, TAG_SPAN, 1, &tag);

	for (nn = n->child; nn; nn = nn->next)
		print_mdoc_node(m, nn, h);

	if (n->child)
		h->flags |= HTML_NOSPACE;

	print_text(h, "BSD");
	return(0);
}


/* ARGSUSED */
static int
mdoc_it_block_pre(MDOC_ARGS, enum mdoc_list type, int comp,
		struct roffsu *offs, struct roffsu *width)
{
	struct htmlpair	 	 tag;
	const struct mdoc_node	*nn;
	struct roffsu		 su;

	nn = n->parent->parent;

	/* XXX: see notes in mdoc_it_pre(). */

	if (LIST_column == type) {
		/* Don't width-pad on the left. */
		SCALE_HS_INIT(width, 0);
		/* Also disallow non-compact. */
		comp = 1;
	}
	if (LIST_diag == type)
		/* Mandate non-compact with empty prior. */
		if (n->prev && NULL == n->prev->body->child)
			comp = 1;

	bufcat_style(h, "clear", "both");
	if (offs->scale > 0)
		bufcat_su(h, "margin-left", offs);
	if (width->scale > 0)
		bufcat_su(h, "padding-left", width);

	PAIR_STYLE_INIT(&tag, h);

	/* Mandate compact following `Ss' and `Sh' starts. */

	for (nn = n; nn && ! comp; nn = nn->parent) {
		if (MDOC_BLOCK != nn->type)
			continue;
		if (MDOC_Ss == nn->tok || MDOC_Sh == nn->tok)
			comp = 1;
		if (nn->prev)
			break;
	}

	if ( ! comp) {
		SCALE_VS_INIT(&su, 1);
		bufcat_su(h, "padding-top", &su);
	}

	PAIR_STYLE_INIT(&tag, h);
	print_otag(h, TAG_DIV, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_it_body_pre(MDOC_ARGS, enum mdoc_list type, struct roffsu *width)
{
	struct htmlpair	 tag;
	struct roffsu	 su;

	switch (type) {
	case (LIST_item):
		/* FALLTHROUGH */
	case (LIST_ohang):
		/* FALLTHROUGH */
	case (LIST_column):
		bufcat_su(h, "min-width", width);
		bufcat_style(h, "clear", "none");
		if (n->next)
			bufcat_style(h, "float", "left");
		PAIR_STYLE_INIT(&tag, h);
		print_otag(h, TAG_DIV, 1, &tag);
		break;
	default:
		/* 
		 * XXX: this tricks CSS into aligning the bodies with
		 * the right-padding in the head. 
		 */
		SCALE_HS_INIT(&su, 2);
		bufcat_su(h, "margin-left", &su);
		PAIR_STYLE_INIT(&tag, h);
		print_otag(h, TAG_DIV, 1, &tag);
		break;
	}

	return(1);
}


/* ARGSUSED */
static int
mdoc_it_head_pre(MDOC_ARGS, enum mdoc_list type, struct roffsu *width)
{
	struct htmlpair	 tag;
	struct ord	*ord;
	char		 nbuf[BUFSIZ];

	switch (type) {
	case (LIST_item):
		return(0);
	case (LIST_ohang):
		print_otag(h, TAG_DIV, 0, &tag);
		return(1);
	case (LIST_column):
		break;
	default:
		bufcat_su(h, "min-width", width);
		SCALE_INVERT(width);
		bufcat_su(h, "margin-left", width);
		if (n->next && n->next->child)
			bufcat_style(h, "float", "left");

		/* XXX: buffer if we run into body. */
		SCALE_HS_INIT(width, 1);
		bufcat_su(h, "margin-right", width);
		PAIR_STYLE_INIT(&tag, h);
		print_otag(h, TAG_DIV, 1, &tag);
		break;
	}

	switch (type) {
	case (LIST_diag):
		PAIR_CLASS_INIT(&tag, "diag");
		print_otag(h, TAG_SPAN, 1, &tag);
		break;
	case (LIST_enum):
		ord = h->ords.head;
		assert(ord);
		nbuf[BUFSIZ - 1] = 0;
		(void)snprintf(nbuf, BUFSIZ - 1, "%d.", ord->pos++);
		print_text(h, nbuf);
		return(0);
	case (LIST_dash):
		print_text(h, "\\(en");
		return(0);
	case (LIST_hyphen):
		print_text(h, "\\(hy");
		return(0);
	case (LIST_bullet):
		print_text(h, "\\(bu");
		return(0);
	default:
		break;
	}

	return(1);
}


static int
mdoc_it_pre(MDOC_ARGS)
{
	int			 i, comp;
	const struct mdoc_node	*bl, *nn;
	struct roffsu		 width, offs;
	enum mdoc_list		 type;

	/* 
	 * XXX: be very careful in changing anything, here.  Lists in
	 * mandoc have many peculiarities; furthermore, they don't
	 * translate well into HTML and require a bit of mangling.
	 */

	bl = n->parent->parent;
	if (MDOC_BLOCK != n->type)
		bl = bl->parent;

	SCALE_HS_INIT(&offs, 0);

	assert(bl->data.Bl);
	type = bl->data.Bl->type;
	comp = bl->data.Bl->comp;

	if (bl->data.Bl->offs)
		a2offs(bl->data.Bl->offs, &offs);

	switch (type) {
	case (LIST_enum):
		/* FALLTHROUGH */
	case (LIST_dash):
		/* FALLTHROUGH */
	case (LIST_hyphen):
		/* FALLTHROUGH */
	case (LIST_bullet):
		SCALE_HS_INIT(&width, 2);
		break;
	default:
		SCALE_HS_INIT(&width, INDENT);
		break;
	}

	if (bl->data.Bl->width)
		a2width(bl->data.Bl->width, &width);

	/* Override width in some cases. */

	switch (type) {
	case (LIST_ohang):
		/* FALLTHROUGH */
	case (LIST_item):
		/* FALLTHROUGH */
	case (LIST_inset):
		/* FALLTHROUGH */
	case (LIST_diag):
		SCALE_HS_INIT(&width, 0);
		break;
	default:
		if (0 == width.scale)
			SCALE_HS_INIT(&width, INDENT);
		break;
	}

	if (LIST_column == type && MDOC_BODY == n->type) {
		nn = n->parent->child;
		for (i = 0; nn && nn != n; nn = nn->next)
			if (MDOC_BODY == nn->type)
				i++;
		if (i < (int)bl->data.Bl->ncols)
			a2width(bl->data.Bl->cols[i], &width);
	}

	if (MDOC_HEAD == n->type)
		return(mdoc_it_head_pre(m, n, h, type, &width));
	else if (MDOC_BODY == n->type)
		return(mdoc_it_body_pre(m, n, h, type, &width));

	return(mdoc_it_block_pre(m, n, h, type, comp, &offs, &width));
}


/* ARGSUSED */
static int
mdoc_bl_pre(MDOC_ARGS)
{
	struct ord	*ord;

	if (MDOC_HEAD == n->type)
		return(0);
	if (MDOC_BLOCK != n->type)
		return(1);
	assert(n->data.Bl);
	if (LIST_enum != n->data.Bl->type)
		return(1);

	ord = malloc(sizeof(struct ord));
	if (NULL == ord) {
		perror(NULL);
		exit((int)MANDOCLEVEL_SYSERR);
	}
	ord->cookie = n;
	ord->pos = 1;
	ord->next = h->ords.head;
	h->ords.head = ord;
	return(1);
}


/* ARGSUSED */
static void
mdoc_bl_post(MDOC_ARGS)
{
	struct ord	*ord;

	if (MDOC_BLOCK != n->type)
		return;
	if (LIST_enum != n->data.Bl->type)
		return;

	ord = h->ords.head;
	assert(ord);
	h->ords.head = ord->next;
	free(ord);
}


/* ARGSUSED */
static int
mdoc_ex_pre(MDOC_ARGS)
{
	const struct mdoc_node	*nn;
	struct tag		*t;
	struct htmlpair		 tag;

	PAIR_CLASS_INIT(&tag, "utility");

	print_text(h, "The");
	for (nn = n->child; nn; nn = nn->next) {
		t = print_otag(h, TAG_SPAN, 1, &tag);
		print_text(h, nn->string);
		print_tagq(h, t);

		h->flags |= HTML_NOSPACE;

		if (nn->next && NULL == nn->next->next)
			print_text(h, ", and");
		else if (nn->next)
			print_text(h, ",");
		else
			h->flags &= ~HTML_NOSPACE;
	}

	if (n->child && n->child->next)
		print_text(h, "utilities exit");
	else
		print_text(h, "utility exits");

       	print_text(h, "0 on success, and >0 if an error occurs.");
	return(0);
}


/* ARGSUSED */
static int
mdoc_em_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "emph");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_d1_pre(MDOC_ARGS)
{
	struct htmlpair	 tag[2];
	struct roffsu	 su;

	if (MDOC_BLOCK != n->type)
		return(1);

	/* FIXME: D1 shouldn't be literal. */

	SCALE_VS_INIT(&su, INDENT - 2);
	bufcat_su(h, "margin-left", &su);
	PAIR_CLASS_INIT(&tag[0], "lit");
	PAIR_STYLE_INIT(&tag[1], h);
	print_otag(h, TAG_DIV, 2, tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_sx_pre(MDOC_ARGS)
{
	struct htmlpair		 tag[2];
	const struct mdoc_node	*nn;
	char			 buf[BUFSIZ];

	strlcpy(buf, "#", BUFSIZ);
	for (nn = n->child; nn; nn = nn->next) {
		html_idcat(buf, nn->string, BUFSIZ);
		if (nn->next)
			html_idcat(buf, " ", BUFSIZ);
	}

	PAIR_CLASS_INIT(&tag[0], "link-sec");
	PAIR_HREF_INIT(&tag[1], buf);

	print_otag(h, TAG_A, 2, tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_bd_pre(MDOC_ARGS)
{
	struct htmlpair	 	 tag[2];
	int		 	 comp;
	const struct mdoc_node	*nn;
	struct roffsu		 su;

	if (MDOC_HEAD == n->type)
		return(0);

	SCALE_VS_INIT(&su, 0);

	assert(n->data.Bd);
	if (n->data.Bd->offs)
		a2offs(n->data.Bd->offs, &su);

	comp = n->data.Bd->comp;

	/* FIXME: -centered, etc. formatting. */
	/* FIXME: does not respect -offset ??? */

	if (MDOC_BLOCK == n->type) {
		bufcat_su(h, "margin-left", &su);
		for (nn = n; nn && ! comp; nn = nn->parent) {
			if (MDOC_BLOCK != nn->type)
				continue;
			if (MDOC_Ss == nn->tok || MDOC_Sh == nn->tok)
				comp = 1;
			if (nn->prev)
				break;
		}
		if (comp) {
			PAIR_STYLE_INIT(&tag[0], h);
			print_otag(h, TAG_DIV, 1, tag);
			return(1);
		}
		SCALE_VS_INIT(&su, 1);
		bufcat_su(h, "margin-top", &su);
		PAIR_STYLE_INIT(&tag[0], h);
		print_otag(h, TAG_DIV, 1, tag);
		return(1);
	}

	if (DISP_unfilled != n->data.Bd->type && 
			DISP_literal != n->data.Bd->type)
		return(1);

	PAIR_CLASS_INIT(&tag[0], "lit");
	bufcat_style(h, "white-space", "pre");
	PAIR_STYLE_INIT(&tag[1], h);
	print_otag(h, TAG_DIV, 2, tag);

	for (nn = n->child; nn; nn = nn->next) {
		print_mdoc_node(m, nn, h);
		if (nn->next && nn->next->line == nn->line)
			continue;
		print_text(h, "\n");
		h->flags |= HTML_NOSPACE;
	}

	return(0);
}


/* ARGSUSED */
static int
mdoc_pa_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "file");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_ad_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "addr");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_an_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	/* TODO: -split and -nosplit (see termp_an_pre()). */

	PAIR_CLASS_INIT(&tag, "author");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_cd_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	synopsis_pre(h, n);
	PAIR_CLASS_INIT(&tag, "config");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_dv_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "define");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_ev_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "env");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_er_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "errno");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_fa_pre(MDOC_ARGS)
{
	const struct mdoc_node	*nn;
	struct htmlpair		 tag;
	struct tag		*t;

	PAIR_CLASS_INIT(&tag, "farg");
	if (n->parent->tok != MDOC_Fo) {
		print_otag(h, TAG_SPAN, 1, &tag);
		return(1);
	}

	for (nn = n->child; nn; nn = nn->next) {
		t = print_otag(h, TAG_SPAN, 1, &tag);
		print_text(h, nn->string);
		print_tagq(h, t);
		if (nn->next)
			print_text(h, ",");
	}

	if (n->child && n->next && n->next->tok == MDOC_Fa)
		print_text(h, ",");

	return(0);
}


/* ARGSUSED */
static int
mdoc_fd_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;

	synopsis_pre(h, n);

	PAIR_CLASS_INIT(&tag, "macro");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_vt_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;

	if (MDOC_BLOCK == n->type) {
		synopsis_pre(h, n);
		return(1);
	} else if (MDOC_ELEM == n->type) {
		synopsis_pre(h, n);
	} else if (MDOC_HEAD == n->type)
		return(0);

	PAIR_CLASS_INIT(&tag, "type");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_ft_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;

	synopsis_pre(h, n);
	PAIR_CLASS_INIT(&tag, "ftype");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_fn_pre(MDOC_ARGS)
{
	struct tag		*t;
	struct htmlpair	 	 tag[2];
	const struct mdoc_node	*nn;
	char			 nbuf[BUFSIZ];
	const char		*sp, *ep;
	int			 sz, i;

	synopsis_pre(h, n);

	/* Split apart into type and name. */
	assert(n->child->string);
	sp = n->child->string;

	ep = strchr(sp, ' ');
	if (NULL != ep) {
		PAIR_CLASS_INIT(&tag[0], "ftype");
		t = print_otag(h, TAG_SPAN, 1, tag);
	
		while (ep) {
			sz = MIN((int)(ep - sp), BUFSIZ - 1);
			(void)memcpy(nbuf, sp, (size_t)sz);
			nbuf[sz] = '\0';
			print_text(h, nbuf);
			sp = ++ep;
			ep = strchr(sp, ' ');
		}
		print_tagq(h, t);
	}

	PAIR_CLASS_INIT(&tag[0], "fname");

	/*
	 * FIXME: only refer to IDs that we know exist.
	 */

#if 0
	if (MDOC_SYNPRETTY & n->flags) {
		nbuf[0] = '\0';
		html_idcat(nbuf, sp, BUFSIZ);
		PAIR_ID_INIT(&tag[1], nbuf);
	} else {
		strlcpy(nbuf, "#", BUFSIZ);
		html_idcat(nbuf, sp, BUFSIZ);
		PAIR_HREF_INIT(&tag[1], nbuf);
	}
#endif

	t = print_otag(h, TAG_SPAN, 1, tag);

	if (sp) {
		strlcpy(nbuf, sp, BUFSIZ);
		print_text(h, nbuf);
	}

	print_tagq(h, t);

	h->flags |= HTML_NOSPACE;
	print_text(h, "(");

	bufinit(h);
	PAIR_CLASS_INIT(&tag[0], "farg");
	bufcat_style(h, "white-space", "nowrap");
	PAIR_STYLE_INIT(&tag[1], h);

	for (nn = n->child->next; nn; nn = nn->next) {
		i = 1;
		if (MDOC_SYNPRETTY & n->flags)
			i = 2;
		t = print_otag(h, TAG_SPAN, i, tag);
		print_text(h, nn->string);
		print_tagq(h, t);
		if (nn->next)
			print_text(h, ",");
	}

	print_text(h, ")");
	if (MDOC_SYNPRETTY & n->flags)
		print_text(h, ";");

	return(0);
}


/* ARGSUSED */
static int
mdoc_sm_pre(MDOC_ARGS)
{

	assert(n->child && MDOC_TEXT == n->child->type);
	if (0 == strcmp("on", n->child->string)) {
		/* FIXME: no p->col to check... */
		h->flags &= ~HTML_NOSPACE;
		h->flags &= ~HTML_NONOSPACE;
	} else
		h->flags |= HTML_NONOSPACE;

	return(0);
}


/* ARGSUSED */
static int
mdoc_sp_pre(MDOC_ARGS)
{
	int		 len;
	struct htmlpair	 tag;
	struct roffsu	 su;

	switch (n->tok) {
	case (MDOC_sp):
		/* FIXME: can this have a scaling indicator? */
		len = n->child ? atoi(n->child->string) : 1;
		break;
	case (MDOC_br):
		len = 0;
		break;
	default:
		assert(n->parent);
		if ((NULL == n->next || NULL == n->prev) &&
				(MDOC_Ss == n->parent->tok ||
				 MDOC_Sh == n->parent->tok))
			return(0);
		len = 1;
		break;
	}

	SCALE_VS_INIT(&su, len);
	bufcat_su(h, "height", &su);
	PAIR_STYLE_INIT(&tag, h);
	print_otag(h, TAG_DIV, 1, &tag);
	/* So the div isn't empty: */
	print_text(h, "\\~");

	return(0);

}


/* ARGSUSED */
static int
mdoc_lk_pre(MDOC_ARGS)
{
	const struct mdoc_node	*nn;
	struct htmlpair		 tag[2];

	nn = n->child;

	PAIR_CLASS_INIT(&tag[0], "link-ext");
	PAIR_HREF_INIT(&tag[1], nn->string);
	print_otag(h, TAG_A, 2, tag);

	if (NULL == nn->next) 
		return(1);

	for (nn = nn->next; nn; nn = nn->next) 
		print_text(h, nn->string);

	return(0);
}


/* ARGSUSED */
static int
mdoc_mt_pre(MDOC_ARGS)
{
	struct htmlpair	 	 tag[2];
	struct tag		*t;
	const struct mdoc_node	*nn;

	PAIR_CLASS_INIT(&tag[0], "link-mail");

	for (nn = n->child; nn; nn = nn->next) {
		bufinit(h);
		bufcat(h, "mailto:");
		bufcat(h, nn->string);
		PAIR_HREF_INIT(&tag[1], h->buf);
		t = print_otag(h, TAG_A, 2, tag);
		print_text(h, nn->string);
		print_tagq(h, t);
	}
	
	return(0);
}


/* ARGSUSED */
static int
mdoc_fo_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;
	struct tag	*t;

	if (MDOC_BODY == n->type) {
		h->flags |= HTML_NOSPACE;
		print_text(h, "(");
		h->flags |= HTML_NOSPACE;
		return(1);
	} else if (MDOC_BLOCK == n->type) {
		synopsis_pre(h, n);
		return(1);
	}

	/* XXX: we drop non-initial arguments as per groff. */

	assert(n->child);
	assert(n->child->string);

	PAIR_CLASS_INIT(&tag, "fname");
	t = print_otag(h, TAG_SPAN, 1, &tag);
	print_text(h, n->child->string);
	print_tagq(h, t);
	return(0);
}


/* ARGSUSED */
static void
mdoc_fo_post(MDOC_ARGS)
{

	if (MDOC_BODY != n->type)
		return;
	print_text(h, ")");
	print_text(h, ";");
}


/* ARGSUSED */
static int
mdoc_in_pre(MDOC_ARGS)
{
	const struct mdoc_node	*nn;
	struct tag		*t;
	struct htmlpair		 tag[2];
	int			 i;

	synopsis_pre(h, n);

	PAIR_CLASS_INIT(&tag[0], "includes");
	print_otag(h, TAG_SPAN, 1, tag);

	if (MDOC_SYNPRETTY & n->flags && MDOC_LINE & n->flags)
		print_text(h, "#include");

	print_text(h, "<");
	h->flags |= HTML_NOSPACE;

	for (nn = n->child; nn; nn = nn->next) {
		PAIR_CLASS_INIT(&tag[0], "link-includes");
		i = 1;
		bufinit(h);
		if (h->base_includes) {
			buffmt_includes(h, nn->string);
			PAIR_HREF_INIT(&tag[i], h->buf);
			i++;
		}
		t = print_otag(h, TAG_A, i, tag);
		print_mdoc_node(m, nn, h);
		print_tagq(h, t);
	}

	h->flags |= HTML_NOSPACE;
	print_text(h, ">");

	return(0);
}


/* ARGSUSED */
static int
mdoc_ic_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "cmd");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_rv_pre(MDOC_ARGS)
{
	const struct mdoc_node	*nn;
	struct htmlpair		 tag;
	struct tag		*t;

	print_otag(h, TAG_DIV, 0, NULL);
	print_text(h, "The");

	for (nn = n->child; nn; nn = nn->next) {
		PAIR_CLASS_INIT(&tag, "fname");
		t = print_otag(h, TAG_SPAN, 1, &tag);
		print_text(h, nn->string);
		print_tagq(h, t);

		h->flags |= HTML_NOSPACE;
		if (nn->next && NULL == nn->next->next)
			print_text(h, "(), and");
		else if (nn->next)
			print_text(h, "(),");
		else
			print_text(h, "()");
	}

	if (n->child && n->child->next)
		print_text(h, "functions return");
	else
		print_text(h, "function returns");

       	print_text(h, "the value 0 if successful; otherwise the value "
			"-1 is returned and the global variable");

	PAIR_CLASS_INIT(&tag, "var");
	t = print_otag(h, TAG_SPAN, 1, &tag);
	print_text(h, "errno");
	print_tagq(h, t);
       	print_text(h, "is set to indicate the error.");
	return(0);
}


/* ARGSUSED */
static int
mdoc_va_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "var");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_ap_pre(MDOC_ARGS)
{
	
	h->flags |= HTML_NOSPACE;
	print_text(h, "\\(aq");
	h->flags |= HTML_NOSPACE;
	return(1);
}


/* ARGSUSED */
static int
mdoc_bf_pre(MDOC_ARGS)
{
	struct htmlpair	 tag[2];
	struct roffsu	 su;

	if (MDOC_HEAD == n->type)
		return(0);
	else if (MDOC_BODY != n->type)
		return(1);

	assert(n->data.Bf);

	if (FONT_Em == n->data.Bf->font) 
		PAIR_CLASS_INIT(&tag[0], "emph");
	else if (FONT_Sy == n->data.Bf->font) 
		PAIR_CLASS_INIT(&tag[0], "symb");
	else if (FONT_Li == n->data.Bf->font) 
		PAIR_CLASS_INIT(&tag[0], "lit");
	else
		PAIR_CLASS_INIT(&tag[0], "none");

	/* 
	 * We want this to be inline-formatted, but needs to be div to
	 * accept block children. 
	 */
	bufcat_style(h, "display", "inline");
	SCALE_HS_INIT(&su, 1);
	/* Needs a left-margin for spacing. */
	bufcat_su(h, "margin-left", &su);
	PAIR_STYLE_INIT(&tag[1], h);
	print_otag(h, TAG_DIV, 2, tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_ms_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "symb");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_pf_pre(MDOC_ARGS)
{

	h->flags |= HTML_IGNDELIM;
	return(1);
}


/* ARGSUSED */
static void
mdoc_pf_post(MDOC_ARGS)
{

	h->flags &= ~HTML_IGNDELIM;
	h->flags |= HTML_NOSPACE;
}


/* ARGSUSED */
static int
mdoc_rs_pre(MDOC_ARGS)
{
	struct htmlpair	 tag;

	if (MDOC_BLOCK != n->type)
		return(1);

	if (n->prev && SEC_SEE_ALSO == n->sec) {
		print_otag(h, TAG_BR, 0, NULL);
		print_otag(h, TAG_BR, 0, NULL);
	} 

	PAIR_CLASS_INIT(&tag, "ref");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}



/* ARGSUSED */
static int
mdoc_li_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "lit");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_sy_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	PAIR_CLASS_INIT(&tag, "symb");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc_bt_pre(MDOC_ARGS)
{

	print_text(h, "is currently in beta test.");
	return(0);
}


/* ARGSUSED */
static int
mdoc_ud_pre(MDOC_ARGS)
{

	print_text(h, "currently under development.");
	return(0);
}


/* ARGSUSED */
static int
mdoc_lb_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	if (SEC_LIBRARY == n->sec && MDOC_LINE & n->flags)
		print_otag(h, TAG_DIV, 0, NULL);
	PAIR_CLASS_INIT(&tag, "lib");
	print_otag(h, TAG_SPAN, 1, &tag);
	return(1);
}


/* ARGSUSED */
static int
mdoc__x_pre(MDOC_ARGS)
{
	struct htmlpair	tag[2];

	switch (n->tok) {
	case(MDOC__A):
		PAIR_CLASS_INIT(&tag[0], "ref-auth");
		if (n->prev && MDOC__A == n->prev->tok)
			if (NULL == n->next || MDOC__A != n->next->tok)
				print_text(h, "and");
		break;
	case(MDOC__B):
		PAIR_CLASS_INIT(&tag[0], "ref-book");
		break;
	case(MDOC__C):
		PAIR_CLASS_INIT(&tag[0], "ref-city");
		break;
	case(MDOC__D):
		PAIR_CLASS_INIT(&tag[0], "ref-date");
		break;
	case(MDOC__I):
		PAIR_CLASS_INIT(&tag[0], "ref-issue");
		break;
	case(MDOC__J):
		PAIR_CLASS_INIT(&tag[0], "ref-jrnl");
		break;
	case(MDOC__N):
		PAIR_CLASS_INIT(&tag[0], "ref-num");
		break;
	case(MDOC__O):
		PAIR_CLASS_INIT(&tag[0], "ref-opt");
		break;
	case(MDOC__P):
		PAIR_CLASS_INIT(&tag[0], "ref-page");
		break;
	case(MDOC__Q):
		PAIR_CLASS_INIT(&tag[0], "ref-corp");
		break;
	case(MDOC__R):
		PAIR_CLASS_INIT(&tag[0], "ref-rep");
		break;
	case(MDOC__T):
		PAIR_CLASS_INIT(&tag[0], "ref-title");
		break;
	case(MDOC__U):
		PAIR_CLASS_INIT(&tag[0], "link-ref");
		break;
	case(MDOC__V):
		PAIR_CLASS_INIT(&tag[0], "ref-vol");
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	if (MDOC__U != n->tok) {
		print_otag(h, TAG_SPAN, 1, tag);
		return(1);
	}

	PAIR_HREF_INIT(&tag[1], n->child->string);
	print_otag(h, TAG_A, 2, tag);

	return(1);
}


/* ARGSUSED */
static void
mdoc__x_post(MDOC_ARGS)
{

	if (MDOC__A == n->tok && n->next && MDOC__A == n->next->tok)
		if (NULL == n->next->next || MDOC__A != n->next->next->tok)
			if (NULL == n->prev || MDOC__A != n->prev->tok)
				return;

	/* TODO: %U */

	if (NULL == n->parent || MDOC_Rs != n->parent->tok)
		return;

	print_text(h, n->next ? "," : ".");
}


/* ARGSUSED */
static int
mdoc_bk_pre(MDOC_ARGS)
{

	switch (n->type) {
	case (MDOC_BLOCK):
		break;
	case (MDOC_HEAD):
		return(0);
	case (MDOC_BODY):
		h->flags |= HTML_PREKEEP;
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	return(1);
}


/* ARGSUSED */
static void
mdoc_bk_post(MDOC_ARGS)
{

	if (MDOC_BODY == n->type)
		h->flags &= ~(HTML_KEEP | HTML_PREKEEP);
}


/* ARGSUSED */
static int
mdoc_quote_pre(MDOC_ARGS)
{
	struct htmlpair	tag;

	if (MDOC_BODY != n->type)
		return(1);

	switch (n->tok) {
	case (MDOC_Ao):
		/* FALLTHROUGH */
	case (MDOC_Aq):
		print_text(h, "\\(la");
		break;
	case (MDOC_Bro):
		/* FALLTHROUGH */
	case (MDOC_Brq):
		print_text(h, "\\(lC");
		break;
	case (MDOC_Bo):
		/* FALLTHROUGH */
	case (MDOC_Bq):
		print_text(h, "\\(lB");
		break;
	case (MDOC_Oo):
		/* FALLTHROUGH */
	case (MDOC_Op):
		print_text(h, "\\(lB");
		PAIR_CLASS_INIT(&tag, "opt");
		print_otag(h, TAG_SPAN, 1, &tag);
		break;
	case (MDOC_Do):
		/* FALLTHROUGH */
	case (MDOC_Dq):
		/* FALLTHROUGH */
	case (MDOC_Qo):
		/* FALLTHROUGH */
	case (MDOC_Qq):
		print_text(h, "\\(lq");
		break;
	case (MDOC_Po):
		/* FALLTHROUGH */
	case (MDOC_Pq):
		print_text(h, "(");
		break;
	case (MDOC_Ql):
		/* FALLTHROUGH */
	case (MDOC_So):
		/* FALLTHROUGH */
	case (MDOC_Sq):
		print_text(h, "\\(oq");
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	h->flags |= HTML_NOSPACE;
	return(1);
}


/* ARGSUSED */
static void
mdoc_quote_post(MDOC_ARGS)
{

	if (MDOC_BODY != n->type)
		return;

	h->flags |= HTML_NOSPACE;

	switch (n->tok) {
	case (MDOC_Ao):
		/* FALLTHROUGH */
	case (MDOC_Aq):
		print_text(h, "\\(ra");
		break;
	case (MDOC_Bro):
		/* FALLTHROUGH */
	case (MDOC_Brq):
		print_text(h, "\\(rC");
		break;
	case (MDOC_Oo):
		/* FALLTHROUGH */
	case (MDOC_Op):
		/* FALLTHROUGH */
	case (MDOC_Bo):
		/* FALLTHROUGH */
	case (MDOC_Bq):
		print_text(h, "\\(rB");
		break;
	case (MDOC_Qo):
		/* FALLTHROUGH */
	case (MDOC_Qq):
		/* FALLTHROUGH */
	case (MDOC_Do):
		/* FALLTHROUGH */
	case (MDOC_Dq):
		print_text(h, "\\(rq");
		break;
	case (MDOC_Po):
		/* FALLTHROUGH */
	case (MDOC_Pq):
		print_text(h, ")");
		break;
	case (MDOC_Ql):
		/* FALLTHROUGH */
	case (MDOC_So):
		/* FALLTHROUGH */
	case (MDOC_Sq):
		print_text(h, "\\(aq");
		break;
	default:
		abort();
		/* NOTREACHED */
	}
}


