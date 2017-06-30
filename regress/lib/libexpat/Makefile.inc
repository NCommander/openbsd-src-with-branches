# $OpenBSD$

EXPATDIR=	${.CURDIR}/../../../../lib/libexpat
.PATH:		${EXPATDIR}/tests ${EXPATDIR}/tests/benchmark
PROG=		${.CURDIR:T}
CFLAGS=		-g -I${EXPATDIR}/lib
CPPFLAGS=	-g -I${EXPATDIR}/lib
LDFLAGS=	-g -static
LDADD=		-lexpat
DPADD=		${LIBEXPAT}
