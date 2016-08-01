#	$OpenBSD: Makefile,v 1.1 1996/06/27 07:20:28 downsj Exp $

PROG=	mount_ext2fs
SRCS=	mount_ext2fs.c getmntopts.c
MAN=	mount_ext2fs.8

MOUNT=	${.CURDIR}/../../../sbin/mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.PATH:  ${.CURDIR}/../../../sbin/mount_ext2fs
.include <bsd.prog.mk>
