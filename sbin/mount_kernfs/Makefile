#	$OpenBSD: Makefile,v 1.6 1995/03/18 14:57:23 cgd Exp $
#	$NetBSD: Makefile,v 1.6 1995/03/18 14:57:23 cgd Exp $

PROG=	mount_kernfs
SRCS=	mount_kernfs.c getmntopts.c
MAN=	mount_kernfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
