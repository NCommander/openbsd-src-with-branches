# $OpenBSD: Makefile,v 1.3 2017/10/04 20:57:09 patrick Exp $

LIB=	LLVMDebugInfoMSF
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/DebugInfo/MSF

SRCS+=	MappedBlockStream.cpp \
	MSFBuilder.cpp \
	MSFCommon.cpp \
	MSFError.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/DebugInfo/MSF
