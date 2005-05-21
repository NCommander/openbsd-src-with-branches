#	$OpenBSD: Makefile,v 1.1 2005/02/24 06:58:36 otto Exp $

PROG=copy
CFLAGS+= -Wall

.if !(${MACHINE} == "i386" || ${MACHINE} == "amd64")
REGRESS_SKIP=
.endif

.include <bsd.regress.mk>
