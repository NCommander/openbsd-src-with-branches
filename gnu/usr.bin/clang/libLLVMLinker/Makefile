# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMLinker
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Linker

SRCS+=	IRMover.cpp \
	LinkModules.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Linker
