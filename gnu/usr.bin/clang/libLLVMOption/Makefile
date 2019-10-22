# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMOption
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Option

SRCS+=	Arg.cpp \
	ArgList.cpp \
	Option.cpp \
	OptTable.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Option
