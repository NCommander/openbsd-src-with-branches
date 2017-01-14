# $OpenBSD$

LIB=	LLVMCoverage
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/ProfileData/Coverage

.include <bsd.own.mk>
SRCS=	CoverageMapping.cpp \
	CoverageMappingReader.cpp \
	CoverageMappingWriter.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/ProfileData/Coverage

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
