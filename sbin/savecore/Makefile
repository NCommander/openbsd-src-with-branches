#	$OpenBSD: Makefile,v 1.16 2000/04/15 20:35:57 mickey Exp $

PROG=	savecore

SRCS=	savecore.c zopen.c
LDADD=	-lkvm
DPADD=	${LIBKVM}
MAN=	savecore.8
.PATH:	${.CURDIR}/../../usr.bin/compress

.include <bsd.prog.mk>
