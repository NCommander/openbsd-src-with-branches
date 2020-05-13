# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMPasses
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Passes

SRCS+=	PassBuilder.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Passes
