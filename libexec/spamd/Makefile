#	$OpenBSD: Makefile,v 1.7 2002/07/16 10:16:10 deraadt Exp $

PROG=	spamd
SRCS=	spamd.c
MAN=	spamd.8

CFLAGS+= -Wall -Werror 

afterinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/spamd-setup.sh ${DESTDIR}${BINDIR}/spamd-setup

.include <bsd.prog.mk>
