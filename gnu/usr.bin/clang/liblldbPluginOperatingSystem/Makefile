# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:36 espie Exp $

LIB=	lldbPluginOperatingSystem
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	OperatingSystemGo.cpp \
	OperatingSystemPython.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/OperatingSystem/Go
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/OperatingSystem/Python

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
