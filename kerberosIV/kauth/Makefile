#	$OpenBSD: Makefile,v 1.5 1997/12/17 07:50:16 art Exp $

.include <bsd.obj.mk>

SRCS= encdata.c rkinit.c kauth.c marshall.c
PROG= kauth
LDADD+= -lkrb -lkafs -ldes
DPADD+=	${LIBKRB} ${LIBKAFS}
MAN=

.include <bsd.prog.mk>
