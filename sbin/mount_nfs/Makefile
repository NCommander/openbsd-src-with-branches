#	$OpenBSD: Makefile,v 1.8 2001/10/03 18:50:07 hin Exp $

PROG=	mount_nfs
SRCS=	mount_nfs.c getmntopts.c
MAN=	mount_nfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -DNFS -I${MOUNT}
.PATH:	${MOUNT}
CFLAGS+=-DNFS

.include <bsd.prog.mk>
