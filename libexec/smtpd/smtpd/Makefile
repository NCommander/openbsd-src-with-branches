#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$OpenBSD: Makefile,v 1.2 1997/01/17 16:12:45 millert Exp $

PROG=	smtpd
CFLAGS+=-I${.CURDIR}/../src
SRCS= smtpd.c address_check.c 		
BINOWN=	root
BINGRP=	daemon
BINMODE=500
BINDIR=	/usr/libexec
MAN=    smtpd.8
MLINKS= smtpd.8 smtpd.5
.PATH:	${.CURDIR}/../src

.include <bsd.prog.mk>
