#	$OpenBSD: Makefile,v 1.4 2000/04/18 15:24:26 mickey Exp $

PROG=	nfsstat
CFLAGS+=-DNFS
DPADD=	${LIBKVM}
LDADD=	-lkvm

.include <bsd.prog.mk>
