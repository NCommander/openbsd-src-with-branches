#	$OpenBSD: Makefile,v 1.5 1995/03/18 14:57:45 cgd Exp $
#	$NetBSD: Makefile,v 1.5 1995/03/18 14:57:45 cgd Exp $

PROG=	mount_null
SRCS=	mount_null.c getmntopts.c
MAN=	mount_null.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
