# $FreeBSD: src/gnu/usr.bin/cc/c++/Makefile,v 1.24.8.1 2009/04/15 03:14:26 kensmith Exp $
# $OpenBSD$

.include <bsd.own.mk>

.include "${.CURDIR}/../Makefile.inc"
.include "${.CURDIR}/../Makefile.fe"

.PATH: ${GCCDIR}/cp

PROG=	c++
SRCS+=	g++spec.c
LINKS=	${BINDIR}/c++ ${BINDIR}/g++
NOMAN=	Yes

DPADD=	${LIBCPP} ${LIBIBERTY}
LDADD=	${LIBCPP} ${LIBIBERTY}

.include <bsd.prog.mk>
