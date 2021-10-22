# $OpenBSD: Makefile,v 1.1 2020/08/03 14:45:25 patrick Exp $

LIB=	LLVMFrontendOpenACC
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Frontend
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Frontend/OpenACC

SRCS+=	ACC.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Frontend/OpenACC

ACC.cpp:
	ln -sf ${.OBJDIR}/../include/llvm/Frontend/OpenACC/ACC.cpp $@

CLEANFILES+= ACC.cpp

