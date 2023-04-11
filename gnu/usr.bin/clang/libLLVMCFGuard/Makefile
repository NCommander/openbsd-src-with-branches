# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:40 jsg Exp $

LIB=	LLVMCFGuard
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms

SRCS+=	CFGuard.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/CFGuard
