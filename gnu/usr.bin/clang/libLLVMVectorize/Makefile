# $OpenBSD: Makefile,v 1.6 2019/01/27 16:59:42 patrick Exp $

LIB=	LLVMVectorize
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms

SRCS+=	LoadStoreVectorizer.cpp \
	LoopVectorizationLegality.cpp \
	LoopVectorize.cpp \
	SLPVectorizer.cpp \
	VPlan.cpp \
	VPlanHCFGBuilder.cpp \
	VPlanVerifier.cpp \
	Vectorize.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Transforms/Vectorize
