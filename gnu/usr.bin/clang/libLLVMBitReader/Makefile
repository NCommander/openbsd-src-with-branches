# $OpenBSD: Makefile,v 1.4 2019/03/05 09:44:40 jsg Exp $

LIB=	LLVMBitReader
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Bitcode

SRCS+=	BitcodeAnalyzer.cpp\
	BitReader.cpp \
	BitcodeReader.cpp \
	MetadataLoader.cpp \
	ValueList.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Bitcode/Reader
