#	$OpenBSD: Makefile,v 1.2 2002/01/01 23:05:30 art Exp $

PROG=	dup2_accept

LIBPTHREAD=     /usr/lib/libpthread.a

LDADD+=         -lpthread
DPADD+=         ${LIBPTHREAD}

.include <bsd.regress.mk>
