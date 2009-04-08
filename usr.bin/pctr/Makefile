#	$OpenBSD: Makefile,v 1.6 2003/07/19 00:03:52 david Exp $

MAN=pctr.1

CFLAGS+=-W -Wall -Werror

.if ${MACHINE} == "amd64" || ${MACHINE} == "i386"
PROG=	pctr
.else
NOPROG=
.endif

.include <bsd.prog.mk>
