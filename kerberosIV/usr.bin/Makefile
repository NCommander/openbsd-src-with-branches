# $OpenBSD: Makefile,v 1.2 2000/08/31 19:44:50 hin Exp $

.include <bsd.own.mk> # for KERBEROS5

SUBDIR=	afslog

.if(${KERBEROS5:L} != "yes")
SUBDIR+=kauth kdestroy kinit klist pagsh
.endif

# these are not built: kftp kstring2key

.include <bsd.subdir.mk>
