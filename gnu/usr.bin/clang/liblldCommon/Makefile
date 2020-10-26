# $OpenBSD: Makefile,v 1.1 2018/04/06 14:44:04 patrick Exp $

LIB=	lldCommon
NOPROFILE=

CPPFLAGS+=	-I${LLD_SRCS}/include

.include <bsd.own.mk>
SRCS=	Args.cpp \
	DWARF.cpp \
	ErrorHandler.cpp \
	Filesystem.cpp \
	Memory.cpp \
	Reproduce.cpp \
	Strings.cpp \
	TargetOptionsCommandFlags.cpp \
	Threads.cpp \
	Version.cpp

.PATH:	${.CURDIR}/../../../llvm/lld/Common

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
