#	$OpenBSD: Makefile,v 1.2 2003/07/12 04:23:16 jason Exp $

PROG=asi
NOMAN=
SRCS=fpregs.S asi.c asi_asm.S
CFLAGS+=-I${.CURDIR}/../goodfreg

.PATH: ${.CURDIR}/../goodfreg

.include <bsd.regress.mk>
