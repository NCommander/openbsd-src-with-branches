#	$OpenBSD: Makefile,v 1.15 1999/04/23 05:21:56 downsj Exp $

PROG=	savecore

# XXX Temporarily until pmax is able to use libkvm
.if	(${MACHINE} == "pmax")
SRCS=	savecore_old.c
.else
SRCS=	savecore.c
LDADD=	-lkvm
DPADD=	${LIBKVM}
.endif
SRCS+=	zopen.c
MAN=	savecore.8
.PATH:	${.CURDIR}/../../usr.bin/compress

.include <bsd.prog.mk>
