# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:26 patrick Exp $

LIB=	LLVMMIRParser
NOPROFILE=

CPPFLAGS+=-I${LLVM_SRC}/include/llvm/CodeGen/MIRParser

SRCS+=	MILexer.cpp \
	MIParser.cpp \
	MIRParser.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/CodeGen/MIRParser
