# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:40 jsg Exp $

LIB=	LLVMBinaryFormat
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/BinaryFormat

SRCS+=	AMDGPUMetadataVerifier.cpp \
	Dwarf.cpp \
	Magic.cpp \
	MsgPackReader.cpp \
	MsgPackTypes.cpp \
	MsgPackWriter.cpp \
	Wasm.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/BinaryFormat
