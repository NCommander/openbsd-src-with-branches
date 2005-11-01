#	$OpenBSD: Makefile,v 1.5 1997/01/03 16:22:09 mickey Exp $

PROG=	quotacheck
SRCS=	quotacheck.c preen.c fsutil.c
CFLAGS+=-I${.CURDIR}/../fsck
MAN=	quotacheck.8
.PATH:	${.CURDIR}/../fsck

.include <bsd.prog.mk>
