# $OpenBSD: Makefile,v 1.5 2004/12/29 06:16:45 david Exp $

SUBDIR+= gcc libiberty

.if defined(REGRESS_FULL)
SUBDIR += gcc-bounds
.endif

install:

.include <bsd.subdir.mk>
