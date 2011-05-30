#	$OpenBSD: Makefile,v 1.1 2009/04/09 01:24:43 martynas Exp $

PROG = cephes
SRCS = cephes.c const.c monot.c monotl.c monotll.c polevll.c \
       testvect.c testvectl.c testvectll.c
LDADD = -lm
DPADD = ${LIBM}

.include <bsd.regress.mk>
