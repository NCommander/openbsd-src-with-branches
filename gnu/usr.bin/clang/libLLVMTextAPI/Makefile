# $OpenBSD: Makefile,v 1.2 2021/04/28 12:55:40 patrick Exp $

LIB=	LLVMTextAPI
NOPROFILE=

# TextAPI
SRCS+=	Architecture.cpp \
	ArchitectureSet.cpp \
	InterfaceFile.cpp \
	PackedVersion.cpp \
	Platform.cpp \
	Symbol.cpp \
	TextAPITarget.cpp \
	TextStub.cpp \
	TextStubCommon.cpp

TextAPITarget.cpp:
	ln -s ${LLVM_SRCS}/lib/TextAPI/Target.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/TextAPI

CLEANFILES+= TextAPITarget.cpp
