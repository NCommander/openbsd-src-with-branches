#	$OpenBSD: Makefile,v 1.5 2002/05/12 17:14:04 deraadt Exp $

PROG=	ping6
MAN=	ping6.8

LDADD=	-lm
DPADD=	${LIBM}

CPPFLAGS+= -DINET6

BINOWN=	root
BINGRP=	bin
BINMODE=4555
LDSTATIC=-static

.include <bsd.prog.mk>
