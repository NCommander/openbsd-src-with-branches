#	$OpenBSD: Makefile,v 1.1 1997/12/18 22:28:02 art Exp $

.include <bsd.obj.mk>

SRCS= ../kauth/encdata.c kauthd.c ../kauth/marshall.c inaddr2str.c mini_inetd.c
PROG= kauthd
LDADD+= -lkrb -lkafs -ldes
DPADD+=	${LIBKRB} ${LIBKAFS}
CFLAGS+= -I${.CURDIR}/../kauth
BINDIR=/usr/libexec
MAN= kauthd.8

.include <bsd.prog.mk>
