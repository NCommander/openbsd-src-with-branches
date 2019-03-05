# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMPowerPCDesc
NOPROFILE=

CPPFLAGS+=      -I${.OBJDIR}/../include/llvm/PowerPC \
		-I${LLVM_SRCS}/lib/Target/PowerPC

SRCS+=	PPCAsmBackend.cpp \
	PPCMCTargetDesc.cpp \
	PPCMCAsmInfo.cpp \
	PPCMCCodeEmitter.cpp \
	PPCMCExpr.cpp \
	PPCPredicates.cpp \
	PPCMachObjectWriter.cpp \
	PPCELFObjectWriter.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/PowerPC/MCTargetDesc
