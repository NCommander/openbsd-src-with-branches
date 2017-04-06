# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:50 pascal Exp $

LIB=	lldConfig
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/tools/lld/include

.include <bsd.own.mk>
SRCS=	Version.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/lld/lib/Config

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
