#	$OpenBSD: Makefile,v 1.3 1996/06/23 14:29:59 deraadt Exp $

PROG=	ccdconfig
MAN=	ccdconfig.8

BINGRP=	kmem
BINMODE=2555

DPADD=	${LIBKVM} ${LIBUTIL}
LDADD=	-lkvm -lutil

.include <bsd.prog.mk>
