#	$OpenBSD: Makefile,v 1.5 2018/04/26 12:12:30 gilles Exp $
.PATH:	${.CURDIR}/..

PROGS=	mail.lmtp mail.maildir mail.mda

MAN=	mail.lmtp.8 mail.maildir.8 mail.mda.8

BINOWN=	root
BINGRP=	wheel
BINMODE?=	0111

BINDIR=	/usr/libexec

CFLAGS+=	-fstack-protector-all
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
CFLAGS+=	-Werror-implicit-function-declaration

.include <bsd.prog.mk>
