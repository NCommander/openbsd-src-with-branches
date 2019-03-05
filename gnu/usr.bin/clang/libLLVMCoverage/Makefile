# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMCoverage
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/ProfileData/Coverage

SRCS+=	CoverageMapping.cpp \
	CoverageMappingReader.cpp \
	CoverageMappingWriter.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/ProfileData/Coverage
