# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:36 espie Exp $

LIB=	lldbPluginJITLoader
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	JITLoaderGDB.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/JITLoader/GDB

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
