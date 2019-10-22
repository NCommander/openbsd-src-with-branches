# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:36 espie Exp $

LIB=	lldbPluginMemoryHistory
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	MemoryHistoryASan.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/MemoryHistory/asan

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
