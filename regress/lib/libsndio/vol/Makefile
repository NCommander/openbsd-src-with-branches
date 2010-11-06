#	$OpenBSD: Makefile,v 1.1 2008/11/11 19:39:35 ratchov Exp $
PROG= vol
LDADD= -lsndio
SRCS = vol.c tools.c
REGRESS_SKIP=
.PATH: ${.CURDIR}/..
.include <bsd.regress.mk>
