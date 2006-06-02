#	$OpenBSD: Makefile,v 1.9 2006/05/29 01:21:38 beck Exp $

PROG=	chio
SRCS=	chio.c parse.y
DPADD=	${LIBUTIL}
LDADD=	-lutil

distribution:
	${INSTALL} ${INSTALL_COPY} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/chio.conf ${DESTDIR}/etc/chio.conf

.include <bsd.prog.mk>
