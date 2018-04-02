#	$OpenBSD: Makefile,v 1.1 2002/02/08 20:15:14 art Exp $

PROG=		main-thread-exited

LIBPTHREAD=	/usr/lib/libpthread.a

LDADD+=		-lpthread
DPADD+=		${LIBPTHREAD}

.include <bsd.regress.mk>
