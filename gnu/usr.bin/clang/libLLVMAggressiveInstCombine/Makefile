# $OpenBSD: Makefile,v 1.5 2020/08/03 14:45:26 patrick Exp $

LIB=	LLVMAggressiveInstCombine
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms

SRCS+=	AggressiveInstCombine.cpp \
	TruncInstCombine.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/AggressiveInstCombine
