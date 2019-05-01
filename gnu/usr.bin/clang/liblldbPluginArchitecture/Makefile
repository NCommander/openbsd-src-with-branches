# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

LIB=	lldbPluginArchitecture
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	ArchitectureArm.cpp \
	ArchitecturePPC64.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Architecture/Arm
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/Architecture/PPC64

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
