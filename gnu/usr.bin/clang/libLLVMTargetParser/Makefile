# $OpenBSD$

LIB=	LLVMTargetParser
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/TargetParser

SRCS+=	AArch64TargetParser.cpp \
	ARMTargetParserCommon.cpp \
	ARMTargetParser.cpp \
	CSKYTargetParser.cpp \
	Host.cpp \
	LoongArchTargetParser.cpp \
	RISCVTargetParser.cpp \
	TargetParser.cpp \
	Triple.cpp \
	X86TargetParser.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/TargetParser
