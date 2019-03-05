# $OpenBSD: Makefile,v 1.6 2019/01/27 16:59:41 patrick Exp $

LIB=	LLVMInstrumentation
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms

SRCS+=	AddressSanitizer.cpp \
	BoundsChecking.cpp \
	CGProfile.cpp \
	DataFlowSanitizer.cpp \
	EfficiencySanitizer.cpp \
	GCOVProfiling.cpp \
	HWAddressSanitizer.cpp \
	IndirectCallPromotion.cpp \
	Instrumentation.cpp \
	InstrProfiling.cpp \
	MemorySanitizer.cpp \
	PGOInstrumentation.cpp \
	PGOMemOPSizeOpt.cpp \
	SanitizerCoverage.cpp \
	ThreadSanitizer.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Transforms/Instrumentation
