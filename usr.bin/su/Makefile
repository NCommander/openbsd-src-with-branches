#	$OpenBSD: Makefile,v 1.5 1997/03/26 00:35:37 deraadt Exp $

PROG=	su
BINOWN=	root
BINMODE=4555

.include <bsd.own.mk>

.if (${SKEY} == "yes")
CFLAGS+=-DSKEY
LDADD+= -lskey
DPADD+= ${LIBSKEY}
.endif

.if (${KERBEROS} == "yes")
CFLAGS+=-DKERBEROS
LDADD+=	-lkrb -ldes
DPADD+=	${LIBKRB} ${LIBDES}
.endif

.include <bsd.prog.mk>
