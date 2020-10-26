# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:40 jsg Exp $

LIB=	LLVMTextAPI
NOPROFILE=

SRCS+=	ELFStub.cpp \
	TBEHandler.cpp \
	Architecture.cpp \
	ArchitectureSet.cpp \
	InterfaceFile.cpp \
	PackedVersion.cpp \
	Platform.cpp \
	Symbol.cpp \
	TextAPITarget.cpp \
	TextStub.cpp \
	TextStubCommon.cpp

TextAPITarget.cpp:
	ln -s ${LLVM_SRCS}/lib/TextAPI/MachO/Target.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/TextAPI
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/TextAPI/ELF
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/TextAPI/MachO

CLEANFILES+= TextAPITarget.cpp
