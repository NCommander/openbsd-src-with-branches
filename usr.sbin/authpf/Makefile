#	$OpenBSD: Makefile,v 1.10 2003/11/20 23:23:09 avsm Exp $

PROG=	authpf
MAN=	authpf.8
BINOWN= root
BINGRP= authpf
BINMODE= 6555
SRCS=	authpf.c
CFLAGS+= -Wall

LDADD+= -lm
DPADD+= ${LIBM}

.include <bsd.prog.mk>
