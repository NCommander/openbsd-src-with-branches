#	$OpenBSD: Makefile,v 1.4 2001/02/07 13:52:23 itojun Exp $

PROG=	route6d
MAN=	route6d.8
LDADD+=	-lutil
DPADD+=	${LIBUTIL}

CPPFLAGS+=	-DINET6

.include <bsd.prog.mk>
