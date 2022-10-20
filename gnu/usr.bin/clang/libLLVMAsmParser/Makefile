# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:25 patrick Exp $

LIB=	LLVMAsmParser
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/ASMParser

SRCS+=	LLLexer.cpp \
	LLParser.cpp \
	Parser.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/AsmParser
