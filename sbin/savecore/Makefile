#	$OpenBSD: Makefile,v 1.5 1997/06/28 16:35:20 grr Exp $

PROG=	savecore

# XXX Temporary, until other ports use new libkvm
.if	(${MACHINE} == "amiga") || \
	(${MACHINE} == "atari") || \
	(${MACHINE} == "sparc") || \
	(${MACHINE} == "sun3")
SRCS=	savecore.c
LDADD=	-lkvm
DPADD=	${LIBKVM}
.else
SRCS=	savecore_old.c
.endif
SRCS+=	zopen.c
MAN=	savecore.8
.PATH:	${.CURDIR}/../../usr.bin/compress

.include <bsd.prog.mk>
