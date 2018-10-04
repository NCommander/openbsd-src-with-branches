# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:36 espie Exp $

LIB=	lldbPluginObjectFile
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	ELFHeader.cpp \
	ObjectFileELF.cpp \
	ObjectFileJIT.cpp \
	ObjectFileMachO.cpp \
	ObjectFilePECOFF.cpp \
	WindowsMiniDump.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ObjectFile/ELF
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ObjectFile/JIT
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ObjectFile/Mach-O
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ObjectFile/PECOFF

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
