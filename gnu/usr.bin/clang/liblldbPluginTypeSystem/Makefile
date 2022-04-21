# $OpenBSD: Makefile,v 1.2 2020/08/03 14:45:31 patrick Exp $

LIB=	lldbPluginTypeSystem
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	TypeSystemClang.cpp \

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/TypeSystem/Clang

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
