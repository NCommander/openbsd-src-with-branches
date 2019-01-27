# $OpenBSD: Makefile,v 1.5 2018/04/06 14:44:04 patrick Exp $

LIB=	LLVMVectorize
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms

.include <bsd.own.mk>
SRCS=	LoadStoreVectorizer.cpp \
	LoopVectorizationLegality.cpp \
	LoopVectorize.cpp \
	SLPVectorizer.cpp \
	VPlan.cpp \
	VPlanHCFGBuilder.cpp \
	VPlanVerifier.cpp \
	Vectorize.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Transforms/Vectorize

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
