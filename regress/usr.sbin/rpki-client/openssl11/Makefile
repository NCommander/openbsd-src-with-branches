# $OpenBSD: Makefile,v 1.17 2023/04/14 15:35:12 tb Exp $

LDADD +=	-Wl,-rpath,/usr/local/lib/eopenssl11 -L/usr/local/lib/eopenssl11
CFLAGS +=	-I${.CURDIR}/ -I/usr/local/include/eopenssl11/

.PATH:		${.CURDIR}/..

.include <bsd.regress.mk>
