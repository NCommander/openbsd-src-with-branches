#	$OpenBSD: Makefile,v 1.11 2004/06/22 21:12:00 otto Exp $

PROG=	mount_nfs
SRCS=	mount_nfs.c getmntopts.c
MAN=	mount_nfs.8

MOUNT=	${.CURDIR}/../../../sbin/mount
CFLAGS+= -DNFS -I${MOUNT}
.PATH:	${MOUNT}

.PATH:  ${.CURDIR}/../../../sbin/mount_nfs
.include <bsd.prog.mk>
