#	$OpenBSD: Makefile,v 1.3 1998/01/26 04:13:45 dgregor Exp $

PROG=	ipnat
MAN=	ipnat.1 ipnat.4 ipnat.5
SRCS=	ipnat.c kmem.c 
.PATH:  ${.CURDIR}/../ipfstat
CFLAGS+=-I${.CURDIR}/../../sbin/ipfstat

.include <bsd.prog.mk>
