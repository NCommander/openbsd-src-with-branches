#	$OpenBSD: Makefile,v 1.5 1997/09/21 11:50:15 deraadt Exp $

PROG=	netstat
SRCS=	if.c inet.c ipx.c iso.c main.c mbuf.c mroute.c ns.c route.c \
	tp_astring.c unix.c atalk.c inet6.c mroute6.c
.PATH:	${.CURDIR}/../../sys/netiso
BINGRP=	kmem
BINMODE=2555
LDADD=	-lkvm
DPADD=	${LIBKVM}

CPPFLAGS+=	-DINET6
#CPPFLAGS+=	-DKAME_SCOPEID

.include <bsd.prog.mk>
