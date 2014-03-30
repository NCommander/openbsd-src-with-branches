# $OpenBSD: Makefile,v 1.3 2005/06/04 02:25:53 cloder Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64")
PROG= acpidump
SRCS= acpidump.c
.else
NOPROG= yes
.endif

MAN= acpidump.8

VPATH=${.CURDIR}/aml
CFLAGS+=-I${.CURDIR}

BINDIR?=/usr/sbin

.include <bsd.prog.mk>
