# $OpenBSD: Makefile,v 1.11 2021/04/28 12:55:39 patrick Exp $

LIB=	LLVMCore
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/IR \
		-I${LLVM_SRCS}/include/llvm/IR

SRCS+=	AbstractCallSite.cpp \
	AsmWriter.cpp \
	Assumptions.cpp \
	Attributes.cpp \
	AutoUpgrade.cpp \
	BasicBlock.cpp \
	BuiltinGCs.cpp \
	Comdat.cpp \
	ConstantFold.cpp \
	ConstantRange.cpp \
	Constants.cpp \
	CoreCore.cpp \
	DIBuilder.cpp \
	DataLayout.cpp \
	DebugInfo.cpp \
	DebugInfoMetadata.cpp \
	DebugLoc.cpp \
	DiagnosticHandler.cpp \
	DiagnosticInfo.cpp \
	DiagnosticPrinter.cpp \
	Dominators.cpp \
	FPEnv.cpp \
	Function.cpp \
	GCStrategy.cpp \
	GVMaterializer.cpp \
	Globals.cpp \
	IRBuilder.cpp \
	IRPrintingPasses.cpp \
	InlineAsm.cpp \
	Instruction.cpp \
	Instructions.cpp \
	IntrinsicInst.cpp \
	LLVMContext.cpp \
	LLVMContextImpl.cpp \
	LLVMRemarkStreamer.cpp \
	LegacyPassManager.cpp \
	MDBuilder.cpp \
	Mangler.cpp \
	Metadata.cpp \
	Module.cpp \
	ModuleSummaryIndex.cpp \
	Operator.cpp \
	OptBisect.cpp \
	Pass.cpp \
	PassInstrumentation.cpp \
	PassManager.cpp \
	PassRegistry.cpp \
	PassTimingInfo.cpp \
	PrintPasses.cpp \
	SafepointIRVerifier.cpp \
	ProfileSummary.cpp \
	PseudoProbe.cpp \
	ReplaceConstant.cpp \
	Statepoint.cpp \
	StructuralHash.cpp \
	Type.cpp \
	TypeFinder.cpp \
	Use.cpp \
	User.cpp \
	Value.cpp \
	ValueSymbolTable.cpp \
	Verifier.cpp

CoreCore.cpp:
	ln -s ${LLVM_SRCS}/lib/IR/Core.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/IR

CLEANFILES+= CoreCore.cpp
