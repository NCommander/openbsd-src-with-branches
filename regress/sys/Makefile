#	$OpenBSD: Makefile,v 1.10 2002/06/11 21:55:57 provos Exp $
#	$NetBSD: Makefile,v 1.4 1995/04/20 22:41:08 cgd Exp $

SUBDIR+= crypto ddb fdescfs kern sys uvm
.if exists(arch/${MACHINE})
SUBDIR+= arch/${MACHINE}
.endif

install:

.include <bsd.subdir.mk>
