#	$OpenBSD: Makefile,v 1.1 2002/01/31 16:35:46 art Exp $

PROG=dtors

REGRESSTARGETS=dtor-exit dtor-return

dtor-exit: dtors
	./dtors

dtor-return: dtors
	./dtors -f

.include <bsd.regress.mk>
