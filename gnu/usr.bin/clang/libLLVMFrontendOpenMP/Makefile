# $OpenBSD: Makefile,v 1.1 2020/08/03 14:45:25 patrick Exp $

LIB=	LLVMFrontendOpenMP
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Frontend
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Frontend/OpenMP

SRCS+=	OMP.cpp \
	OMPContext.cpp \
	OMPIRBuilder.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Frontend/OpenMP

OMP.cpp:
	ln -sf ${.OBJDIR}/../include/llvm/Frontend/OpenMP/OMP.cpp $@

CLEANFILES+= OMP.cpp
