#	$OpenBSD: Makefile,v 1.4 1997/04/19 21:26:28 millert Exp $

PROG=	rlogin
SRCS=	rlogin.c
BINOWN=	root
BINMODE=4555
#INSTALLFLAGS=-fschg

.include <bsd.own.mk>

.if (${KERBEROS} == "yes")
CFLAGS+=-DKERBEROS
SRCS+=	krcmd.c kcmd.c des_rw.c
LDADD+=	-lkrb -ldes
DPADD+=	${LIBKRB} ${LIBDES}
.endif

.include <bsd.prog.mk>
