#	$OpenBSD$

PROG=		procmap
SRCS=		main.c 
SRCS=		procmap.c
BINGRP=		kmem
BINMODE=	2555
LDADD=		-lkvm
DPADD=		${LIBKVM}

.include <bsd.prog.mk>
