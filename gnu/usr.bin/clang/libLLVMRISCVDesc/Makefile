# $OpenBSD: Makefile,v 1.5 2020/08/03 14:45:24 patrick Exp $

LIB=	LLVMRISCVDesc
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/RISCV \
		-I${LLVM_SRCS}/lib/Target/RISCV

SRCS+=	RISCVAsmBackend.cpp \
	RISCVELFObjectWriter.cpp \
	RISCVELFStreamer.cpp \
	RISCVInstPrinter.cpp \
	RISCVMCAsmInfo.cpp \
	RISCVMCCodeEmitter.cpp \
	RISCVMCExpr.cpp \
	RISCVMCTargetDesc.cpp \
	RISCVTargetStreamer.cpp


.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/RISCV/MCTargetDesc
