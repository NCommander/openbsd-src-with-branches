#	$OpenBSD: Makefile,v 1.3 1996/06/23 14:20:48 deraadt Exp $

PROG=	ps
SRCS=	fmt.c keyword.c nlist.c print.c ps.c
DPADD=	${LIBM} ${LIBKVM}
LDADD=	-lm -lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
