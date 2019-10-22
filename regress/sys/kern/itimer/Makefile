#	$OpenBSD: Makefile,v 1.3 2002/09/02 20:01:44 avsm Exp $

PROG=	itimer
SRCS=	itimer.c

real: itimer
	./itimer -r

virtual: itimer
	./itimer -v

prof: itimer
	./itimer -p

REGRESS_TARGETS=real virtual prof

.include <bsd.regress.mk>
