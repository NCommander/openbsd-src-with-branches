#	$OpenBSD: Makefile,v 1.2 1996/06/26 05:37:36 deraadt Exp $

PROG=	rdist
CFLAGS+=-I${.CURDIR}
SRCS=	docmd.c expand.c lookup.c main.c server.c rshrcmd.c
OBJS+=	gram.o
BINOWN=	root
BINMODE=555
CLEANFILES=y.tab.h
MAN=	oldrdist.1

LDADD=	-lcompat
DPADD=	${LIBCOMPAT}

.PATH:	${.CURDIR}/../rdist

realinstall:
	install ${COPY} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${PROG} ${DESTDIR}${BINDIR}/oldrdist

.include <bsd.prog.mk>
