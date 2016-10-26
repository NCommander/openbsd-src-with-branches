#	$OpenBSD: Makefile,v 1.1 2011/03/21 21:51:28 martynas Exp $

PROG=fenv

CFLAGS=-ffloat-store
LDADD=-lm
DPADD=${LIBM}

.include <bsd.regress.mk>
