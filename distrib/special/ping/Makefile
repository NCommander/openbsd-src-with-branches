#	$OpenBSD: Makefile,v 1.1 2011/09/17 14:05:27 haesbaert Exp $

PROG=	ping
COPTS+=	-DSMALL 
.PATH:  ${.CURDIR}/../../../sbin/ping

LDADD=	-lm
DPADD=	${LIBM}

.include <bsd.prog.mk>
