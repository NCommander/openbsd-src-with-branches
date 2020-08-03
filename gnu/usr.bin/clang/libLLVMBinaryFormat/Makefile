# $OpenBSD: Makefile,v 1.4 2019/06/23 22:07:39 patrick Exp $

LIB=	LLVMBinaryFormat
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/BinaryFormat

SRCS+=	AMDGPUMetadataVerifier.cpp \
	BinaryFormatMinidump.cpp \
	Dwarf.cpp \
	Magic.cpp \
	MsgPackDocument.cpp \
	MsgPackDocumentYAML.cpp \
	MsgPackReader.cpp \
	MsgPackWriter.cpp \
	Wasm.cpp \
	XCOFF.cpp

BinaryFormatMinidump.cpp:
	ln -s ${LLVM_SRCS}/lib/BinaryFormat/Minidump.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/BinaryFormat

CLEANFILES+= BinaryFormatMinidump.cpp
