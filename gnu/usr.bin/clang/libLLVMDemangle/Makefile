# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMDemangle
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Demangle

SRCS+=	ItaniumDemangle.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Demangle
