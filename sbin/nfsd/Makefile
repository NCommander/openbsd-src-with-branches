#	$OpenBSD: Makefile,v 1.4 1997/04/19 21:26:23 millert Exp $

PROG=	nfsd
MAN=	nfsd.8

.include <bsd.own.mk>	# For KERBEROS

.if (${KERBEROS} == "yes")
CFLAGS+=-DKERBEROS
LDADD+=	-lkrb -ldes
DPADD+=	${LIBKRB} ${LIBDES}
.endif

.include <bsd.prog.mk>
