#	$OpenBSD: Makefile,v 1.8 2004/11/10 22:07:46 drahn Exp $

PROG=	ping6
MAN=	ping6.8

LDADD=	-lm
DPADD=	${LIBM}

BINOWN=	root
BINGRP=	bin
BINMODE=4555
LDSTATIC=${STATIC}

.include <bsd.prog.mk>
