# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:40 jsg Exp $

LIB=	LLVMFrontendOpenMP
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Frontend
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Frontend/OpenMP

SRCS+=	OMPConstants.cpp \
	OMPIRBuilder.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Frontend/OpenMP
