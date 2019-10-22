# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:36 espie Exp $

LIB=	lldCommon
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/tools/lld/include

.include <bsd.own.mk>
SRCS=	Args.cpp \
	ErrorHandler.cpp \
	Memory.cpp \
	Reproduce.cpp \
	Strings.cpp \
	TargetOptionsCommandFlags.cpp \
	Threads.cpp \
	Version.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/lld/Common

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
