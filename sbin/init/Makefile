#	$OpenBSD: Makefile,v 1.2 1996/02/15 04:03:26 etheisen Exp $

PROG=	init
MAN=	init.8
#DPADD=	${LIBUTIL} ${LIBCRYPT}
DPADD=	${LIBUTIL} 
#LDADD=	-lutil -lcrypt
LDADD=	-lutil 
BINMODE=500
INSTALLFLAGS=-fschg
CFLAGS+=-DDEBUGSHELL -DSECURE

.include <bsd.prog.mk>
