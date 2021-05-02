# $OpenBSD: Makefile,v 1.2 2019/01/27 16:59:42 patrick Exp $

LIB=	lldbPluginDynamicLoader
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	DYLDRendezvous.cpp \
	DynamicLoaderDarwin.cpp \
	DynamicLoaderDarwinKernel.cpp \
	DynamicLoaderHexagonDYLD.cpp \
	DynamicLoaderMacOS.cpp \
	DynamicLoaderMacOSXDYLD.cpp \
	DynamicLoaderPOSIXDYLD.cpp \
	DynamicLoaderStatic.cpp \
	DynamicLoaderWindowsDYLD.cpp \
	HexagonDYLDRendezvous.cpp \

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/DynamicLoader/Darwin-Kernel
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/DynamicLoader/Hexagon-DYLD
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/DynamicLoader/MacOSX-DYLD
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/DynamicLoader/POSIX-DYLD
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/DynamicLoader/Static
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/DynamicLoader/Windows-DYLD

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${.OBJDIR}/../include/lldb/Plugins
