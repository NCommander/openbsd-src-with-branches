#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:37:14 deraadt Exp $

PROG=	mount_ffs
SRCS=	mount_ffs.c getmntopts.c
MAN=	mount_ffs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
