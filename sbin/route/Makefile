#	$OpenBSD: Makefile,v 1.2 1996/06/23 14:32:20 deraadt Exp $
#	$NetBSD: Makefile,v 1.12 1995/04/19 09:08:28 cgd Exp $

PROG=	route
MAN=	route.8
SRCS=	route.c show.c keywords.c ccitt_addr.c
BINOWN=	root
BINMODE=4555

route.o .depend lint tags: keywords.h

.include <bsd.prog.mk>
