#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:37:36 deraadt Exp $

PROG=	mount_tcfs
SRCS=	mount_tcfs.c getmntopts.c
MAN=	mount_tcfs.8

MOUNT=	${.CURDIR}/../mount_nfs
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
