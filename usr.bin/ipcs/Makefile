#	$OpenBSD: Makefile,v 1.2 1996/06/26 05:34:36 deraadt Exp $

PROG=	ipcs
BINGRP=	kmem
BINMODE= 2555
DPADD+=	${LIBKVM}
LDADD+=	-lkvm

.include <bsd.prog.mk>
