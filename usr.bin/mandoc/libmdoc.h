/*	$OpenBSD: libmdoc.h,v 1.72 2015/04/18 17:01:28 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2013, 2014, 2015 Ingo Schwarze <schwarze@openbsd.org>
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

#define	MACRO_PROT_ARGS	struct roff_man *mdoc, \
			int tok, \
			int line, \
			int ppos, \
			int *pos, \
			char *buf

struct	mdoc_macro {
	void		(*fp)(MACRO_PROT_ARGS);
	int		  flags;
#define	MDOC_CALLABLE	 (1 << 0)
#define	MDOC_PARSED	 (1 << 1)
#define	MDOC_EXPLICIT	 (1 << 2)
#define	MDOC_PROLOGUE	 (1 << 3)
#define	MDOC_IGNDELIM	 (1 << 4)
#define	MDOC_JOIN	 (1 << 5)
};

enum	margserr {
	ARGS_ERROR,
	ARGS_EOLN, /* end-of-line */
	ARGS_WORD, /* normal word */
	ARGS_PUNCT, /* series of punctuation */
	ARGS_QWORD, /* quoted word */
	ARGS_PHRASE, /* Ta'd phrase (-column) */
	ARGS_PPHRASE, /* tabbed phrase (-column) */
	ARGS_PEND /* last phrase (-column) */
};

/*
 * A punctuation delimiter is opening, closing, or "middle mark"
 * punctuation.  These govern spacing.
 * Opening punctuation (e.g., the opening parenthesis) suppresses the
 * following space; closing punctuation (e.g., the closing parenthesis)
 * suppresses the leading space; middle punctuation (e.g., the vertical
 * bar) can do either.  The middle punctuation delimiter bends the rules
 * depending on usage.
 */
enum	mdelim {
	DELIM_NONE = 0,
	DELIM_OPEN,
	DELIM_MIDDLE,
	DELIM_CLOSE,
	DELIM_MAX
};

extern	const struct mdoc_macro *const mdoc_macros;

__BEGIN_DECLS

void		  mdoc_macro(MACRO_PROT_ARGS);
void		  mdoc_word_alloc(struct roff_man *, int, int, const char *);
void		  mdoc_word_append(struct roff_man *, const char *);
void		  mdoc_elem_alloc(struct roff_man *, int, int,
			int, struct mdoc_arg *);
struct roff_node *mdoc_block_alloc(struct roff_man *, int, int,
			int, struct mdoc_arg *);
struct roff_node *mdoc_head_alloc(struct roff_man *, int, int, int);
void		  mdoc_tail_alloc(struct roff_man *, int, int, int);
struct roff_node *mdoc_body_alloc(struct roff_man *, int, int, int);
struct roff_node *mdoc_endbody_alloc(struct roff_man *, int, int, int,
			struct roff_node *, enum mdoc_endbody);
void		  mdoc_node_relink(struct roff_man *, struct roff_node *);
int		  mdoc_hash_find(const char *);
const char	 *mdoc_a2att(const char *);
const char	 *mdoc_a2st(const char *);
const char	 *mdoc_a2arch(const char *);
void		  mdoc_valid_pre(struct roff_man *, struct roff_node *);
void		  mdoc_valid_post(struct roff_man *);
void		  mdoc_argv(struct roff_man *, int, int,
			struct mdoc_arg **, int *, char *);
void		  mdoc_argv_free(struct mdoc_arg *);
enum margserr	  mdoc_args(struct roff_man *, int,
			int *, char *, int, char **);
void		  mdoc_macroend(struct roff_man *);
enum mdelim	  mdoc_isdelim(const char *);

__END_DECLS
