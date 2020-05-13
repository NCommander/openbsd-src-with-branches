# $OpenBSD: Makefile.inc,v 1.1.1.1 2017/06/30 14:56:08 bluhm Exp $

EXPATDIR=	${.CURDIR}/../../../../lib/libexpat
.PATH:		${EXPATDIR}/tests ${EXPATDIR}/tests/benchmark
PROG=		${.CURDIR:T}
CFLAGS=		-I${EXPATDIR}/lib
CPPFLAGS=	-I${EXPATDIR}/lib
LDADD=		-lexpat
DPADD=		${LIBEXPAT}
DEBUG?=		-g
