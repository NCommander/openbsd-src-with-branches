# $OpenBSD$

PROG=	keynote-sign

BINOWN= root
BINGRP= bin
BINMODE= 555
BINDIR= /usr/bin
LDADD=	-lkeynote -lm -lcrypto
MAN=	keynote-sign.1

KEYNOTEDIST= lib/libkeynote

KEYNOTE_SRC= ${.CURDIR}/../../${KEYNOTEDIST}

CFLAGS+= -DCRYPTO

SRCS=	keynote-sign.c

.PATH:	${KEYNOTE_SRC}

.include<bsd.prog.mk>

