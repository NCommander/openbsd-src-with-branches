# $OpenBSD$

LIB=	LLVMProfileData
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/ProfileData

.include <bsd.own.mk>
SRCS=	InstrProf.cpp \
	InstrProfReader.cpp \
	InstrProfWriter.cpp \
	CoverageMapping.cpp \
	CoverageMappingWriter.cpp \
	CoverageMappingReader.cpp \
	SampleProf.cpp \
	SampleProfReader.cpp \
	SampleProfWriter.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/ProfileData

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
