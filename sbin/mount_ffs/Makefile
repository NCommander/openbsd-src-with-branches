#	$OpenBSD: Makefile,v 1.2 1996/06/23 14:31:19 deraadt Exp $

PROG=	mount_ffs
SRCS=	mount_ffs.c getmntopts.c
MAN=	mount_ffs.8

MLINKS=	mount_ffs.8 mount_ufs.8
LINKS=  ${BINDIR}/mount_ffs ${BINDIR}/mount_ufs

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
