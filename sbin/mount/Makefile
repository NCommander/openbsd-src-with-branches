#	$OpenBSD: Makefile,v 1.4 1996/12/03 01:04:43 downsj Exp $

PROG=	mount
SRCS=	mount.c getmntopts.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	mount.8
# We do NOT install the getmntopts.3 man page.

.include <bsd.prog.mk>
