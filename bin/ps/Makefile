#	$OpenBSD: Makefile,v 1.15 1995/05/21 13:38:25 mycroft Exp $
#	$NetBSD: Makefile,v 1.15 1995/05/21 13:38:25 mycroft Exp $

PROG=	ps
SRCS=	fmt.c keyword.c nlist.c print.c ps.c
DPADD=	${LIBM} ${LIBKVM}
LDADD=	-lm -lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
