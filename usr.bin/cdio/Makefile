#	$OpenBSD: Makefile,v 1.1 1996/08/23 23:42:28 downsj Exp $

PROG=	cdio
DPADD=	${LIBUTIL} ${LIBEDIT} ${LIBTERMCAP}
LDADD=	-lutil -ledit -ltermcap
SRCS=	cdio.c cddb.c
CDIAGFLAGS=-Wall -W -Wmissing-prototypes -pedantic

.include <bsd.prog.mk>
