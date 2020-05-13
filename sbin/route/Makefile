#	$OpenBSD: Makefile,v 1.15 2017/06/12 18:49:34 espie Exp $

PROG=	route
MAN=	route.8
SRCS=	route.c show.c

CFLAGS+=	-Wall -DBFD

# XXX keywords.h is generated from keywords.sh, but see distrib/special
# easier to keep the copy in source than over-reach from there.
#
route.o tags: keywords.h

.include <bsd.prog.mk>
