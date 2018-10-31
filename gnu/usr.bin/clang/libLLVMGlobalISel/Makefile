# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMGlobalISel
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	CallLowering.cpp \
	GlobalISel.cpp \
	IRTranslator.cpp \
	InstructionSelect.cpp \
	InstructionSelector.cpp \
	Legalizer.cpp \
	LegalizerHelper.cpp \
	LegalizerInfo.cpp \
	Localizer.cpp \
	MachineIRBuilder.cpp \
	RegBankSelect.cpp \
	RegisterBank.cpp \
	RegisterBankInfo.cpp \
	Utils.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/CodeGen/GlobalISel

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
