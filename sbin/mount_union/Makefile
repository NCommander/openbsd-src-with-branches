#	$OpenBSD: Makefile,v 1.5 1995/03/18 14:58:21 cgd Exp $
#	$NetBSD: Makefile,v 1.5 1995/03/18 14:58:21 cgd Exp $

PROG=	mount_union
SRCS=	mount_union.c getmntopts.c
MAN=	mount_union.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
