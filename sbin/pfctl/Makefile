#	$OpenBSD: Makefile,v 1.7 2002/09/14 17:50:17 henning Exp $

PROG=	pfctl
SRCS=	pfctl.c parse.y pfctl_parser.c pf_print_state.c pfctl_altq.c
CFLAGS+= -Wall -Wmissing-prototypes -Wno-uninitialized
CFLAGS+= -Wstrict-prototypes
YFLAGS=
MAN=	pfctl.8
 
LDADD+=	-lm
DPADD+=	${LIBM}

.include <bsd.prog.mk>
