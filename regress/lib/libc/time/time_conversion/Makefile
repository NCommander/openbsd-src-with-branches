#	$OpenBSD: Makefile,v 1.3 2012/12/16 18:28:53 jasper Exp $

PROG=		timetest
SRCS=		timetest.c

REGRESS_TARGETS=do-timetest

do-timetest: ${PROG}
	./${PROG}

.include <bsd.regress.mk>
