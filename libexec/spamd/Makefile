#	$OpenBSD: Makefile,v 1.2 2003/02/14 05:32:02 jason Exp $

PROG=	spamd
SRCS=	spamd.c sdl.c
MAN=	spamd.8 spamd-setup.8

CFLAGS+= -Wall -Werror

afterinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/spamd-setup.pl ${DESTDIR}${BINDIR}/spamd-setup

.include <bsd.prog.mk>
