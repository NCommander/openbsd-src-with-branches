# $OpenBSD: Makefile,v 1.1.1.1 2007/07/31 20:31:42 kurt Exp $

.include <bsd.own.mk>

.if ${COMPILER_VERSION:L} == "gcc3" || ${COMPILER_VERSION:L} == "gcc4" 
SUBDIR+= libaa libab test1 test2
.endif

install:

.include <bsd.subdir.mk>
