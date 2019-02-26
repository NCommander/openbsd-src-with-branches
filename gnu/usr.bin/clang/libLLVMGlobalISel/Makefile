# $OpenBSD: Makefile,v 1.4 2019/01/27 16:59:41 patrick Exp $

LIB=	LLVMGlobalISel
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	CallLowering.cpp \
	GlobalISel.cpp \
	IRTranslator.cpp \
	InstructionSelect.cpp \
	InstructionSelector.cpp \
	LegalityPredicates.cpp \
	LegalizeMutations.cpp \
	Legalizer.cpp \
	LegalizerHelper.cpp \
	LegalizerInfo.cpp \
	Localizer.cpp \
	MachineIRBuilder.cpp \
	RegBankSelect.cpp \
	RegisterBank.cpp \
	RegisterBankInfo.cpp \
	GlobalISelUtils.cpp

GlobalISelUtils.cpp:
	ln -s ${LLVM_SRCS}/lib/CodeGen/GlobalISel/Utils.cpp $@

.PATH:	${.CURDIR}/../../../llvm/lib/CodeGen/GlobalISel

install:
	@# Nothing here so far ...

CLEANFILES+= GlobalISelUtils.cpp

.include <bsd.lib.mk>
