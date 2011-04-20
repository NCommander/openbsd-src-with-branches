#	$OpenBSD$

PROG=	mount_ext2fs
SRCS=	mount_ext2fs.c getmntopts.c
MAN=	mount_ext2fs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
