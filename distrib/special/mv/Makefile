#	$OpenBSD: Makefile,v 1.1 2014/12/23 17:16:02 deraadt Exp $

PROG=	mv
SRCS=	mv.c cp.c rm.c

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

.PATH:  ${.CURDIR}/../../../bin/mv
.include <bsd.prog.mk>
