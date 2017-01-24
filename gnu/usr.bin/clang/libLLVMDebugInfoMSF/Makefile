# $OpenBSD: Makefile,v 1.1 2017/01/14 20:12:41 patrick Exp $

LIB=	LLVMDebugInfoMSF
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/DebugInfo/MSF

.include <bsd.own.mk>
SRCS=	MappedBlockStream.cpp \
	MSFBuilder.cpp \
	MSFCommon.cpp \
	MSFError.cpp \
	StreamReader.cpp \
	StreamWriter.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/DebugInfo/MSF

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
