#	$OpenBSD$

PROG=	mount_udf
SRCS=	mount_udf.c getmntopts.c
MAN=

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
