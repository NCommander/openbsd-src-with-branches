#	$OpenBSD: Makefile,v 1.4 1997/09/21 11:36:07 deraadt Exp $

PROG=	ccdconfig
MAN=	ccdconfig.8

# Only permit root to use -g 
#BINGRP= kmem
#BINMODE=2555

DPADD=	${LIBKVM} ${LIBUTIL}
LDADD=	-lkvm -lutil

.include <bsd.prog.mk>
