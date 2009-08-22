/*	$Id: man.h,v 1.5 2009/07/07 00:54:46 schwarze Exp $ */
/*
 * Copyright (c) 2009 Kristaps Dzonsons <kristaps@kth.se>
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
#ifndef MAN_H
#define MAN_H

#include <time.h>

#define	MAN_br	 	 0
#define	MAN_TH		 1
#define	MAN_SH		 2
#define	MAN_SS		 3
#define	MAN_TP		 4
#define	MAN_LP		 5
#define	MAN_PP		 6
#define	MAN_P		 7
#define	MAN_IP		 8
#define	MAN_HP		 9
#define	MAN_SM		 10
#define	MAN_SB		 11
#define	MAN_BI		 12
#define	MAN_IB		 13
#define	MAN_BR		 14
#define	MAN_RB		 15
#define	MAN_R		 16
#define	MAN_B		 17
#define	MAN_I		 18
#define	MAN_IR		 19
#define	MAN_RI		 20
#define	MAN_na		 21
#define	MAN_i		 22
#define	MAN_sp		 23
#define	MAN_MAX	 	 24

enum	man_type {
	MAN_TEXT,
	MAN_ELEM,
	MAN_ROOT
};

struct	man_meta {
	int		 msec;
	time_t		 date;
	char		*vol;
	char		*title;
	char		*source;
};

struct	man_node {
	struct man_node	*parent;
	struct man_node	*child;
	struct man_node	*next;
	struct man_node	*prev;
	int		 nchild;
	int		 line;
	int		 pos;
	int		 tok;
	int		 flags;
#define	MAN_VALID	(1 << 0)
#define	MAN_ACTED	(1 << 1)
	enum man_type	 type;
	char		*string;
};

#define	MAN_IGN_MACRO	 (1 << 0)
#define	MAN_IGN_CHARS	 (1 << 1)
#define	MAN_IGN_ESCAPE	 (1 << 2)

extern	const char *const *man_macronames;

struct	man_cb {
	int	(*man_warn)(void *, int, int, const char *);
	int	(*man_err)(void *, int, int, const char *);
};

__BEGIN_DECLS

struct	man;

void	 	  man_free(struct man *);
struct	man	 *man_alloc(void *, int, const struct man_cb *);
int		  man_reset(struct man *);
int	 	  man_parseln(struct man *, int, char *buf);
int		  man_endparse(struct man *);
int		  man_valid_post(struct man *);

const struct man_node *man_node(const struct man *);
const struct man_meta *man_meta(const struct man *);

__END_DECLS

#endif /*!MAN_H*/
