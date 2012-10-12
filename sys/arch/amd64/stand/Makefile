#	$OpenBSD: Makefile,v 1.2 2004/03/21 21:37:40 tom Exp $

.if ${MACHINE} == "amd64"
SUBDIR=	etc mbr cdbr
.endif
SUBDIR+= biosboot boot cdboot fdboot installboot pxeboot

.include <bsd.subdir.mk>
