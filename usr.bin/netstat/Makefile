#	$OpenBSD: Makefile,v 1.4 1997/07/23 04:38:32 denny Exp $

PROG=	netstat
SRCS=	if.c inet.c ipx.c iso.c main.c mbuf.c mroute.c ns.c route.c \
	tp_astring.c unix.c atalk.c
.PATH:	${.CURDIR}/../../sys/netiso
BINGRP=	kmem
BINMODE=2555
LDADD=	-lkvm
DPADD=	${LIBKVM}

.include <bsd.prog.mk>
