#	$OpenBSD: Makefile,v 1.10 2002/06/10 22:17:38 weingart Exp $

.if ${MACHINE} == "amd64"
SUBDIR=	etc mbr
.endif
SUBDIR+= biosboot installboot boot

.include <bsd.subdir.mk>
