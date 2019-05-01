# $OpenBSD: Makefile,v 1.2 2019/01/27 16:59:41 patrick Exp $

LIB=	LLVMBinaryFormat
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/BinaryFormat

SRCS+=	Dwarf.cpp \
	Magic.cpp \
	Wasm.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/BinaryFormat
