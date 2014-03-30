#	$OpenBSD: Makefile,v 1.9 2009/06/07 03:10:09 millert Exp $

PROG=	fstat
SRCS=	fstat.c fuser.c
MAN=	fstat.1 fuser.1
DPADD=	${LIBKVM}
LDADD=	-lkvm
LINKS=  ${BINDIR}/fstat ${BINDIR}/fuser

CFLAGS+=-DINET6

.include <bsd.prog.mk>

