# $OpenBSD: Makefile,v 1.1 2000/02/25 16:35:32 hin Exp $

.include <bsd.own.mk> # for KERBEROS5

SUBDIR=	ext_srvtab kdb_destroy kdb_init kdb_util kdb_edit kstash ksrvutil \
	kprop

.if(${KERBEROS5:L} != "yes")
kadmin 
.endif

.include <bsd.subdir.mk>