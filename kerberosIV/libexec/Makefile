# $OpenBSD: Makefile,v 1.1 2000/02/25 16:35:26 hin Exp $

.include <bsd.own.mk> # for KERBEROS5

SUBDIR=	kauthd kerberos kpropd

.if (${KERBEROS5:L} != "yes")
SUBDIR+=kadmin 
.endif

.include <bsd.subdir.mk>