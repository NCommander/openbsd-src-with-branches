# $OpenBSD: Makefile,v 1.4 2019/06/23 22:07:40 patrick Exp $

LIB=	LLVMDemangle
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Demangle

SRCS+=	Demangle.cpp \
	ItaniumDemangle.cpp \
	MicrosoftDemangle.cpp \
	MicrosoftDemangleNodes.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Demangle
