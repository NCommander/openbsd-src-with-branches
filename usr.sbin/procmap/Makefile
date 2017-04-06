#	$OpenBSD: Makefile,v 1.5 2013/06/25 15:11:58 dcoppa Exp $

PROG=		procmap
SRCS=		procmap.c
CFLAGS+=	-Wall
LDADD=		-lkvm
DPADD=		${LIBKVM}

.PATH: ${.CURDIR}/../../sys/kern
SRCS+=		subr_tree.c

.include <bsd.prog.mk>
