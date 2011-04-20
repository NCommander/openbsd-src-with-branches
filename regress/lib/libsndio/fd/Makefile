#	$OpenBSD: Makefile,v 1.1 2008/10/27 00:26:33 ratchov Exp $
PROG= fd
LDADD= -lsndio
SRCS = fd.c tools.c
REGRESS_SKIP=
.PATH: ${.CURDIR}/..
.include <bsd.regress.mk>
