#	$OpenBSD: Makefile,v 1.8 2017/07/09 21:23:19 espie Exp $

PROG=	tcpbench
LDADD=	-lm -lkvm -levent
DPADD=	${LIBM} ${LIBKVM} ${LIBEVENT}

.include <bsd.prog.mk>
