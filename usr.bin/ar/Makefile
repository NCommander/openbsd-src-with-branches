#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:48:19 deraadt Exp $

.include <bsd.own.mk>

.if (${ELF_TOOLCHAIN:L} == "no")
PROG=	ar
CFLAGS+=-I${.CURDIR}
SRCS=	append.c ar.c archive.c contents.c delete.c extract.c misc.c \
	move.c print.c replace.c 
MAN=	ar.1
.else
NOPROG=
.endif

MAN+=	ar.5

.include <bsd.prog.mk>
