#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:37:26 deraadt Exp $

PROG=	mount_xfs
SRCS=	mount_xfs.c getmntopts.c
MAN=	mount_xfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
