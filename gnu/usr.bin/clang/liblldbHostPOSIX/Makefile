# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:36 espie Exp $

LIB=	lldbHostPOSIX
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	ConnectionFileDescriptorPosix.cpp \
	DomainSocket.cpp \
	FileSystem.cpp \
	HostInfoPosix.cpp \
	HostProcessPosix.cpp \
	HostThreadPosix.cpp \
	LockFilePosix.cpp \
	PipePosix.cpp \
	ProcessLauncherPosixFork.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Host/posix

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
