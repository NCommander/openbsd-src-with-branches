#	$OpenBSD: Makefile,v 1.2 1997/09/21 11:35:51 deraadt Exp $

PROG=	factor
SRCS=	factor.c pattern.c pr_tbl.c
CFLAGS+=-I${.CURDIR}/../primes
MAN=	factor.6
MLINKS+=factor.6 primes.6
DPADD=  ${LIBMATH}
LDADD=  -lm
.PATH:	${.CURDIR}/../primes

.include <bsd.prog.mk>
