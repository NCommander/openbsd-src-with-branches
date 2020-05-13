# $FreeBSD: src/gnu/usr.bin/cc/gcov/Makefile,v 1.5.8.1 2009/04/15 03:14:26 kensmith Exp $
# $OpenBSD$

.include <bsd.own.mk>

.include "${.CURDIR}/../Makefile.inc"
.include "${.CURDIR}/../Makefile.ver"

.PATH: ${GCCDIR} ${GCCDIR}/doc

PROG=	gcov
SRCS=	gcov.c version.c errors.c
MAN=	gcov.1

DPADD=	${LIBIBERTY}
LDADD=	${LIBIBERTY}

.include <bsd.prog.mk>
