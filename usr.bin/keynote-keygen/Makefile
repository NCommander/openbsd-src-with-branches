# $OpenBSD$

PROG=	keynote-keygen

BINOWN= root
BINGRP= bin
BINMODE= 555
BINDIR= /usr/bin
LDADD=	-lkeynote -lm -lcrypto
MAN=	keynote-keygen.1

KEYNOTEDIST= lib/libkeynote

KEYNOTE_SRC= ${.CURDIR}/../../${KEYNOTEDIST}

CFLAGS+= -DCRYPTO

SRCS=	keynote-keygen.c

.PATH:	${KEYNOTE_SRC}

.include<bsd.prog.mk>

