# $OpenBSD: Makefile,v 1.4 2019/03/05 09:44:40 jsg Exp $

LIB=	LLVMBitstreamReader
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Bitcode
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Bitstream

SRCS+=	BitstreamReader.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Bitstream/Reader
