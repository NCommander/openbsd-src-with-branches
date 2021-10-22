# $OpenBSD: Makefile,v 1.9 2020/08/03 14:45:28 patrick Exp $

LIB=	LLVMVectorize
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms \
	-I${LLVM_SRCS}/include/llvm/Transforms/Vectorize

SRCS+=	LoadStoreVectorizer.cpp \
	LoopVectorizationLegality.cpp \
	LoopVectorize.cpp \
	SLPVectorizer.cpp \
	Vectorize.cpp \
	VectorCombine.cpp \
	VPlan.cpp \
	VPlanHCFGBuilder.cpp \
	VPlanPredicator.cpp \
	VPlanSLP.cpp \
	VPlanTransforms.cpp \
	VPlanVerifier.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/Vectorize
