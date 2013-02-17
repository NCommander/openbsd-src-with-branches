#	$OpenBSD: Makefile,v 1.3 2003/11/20 23:23:09 avsm Exp $

MAN=	ldd.1

.include <bsd.own.mk>

.if (${ELF_TOOLCHAIN:L} == "yes")
PROG=	ldd
SRCS=	ldd.c
#CFLAGS+=-Werror

BINDIR=	/usr/bin

.endif

.include <bsd.prog.mk>
