#	$OpenBSD: Makefile,v 1.3 1996/06/23 14:30:56 deraadt Exp $

PROG=	ipfstat
MAN=	ipfstat.8
SRCS=	fils.c parse.c opt.c kmem.c 
.PATH:  ${.CURDIR}/../../sbin/ipf
CFLAGS+=-I${.CURDIR}/../../sbin/ipf



.include <bsd.prog.mk>
