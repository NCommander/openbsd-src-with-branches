#	$OpenBSD: Makefile,v 1.4 1995/10/08 23:44:02 gwr Exp $

PROG=	installboot
MAN=	installboot.8
MANSUBDIR=/mvme88k

BINDIR=/usr/mdec
DPADD=${LIBUTIL}
LDADD=-lutil
CFLAGS+=-O0
# Need this to work in the miniroot

LDSTATIC=-static

.include <bsd.prog.mk>
