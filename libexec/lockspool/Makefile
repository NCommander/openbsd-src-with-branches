#	$OpenBSD: Makefile,v 1.1.1.1 1995/10/18 08:43:19 deraadt Exp $

PROG=	lockspool
SRCS=	lockspool.c locking.c
BINOWN=	root
BINMODE=4555
CFLAGS+=-I${.CURDIR}/../mail.local
.PATH:	${.CURDIR}/../mail.local

.include <bsd.prog.mk>
