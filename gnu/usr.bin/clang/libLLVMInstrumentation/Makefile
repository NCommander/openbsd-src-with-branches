# $OpenBSD: Makefile,v 1.8 2019/06/23 22:07:40 patrick Exp $

LIB=	LLVMInstrumentation
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms

SRCS+=	AddressSanitizer.cpp \
	BoundsChecking.cpp \
	CGProfile.cpp \
	ControlHeightReduction.cpp \
	DataFlowSanitizer.cpp \
	GCOVProfiling.cpp \
	HWAddressSanitizer.cpp \
	IndirectCallPromotion.cpp \
	Instrumentation.cpp \
	InstrOrderFile.cpp \
	InstrProfiling.cpp \
	MemorySanitizer.cpp \
	PGOInstrumentation.cpp \
	PGOMemOPSizeOpt.cpp \
	PoisonChecking.cpp \
	SanitizerCoverage.cpp \
	ThreadSanitizer.cpp \
	ValueProfileCollector.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/Instrumentation
