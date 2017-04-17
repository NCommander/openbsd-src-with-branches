#	$OpenBSD: Makefile,v 1.5 2013/07/06 23:06:36 espie Exp $

.include <bsd.own.mk>

SUBDIR+= X11 c++abi exceptions sse2

.if ${COMPILER_VERSION:L} != "clang"
SUBDIR+= objc
.endif

install:

.include <bsd.subdir.mk>
