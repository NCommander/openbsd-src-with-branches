#	$OpenBSD: Makefile,v 1.9 2007/11/25 18:25:29 deraadt Exp $
#	$NetBSD: Makefile,v 1.7 1997/04/10 23:03:38 cgd Exp $

SUBDIR=	 nboot

.if ${MACHINE} == "alpha"
SUBDIR+= bootxx netboot libsa libz
.endif

SUBDIR+= setnetbootinfo installboot

.include <bsd.subdir.mk>
