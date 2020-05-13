# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:36 espie Exp $

LIB=	lldbPluginObjectContainer
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	ObjectContainerBSDArchive.cpp \
	ObjectContainerUniversalMachO.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ObjectContainer/BSD-Archive
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ObjectContainer/Universal-Mach-O

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
