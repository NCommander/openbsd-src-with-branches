#	$OpenBSD: Makefile,v 1.6 1997/09/21 11:35:22 deraadt Exp $

PROG=	mt
SRCS=	mt.c mtrmt.c
DPADD=	${LIBUTIL}
LDADD=	-lutil

LINKS=	${BINDIR}/mt ${BINDIR}/eject
MLINKS=	mt.1 eject.1

.include <bsd.prog.mk>
